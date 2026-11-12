#include "ArenaCombat.h"
#include "SuitReactiveState.h"
#include "ArenaAudio.h"
#include "ArenaBlastFX.h"
#include "ArenaFeel.h"
#include "ArenaVisuals.h"
#include "CityLife.h"
#include "EnemyBase.h"
#include "IronManCharacter.h"
#include "IronProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"

namespace
{
	double GLastHit = -100.0, GLastKill = -100.0;
	int32 GStreak = 0;
	bool GFocus = false;
	float GCharge = 0.f, GBloom = 0.f;
	TArray<TWeakObjectPtr<AArenaTracer>> GTracerPool;
	constexpr int32 MaxTracers = 24;
}

// ============================================================================================ tracer

AArenaTracer::AArenaTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
	auto Make = [this](FName N) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(N);
		C->SetupAttachment(Pivot);
		C->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetVisibleInRayTracing(false);
		return C;
	};
	Core = Make(TEXT("Core"));
	Glow = Make(TEXT("Glow"));
	SetActorHiddenInGame(true);
}

void AArenaTracer::BeginPlay()
{
	Super::BeginPlay();
	CoreMID = ArenaVisuals::PaintComponent(this, Core, FLinearColor::White, 12.f, EArenaSurface::Glow);
	GlowMID = ArenaVisuals::PaintComponent(this, Glow, Color, 6.f, EArenaSurface::Glow);
}

void AArenaTracer::Spawn(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& CoreColor, float InWidth, float InLife, FLinearColor InRim)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer) { return; }
	AArenaTracer* Tracer = nullptr;
	GTracerPool.RemoveAll([](const TWeakObjectPtr<AArenaTracer>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<AArenaTracer>& W : GTracerPool)
	{
		if (W->GetWorld() == World && !W->bActive) { Tracer = W.Get(); break; }
	}
	if (!Tracer)
	{
		if (GTracerPool.Num() >= MaxTracers)
		{
			// Steal the oldest in this world.
			for (const TWeakObjectPtr<AArenaTracer>& W : GTracerPool) { if (W->GetWorld() == World) { Tracer = W.Get(); break; } }
		}
		if (!Tracer)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Tracer = World->SpawnActor<AArenaTracer>(AArenaTracer::StaticClass(), From, FRotator::ZeroRotator, P);
			if (!Tracer) { return; }
			GTracerPool.Add(Tracer);
		}
	}
	Tracer->Begin(From, To, CoreColor, InWidth, InLife, InRim);
}

void AArenaTracer::Begin(const FVector& From, const FVector& To, const FLinearColor& CoreColor, float InWidth, float InLife, const FLinearColor& InRim)
{
	const FVector D = To - From;
	Length = FMath::Max(10.f, float(D.Size()));
	Color = CoreColor;
	// Gold / suit rim: explicit A>0, else warm gold when core is cyan-white (Mark), else tinted core.
	if (InRim.A > 0.01f)
	{
		Rim = InRim;
		Rim.A = 1.f;
	}
	else
	{
		const float Cool = Color.B - FMath::Max(Color.R, Color.G) * 0.35f;
		Rim = (Cool > 0.15f)
			? FLinearColor(1.f, 0.78f, 0.32f) // Mark-style gold sleeve on cyan-white core
			: FLinearColor::LerpUsingHSV(Color, FLinearColor(1.f, 0.85f, 0.45f), 0.35f);
	}
	Width = InWidth;
	Life = FMath::Max(0.028f, InLife);
	Age = 0.f;
	bActive = true;
	SetActorLocationAndRotation(From, FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
	Core->SetRelativeLocation(FVector(0, 0, Length * 0.5f));
	Glow->SetRelativeLocation(FVector(0, 0, Length * 0.5f));
	if (GlowMID) { GlowMID->SetVectorParameterValue(TEXT("Color"), Rim); }
	if (CoreMID) { CoreMID->SetVectorParameterValue(TEXT("Color"), FLinearColor::LerpUsingHSV(FLinearColor(1.f, 1.f, 1.f), Color, 0.18f)); }
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	Tick(0.f);
}

void AArenaTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive) { return; }
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	// Pass 2 film beam: needle core + gold rim sleeve; hard foreshorten; no soft orb bloom.
	const float CoreFade = 1.f - FMath::Pow(T, 0.42f);
	const float BloomFade = 1.f - FMath::Pow(T, 0.85f);
	const float Pulse = (T < 0.08f) ? (1.f + 0.85f * (1.f - T / 0.08f)) : 1.f;
	const float WCore = Width * (0.10f + 0.16f * CoreFade) * Pulse;
	const float WBloom = Width * (0.42f + 0.28f * BloomFade) * Pulse;
	const float LenNow = Length * (0.48f + 0.52f * CoreFade);
	Core->SetRelativeLocation(FVector(0, 0, LenNow * 0.5f));
	Glow->SetRelativeLocation(FVector(0, 0, LenNow * 0.5f));
	Core->SetRelativeScale3D(FVector(WCore / 100.f, WCore / 100.f, LenNow / 100.f));
	Glow->SetRelativeScale3D(FVector(WBloom / 100.f, WBloom / 100.f, LenNow / 100.f));
	if (CoreMID)
	{
		CoreMID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(28.f * CoreFade * Pulse));
		CoreMID->SetVectorParameterValue(TEXT("Color"), FLinearColor::LerpUsingHSV(FLinearColor(1.f, 1.f, 1.f), Color, 0.18f));
	}
	if (GlowMID)
	{
		GlowMID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(9.f * BloomFade));
		GlowMID->SetVectorParameterValue(TEXT("Color"), Rim);
	}
	if (T >= 1.f)
	{
		bActive = false;
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
	}
}

