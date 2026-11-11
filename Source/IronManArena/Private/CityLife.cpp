#include "CityLife.h"
#include "WantedPressure.h"
#include "CareerEconomy.h"
#include "ArenaGameUserSettings.h"
#include "ArenaAudio.h"
#include "Components/AudioComponent.h"

#include "ArenaBlastFX.h"
#include "ArenaGameMode.h"
#include "CareerSubsystem.h"
#include "DowntownDistrict.h"
#include "EnemyBase.h"
#include "IronManCharacter.h"
#include "NpcManifest.h"
#include "PilotAvatar.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "CityLife"

namespace
{
	const FName ResponderTag(TEXT("WantedResponder"));
	const FName GunshipTag(TEXT("WantedGunship"));
	constexpr float StarHeat[5] = { 50.f, 150.f, 350.f, 700.f, 1200.f };

	AIronManCharacter* PlayerOf(const UObject* Ctx)
	{
		return Ctx ? Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(Ctx, 0)) : nullptr;
	}
}

// ============================================================================================ CityLife::

void CityLife::Damage(UWorld* World, const FVector& Location, float Radius, float Amount, AActor* DirectHit, bool bFromPlayer)
{
	ACityLife* Life = World ? ACityLife::Find(World) : nullptr;
	if (!Life || Amount <= 0.f) { return; }
	ADowntownDistrict* Dist = Life->GetDistrict();
	// Parked cars are HISM instances until something hits them: swap the nearby ones for live actors first.
	if (Dist && Dist->IsDistrictVisible())
	{
		FTransform Xf; int32 Paint = 0;
		for (int32 n = 0; n < 4 && Dist->TakeParkedCarNear(Location, Radius + 260.f, Xf, Paint); ++n)
		{
			if (ACityCar* Woke = Life->WakeParkedCar(Xf, Paint))
			{
				if (!DirectHit && n == 0 && FVector::Dist(Woke->GetActorLocation(), Location) < 320.f) { DirectHit = Woke; }
			}
		}
	}
	TArray<AActor*> Hit;
	for (TActorIterator<ACityCar> It(World); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Location) < Radius + 230.f || *It == DirectHit) { Hit.AddUnique(*It); }
	}
	for (ACityPedestrian* P : Life->GetPeds())
	{
		if (P && !P->IsPooled() && (FVector::Dist(P->GetActorLocation() + FVector(0, 0, 90.f), Location) < Radius + 40.f || P == DirectHit)) { Hit.AddUnique(P); }
	}
	for (AActor* A : Hit)
	{
		const float Fall = (A == DirectHit) ? 1.f : FMath::Clamp(1.f - FVector::Dist(A->GetActorLocation(), Location) / FMath::Max(1.f, Radius + 230.f), 0.25f, 1.f);
		if (ACityCar* Car = Cast<ACityCar>(A)) { Car->ApplyCityDamage(Amount * Fall, Location, bFromPlayer); }
		else if (ACityPedestrian* Ped = Cast<ACityPedestrian>(A)) { Ped->ApplyCityDamage(Amount * Fall, Location, bFromPlayer); }
	}
	Life->ScareAround(Location, Radius > 300.f ? 4500.f : 2000.f, Radius > 300.f);
}

bool CityLife::IsResponder(const AActor* Actor) { return Actor && Actor->Tags.Contains(ResponderTag); }

// ============================================================================================ ACityCar

ACityCar::ACityCar()
{
	PrimaryActorTick.bCanEverTick = true;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(225.f, 92.f, 70.f));
	Box->SetCollisionProfileName(TEXT("PhysicsActor"));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetRootComponent(Box);
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Box);
	Body->SetRelativeLocation(FVector(0, 0, -70.f));
}

void ACityCar::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// SetSimulatePhysics / SetMassOverrideInKg query the physical material through
	// FBodyInstance, which logs an Error (and fails the cook commandlet) if called
	// while constructing the native CDO — GEngine is not up yet.
	if (Box && !HasAnyFlags(RF_ClassDefaultObject))
	{
		Box->SetSimulatePhysics(false);
		Box->SetMassOverrideInKg(NAME_None, 1400.f, true);
	}
}

