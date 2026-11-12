#include "HUDDataSubsystem.h"

#include "ArenaGameUserSettings.h"
#include "EnemyBase.h"
#include "EngineUtils.h"
#include "IronManCharacter.h"
#include "Pickup.h"
#include "ReactorComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

// Mock-free zone wear: how much integrity one hit takes from the zone it struck, per unit of
// (damage / max health). 2.2 means a 25%-of-max hit takes ~55% of that zone, like the JS HUD.
static constexpr float ZoneWearPerHealthFraction = 2.2f;

UHUDDataSubsystem* UHUDDataSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHUDDataSubsystem>() : nullptr;
}

bool UHUDDataSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UHUDDataSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHUDDataSubsystem, STATGROUP_Tickables);
}

void UHUDDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegionIntegrity.Init(1.f, int32(EHUDBodyRegion::Count));
	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UHUDDataSubsystem::HandleActorSpawned));
	}
}

void UHUDDataSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();
	TrackedEnemies.Reset();
	TrackedPickups.Reset();
	Super::Deinitialize();
}

void UHUDDataSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RescanTrackedActors(); // level-placed actors never fire OnActorSpawned
}

// ------------------------------------------------------------------------------------------------
// Tracked actors (HUD radar / brackets) and AGL
// ------------------------------------------------------------------------------------------------

void UHUDDataSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Actor))
	{
		TrackedEnemies.Add(Enemy);
	}
	else if (APickup* Pickup = Cast<APickup>(Actor))
	{
		TrackedPickups.Add(Pickup);
	}
}

void UHUDDataSubsystem::RescanTrackedActors()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	TrackedEnemies.Reset();
	TrackedPickups.Reset();
	for (TActorIterator<AEnemyBase> It(World); It; ++It) { TrackedEnemies.Add(*It); }
	for (TActorIterator<APickup> It(World); It; ++It) { TrackedPickups.Add(*It); }
}

float UHUDDataSubsystem::GetAglM(const AActor* Player)
{
	UWorld* World = GetWorld();
	if (!Player || !World) { return -1.f; }
	const FVector Location = Player->GetActorLocation();
	const double NowS = World->GetRealTimeSeconds(); // undilated, like the arcs
	const bool bStale = Player != AglActor.Get() || AglTraceTime < 0.0
		|| NowS - AglTraceTime >= AglTraceIntervalS
		|| FVector::DistSquared2D(Location, AglTraceLocation) > FMath::Square(500.0);
	if (bStale)
	{
		AglActor = Player;
		AglTraceLocation = Location;
		AglTraceTime = NowS;
		AglTraceM = -1.f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HelmetHUDAgl), false, Player);
		if (World->LineTraceSingleByChannel(Hit, Location, Location - FVector(0.0, 0.0, 20000.0), ECC_Visibility, Params))
		{
			AglTraceM = FMath::Max(0.f, (Hit.Distance - Player->GetSimpleCollisionHalfHeight()) / 100.f);
		}
	}
	if (AglTraceM < 0.f) { return -1.f; }
	return FMath::Max(0.f, AglTraceM + float(Location.Z - AglTraceLocation.Z) / 100.f);
}

bool UHUDDataSubsystem::IsReducedMotion() const
{
	const UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	return S && S->bHudReducedMotion;
}

// ------------------------------------------------------------------------------------------------
// Hits
// ------------------------------------------------------------------------------------------------

bool UHUDDataSubsystem::ResolveHitSource(const AActor* Victim, const FDamageEvent& DamageEvent, const AController* EventInstigator,
	const AActor* DamageCauser, FVector& OutFromDir, FVector& OutHitLocation)
{
	const FVector Here = Victim ? Victim->GetActorLocation() : FVector::ZeroVector;
	OutHitLocation = Here;
	OutFromDir = Victim ? Victim->GetActorForwardVector() : FVector::ForwardVector;

	auto Accept = [&OutFromDir](const FVector& V) -> bool
	{
		if (V.SizeSquared2D() < KINDA_SMALL_NUMBER) { return false; }
		OutFromDir = V.GetSafeNormal();
		return true;
	};

	// 1. point damage: ShotDirection is the travel direction of the shot
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& P = static_cast<const FPointDamageEvent&>(DamageEvent);
		if (P.HitInfo.bBlockingHit) { OutHitLocation = P.HitInfo.ImpactPoint; }
		if (Accept(-P.ShotDirection)) { return true; }
	}
	// 2. radial damage: from the blast origin
	if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent& Rd = static_cast<const FRadialDamageEvent&>(DamageEvent);
		if (Accept(Rd.Origin - Here)) { return true; }
	}
	if (DamageCauser)
	{
		const FVector CauserLoc = DamageCauser->GetActorLocation();
		// 3. direct projectile hit (AIronProjectile calls ApplyDamage before Park(), so its velocity is still live)
		const FVector Vel = DamageCauser->GetVelocity();
		if (FVector::Dist(CauserLoc, Here) < 200.f && Vel.SizeSquared() > 1.f)
		{
			OutHitLocation = CauserLoc;
			if (Accept(-Vel)) { return true; }
		}
		// 4. splash centre / contact (ABossTitan ram)
		if (Accept(CauserLoc - Here)) { OutHitLocation = CauserLoc; return true; }
	}
	// 5. whoever fired
	if (EventInstigator)
	{
		if (const APawn* Pawn = EventInstigator->GetPawn())
		{
			if (Accept(Pawn->GetActorLocation() - Here)) { return true; }
		}
	}
	return false;
}