// ============================================================================================ sparks / debris

AArenaSparks::AArenaSparks()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	InitialLifeSpan = 5.f;
}

void AArenaSparks::Spawn(UWorld* World, const FVector& Location, const FVector& Normal, const FLinearColor& InColor, int32 Count, float Speed)
{
	if (!World) { return; }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaSparks* S = World->SpawnActor<AArenaSparks>(AArenaSparks::StaticClass(), Location, FRotator::ZeroRotator, P);
	if (!S) { return; }
	S->Color = InColor;
	S->Life = 0.42f;
	S->SetLifeSpan(1.f);
	UStaticMesh* Mesh = ArenaVisuals::GetShapeMesh(EArenaShape::Cube);
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	for (int32 i = 0; i < FMath::Clamp(Count, 1, 16); ++i)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(S);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->SetupAttachment(S->GetRootComponent());
		C->RegisterComponent();
		C->SetWorldScale3D(FVector(0.12f, 0.018f, 0.018f));
		const FLinearColor Hot = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.85f, 0.5f), InColor, FMath::FRand() * 0.5f);
		S->MIDs.Add(ArenaVisuals::PaintComponent(S, C, Hot, 10.f, EArenaSurface::Glow));
		S->Comps.Add(C);
		FBit B;
		B.C = C;
		B.V = FMath::VRandCone(N, FMath::DegreesToRadians(65.f)) * Speed * FMath::FRandRange(0.4f, 1.2f);
		S->Bits.Add(B);
	}
}

void AArenaSparks::SpawnDebris(UWorld* World, const FVector& Location, const FVector& Dir, const FLinearColor& InColor, float Size, int32 Count)
{
	if (!World) { return; }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaSparks* S = World->SpawnActor<AArenaSparks>(AArenaSparks::StaticClass(), Location, FRotator::ZeroRotator, P);
	if (!S) { return; }
	S->bDebris = true;
	S->Life = 3.2f;
	S->Gravity = 1400.f;
	S->SetLifeSpan(4.f);
	S->Color = InColor;
	const FVector Push = Dir.GetSafeNormal();
	for (int32 i = 0; i < FMath::Clamp(Count, 1, 12); ++i)
	{
		const bool bSphere = (i % 4) == 3;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(S);
		C->SetStaticMesh(ArenaVisuals::GetShapeMesh(bSphere ? EArenaShape::Sphere : EArenaShape::Cube));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(S->GetRootComponent());
		C->RegisterComponent();
		const float K = Size / 100.f * FMath::FRandRange(0.25f, 0.6f);
		C->SetWorldScale3D(FVector(K, K * FMath::FRandRange(0.4f, 1.f), K * FMath::FRandRange(0.3f, 0.8f)));
		C->SetWorldLocation(Location + FMath::VRand() * Size * 0.4f);
		const FLinearColor Col = (i % 3 == 0) ? FLinearColor(0.05f, 0.05f, 0.06f) : InColor * FMath::FRandRange(0.5f, 1.f);
		S->MIDs.Add(ArenaVisuals::PaintComponent(S, C, Col, i == 0 ? 6.f : 0.f));
		S->Comps.Add(C);
		FBit B;
		B.C = C;
		B.V = (FMath::VRandCone(Push + FVector(0, 0, 0.6f), FMath::DegreesToRadians(55.f))) * FMath::FRandRange(500.f, 1400.f);
		B.Spin = FRotator(FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-720.f, 720.f));
		S->Bits.Add(B);
	}
}