void ACityCar::Setup(int32 Paint, ECityCarMode InMode)
{
	PaintIndex = Paint;
	Mode = InMode;
	Health = MaxHealth;
	BodyVariant = FMath::Abs(Paint) % 3;
	const TArray<FLinearColor>& Paints = ADowntownDistrict::CarPaints();
	FLinearColor PaintC = Paints.Num() ? Paints[FMath::Abs(Paint) % Paints.Num()] : FLinearColor::Red;
	if (BodyVariant == 2) { PaintC = FLinearColor(0.85f, 0.75f, 0.15f); } // taxi-ish
	auto Put = [this](EArenaShape S, const FVector& L, const FVector& Size, const FLinearColor& C, EArenaSurface Surf, float Em = 0.f, const FRotator& R = FRotator::ZeroRotator)
	{
		ArenaVisuals::AddRuntimePart(this, Body, S, L, Size / 100.f, R, C, Em, Surf, false, Parts);
	};
	const float Len = BodyVariant == 0 ? 400.f : BodyVariant == 1 ? 450.f : 500.f;
	const float Wid = BodyVariant == 2 ? 195.f : 185.f;
	const float CabH = BodyVariant == 2 ? 95.f : 70.f;
	Put(EArenaShape::Cube, FVector(0, 0, 60.f), FVector(Len, Wid, CabH), PaintC, EArenaSurface::SuitPaint);
	Put(EArenaShape::Cube, FVector(BodyVariant == 0 ? -20.f : -30.f, 0, 112.f), FVector(BodyVariant == 2 ? 280.f : 230.f, Wid - 17.f, BodyVariant == 2 ? 55.f : 40.f), PaintC, EArenaSurface::SuitPaint);
	Put(EArenaShape::Cube, FVector(-30.f, 0, BodyVariant == 2 ? 155.f : 135.f), FVector(220.f, Wid - 15.f, 30.f), FLinearColor(0.02f, 0.025f, 0.03f), EArenaSurface::Steel);
	for (const FVector2D& W : { FVector2D(140, 82), FVector2D(140, -82), FVector2D(-140, 82), FVector2D(-140, -82) })
	{
		Put(EArenaShape::Cylinder, FVector(W.X, W.Y, 33.f), FVector(66.f, 66.f, 24.f), FLinearColor(0.015f, 0.015f, 0.015f), EArenaSurface::Plain, 0.f, FRotator(0.f, 0.f, 90.f));
	}
	LightParts.Add(Parts.Num());
	Put(EArenaShape::Cube, FVector(226.f, 0, 70.f), FVector(4.f, 150.f, 14.f), FLinearColor(1.f, 0.95f, 0.85f), EArenaSurface::Plain, InMode == ECityCarMode::Traffic ? 0.5f : 0.f);
	LightParts.Add(Parts.Num());
	Put(EArenaShape::Cube, FVector(-226.f, 0, 78.f), FVector(4.f, 150.f, 12.f), FLinearColor(1.f, 0.02f, 0.02f), EArenaSurface::Plain, InMode == ECityCarMode::Traffic ? 0.5f : 0.1f);
	for (FArenaPaintedPart& P : Parts) { if (P.Component) { P.Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	// Smoke puffs (hidden until damaged).
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector L(150.f, (i - 2) * 25.f, 110.f);
		SmokeBase.Add(L);
		UStaticMeshComponent* C = ArenaVisuals::AddRuntimePart(this, Body, EArenaShape::Sphere, L, FVector(0.8f), FRotator::ZeroRotator,
			FLinearColor(0.12f, 0.12f, 0.12f), 0.f, EArenaSurface::Smoke, false, SmokeParts);
		if (C) { C->SetVisibility(false); C->SetCastShadow(false); }
	}
	FireLight = NewObject<UPointLightComponent>(this);
	FireLight->SetupAttachment(Body);
	FireLight->SetRelativeLocation(FVector(150.f, 0, 160.f));
	FireLight->SetIntensityUnits(ELightUnits::Lumens);
	FireLight->SetIntensity(0.f);
	FireLight->SetLightColor(FLinearColor(1.f, 0.45f, 0.12f));
	FireLight->SetAttenuationRadius(900.f);
	FireLight->SetCastShadows(false);
	FireLight->RegisterComponent();
}

void ACityCar::StartDriving(int32 Axis, float Dir, float Speed)
{
	Mode = ECityCarMode::Traffic;
	DriveAxis = Axis;
	DriveDir = Dir;
	DriveSpeed = Speed;
	CurrentSpeed = Speed;
	bRecycle = false;
	SetActorRotation(FRotator(0.f, Axis == 0 ? (Dir > 0.f ? 0.f : 180.f) : (Dir > 0.f ? 90.f : 270.f), 0.f));
}

void ACityCar::ApplyCityDamage(float Amount, const FVector& From, bool bFromPlayer)
{
	if (Mode == ECityCarMode::Wreck) { return; }
	const bool bWasTraffic = Mode == ECityCarMode::Traffic;
	Health -= Amount;
	bLastHitByPlayer = bFromPlayer || bLastHitByPlayer;
	if (bWasTraffic) { CurrentSpeed = 0.f; DriveSpeed = 0.f; } // driver bails / stops
	if (bFromPlayer)
	{
		if (ACityLife* L = ACityLife::Find(this)) { L->AddCrime(12.f, GetActorLocation()); } // ULTRON W3 car scrape
	}
	if (Health <= 0.f) { Explode(bLastHitByPlayer); }
}

void ACityCar::Explode(bool bFromPlayer)
{
	Mode = ECityCarMode::Wreck;
	Health = 0.f;
	UWorld* World = GetWorld();
	const FVector Here = GetActorLocation();
	AArenaBlastFX::Spawn(World, Here + FVector(0, 0, 60.f), 650.f, FLinearColor(1.f, 0.45f, 0.1f), 0.7f);
	ArenaAudio::PlayAt(this, TEXT("explosion_car"), Here, 1.2f);
	ArenaAudio::PlayAt(this, TEXT("glass_break"), Here, 0.8f);
	// Charred husk.
	for (FArenaPaintedPart& P : Parts)
	{
		P.Color = FLinearColor(0.025f, 0.022f, 0.02f);
		P.Emissive = 0.f;
		ArenaVisuals::RepaintPart(P);
	}
	BurnTimer = 14.f;
	// Physics kick: up and away from the blast centre with a tumble.
	if (Box)
	{
		Box->SetSimulatePhysics(true);
		Box->AddImpulse(FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 650.f), NAME_None, true);
		Box->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-260.f, 260.f), FMath::FRandRange(-60.f, 60.f)), NAME_None, true);
		PhysicsTimer = 5.f;
	}
	if (ACityLife* L = ACityLife::Find(this))
	{
		if (bFromPlayer) { L->AddCrime(70.f, Here); } // ULTRON W3 car explode
		L->ScareAround(Here, 5000.f, true);
	}
	// Hurt the player and enemies close by.
	if (AIronManCharacter* Pl = PlayerOf(this))
	{
		if (FVector::Dist(Pl->GetActorLocation(), Here) < 550.f) { UGameplayStatics::ApplyDamage(Pl, 12.f, nullptr, this, UDamageType::StaticClass()); }
	}
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (!It->IsDead() && FVector::Dist(It->GetActorLocation(), Here) < 650.f) { UGameplayStatics::ApplyDamage(*It, 6.f, nullptr, this, UDamageType::StaticClass()); }
	}
	// Chain reaction (a frame later, so blasts do not recurse inside one call).
	FTimerHandle H;
	TWeakObjectPtr<ACityCar> Self(this);
	World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, Here, bFromPlayer]()
	{
		if (Self.IsValid()) { CityLife::Damage(Self->GetWorld(), Here, 520.f, 6.f, nullptr, bFromPlayer); }
	}), 0.18f, false);
}

