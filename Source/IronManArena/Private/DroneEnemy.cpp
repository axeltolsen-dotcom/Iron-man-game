#include "DroneEnemy.h"
#include "IronManCharacter.h"

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
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(Age * 3.f) * 10.f));

	if (!Player || Player->IsDead())
	{
		Velocity *= FMath::Max(0.f, 1.f - DeltaSeconds);
		MoveBy(Velocity * DeltaSeconds);
		return;
	}

	// --- Movement: spring toward an orbit point around the player ---
	OrbitAngle += OrbitDirection * OrbitDriftSpeed * DeltaSeconds;
	const FVector PlayerLoc = Player->GetActorLocation();
	const float Rad = FMath::DegreesToRadians(OrbitAngle);
	FVector Target = PlayerLoc + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.f) * OrbitDistance + FVector(0.f, 0.f, HeightOffset);

	float FloorZ = 0.f;
	if (FindFloorZ(Target, FloorZ)) { Target.Z = FMath::Max(Target.Z, FloorZ + MinAltitude); }

	const FVector Loc = GetActorLocation();
	MoveBy(IntegrateSpringSteer(Velocity, Loc, Target, Spring, Damping, DeltaSeconds)); // B17: fps-independent

	// Bank a little into the movement and face the player.
	FaceToward(PlayerLoc, DeltaSeconds, 240.f);
	const float Bank = FMath::Clamp(FVector::DotProduct(Velocity, GetActorRightVector()) * 0.03f, -25.f, 25.f);
	VisualRoot->SetRelativeRotation(FRotator(0.f, 0.f, Bank));

	// --- Shooting ---
	FireCooldown -= DeltaSeconds;
	const FVector AimAt = Player->GetActorLocation();
	const float Dist = FVector::Dist(AimAt, Loc);
	if (FireCooldown <= 0.f && Dist < FireRange)
	{
		FireCooldown = Rand(FireIntervalMin, FireIntervalMax);
		const FVector Muzzle = Loc + GetActorForwardVector() * 70.f - FVector(0.f, 0.f, 30.f);
		// Light lead: aim where the player will be in ~40% of the flight time.
		const float Flight = Dist / BoltSpeed;
		const FVector Lead = AimAt + Player->GetVelocity() * Flight * 0.4f;
		FireShot(Muzzle, (Lead - Muzzle).GetSafeNormal() * BoltSpeed, BoltDamage, 14.f, FLinearColor(1.f, 0.3f, 0.1f));
	}
}

void ADroneEnemy::OnKilled()
{
	const float R = FMath::FRand();
	if (R < GemDropChance) { DropPickup(EPickupType::Gem); }
	else if (R < GemDropChance + EnergyDropChance) { DropPickup(EPickupType::Energy); }
}