void AArenaSparks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	UWorld* World = GetWorld();
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	for (int32 i = 0; i < Bits.Num(); ++i)
	{
		FBit& B = Bits[i];
		if (!B.C) { continue; }
		B.V.Z -= Gravity * DeltaSeconds;
		B.V *= FMath::Exp(-(bDebris ? 0.4f : 2.2f) * DeltaSeconds);
		FVector P = B.C->GetComponentLocation();
		FVector Next = P + B.V * DeltaSeconds;
		if (bDebris && World)
		{
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(Debris), false);
			if (World->LineTraceSingleByChannel(H, P, Next, ECC_WorldStatic, Q))
			{
				Next = H.ImpactPoint + H.ImpactNormal * 4.f;
				B.V = FMath::GetReflectionVector(B.V, H.ImpactNormal) * 0.35f;
				B.Spin *= 0.5f;
			}
			B.C->SetWorldLocation(Next);
			B.C->AddWorldRotation(B.Spin * DeltaSeconds);
			if (T > 0.75f) { B.C->SetWorldScale3D(B.C->GetComponentScale() * FMath::Max(0.f, 1.f - DeltaSeconds * 4.f)); }
		}
		else
		{
			B.C->SetWorldLocationAndRotation(Next, B.V.Rotation());
			if (MIDs.IsValidIndex(i) && MIDs[i]) { MIDs[i]->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(10.f * (1.f - T))); }
		}
	}
	if (T >= 1.f) { Destroy(); }
}

// ============================================================================================ hitscan

void ArenaCombat::Knockback(AActor* Target, const FVector& Dir, float Strength)
{
	if (!Target || Strength <= 0.f) { return; }
	const FVector D = Dir.GetSafeNormal2D() + FVector(0, 0, 0.15f);
	if (ACharacter* C = Cast<ACharacter>(Target)) { C->LaunchCharacter(D * Strength, false, false); return; }
	// Drones / tanks / bosses: knockback now comes from AEnemyBase::TakeDamage (Jocasta combat realism:
	// decaying velocity, damage x kind scale, swept), so the old one-frame shove would double it.
	if (Target->IsA<AEnemyBase>()) { return; }
	Target->AddActorWorldOffset(D * Strength * 0.06f, true);
}