void ACityCar::TickDamageFX(float DeltaSeconds)
{
	FxTime += DeltaSeconds;
	const float Frac = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	const bool bSmoke = Mode == ECityCarMode::Wreck || Frac < 0.6f;
	const bool bFire = Mode == ECityCarMode::Wreck ? BurnTimer > 0.f : Frac < 0.3f;
	for (int32 i = 0; i < SmokeParts.Num(); ++i)
	{
		UStaticMeshComponent* C = SmokeParts[i].Component;
		if (!C) { continue; }
		C->SetVisibility(bSmoke);
		if (!bSmoke) { continue; }
		const float T = FMath::Fmod(FxTime * 0.45f + i * 0.2f, 1.f);
		C->SetRelativeLocation(SmokeBase[i] + FVector(-T * 120.f, FMath::Sin(FxTime + i) * 30.f, T * 520.f));
		const float S = 0.6f + T * (bFire ? 3.2f : 2.2f);
		C->SetRelativeScale3D(FVector(S));
		SmokeParts[i].Color = bFire ? FLinearColor(0.05f, 0.045f, 0.04f) : FLinearColor(0.2f, 0.2f, 0.2f) * (1.f - T * 0.4f);
		if ((int32(FxTime * 10.f) + i) % 6 == 0) { ArenaVisuals::RepaintPart(SmokeParts[i]); }
	}
	if (FireLight)
	{
		FireLight->SetIntensity(bFire ? 9000.f * (0.75f + 0.25f * FMath::Sin(FxTime * 23.f) * FMath::Sin(FxTime * 7.f)) : 0.f);
	}
	if (Mode != ECityCarMode::Wreck && bFire)
	{
		// Burning cars cook off after a while (GTA rule).
		Health -= DeltaSeconds * 0.18f;
		if (Health <= 0.f) { Explode(bLastHitByPlayer); }
	}
}

void ACityCar::TickTraffic(float DeltaSeconds)
{
	ACityLife* Life = ACityLife::Find(this);
	ADowntownDistrict* Dist = Life ? Life->GetDistrict() : nullptr;
	if (!Dist || bRecycle) { return; }
	const FVector Fwd = DriveAxis == 0 ? FVector(DriveDir, 0, 0) : FVector(0, DriveDir, 0);
	const FVector Here = GetActorLocation();
	if (StopHold > 0.f)
	{
		StopHold = FMath::Max(0.f, StopHold - DeltaSeconds);
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.f, DeltaSeconds, 4.f);
		return;
	}
	// Brake for the player, pedestrians and cars ahead in the lane.
	float Want = DriveSpeed;
	auto Blocks = [&](const FVector& P, float Ahead)
	{
		const FVector D = P - Here;
		const float Along = FVector::DotProduct(D, Fwd);
		const float Side = (D - Fwd * Along).Size2D();
		return Along > 0.f && Along < Ahead && Side < 260.f && FMath::Abs(D.Z) < 400.f;
	};
	if (AIronManCharacter* Pl = PlayerOf(this)) { if (Blocks(Pl->GetActorLocation(), 1400.f)) { Want = 0.f; } }
	for (ACityCar* C : Life->GetCars()) { if (C && C != this && Blocks(C->GetActorLocation(), 1000.f)) { Want = 0.f; break; } }
	if (Want > 0.f) { for (ACityPedestrian* P : Life->GetPeds()) { if (P && !P->IsPooled() && Blocks(P->GetActorLocation(), 900.f)) { Want = 0.f; break; } } }
	CurrentSpeed = FMath::FInterpTo(CurrentSpeed, Want, DeltaSeconds, Want < CurrentSpeed ? 3.f : 0.8f);
	FVector Next = Here + Fwd * CurrentSpeed * DeltaSeconds;
	// Keep in lane: right-hand traffic, lane centre 300 cm right of the road centre line.
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);
	if (DriveAxis == 0) { Next.Y = LaneLine + Right.Y * 300.f; } else { Next.X = LaneLine + Right.X * 300.f; }
	Next.Z = Dist->StreetZ() + 70.f;
	// Intersections: stop, turn, or continue.
	const FVector Local = Dist->GetActorTransform().InverseTransformPosition(Next);
	const float AlongLocal = DriveAxis == 0 ? Local.X : Local.Y;
	const float Half = ADowntownDistrict::HalfExtent;
	const float Cross = FMath::RoundToFloat((AlongLocal + Half) / ADowntownDistrict::Pitch) * ADowntownDistrict::Pitch - Half;
	const float Now = GetWorld()->GetTimeSeconds();
	if (FMath::Abs(AlongLocal - Cross) < 40.f && Now - LastTurnAt > 3.f)
	{
		LastTurnAt = Now;
		const float Roll = FMath::FRand();
		const float StopChance = CareerEconomy::StopChanceAtIntersection();
		if (Roll < StopChance)
		{
			StopHold = FMath::FRandRange(1.2f, 2.8f);
			CurrentSpeed = 0.f;
			return;
		}
		if (Roll < StopChance + 0.3f && FMath::Abs(Cross) < Half - 10.f)
		{
			const float NewDir = FMath::FRand() < 0.5f ? -1.f : 1.f;
			const FVector CrossWorld = Dist->GetActorTransform().TransformPosition(DriveAxis == 0 ? FVector(Cross, 0, 0) : FVector(0, Cross, 0));
			LaneLine = DriveAxis == 0 ? CrossWorld.X : CrossWorld.Y;
			StartDriving(1 - DriveAxis, NewDir, DriveSpeed);
			CurrentSpeed *= 0.6f;
			return;
		}
	}
	if (FMath::Abs(AlongLocal) > Half) { bRecycle = true; }
	SetActorLocation(Next, false);
}

void ACityCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PhysicsTimer > 0.f)
	{
		PhysicsTimer -= DeltaSeconds;
		if (PhysicsTimer <= 0.f && Box) { Box->SetSimulatePhysics(false); }
	}
	if (Mode == ECityCarMode::Wreck) { BurnTimer = FMath::Max(0.f, BurnTimer - DeltaSeconds); }
	const float Frac = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	if (Mode == ECityCarMode::Wreck || Frac < 0.6f) { TickDamageFX(DeltaSeconds); }
	if (Mode == ECityCarMode::Traffic) { TickTraffic(DeltaSeconds); }
}

// ============================================================================================ ACityPedestrian

ACityPedestrian::ACityPedestrian()
{
	PrimaryActorTick.bCanEverTick = true;
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetGenerateOverlapEvents(true);
	SetRootComponent(Capsule);
}

