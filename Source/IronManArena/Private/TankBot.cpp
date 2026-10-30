#include "TankBot.h"
#include "IronManCharacter.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ATankBot::ATankBot()
{
	Kind = EEnemyKind::Tank;
	MaxHealth = 16.f;
	ScoreValue = 350;
	DeathBlastRadius = 420.f;
	Collision->SetSphereRadius(120.f);

	using namespace ArenaVisuals;
	const FLinearColor Olive(0.12f, 0.14f, 0.1f);
	const FLinearColor Hazard(0.9f, 0.45f, 0.05f);

	// Hull + treads (Z is relative to the hull centre; floor is at -RideHeight).
	AddPart(this, VisualRoot, TEXT("Hull"), EArenaShape::Cube, FVector(0.f, 0.f, 0.f), FVector(2.6f, 1.7f, 0.7f), FRotator::ZeroRotator, EnemyGrey, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("TreadL"), EArenaShape::Cube, FVector(0.f, -95.f, -85.f), FVector(2.9f, 0.45f, 0.6f), FRotator::ZeroRotator, EnemyDark, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("TreadR"), EArenaShape::Cube, FVector(0.f, 95.f, -85.f), FVector(2.9f, 0.45f, 0.6f), FRotator::ZeroRotator, EnemyDark, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("Stripe"), EArenaShape::Cube, FVector(125.f, 0.f, 10.f), FVector(0.1f, 1.5f, 0.15f), FRotator::ZeroRotator, Hazard, 0.f, Parts);

	Turret = CreateDefaultSubobject<USceneComponent>(TEXT("Turret"));
	Turret->SetupAttachment(VisualRoot);
	Turret->SetRelativeLocation(FVector(-10.f, 0.f, 55.f));
	AddPart(this, Turret, TEXT("TurretDome"), EArenaShape::Cylinder, FVector::ZeroVector, FVector(1.3f, 1.3f, 0.5f), FRotator::ZeroRotator, Olive, 0.f, Parts);
	AddPart(this, Turret, TEXT("Sensor"), EArenaShape::Sphere, FVector(45.f, 30.f, 25.f), FVector(0.2f), FRotator::ZeroRotator, EnemyEye, 8.f, Parts);

	BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
	BarrelPivot->SetupAttachment(Turret);
	BarrelPivot->SetRelativeLocation(FVector(40.f, 0.f, 5.f));
	// Cylinder is along Z; pitch -90 lays it along +X. Offset half its length forward.
	AddPart(this, BarrelPivot, TEXT("Barrel"), EArenaShape::Cylinder, FVector(80.f, 0.f, 0.f), FVector(0.22f, 0.22f, 1.6f), FRotator(-90.f, 0.f, 0.f), EnemyDark, 0.f, Parts);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(BarrelPivot);
	Muzzle->SetRelativeLocation(FVector(170.f, 0.f, 0.f));
}

void ATankBot::BeginPlay()
{
	Super::BeginPlay();
	FireCooldown = Rand(1.6f, 2.6f);
	float Z = 0.f;
	if (FindFloorZ(GetActorLocation(), Z))
	{
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Z + RideHeight));
	}
}

void ATankBot::InitForWave(int32 InWave)
{
	Super::InitForWave(InWave);
	// Reference: cooldown * max(0.6, 1 - wave * 0.03)
	const float Mul = FMath::Max(0.6f, 1.f - 0.03f * InWave);
	FireIntervalMin *= Mul;
	FireIntervalMax *= Mul;
}

FVector ATankBot::GetAimPoint() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 40.f);
}

void ATankBot::TickEnemy(float DeltaSeconds, AIronManCharacter* Player)
{
	Recoil = FMath::Max(0.f, Recoil - DeltaSeconds * 3.f);
	if (!Player || Player->IsDead()) { return; }

	const FVector Loc = GetActorLocation();
	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector To2D = FVector(PlayerLoc.X - Loc.X, PlayerLoc.Y - Loc.Y, 0.f);
	const float Dist2D = To2D.Size();
	const FVector Dir = To2D.GetSafeNormal();

	// --- Drive: close in, back off, or hold ---
	float Want = 0.f;
	if (Dist2D > KeepDistance + 150.f)      { Want = MoveSpeed; }
	else if (Dist2D < KeepDistance - 350.f) { Want = -MoveSpeed * 0.7f; }

	FaceToward(PlayerLoc, DeltaSeconds, 45.f); // hull turns slowly
	FVector Delta = Dir * Want * DeltaSeconds;

	// Stick to the floor.
	float FloorZ = 0.f;
	if (FindFloorZ(Loc + Delta, FloorZ))
	{
		Delta.Z = (FloorZ + RideHeight) - Loc.Z;
	}
	MoveBy(Delta);

	// --- Turret: yaw toward the player, pitch toward the lob angle ---
	const FRotator TurretWorld(0.f, To2D.Rotation().Yaw, 0.f);
	Turret->SetWorldRotation(FMath::RInterpConstantTo(Turret->GetComponentRotation(), TurretWorld, DeltaSeconds, 120.f));
	BarrelPivot->SetRelativeRotation(FRotator(35.f, 0.f, 0.f));
	BarrelPivot->SetRelativeLocation(FVector(40.f - Recoil * 30.f, 0.f, 5.f));

	// --- Fire a ballistic shell that lands on the player's position in ShellFlightTime ---
	FireCooldown -= DeltaSeconds;
	if (FireCooldown <= 0.f && Dist2D < FireRange)
	{
		FireCooldown = Rand(FireIntervalMin, FireIntervalMax);
		Recoil = 1.f;

		const FVector Origin = Muzzle->GetComponentLocation();
		FVector TargetPt = PlayerLoc;
		float PFloor = 0.f;
		if (!Player->IsFlying() && FindFloorZ(PlayerLoc, PFloor)) { TargetPt.Z = PFloor + 20.f; }

		const float T = FMath::Max(0.3f, ShellFlightTime);
		const float G = FMath::Abs(GetWorld()->GetGravityZ()) * ShellGravityScale;
		// p(t) = O + v t + 0.5 g t^2  ->  v = (P - O)/t - 0.5 g t   (g points down)
		FVector V = (TargetPt - Origin) / T;
		V.Z += 0.5f * G * T;
		FireShot(Origin, V, ShellDamage, 22.f, FLinearColor(1.f, 0.5f, 0.1f), ShellBlastRadius, ShellGravityScale, nullptr, 0.f, 5.f);
	}
}

void ATankBot::OnKilled()
{
	const float R = FMath::FRand();
	if (R < HulkbusterDropChance)                         { DropPickup(EPickupType::Hulkbuster, FVector(0, 0, 80)); }
	else if (R < HulkbusterDropChance + HealthDropChance) { DropPickup(EPickupType::Health, FVector(0, 0, 80)); }
	else                                                  { DropPickup(EPickupType::Energy, FVector(0, 0, 80)); }
	DropPickup(EPickupType::Gem, FVector(60, 0, 80));
}