ArenaCombat::FShot ArenaCombat::FireHitscan(AActor* Shooter, AController* Inst, const FVector& Muzzle, const FVector& Dir,
	float Range, float Damage, float BlastRadius, float Radius, const FLinearColor& Color, float KnockStrength)
{
	FShot R;
	UWorld* World = Shooter ? Shooter->GetWorld() : nullptr;
	if (!World) { return R; }
	const FVector D = Dir.GetSafeNormal();
	const FVector End = Muzzle + D * Range;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	Obj.AddObjectTypesToQuery(ECC_Pawn);
	Obj.AddObjectTypesToQuery(ECC_PhysicsBody);
	Obj.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArenaHitscan), false, Shooter);
	TArray<AActor*> Attached;
	Shooter->GetAttachedActors(Attached, true, true);
	Q.AddIgnoredActors(Attached);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Muzzle, End, FQuat::Identity, Obj, FCollisionShape::MakeSphere(FMath::Clamp(Radius * 0.8f, 4.f, 30.f)), Q);
	const FHitResult* Use = nullptr;
	for (const FHitResult& H : Hits)
	{
		AActor* A = H.GetActor();
		if (AIronProjectile* P = Cast<AIronProjectile>(A))
		{
			if (P->IsHostile() && P->bDestroyableByPlayer && P->IsActiveShot()) { P->Intercept(); ArenaAudio::PlayAt(Shooter, TEXT("hit_laser"), H.ImpactPoint, 0.8f); }
			continue; // shots pass through projectiles
		}
		if (const AEnemyBase* E = Cast<AEnemyBase>(A)) { if (E->IsDead()) { continue; } }
		if (A && A->IsA<AIronManCharacter>()) { continue; }
		if (H.bStartPenetrating && H.Distance <= 0.f && !(A && A->IsA<AEnemyBase>())) { continue; }
		Use = &H;
		break;
	}
	R.End = Use ? FVector(Use->ImpactPoint) : End;
	if (!Use) { return R; }
	R.bHit = true;
	AActor* A = Use->GetActor();
	const FVector Impact = Use->ImpactPoint;
	const FVector Normal = Use->ImpactNormal;

	auto HitEnemy = [&](AEnemyBase* E, float Amount, bool bDirect)
	{
		const FVector Loc = E->GetActorLocation();
		const float Size = FMath::Clamp(E->GetSimpleCollisionRadius(), 40.f, 400.f);
		UGameplayStatics::ApplyDamage(E, Amount, Inst, Shooter, UDamageType::StaticClass());
		R.bEnemy = true;
		const bool bDead = !IsValid(E) || E->IsDead();
		if (bDead)
		{
			R.bKill = true;
			AArenaSparks::SpawnDebris(World, Loc, D, FLinearColor(0.35f, 0.36f, 0.4f), Size, Size > 150.f ? 11 : 7);
			ArenaAudio::PlayAt(Shooter, TEXT("explosion_mid"), Loc, 1.f);
		}
		else if (bDirect)
		{
			Knockback(E, D, KnockStrength);
		}
	};

	if (AEnemyBase* E = Cast<AEnemyBase>(A))
	{
		HitEnemy(E, Damage, true);
		ArenaAudio::PlayAt(Shooter, TEXT("hit_metal"), Impact, 1.f, 0.1f);
		AArenaSparks::Spawn(World, Impact, -D, Color, 12, 1300.f);
	}
	else
	{
		AArenaSparks::Spawn(World, Impact, Normal, Color, 8, 950.f);
		ArenaAudio::PlayAt(Shooter, TEXT("repulsor_impact"), Impact, 0.95f, 0.1f);
		if (Use->Component.IsValid() && Use->Component->GetCollisionObjectType() == ECC_WorldStatic)
		{
			// Existing ArenaFeel decal path — brief scorch (not a new system).
			const float DecalSize = BlastRadius > 0.f ? FMath::Clamp(BlastRadius * 0.75f, 55.f, 280.f) : 28.f;
			ArenaFeel::SpawnImpactDecal(World, Impact, Normal, DecalSize, 5.5f);
		}
	}
	// City: cars and pedestrians (direct or splash). Responders (drones / tanks) are enemies above.
	const bool bCityActor = A && (A->IsA<ACityCar>() || A->IsA<ACityPedestrian>());
	if (bCityActor || BlastRadius > 0.f)
	{
		CityLife::Damage(World, Impact, FMath::Max(BlastRadius, bCityActor ? 30.f : 0.f), Damage, bCityActor ? A : nullptr, true);
		if (bCityActor) { R.bEnemy = true; }
	}
	if (BlastRadius > 0.f)
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, Impact, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(BlastRadius));
		TSet<AEnemyBase*> Done;
		if (AEnemyBase* Direct = Cast<AEnemyBase>(A)) { Done.Add(Direct); }
		for (const FOverlapResult& O : Overlaps)
		{
			AEnemyBase* E = Cast<AEnemyBase>(O.GetActor());
			if (!E || E->IsDead() || Done.Contains(E)) { continue; }
			Done.Add(E);
			HitEnemy(E, Damage * 0.6f, false);
			if (IsValid(E) && !E->IsDead()) { Knockback(E, E->GetActorLocation() - Impact, KnockStrength * 0.8f); }
		}
		AArenaBlastFX::Spawn(World, Impact, BlastRadius, FLinearColor(1.f, 0.55f, 0.2f), 0.22f, true);
		USuitReactiveComponent::ReportExplosion(World, Impact, BlastRadius);
		ArenaAudio::PlayAt(Shooter, TEXT("explosion_near"), Impact, 0.95f);
		ArenaFeel::SpawnImpactDecal(World, Impact, Normal, FMath::Clamp(BlastRadius * 0.85f, 70.f, 320.f), 6.5f);
	}
	else
	{
		// Punchier impact bloom: tighter, faster plate + shock (Pass 2).
		AArenaBlastFX::Spawn(World, Impact + Normal * 4.f, FMath::Clamp(Radius * 2.8f, 18.f, 52.f), Color, 0.09f, false);
	}
	if (R.bEnemy || R.bKill) { RegisterHit(R.bKill); }
	return R;
}

// ============================================================================================ state

void ArenaCombat::RegisterHit(bool bKill)
{
	const double Now = FPlatformTime::Seconds();
	GLastHit = Now;
	if (bKill)
	{
		GStreak = (Now - GLastKill < 3.0) ? GStreak + 1 : 1;
		GLastKill = Now;
	}
}

float ArenaCombat::HitMarkerAge() { return float(FPlatformTime::Seconds() - GLastHit); }
float ArenaCombat::KillMarkerAge() { return float(FPlatformTime::Seconds() - GLastKill); }
int32 ArenaCombat::KillStreak() { return KillMarkerAge() < 3.f ? GStreak : 0; }

void ArenaCombat::Rumble(const UObject* Ctx, float Intensity, float Duration, bool bHeavy)
{
	APlayerController* P = Ctx ? UGameplayStatics::GetPlayerController(Ctx, 0) : nullptr;
	if (!P || Intensity <= 0.f) { return; }
	P->PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), Duration, bHeavy, true, bHeavy, true);
}

void ArenaCombat::SetAimState(bool bFocus, float Charge01, float Bloom01) { GFocus = bFocus; GCharge = Charge01; GBloom = Bloom01; }
bool ArenaCombat::IsFocus() { return GFocus; }
float ArenaCombat::GetCharge() { return GCharge; }
float ArenaCombat::GetBloom() { return GBloom; }