EHUDBodyRegion UHUDDataSubsystem::RegionForHit(float ScreenAngleDeg, float HeightAlpha01)
{
	const float A = FMath::Abs(FRotator::NormalizeAxis(ScreenAngleDeg));
	const bool bRight = ScreenAngleDeg > 0.f;
	if (HeightAlpha01 > 0.86f) { return EHUDBodyRegion::Helmet; }
	if (HeightAlpha01 < 0.42f) { return bRight ? EHUDBodyRegion::LegR : EHUDBodyRegion::LegL; }
	if (A > 135.f) { return EHUDBodyRegion::SpineMount; }       // from behind: Harley's back-mounted reactor
	if (A < 45.f) { return EHUDBodyRegion::Torso; }
	return bRight ? EHUDBodyRegion::ArmR : EHUDBodyRegion::ArmL;
}

void UHUDDataSubsystem::ReportPlayerHit(const AIronManCharacter* Victim, float AppliedDamage, float MaxHealth,
	const FDamageEvent& DamageEvent, const AController* EventInstigator, const AActor* DamageCauser)
{
	if (!Victim || AppliedDamage <= 0.f) { return; }
	bHookSeen = true;
	bReportedSinceSnapshot = true;

	FHUDHitReport R;
	R.Damage = AppliedDamage;
	R.MaxHealth = MaxHealth > 0.f ? MaxHealth : 100.f;
	R.bDirectionKnown = ResolveHitSource(Victim, DamageEvent, EventInstigator, DamageCauser, R.FromDir, R.HitLocation);

	// Angle relative to the CONTROL rotation (where the player is looking), not the actor yaw:
	// in first person they match, in the third-person chase camera the arc still points from the view.
	const float ControlYaw = Victim->GetControlRotation().Yaw;
	R.ScreenAngleDeg = R.bDirectionKnown
		? FMath::RadiansToDegrees(DamageArcMath::RelativeAngleRad(R.FromDir.X, R.FromDir.Y, ControlYaw))
		: 0.f;

	float HeightAlpha = 0.6f;
	if (const UCapsuleComponent* Cap = Victim->GetCapsuleComponent())
	{
		const float Half = FMath::Max(Cap->GetScaledCapsuleHalfHeight(), 1.f);
		HeightAlpha = FMath::Clamp((R.HitLocation.Z - (Victim->GetActorLocation().Z - Half)) / (2.f * Half), 0.f, 1.f);
		if (R.HitLocation.Equals(Victim->GetActorLocation(), 1.f)) { HeightAlpha = 0.6f; }   // unknown -> chest height
	}
	R.Region = RegionForHit(R.ScreenAngleDeg, HeightAlpha);

	AddArc(R);
}

void UHUDDataSubsystem::ReportPlayerHitFromDirection(FVector FromDir, float Damage, float MaxHealth, float ControlYawDeg)
{
	FHUDHitReport R;
	R.FromDir = FromDir.GetSafeNormal();
	R.Damage = Damage;
	R.MaxHealth = MaxHealth > 0.f ? MaxHealth : 100.f;
	bool bValid = false;
	R.ScreenAngleDeg = FMath::RadiansToDegrees(DamageArcMath::RelativeAngleRad(FromDir.X, FromDir.Y, ControlYawDeg, &bValid));
	R.bDirectionKnown = bValid;
	R.Region = RegionForHit(R.ScreenAngleDeg, 0.6f);
	AddArc(R);
}

void UHUDDataSubsystem::AddArc(const FHUDHitReport& Report)
{
	const bool bHealthCritical = HealthFraction <= 0.25f;
	Arcs.Add(FMath::DegreesToRadians(Report.ScreenAngleDeg), Report.Damage, Report.MaxHealth, bHealthCritical);

	if (RegionIntegrity.Num() != int32(EHUDBodyRegion::Count)) { RegionIntegrity.Init(1.f, int32(EHUDBodyRegion::Count)); }
	float& Zone = RegionIntegrity[int32(Report.Region)];
	Zone = FMath::Clamp(Zone - Report.Damage / Report.MaxHealth * ZoneWearPerHealthFraction, 0.f, 1.f);

	LastHit = Report;
	SecondsSinceHit = 0.f;
	++HitSerial;
	OnPlayerHit.Broadcast(Report);
}