void ACityPedestrian::Randomize(FRandomStream& InRng)
{
	Rng = FRandomStream(InRng.RandRange(1, 1 << 30));
	FAvatarConfig C;
	C.Body = Rng.FRand() < 0.5f ? EAvatarBody::Feminine : EAvatarBody::Masculine;
	const bool bFem = C.Body == EAvatarBody::Feminine;
	bFemaleSfx = bFem;
	C.SkinTone = Rng.RandRange(0, FMath::Max(0, Career::SkinTones().Num() - 1));
	C.HairColor = Rng.RandRange(0, FMath::Max(0, Career::HairColors().Num() - 1));
	C.Face = Rng.RandRange(0, FMath::Max(0, Career::FaceNames().Num() - 1));
	const EAvatarHair FemHair[] = { EAvatarHair::Long, EAvatarHair::Ponytail, EAvatarHair::Bun, EAvatarHair::Curly, EAvatarHair::Short };
	const EAvatarHair MascHair[] = { EAvatarHair::Short, EAvatarHair::Buzz, EAvatarHair::Curly, EAvatarHair::Bald, EAvatarHair::Short };
	C.Hair = bFem ? FemHair[Rng.RandRange(0, 4)] : MascHair[Rng.RandRange(0, 4)];
	C.FacialHair = bFem ? EAvatarFacialHair::None : EAvatarFacialHair(Rng.RandRange(0, int32(EAvatarFacialHair::Count) - 1));
	const int32 NC = FMath::Max(1, Career::OutfitColors().Num());
	FAvatarOutfit& O = C.Outfit;
	O.Top = EOutfitTop(Rng.RandRange(0, int32(EOutfitTop::Count) - 1));
	O.TopColor = Rng.RandRange(0, NC - 1);
	O.Jacket = Rng.FRand() < 0.25f ? EOutfitJacket(Rng.RandRange(1, int32(EOutfitJacket::Count) - 1)) : EOutfitJacket::None;
	O.JacketColor = Rng.RandRange(0, NC - 1);
	O.Pants = EOutfitPants(Rng.RandRange(0, bFem ? 4 : 3)); // Florida: plenty of shorts
	O.PantsColor = Rng.RandRange(0, NC - 1);
	O.Shoes = EOutfitShoes(Rng.RandRange(0, int32(EOutfitShoes::Count) - 1));
	O.ShoesColor = Rng.RandRange(0, NC - 1);
	O.Hat = Rng.FRand() < 0.2f ? EOutfitHat(Rng.RandRange(1, int32(EOutfitHat::Count) - 1)) : EOutfitHat::None;
	O.HatColor = Rng.RandRange(0, NC - 1);
	O.Glasses = Rng.FRand() < 0.3f ? EOutfitGlasses(Rng.RandRange(1, int32(EOutfitGlasses::Count) - 1)) : EOutfitGlasses::None;
	C.Height = Rng.FRandRange(bFem ? 0.9f : 0.95f, bFem ? 1.0f : 1.07f);
	WalkSpeed = Rng.FRandRange(110.f, 165.f);
	// Voice matching the body (police PA excluded).
	TArray<FName> Voices;
	for (const FNpcVoiceDef& V : NpcManifest::Voices())
	{
		if (!V.bPlaceholder && V.Id != FName(TEXT("police_pa")) && V.bFeminine == bFem) { Voices.Add(V.Id); }
	}
	VoiceId = Voices.Num() ? Voices[Rng.RandRange(0, Voices.Num() - 1)] : NAME_None;

	if (!Avatar)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Avatar = GetWorld()->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), GetActorTransform(), SP);
		if (Avatar) { Avatar->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform); Avatar->SetActorRelativeLocation(FVector(0, 0, -88.f)); }
	}
	if (Avatar)
	{
		// Manifest bodies of the right type, picked by weight.
		const TArray<FNpcBodyDef>& Bodies = NpcManifest::Bodies();
		TArray<int32> Match;
		for (int32 i = 0; i < Bodies.Num(); ++i) { if (Bodies[i].bFeminine == bFem) { Match.Add(i); } }
		Avatar->ForcedBodyIndex = Match.Num() ? Match[Rng.RandRange(0, Match.Num() - 1)] : -1;
		if (Bodies.IsValidIndex(Avatar->ForcedBodyIndex))
		{
			const FNpcBodyDef& B = Bodies[Avatar->ForcedBodyIndex];
			C.Height = Rng.FRandRange(B.ScaleMin, B.ScaleMax);
		}
		Avatar->AnimRate = Rng.FRandRange(0.9f, 1.1f);
		Avatar->ApplyConfig(C, true);
	}
}

void ACityPedestrian::Activate(const FVector& Location)
{
	SetActorLocation(Location + FVector(0, 0, 88.f));
	SetActorRotation(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	if (Avatar) { Avatar->SetActorHiddenInGame(false); Avatar->SetActorTickEnabled(true); }
	GroundZ = Location.Z;
	PedHealth = 1.f;
	State = EPedState::Wander;
	StateTime = 0.f;
	PickWanderTarget();
}

void ACityPedestrian::Pool()
{
	State = EPedState::Pooled;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	if (Avatar) { Avatar->SetActorHiddenInGame(true); Avatar->SetActorTickEnabled(false); }
}

void ACityPedestrian::PickWanderTarget()
{
	ACityLife* Life = ACityLife::Find(this);
	ADowntownDistrict* Dist = Life ? Life->GetDistrict() : nullptr;
	FVector P;
	if (Dist && Dist->RandomSidewalkPoint(Rng, GetActorLocation(), 600.f, 3500.f, P)) { Target = P; }
	else { Target = GetActorLocation() + FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector() * 1500.f; }
}

bool ACityLife::PlayBark(FName VoiceId, FName Category, const FVector& Location)
{
	const double Now = FPlatformTime::Seconds();
	if (VoiceId.IsNone() || Now - LastBarkTime < 1.2) { return false; }
	USoundBase* S = NpcManifest::PickLine(VoiceId, Category, Rng);
	if (!S) { return false; }
	if (!BarkAttenuation)
	{
		BarkAttenuation = NewObject<USoundAttenuation>(this);
		BarkAttenuation->Attenuation.bAttenuate = true;
		BarkAttenuation->Attenuation.bSpatialize = true;
		BarkAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		BarkAttenuation->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
		BarkAttenuation->Attenuation.FalloffDistance = 3200.f;
		BarkAttenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	}
	if (!UArenaGameUserSettings::AreVoicesEnabled()) { return false; } // voices off by default
	UGameplayStatics::PlaySoundAtLocation(this, S, Location, 1.f, 1.f, 0.f, BarkAttenuation);
	LastBarkTime = Now;
	return true;
}

void ACityPedestrian::Bark(FName Category, float Cooldown)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextBarkAt) { return; }
	if (ACityLife* L = ACityLife::Find(this))
	{
		if (L->PlayBark(VoiceId, Category, GetActorLocation() + FVector(0, 0, 70.f))) { NextBarkAt = Now + Cooldown; }
	}
}

