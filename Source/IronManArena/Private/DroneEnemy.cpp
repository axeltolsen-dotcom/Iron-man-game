#include "DroneEnemy.h"
#include "IronManCharacter.h"
#include "ArenaCombat.h"
#include "EnemyDeathSpiral.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ADroneEnemy::ADroneEnemy()
{
	Kind = EEnemyKind::Drone;
	MaxHealth = 3.f;
	ScoreValue = 100;
	DeathBlastRadius = 220.f;
	Collision->SetSphereRadius(60.f);

	using namespace ArenaVisuals;
	// Body: dark sphere with a grey armour band, a glowing red "eye" facing +X, two rotor pods.
	AddPart(this, VisualRoot, TEXT("Body"), EArenaShape::Sphere, FVector::ZeroVector, FVector(0.9f), FRotator::ZeroRotator, EnemyGrey, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("Band"), EArenaShape::Cylinder, FVector::ZeroVector, FVector(1.05f, 1.05f, 0.18f), FRotator::ZeroRotator, EnemyDark, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("Eye"), EArenaShape::Sphere, FVector(40.f, 0.f, 5.f), FVector(0.28f), FRotator::ZeroRotator, EnemyEye, 8.f, Parts);
	AddPart(this, VisualRoot, TEXT("Gun"), EArenaShape::Cylinder, FVector(35.f, 0.f, -35.f), FVector(0.1f, 0.1f, 0.5f), FRotator(-90.f, 0.f, 0.f), EnemyDark, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("ArmL"), EArenaShape::Cube, FVector(0.f, -60.f, 10.f), FVector(0.15f, 0.6f, 0.08f), FRotator::ZeroRotator, EnemyDark, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("ArmR"), EArenaShape::Cube, FVector(0.f, 60.f, 10.f), FVector(0.15f, 0.6f, 0.08f), FRotator::ZeroRotator, EnemyDark, 0.f, Parts);
	RotorL = AddPart(this, VisualRoot, TEXT("RotorL"), EArenaShape::Cylinder, FVector(0.f, -95.f, 18.f), FVector(0.55f, 0.55f, 0.03f), FRotator::ZeroRotator, FLinearColor(1.f, 0.5f, 0.15f), 3.f, Parts);
	RotorR = AddPart(this, VisualRoot, TEXT("RotorR"), EArenaShape::Cylinder, FVector(0.f, 95.f, 18.f), FVector(0.55f, 0.55f, 0.03f), FRotator::ZeroRotator, FLinearColor(1.f, 0.5f, 0.15f), 3.f, Parts);

	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(VisualRoot);
	EyeLight->SetRelativeLocation(FVector(60.f, 0.f, 0.f));
	EyeLight->SetLightColor(EnemyEye);
	EyeLight->SetIntensity(2000.f);
	EyeLight->SetAttenuationRadius(350.f);
	EyeLight->SetCastShadows(false);
}

void ADroneEnemy::BeginPlay()
{
	Super::BeginPlay();
	OrbitDistance = Rand(OrbitDistanceRange.X, OrbitDistanceRange.Y);
	HeightOffset = Rand(HeightOffsetRange.X, HeightOffsetRange.Y);
	OrbitDirection = FMath::RandBool() ? 1.f : -1.f;
	FireCooldown = Rand(1.5f, 3.f);

	// Start the orbit at whatever angle we spawned at relative to the player.
	if (APawn* P = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
	{
		const FVector From = GetActorLocation() - P->GetActorLocation();
		OrbitAngle = FMath::RadiansToDegrees(FMath::Atan2(From.Y, From.X));
	}
}

void ADroneEnemy::InitForWave(int32 InWave)
{
	Super::InitForWave(InWave);
	// Later waves fire a little faster (same curve as the 2-D tanks: -3% per wave, floor 60%).
	const float Mul = FMath::Max(0.6f, 1.f - 0.03f * (InWave - 1));
	FireIntervalMin *= Mul;
	FireIntervalMax *= Mul;
}

void ADroneEnemy::TickEnemy(float DeltaSeconds, AIronManCharacter* Player)
{
	// Spin the rotors and bob.
	RotorL->AddLocalRotation(FRotator(0.f, 1400.f * DeltaSeconds, 0.f));
	RotorR->AddLocalRotation(FRotator(0.f, -1400.f * DeltaSeconds, 0.f));
	TickDroneDamageState(DeltaSeconds);
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(Age * 3.f) * 10.f + SagOffset));

	if (!Player || Player->IsDead())
	{
		Velocity *= FMath::Max(0.f, 1.f - DeltaSeconds);
		MoveBy(Velocity * DeltaSeconds);
		VisualRoot->SetRelativeRotation(FRotator(KickPitch + DamagePitch, 0.f, KickRoll + DamageRoll));
		return;
	}

	// --- Movement: spring toward an orbit point around the player ---
	OrbitAngle += OrbitDirection * OrbitDriftSpeed * DeltaSeconds;
	const FVector PlayerLoc = Player->GetActorLocation();
	const float Rad = FMath::DegreesToRadians(OrbitAngle);
	FVector Target = PlayerLoc + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.f) * OrbitDistance + FVector(0.f, 0.f, HeightOffset);

	float FloorZ = 0.f;
	if (FindFloorZ(Target, FloorZ)) { Target.Z = FMath::Max(Target.Z, FloorZ + MinAltitude); }
	// Combat realism: at <= 25% hp it can't hold its altitude (sags ~30 cm below its orbit height).
	if (GetHealthFraction() <= CombatRealism::Get().DroneSmokeBelow) { Target.Z -= CombatRealism::Get().DroneSagCm; }

	const FVector Loc = GetActorLocation();
	MoveBy(IntegrateSpringSteer(Velocity, Loc, Target, Spring, Damping, DeltaSeconds)); // B17: fps-independent

	// Bank a little into the movement and face the player.
	FaceToward(PlayerLoc, DeltaSeconds, 240.f);
	const float Bank = FMath::Clamp(FVector::DotProduct(Velocity, GetActorRightVector()) * 0.03f, -25.f, 25.f);
	VisualRoot->SetRelativeRotation(FRotator(KickPitch + DamagePitch, 0.f, Bank + KickRoll + DamageRoll));

	// --- Shooting ---
	FireCooldown -= DeltaSeconds;
	const FVector AimAt = Player->GetActorLocation();
	const float Dist = FVector::Dist(AimAt, Loc);
	if (FireCooldown <= 0.f && Dist < FireRange && !IsStaggered()) // combat realism: heavy hits stagger (no fire)
	{
		FireCooldown = Rand(FireIntervalMin, FireIntervalMax);
		const FVector Muzzle = Loc + GetActorForwardVector() * 70.f - FVector(0.f, 0.f, 30.f);
		// Light lead: aim where the player will be in ~40% of the flight time.
		const float Flight = Dist / BoltSpeed;
		const FVector Lead = AimAt + Player->GetVelocity() * Flight * 0.4f;
		FVector AimDir = (Lead - Muzzle).GetSafeNormal();
		// Combat realism: a wobbling (<= 50% hp) drone's aim is shakier (it has no spread when healthy).
		if (GetHealthFraction() <= CombatRealism::Get().DroneWobbleBelow && DamagedAimSpreadDeg > 0.f)
		{
			AimDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(DamagedAimSpreadDeg * CombatRealism::Get().DroneAimSpreadMul));
		}
		FireShot(Muzzle, AimDir * BoltSpeed, BoltDamage, 14.f, FLinearColor(1.f, 0.3f, 0.1f));
	}
}