FVector2D UHUDDataSubsystem::GetImpactDir2D() const
{
	if (SecondsSinceHit >= 0.1f) { return FVector2D::ZeroVector; }
	const float A = FMath::DegreesToRadians(LastHit.ScreenAngleDeg);
	return FVector2D(FMath::Sin(A), -FMath::Cos(A));   // screen space: x right, y down
}

// ------------------------------------------------------------------------------------------------
// Per-frame snapshot
// ------------------------------------------------------------------------------------------------

void UHUDDataSubsystem::Tick(float DeltaTime)
{
	// Undilated: hit-stop (time dilation 0.05) must not freeze the 0.5 s fade.
	const float RealDt = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.f, 0.25f);
	Arcs.Advance(RealDt);
	SecondsSinceHit += RealDt;
	SnapshotPlayer(RealDt);
	RescanTimer += RealDt;
	if (RescanTimer >= 1.f)
	{
		RescanTimer = 0.f;
		RescanTrackedActors(); // drops destroyed entries, picks up streamed-in actors
	}
}

void UHUDDataSubsystem::SnapshotPlayer(float RealDt)
{
	const AIronManCharacter* Player = Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player) { return; }

	const float MaxH = FMath::Max(Player->GetMaxHealth(), 1.f);
	HealthFraction = FMath::Clamp(Player->GetHealth() / MaxH, 0.f, 1.f);

	// No hook reported this drop (IronManCharacter_TakeDamage_hud_hook.patch not applied): still show an arc.
	if (!bHookSeen && !bReportedSinceSnapshot && LastHealth >= 0.f && Player->GetHealth() < LastHealth - 0.5f && !Player->IsDead())
	{
		ReportUnhookedHealthDrop(Player, LastHealth - Player->GetHealth());
	}
	bReportedSinceSnapshot = false;

	// Repairs (pickups, workshop) restore zones up to the health fraction.
	if (LastHealth >= 0.f && Player->GetHealth() > LastHealth + 0.5f)
	{
		for (float& Z : RegionIntegrity) { Z = FMath::Max(Z, HealthFraction); }
	}
	if (Player->GetHealth() >= MaxH - KINDA_SMALL_NUMBER) { for (float& Z : RegionIntegrity) { Z = 1.f; } }
	LastHealth = Player->GetHealth();

	const float MaxE = FMath::Max(Player->GetMaxEnergy(), 1.f);
	const float Pwr = FMath::Clamp(Player->GetEnergy() / MaxE, 0.f, 1.f);
	if (LastPower >= 0.f && RealDt > 0.f)
	{
		PowerRate = FMath::Lerp(PowerRate, (Pwr - LastPower) / RealDt, FMath::Clamp(RealDt * 6.f, 0.f, 1.f));
	}
	LastPower = PowerFraction = Pwr;

	if (const UReactorComponent* Reactor = Player->GetReactor())
	{
		HeatFraction = Reactor->GetHeatFraction();
		bVenting = Reactor->IsVenting();
	}

	bFlying = Player->IsFlying();
	const FVector V = Player->GetVelocity();
	AirspeedMs = float(V.Size()) / 100.f;
	VerticalSpeedMs = float(V.Z) / 100.f;

	// AGL: not traced here (the helmet HUD asks GetAglM(), throttled to ~10 Hz); AltitudeM stays -1.
	AltitudeM = -1.f;
}

void UHUDDataSubsystem::ReportUnhookedHealthDrop(const AIronManCharacter* Player, float Damage)
{
	const FVector Here = Player->GetActorLocation();
	const AEnemyBase* Nearest = nullptr;
	double BestD2 = FMath::Square(6000.0); // 60 m
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AEnemyBase> It(World); It; ++It)
		{
			const AEnemyBase* E = *It;
			if (!E || E->IsDead()) { continue; }
			const double D2 = FVector::DistSquared(E->GetActorLocation(), Here);
			if (D2 < BestD2) { BestD2 = D2; Nearest = E; }
		}
	}
	FHUDHitReport R;
	R.Damage = Damage;
	R.MaxHealth = FMath::Max(Player->GetMaxHealth(), 1.f);
	R.HitLocation = Here;
	if (Nearest)
	{
		R.FromDir = (Nearest->GetActorLocation() - Here).GetSafeNormal();
		bool bValid = false;
		R.ScreenAngleDeg = FMath::RadiansToDegrees(DamageArcMath::RelativeAngleRad(R.FromDir.X, R.FromDir.Y, Player->GetControlRotation().Yaw, &bValid));
		R.bDirectionKnown = bValid;
	}
	R.Region = RegionForHit(R.ScreenAngleDeg, 0.6f);
	AddArc(R);
}