void ACityPedestrian::ApplyCityDamage(float Amount, const FVector& From, bool bFromPlayer)
{
	if (State == EPedState::Dead || State == EPedState::Pooled) { return; }
	PedHealth -= Amount;
	if (PedHealth > 0.f)
	{
		Bark(TEXT("hit"), 3.f);
		Scare(From, false);
		return;
	}
	State = EPedState::Dead;
	DeadTime = 0.f;
	ArenaAudio::PlayAt(this, bFemaleSfx ? FName(TEXT("crowd_scream_female")) : FName(TEXT("crowd_scream_male")), GetActorLocation() + FVector(0, 0, 150.f), 0.9f, 0.08f);
	SetActorEnableCollision(false);
	if (Avatar) { Avatar->Collapse((GetActorLocation() - From).GetSafeNormal() * 900.f + FVector(0, 0, 300.f)); }
	if (ACityLife* L = ACityLife::Find(this)) { L->OnPedKilled(this, bFromPlayer); }
}

void ACityPedestrian::Scare(const FVector& Location, bool bExplosion)
{
	if (State == EPedState::Dead || State == EPedState::Pooled) { return; }
	if (State != EPedState::Flee)
	{
		Bark(bExplosion ? FName(TEXT("explosion")) : FName(TEXT("panic")), 6.f);
		// Screams are SFX (not speech): they play with voices off. ~1 in 4 panicking peds.
		if (FMath::FRand() < 0.25f) { ArenaAudio::PlayAt(this, bFemaleSfx ? FName(TEXT("crowd_scream_female")) : FName(TEXT("crowd_scream_male")), GetActorLocation() + FVector(0, 0, 150.f), 0.7f, 0.1f); }
	}
	State = EPedState::Flee;
	StateTime = 0.f;
	Threat = Location;
	if (Avatar) { Avatar->SetPanic(true); }
}

void ACityPedestrian::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == EPedState::Pooled) { return; }
	StateTime += DeltaSeconds;
	if (State == EPedState::Dead)
	{
		DeadTime += DeltaSeconds;
		return;
	}
	FVector Here = GetActorLocation();
	float Speed = 0.f;
	FVector Dir = FVector::ZeroVector;
	AIronManCharacter* Pl = PlayerOf(this);
	switch (State)
	{
	case EPedState::Wander:
	{
		const FVector To = (Target - Here) * FVector(1, 1, 0);
		if (To.Size() < 80.f || StateTime > 40.f)
		{
			State = Rng.FRand() < 0.35f ? EPedState::Idle : EPedState::Wander;
			StateTime = 0.f;
			PickWanderTarget();
			break;
		}
		Dir = To.GetSafeNormal();
		Speed = WalkSpeed;
		if (Pl && FVector::Dist(Pl->GetActorLocation(), Here) < 1800.f && Rng.FRand() < DeltaSeconds * 0.5f)
		{
			State = EPedState::Watch;
			StateTime = 0.f;
			Bark(TEXT("see_ironman"), 12.f);
		}
		else if (Rng.FRand() < DeltaSeconds * 0.01f) { Bark(TEXT("idle"), 20.f); }
		break;
	}
	case EPedState::Idle:
		if (StateTime > Rng.FRandRange(3.f, 8.f)) { State = EPedState::Wander; StateTime = 0.f; }
		break;
	case EPedState::Watch:
		if (Pl)
		{
			const FVector To = (Pl->GetActorLocation() - Here) * FVector(1, 1, 0);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, 4.f));
		}
		if (StateTime > 5.f) { State = EPedState::Wander; StateTime = 0.f; }
		break;
	case EPedState::Flee:
	{
		Dir = ((Here - Threat) * FVector(1, 1, 0)).GetSafeNormal();
		if (Dir.IsNearlyZero()) { Dir = GetActorForwardVector(); }
		Speed = 460.f;
		if (StateTime > 9.f)
		{
			State = EPedState::Wander;
			StateTime = 0.f;
			if (Avatar) { Avatar->SetPanic(false); }
			PickWanderTarget();
		}
		break;
	}
	default: break;
	}
	if (Speed > 0.f && !Dir.IsNearlyZero())
	{
		Here += Dir * Speed * DeltaSeconds;
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), DeltaSeconds, 6.f));
		GroundTimer -= DeltaSeconds;
		if (GroundTimer <= 0.f)
		{
			GroundTimer = 0.35f;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(PedGround), false, this);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Here + FVector(0, 0, 150.f), Here - FVector(0, 0, 400.f), ECC_Visibility, Q)) { GroundZ = Hit.ImpactPoint.Z; }
		}
		Here.Z = FMath::FInterpTo(Here.Z, GroundZ + 88.f, DeltaSeconds, 10.f);
		SetActorLocation(Here, false);
	}
	if (Avatar) { Avatar->SetMoveSpeed(Speed); }
}

// ============================================================================================ ACityLife

ACityLife::ACityLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Rng.Initialize(9091);
}

ACityLife* ACityLife::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return nullptr; }
	static TWeakObjectPtr<ACityLife> Cached;
	if (Cached.IsValid() && Cached->GetWorld() == World) { return Cached.Get(); }
	TActorIterator<ACityLife> It(World);
	if (It) { Cached = *It; return *It; }
	return nullptr;
}

ACityLife* ACityLife::Spawn(ADowntownDistrict* InDistrict)
{
	if (!InDistrict || !InDistrict->GetWorld()) { return nullptr; }
	if (ACityLife* Existing = Find(InDistrict)) { return Existing; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACityLife* L = InDistrict->GetWorld()->SpawnActor<ACityLife>(ACityLife::StaticClass(), InDistrict->GetActorTransform(), SP);
	if (L)
	{
		L->District = InDistrict;
		L->MaxTraffic = CareerEconomy::MaxTraffic();
		L->MaxPeds = CareerEconomy::MaxPeds();
		// Keep ArenaSmoke deterministic / cheap: optional no-op for traffic + peds.
		if (CareerEconomy::DisableLifeDuringArenaSmoke() && FParse::Param(FCommandLine::Get(), TEXT("ArenaSmoke")))
		{
			L->bLifeEnabled = false;
			L->MaxTraffic = 0;
			L->MaxPeds = 0;
		}
	}
	return L;
}

void ACityLife::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ACityPedestrian* P : Peds) { if (P) { P->Destroy(); } }
	for (ACityCar* C : Cars) { if (C) { C->Destroy(); } }
	for (ACityCar* C : Wrecks) { if (C) { C->Destroy(); } }
	Super::EndPlay(EndPlayReason);
}