void ADroneEnemy::OnKilled()
{
	const float R = FMath::FRand();
	if (R < GemDropChance) { DropPickup(EPickupType::Gem); }
	else if (R < GemDropChance + EnergyDropChance) { DropPickup(EPickupType::Energy); }
}

void ADroneEnemy::TickDroneDamageState(float DeltaSeconds)
{
	const CombatRealism::FTuning& Tune = CombatRealism::Get();
	const float HpFrac = GetHealthFraction();
	DamagePitch = 0.f;
	DamageRoll = 0.f;
	SagOffset = 0.f;
	if (HpFrac > Tune.DroneWobbleBelow || HpFrac <= 0.f) { return; }

	// The side that took the hit that crossed the line is the damaged rotor.
	if (DamagedSide == 0)
	{
		DamagedSide = FVector::DotProduct(LastHitDir, GetActorRightVector()) > 0.f ? -1 : 1;
		SparkTimer = Rand(Tune.DroneSparkMin * 0.5f, Tune.DroneSparkMax * 0.5f);
	}

	// 4 deg wobble at 2 Hz plus a 3 deg list toward the damaged side (positive roll = right side down).
	const float Phase = 2.f * PI * Tune.DroneWobbleHz * Age;
	DamageRoll = Tune.DroneWobbleDeg * FMath::Sin(Phase) + 3.f * float(DamagedSide);
	DamagePitch = 0.5f * Tune.DroneWobbleDeg * FMath::Cos(Phase * 0.5f);

	// The damaged rotor flickers (skipped during the white hit flash so it doesn't fight it).
	UStaticMeshComponent* Rotor = DamagedSide < 0 ? RotorL.Get() : RotorR.Get();
	if (FlashLeft <= 0.f)
	{
		for (FArenaPaintedPart& P : Parts)
		{
			if (P.Component == Rotor)
			{
				const bool bOut = FMath::Frac(Age * 7.3f) < 0.3f || FMath::Frac(Age * 2.9f) < 0.12f;
				ArenaVisuals::SetPartGlow(P, bOut ? P.Emissive * 0.1f : P.Emissive);
				break;
			}
		}
	}

	if (HpFrac <= Tune.DroneSmokeBelow)
	{
		// Fighting to stay up: a small labouring bob on top of the real altitude sag (see TickEnemy).
		SagOffset = -4.f + 4.f * FMath::Sin(Age * 15.f);
		SparkTimer -= DeltaSeconds;
		if (SparkTimer <= 0.f && Rotor)
		{
			SparkTimer = Rand(Tune.DroneSparkMin, Tune.DroneSparkMax);
			AArenaSparks::Spawn(GetWorld(), Rotor->GetComponentLocation(), -FVector::UpVector, FLinearColor(1.f, 0.7f, 0.3f), 5, 420.f);
		}
	}
}

void ADroneEnemy::SpawnDeathFX()
{
	// Combat realism death spiral: cosmetic copy keeps our momentum + knockback and spins toward the
	// damaged side for 0.6-1.2 s, then plays the usual blast. Bookkeeping already happened.
	const CombatRealism::FTuning& Tune = CombatRealism::Get();
	const FVector Momentum = Velocity + KnockVelocity;
	int32 SpinSide = DamagedSide;
	if (SpinSide == 0 && !LastHitDir.IsNearlyZero()) { SpinSide = FVector::DotProduct(LastHitDir, GetActorRightVector()) > 0.f ? 1 : -1; }
	if (AEnemyDeathSpiral::Launch(this, Parts, Momentum, SpinSide, Rand(Tune.DroneSpiralMin, Tune.DroneSpiralMax), DeathBlastRadius))
	{
		return;
	}
	Super::SpawnDeathFX();
}
