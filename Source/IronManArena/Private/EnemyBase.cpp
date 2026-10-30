#include "EnemyBase.h"
#include "IronManArena.h"
#include "IronManCharacter.h"
#include "IronProjectile.h"
#include "ArenaGameMode.h"
#include "ArenaBlastFX.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(60.f);
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	// Make sure the player's aim trace (Visibility) can see enemies.
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Collision);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	ArenaVisuals::PaintAll(this, Parts);
	VisualRoot->SetVisibility(bUsePlaceholderVisuals, true);
}

float AEnemyBase::GetHitRadius() const
{
	return Collision ? Collision->GetScaledSphereRadius() : 60.f;
}

AArenaGameMode* AEnemyBase::GetArenaGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
}

void AEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) { return; }
	Age += DeltaSeconds;

	if (FlashLeft > 0.f)
	{
		FlashLeft -= DeltaSeconds;
		if (FlashLeft <= 0.f) { ArenaVisuals::FlashAll(Parts, FLinearColor::White, true); }
	}

	// Cloaked (stealth suits), in a cutscene, or dead: enemies lose track and idle (every TickEnemy handles nullptr).
	AIronManCharacter* Player = Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	TickEnemy(DeltaSeconds, (Player && !Player->IsHiddenFromEnemies()) ? Player : nullptr);
}

float AEnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || DamageAmount <= 0.f) { return 0.f; }
	const float Applied = ModifyIncomingDamage(DamageAmount);
	if (Applied <= 0.f) { return 0.f; }

	Super::TakeDamage(Applied, DamageEvent, EventInstigator, DamageCauser);
	Health -= Applied;

	FlashLeft = HitFlashTime;
	ArenaVisuals::FlashAll(Parts, FLinearColor::White, false);

	if (Health <= 0.f)
	{
		Health = 0.f;
		bDead = true;
		SetActorEnableCollision(false);
		OnKilled();
		if (AArenaGameMode* GM = GetArenaGameMode()) { GM->OnEnemyKilled(this); }
		AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), DeathBlastRadius, FLinearColor(1.f, 0.55f, 0.15f), 0.5f);
		Destroy();
	}
	return Applied;
}

void AEnemyBase::OnKilled()
{
	// Subclasses spawn their drops.
}

void AEnemyBase::DropPickup(EPickupType Type, const FVector& Offset)
{
	APickup::SpawnPickup(GetWorld(), Type, GetActorLocation() + Offset);
}

AIronProjectile* AEnemyBase::FireShot(const FVector& Origin, const FVector& InVelocity, float Damage, float Radius,
	const FLinearColor& Color, float BlastRadius, float GravityScale, USceneComponent* HomingTarget, float HomingAccel, float LifeSeconds)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIronProjectile* P = World->SpawnActor<AIronProjectile>(AIronProjectile::StaticClass(), Origin, InVelocity.Rotation(), Params);
	if (P)
	{
		P->LifeSeconds = LifeSeconds;
		P->Configure(Damage, true, BlastRadius, Color, Radius);
		P->Launch(InVelocity, GravityScale, HomingTarget, HomingAccel);
	}
	return P;
}

bool AEnemyBase::MoveBy(const FVector& Delta)
{
	FHitResult Hit;
	SetActorLocation(GetActorLocation() + Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		// Slide along whatever we bumped into.
		const FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
		SetActorLocation(GetActorLocation() + Slide, true);
		return false;
	}
	return true;
}

void AEnemyBase::FaceToward(const FVector& Target, float DeltaSeconds, float DegreesPerSecond)
{
	const FVector To = Target - GetActorLocation();
	if (To.SizeSquared2D() < 1.f) { return; }
	const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Want, DeltaSeconds, DegreesPerSecond));
}

bool AEnemyBase::FindFloorZ(const FVector& Location, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyFloor), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 500), Location - FVector(0, 0, 20000),
		FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}