ACityCar* ACityLife::WakeParkedCar(const FTransform& Xf, int32 Paint)
{
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACityCar* C = GetWorld()->SpawnActor<ACityCar>(ACityCar::StaticClass(), FTransform(Xf.GetRotation(), Xf.GetLocation() + FVector(0, 0, 70.f)), SP);
	if (!C) { return nullptr; }
	C->Setup(Paint, ECityCarMode::Parked);
	Wrecks.Add(C);
	// Keep the live-car count bounded (oldest wrecks go first).
	while (Wrecks.Num() > 24)
	{
		if (Wrecks[0]) { Wrecks[0]->Destroy(); }
		Wrecks.RemoveAt(0);
	}
	return C;
}

void ACityLife::ScareAround(const FVector& Where, float Radius, bool bExplosion)
{
	for (ACityPedestrian* P : Peds)
	{
		if (P && !P->IsPooled() && FVector::Dist(P->GetActorLocation(), Where) < Radius) { P->Scare(Where, bExplosion); }
	}
}

void ACityLife::OnPedKilled(ACityPedestrian* Ped, bool bByPlayer)
{
	if (!bByPlayer) { return; }
	AddCrime(90.f, Ped ? Ped->GetActorLocation() : SearchCenter); // ULTRON W3 ped kill
	if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->AddCredits(-100, LOCTEXT("Civilian", "civilian casualty")); }
}

float ACityLife::GetSearchRadius() const { return 20000.f + 12000.f * Stars; }

float ACityLife::GetEscapeFraction() const
{
	if (Stars <= 0 || bInSearch) { return 0.f; }
	const float Need = 8.f + 4.f * Stars;
	return FMath::Clamp(HiddenTime / Need, 0.f, 1.f);
}

void ACityLife::AddCrime(float HeatAmount, const FVector& Where)
{
	const UWorld* World = GetWorld();
	const AArenaGameMode* GM = World ? World->GetAuthGameMode<AArenaGameMode>() : nullptr;
	if (GM && GM->GetPlayMode() != EArenaPlayMode::Arena) { return; } // story missions stay story
	Heat = FMath::Min(Heat + HeatAmount, StarHeat[4] + 300.f);
	if (AIronManCharacter* Pl = PlayerOf(this)) { SearchCenter = Pl->GetActorLocation(); } else { SearchCenter = Where; }
	HiddenTime = 0.f;
	int32 NewStars = 0;
	for (int32 i = 0; i < 5; ++i) { if (Heat >= StarHeat[i]) { NewStars = i + 1; } }
	if (NewStars > Stars)
	{
		Stars = NewStars;
		ResponderTimer = 0.f;
		if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->Toast(FText::Format(LOCTEXT("Wanted", "WANTED  {0}"), FText::FromString(FString::ChrN(Stars, TEXT('*')))), FLinearColor(1.f, 0.3f, 0.25f)); }
	}
}

void ACityLife::SetWanted(int32 InStars)
{
	InStars = FMath::Clamp(InStars, 0, 5);
	if (InStars == 0) { ClearWanted(); return; }
	Heat = StarHeat[InStars - 1] + 1.f;
	Stars = InStars;
	if (AIronManCharacter* Pl = PlayerOf(this)) { SearchCenter = Pl->GetActorLocation(); }
	HiddenTime = 0.f;
	ResponderTimer = 0.f;
}

void ACityLife::ClearWanted()
{
	Heat = 0.f;
	Stars = 0;
	HiddenTime = 0.f;
	for (const TWeakObjectPtr<AEnemyBase>& R : Responders) { if (R.IsValid()) { R->Destroy(); } }
	Responders.Reset();
}

void ACityLife::OnResponderKilled(AEnemyBase* Enemy)
{
	AddCrime(Enemy && Enemy->Tags.Contains(GunshipTag) ? 110.f : 35.f, Enemy ? Enemy->GetActorLocation() : SearchCenter); // ULTRON W3
}

void ACityLife::SpawnResponders()
{
	AArenaGameMode* GM = GetWorld()->GetAuthGameMode<AArenaGameMode>();
	AIronManCharacter* Pl = PlayerOf(this);
	if (!GM || !Pl || !GM->DroneClass) { return; }
	Responders.RemoveAll([](const TWeakObjectPtr<AEnemyBase>& R) { return !R.IsValid() || R->IsDead(); });
	// ULTRON W1/W2: WantedPressure table + absolute cap NonResponder + Responders <= 12.
	const WantedPressure::FMul Press = WantedPressure::ForStars(Stars);
	const int32 NonResp = WantedPressure::CountNonResponderAlive(this);
	const int32 Room = FMath::Max(0, WantedPressure::AbsoluteEnemyCap - NonResp - Responders.Num());
	int32 WantDrones = Press.WantDrones;
	int32 WantTanks = Press.WantTanks;
	int32 WantGunships = Press.WantGunships;
	auto TrimTo = [&](int32 Budget)
	{
		int32 Need = WantDrones + WantTanks + WantGunships;
		while (Need > Budget && WantGunships > 0) { --WantGunships; --Need; }
		while (Need > Budget && WantTanks > 0) { --WantTanks; --Need; }
		while (Need > Budget && WantDrones > 0) { --WantDrones; --Need; }
	};
	TrimTo(FMath::Min(Press.Cap, Responders.Num() + Room));
	int32 Drones = 0, Tanks = 0, Gunships = 0;
	for (const TWeakObjectPtr<AEnemyBase>& R : Responders)
	{
		if (R->Tags.Contains(GunshipTag)) { ++Gunships; }
		else if (GM->TankClass && R->IsA(GM->TankClass)) { ++Tanks; }
		else { ++Drones; }
	}
	auto SpawnOne = [&](TSubclassOf<AEnemyBase> Cls, bool bGunship, bool bGround)
	{
		// Re-check room each spawn so we never breach AbsoluteEnemyCap mid-batch.
		if (Responders.Num() + NonResp >= WantedPressure::AbsoluteEnemyCap) { return; }
		const FVector Off = FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector() * Rng.FRandRange(3500.f, 6000.f);
		FVector Loc = Pl->GetActorLocation() + Off + FVector(0, 0, bGround ? 0.f : Rng.FRandRange(800.f, 1800.f));
		if (bGround && District.IsValid()) { Loc = District->GetRoadsideSpotNear(Loc).GetLocation() + FVector(0, 0, 120.f); }
		AEnemyBase* E = GM->SpawnEnemyAt(Cls, Loc);
		if (!E) { return; }
		E->Tags.Add(ResponderTag);
		if (bGunship) { E->Tags.Add(GunshipTag); E->SetActorScale3D(FVector(2.2f)); }
		// W5: one police light, color toggled in TickWanted (was two PointLights).
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(E);
			L->SetupAttachment(E->GetRootComponent());
			L->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
			L->SetIntensityUnits(ELightUnits::Lumens);
			L->SetIntensity(12000.f);
			L->SetAttenuationRadius(900.f);
			L->SetCastShadows(false);
			L->SetLightColor(FLinearColor(1.f, 0.05f, 0.05f));
			L->ComponentTags.Add(FName(TEXT("PoliceLight")));
			L->RegisterComponent();
		}
		Responders.Add(E);
	};
	for (int32 i = Drones; i < WantDrones; ++i) { SpawnOne(GM->DroneClass, false, false); }
	for (int32 i = Tanks; i < WantTanks && GM->TankClass; ++i) { SpawnOne(GM->TankClass, false, true); }
	for (int32 i = Gunships; i < WantGunships; ++i) { SpawnOne(GM->DroneClass, true, false); }
}

