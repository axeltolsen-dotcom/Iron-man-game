#include "IronProjectile.h"
#include "ArenaFeel.h"
#include "ArenaVisuals.h"
#include "ArenaBlastFX.h"
#include "IronManCharacter.h"
#include "EnemyBase.h"
#include "ArenaGameMode.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"

AIronProjectile::AIronProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Sphere));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->CastShadow = false;
	Visual->SetRelativeScale3D(FVector(0.24f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensity(1500.f);
	Glow->SetAttenuationRadius(250.f);
	Glow->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;                 // 0 = unlimited
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
}

void AIronProjectile::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AIronProjectile::OnSphereOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &AIronProjectile::OnProjectileStopped);
	if (GetOwner())
	{
		Collision->IgnoreActorWhenMoving(GetOwner(), true);
	}
	SetLifeSpan(LifeSeconds);
	MID = ArenaVisuals::PaintComponent(this, Visual, Color, 6.f);
}

void AIronProjectile::Configure(float InDamage, bool bInHostile, float InBlastRadius, const FLinearColor& InColor, float InRadius)
{
	Damage = InDamage;
	bHostile = bInHostile;
	BlastRadius = InBlastRadius;
	Color = InColor;
	Collision->SetSphereRadius(InRadius);
	Visual->SetRelativeScale3D(FVector(InRadius * 2.f / 100.f));
	Glow->SetLightColor(InColor);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), InColor);
	}
	SetLifeSpan(LifeSeconds);
}

void AIronProjectile::Launch(const FVector& InVelocity, float GravityScale, USceneComponent* HomingTarget, float HomingAcceleration)
{
	Movement->ProjectileGravityScale = GravityScale;
	Movement->Velocity = InVelocity;
	if (HomingTarget && HomingAcceleration > 0.f)
	{
		Movement->bIsHomingProjectile = true;
		Movement->HomingTargetComponent = HomingTarget;
		Movement->HomingAccelerationMagnitude = HomingAcceleration;
		Movement->MaxSpeed = InVelocity.Size() * 1.6f; // keep homing missiles dodgeable
	}
	Movement->UpdateComponentVelocity();
	SetActorRotation(InVelocity.Rotation());
}

void AIronProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bExploded || !OtherActor || OtherActor == this || OtherActor == GetOwner()) { return; }

	if (bHostile)
	{
		// Enemy shots only care about the player.
		if (OtherActor->IsA<AIronManCharacter>()) { Explode(OtherActor); }
	}
	else
	{
		// Player shots only care about enemies.
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor))
		{
			if (!Enemy->IsDead()) { Explode(OtherActor); }
		}
	}
}

void AIronProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	// Hit the floor or a wall (WorldStatic): leave a scorch mark (player shots and blasts).
	if (!bExploded && (!bHostile || BlastRadius > 0.f) && ImpactResult.bBlockingHit)
	{
		ArenaFeel::SpawnImpactDecal(GetWorld(), ImpactResult.ImpactPoint, ImpactResult.ImpactNormal,
			BlastRadius > 0.f ? FMath::Clamp(BlastRadius * 0.9f, 60.f, 400.f) : 28.f + Collision->GetUnscaledSphereRadius() * 1.5f);
	}
	Explode(nullptr);
}

void AIronProjectile::Intercept()
{
	if (bExploded) { return; }
	bExploded = true;
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 120.f, FLinearColor(1.f, 0.6f, 0.2f), 0.3f);
	if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		GM->AddScore(InterceptScore);
	}
	Destroy();
}

void AIronProjectile::Explode(AActor* DirectHitActor)
{
	if (bExploded) { return; }
	bExploded = true;

	UWorld* World = GetWorld();
	const FVector Here = GetActorLocation();
	AController* InstigatorController = GetInstigatorController();

	if (BlastRadius > 0.f && World)
	{
		// Splash: find pawns in range and damage the ones on the opposite team.
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjParams(ECC_Pawn);
		World->OverlapMultiByObjectType(Overlaps, Here, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(BlastRadius));
		TSet<AActor*> Damaged;
		for (const FOverlapResult& O : Overlaps)
		{
			AActor* A = O.GetActor();
			if (!A || Damaged.Contains(A) || A == GetOwner()) { continue; }
			const bool bValid = bHostile ? A->IsA<AIronManCharacter>() : A->IsA<AEnemyBase>();
			if (!bValid) { continue; }
			Damaged.Add(A);
			UGameplayStatics::ApplyDamage(A, Damage, InstigatorController, this, UDamageType::StaticClass());
		}
		AArenaBlastFX::Spawn(World, Here, BlastRadius, FLinearColor(1.f, 0.45f, 0.1f), 0.45f);
	}
	else
	{
		if (DirectHitActor)
		{
			UGameplayStatics::ApplyDamage(DirectHitActor, Damage, InstigatorController, this, UDamageType::StaticClass());
		}
		AArenaBlastFX::Spawn(World, Here, 45.f, Color, 0.15f);
	}
	Destroy();
}