void ACityLife::TickWanted(float DeltaSeconds, const FVector& PlayerLoc)
{
	if (Stars <= 0)
	{
		if (SirenAudio) { SirenAudio->FadeOut(1.5f, 0.f); SirenAudio = nullptr; SirenOwner = nullptr; }
		return;
	}
	// Responders that see you (within 2.5 km with line of sight) move the search area onto you.
	bInSearch = FVector::Dist2D(PlayerLoc, SearchCenter) < GetSearchRadius();
	bool bSeen = false;
	for (const TWeakObjectPtr<AEnemyBase>& R : Responders)
	{
		if (!R.IsValid() || R->IsDead()) { continue; }
		if (FVector::Dist(R->GetActorLocation(), PlayerLoc) < 2500.f)
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WantedLOS), false, R.Get());
			if (AIronManCharacter* Pl = PlayerOf(this)) { Q.AddIgnoredActor(Pl); }
			if (!GetWorld()->LineTraceSingleByChannel(Hit, R->GetActorLocation(), PlayerLoc, ECC_Visibility, Q)) { bSeen = true; break; }
		}
	}
	if (bSeen) { SearchCenter = PlayerLoc; HiddenTime = 0.f; bInSearch = true; }
	if (!bInSearch)
	{
		HiddenTime += DeltaSeconds;
		if (HiddenTime >= 8.f + 4.f * Stars)
		{
			if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->Toast(LOCTEXT("Lost", "You lost the cops"), FLinearColor(0.5f, 1.f, 0.6f)); }
			ClearWanted();
			return;
		}
	}
	else if (!bSeen)
	{
		// Inside the search area but unseen: heat cools slowly; stars drop as it falls under the thresholds.
		// W4: allow clear to 0 when heat < 1★ and unseen long enough (half escape time).
		Heat = FMath::Max(0.f, Heat - DeltaSeconds * 6.f);
		HiddenTime += DeltaSeconds; // deepen: track unseen time inside search for clear-to-zero
		int32 NewStars = 0;
		for (int32 i = 0; i < 5; ++i) { if (Heat >= StarHeat[i]) { NewStars = i + 1; } }
		if (NewStars <= 0 && HiddenTime >= 0.5f * (8.f + 4.f * float(Stars)))
		{
			if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->Toast(LOCTEXT("LostQuiet", "You lost the cops"), FLinearColor(0.5f, 1.f, 0.6f)); }
			ClearWanted();
			return;
		}
		Stars = FMath::Max(NewStars, 0);
		if (Stars <= 0) { ClearWanted(); return; }
	}
	ResponderTimer -= DeltaSeconds;
	// W6: respawn interval scales with stars (~5.2s at 1★ → ~3.75s at 5★).
	if (ResponderTimer <= 0.f) { ResponderTimer = WantedPressure::ForStars(Stars).RespawnSec; SpawnResponders(); }
	// Police PA + light bar flicker.
	PaTimer -= DeltaSeconds;
	LightPhase += DeltaSeconds * 7.f;
	const bool bRedOn = FMath::Fmod(LightPhase, 2.f) < 1.f;
	AEnemyBase* Closest = nullptr;
	float Best = 1e12f;
	for (const TWeakObjectPtr<AEnemyBase>& R : Responders)
	{
		if (!R.IsValid()) { continue; }
		TArray<UPointLightComponent*> Lights;
		R->GetComponents(Lights);
		for (UPointLightComponent* L : Lights)
		{
			// W5: single PoliceLight toggles red/blue; keep legacy Red/Blue tags working if old responders linger.
			if (L->ComponentHasTag(TEXT("PoliceLight")))
			{
				L->SetLightColor(bRedOn ? FLinearColor(1.f, 0.05f, 0.05f) : FLinearColor(0.1f, 0.25f, 1.f));
				L->SetVisibility(true);
			}
			else if (L->ComponentHasTag(TEXT("PoliceRed"))) { L->SetVisibility(bRedOn); }
			else if (L->ComponentHasTag(TEXT("PoliceBlue"))) { L->SetVisibility(!bRedOn); }
		}
		const float D = FVector::DistSquared(R->GetActorLocation(), PlayerLoc);
		if (D < Best) { Best = D; Closest = R.Get(); }
	}
	if (Closest && SirenOwner.Get() != Closest)
	{
		if (SirenAudio) { SirenAudio->FadeOut(0.5f, 0.f); }
		SirenAudio = ArenaAudio::Attach(Stars >= 4 ? FName(TEXT("police_siren_facility")) : FName(TEXT("police_siren")), Closest->GetRootComponent(), 0.55f, true);
		if (SirenAudio) { SirenAudio->Play(); }
		SirenOwner = Closest;
	}
	if (PaTimer <= 0.f && Closest && Best < FMath::Square(3000.f))
	{
		PaTimer = 11.f;
		LastBarkTime = 0.0;
		PlayBark(TEXT("police_pa"), TEXT("police"), Closest->GetActorLocation());
	}
}

void ACityLife::TickPeds(float DeltaSeconds, const FVector& PlayerLoc, bool bNear)
{
	if (Peds.Num() == 0)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 i = 0; i < MaxPeds; ++i)
		{
			ACityPedestrian* P = GetWorld()->SpawnActor<ACityPedestrian>(ACityPedestrian::StaticClass(), FTransform(GetActorLocation() - FVector(0, 0, 5000.f)), SP);
			if (!P) { continue; }
			P->Randomize(Rng);
			P->Pool();
			Peds.Add(P);
		}
	}
	const ADowntownDistrict* Dist = District.Get();
	for (ACityPedestrian* P : Peds)
	{
		if (!P) { continue; }
		if (!P->IsPooled())
		{
			const float D = FVector::Dist2D(P->GetActorLocation(), PlayerLoc);
			if (!bNear || D > 14000.f || (P->IsDeadPed() && (P->DeadTime > 25.f || D > 6000.f))) { P->Pool(); }
			continue;
		}
		if (!bNear || !Dist) { continue; }
		// Respawn out of sight-ish (2.5 .. 9 km... in cm: 25 .. 90 m) on a pavement, with a new look.
		FVector Spot;
		float PedRate = 2.f;
		if (const UCareerSubsystem* Care = UCareerSubsystem::Get(this)) { PedRate *= CareerEconomy::PedMulForHour(Care->GetTimeOfDay()); }
		if (Rng.FRand() < DeltaSeconds * PedRate && Dist->RandomSidewalkPoint(Rng, PlayerLoc, 2500.f, 9000.f, Spot))
		{
			P->Randomize(Rng);
			P->Activate(Spot);
		}
	}
}

bool ACityLife::PlaceTrafficCar(ACityCar* Car, const FVector& PlayerLoc)
{
	ADowntownDistrict* Dist = District.Get();
	if (!Dist || !Car) { return false; }
	const FVector Local = Dist->GetActorTransform().InverseTransformPosition(PlayerLoc);
	const int32 Axis = Rng.RandRange(0, 1);
	const float Dir = Rng.FRand() < 0.5f ? -1.f : 1.f;
	// Pick the road line nearest to the player on the cross axis (+- 2 roads), then a spot 60..140 m along it.
	const float CrossLocal = Axis == 0 ? Local.Y : Local.X;
	int32 Line = FMath::RoundToInt((CrossLocal + ADowntownDistrict::HalfExtent) / ADowntownDistrict::Pitch) + Rng.RandRange(-2, 2);
	Line = FMath::Clamp(Line, 0, ADowntownDistrict::NumBlocks);
	const float LineLocal = -ADowntownDistrict::HalfExtent + Line * ADowntownDistrict::Pitch;
	float AlongLocal = (Axis == 0 ? Local.X : Local.Y) + Rng.FRandRange(-14000.f, 14000.f);
	AlongLocal = FMath::Clamp(AlongLocal, -ADowntownDistrict::HalfExtent + 500.f, ADowntownDistrict::HalfExtent - 500.f);
	const FVector P = Dist->GetActorTransform().TransformPosition(Axis == 0 ? FVector(AlongLocal, LineLocal, 0.f) : FVector(LineLocal, AlongLocal, 0.f));
	if (FVector::Dist2D(P, PlayerLoc) < 4000.f) { return false; }
	Car->LaneLine = Axis == 0 ? P.Y : P.X;
	Car->SetActorLocation(FVector(P.X, P.Y, Dist->StreetZ() + 70.f));
	Car->StartDriving(Axis, Dir, Rng.FRandRange(900.f, 1800.f));
	Car->SetActorHiddenInGame(false);
	Car->SetActorEnableCollision(true);
	return true;
}

void ACityLife::TickTraffic(float DeltaSeconds, const FVector& PlayerLoc, bool bNear)
{
	if (!bNear) { return; }
	float Hour = 13.f;
	if (const UCareerSubsystem* Care = UCareerSubsystem::Get(this)) { Hour = Care->GetTimeOfDay(); }
	const int32 WantCars = FMath::Clamp(FMath::RoundToInt(float(MaxTraffic) * CareerEconomy::TrafficMulForHour(Hour)), 0, 24);
	Cars.RemoveAll([this](const TObjectPtr<ACityCar>& C)
	{
		// Wrecks and cars whose driver bailed (shot at) leave the traffic pool and stay where they are.
		if (C && (C->IsWrecked() || (C->GetMode() == ECityCarMode::Traffic && C->DriveSpeed <= 0.f && !C->bRecycle))) { Wrecks.Add(C); return true; }
		return !C;
	});
	while (Cars.Num() > WantCars)
	{
		if (ACityCar* Extra = Cars.Pop()) { Extra->Destroy(); }
	}
	while (Cars.Num() < WantCars)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACityCar* C = GetWorld()->SpawnActor<ACityCar>(ACityCar::StaticClass(), FTransform(GetActorLocation() - FVector(0, 0, 5000.f)), SP);
		if (!C) { break; }
		C->Setup(Rng.RandRange(0, FMath::Max(1, ADowntownDistrict::CarPaints().Num()) * 3 - 1), ECityCarMode::Traffic);
		C->bRecycle = true;
		Cars.Add(C);
	}
	for (ACityCar* C : Cars)
	{
		if (!C) { continue; }
		if (C->bRecycle || FVector::Dist2D(C->GetActorLocation(), PlayerLoc) > 20000.f)
		{
			if (!PlaceTrafficCar(C, PlayerLoc)) { C->bRecycle = true; C->SetActorHiddenInGame(true); }
		}
	}
}

void ACityLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADowntownDistrict* Dist = District.Get();
	AIronManCharacter* Pl = PlayerOf(this);
	if (!Dist || !Pl) { return; }
	const FVector PlayerLoc = Pl->GetActorLocation();
	const FBox B = Dist->GetPlayBounds().ExpandBy(8000.f);
	const bool bNear = bLifeEnabled && Dist->IsDistrictVisible() && B.IsInsideXY(PlayerLoc) && PlayerLoc.Z < Dist->StreetZ() + 30000.f;
	TickPeds(DeltaSeconds, PlayerLoc, bNear);
	TickTraffic(DeltaSeconds, PlayerLoc, bNear);
	TickWanted(DeltaSeconds, PlayerLoc);
	if (!bNear)
	{
		for (ACityCar* C : Cars) { if (C && C->GetMode() == ECityCarMode::Traffic) { C->SetActorHiddenInGame(true); C->SetActorEnableCollision(false); C->bRecycle = true; } }
	}
}

#undef LOCTEXT_NAMESPACE
