#include "IronProjectile.h"
#include "ArenaAudio.h"
#include "CityLife.h"
#include "ArenaFXPool.h"
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
	Glow->SetIndirectLightingIntensity(0.f); // keep gameplay lights out of Lumen scene lighting
	Glow->SetVisibility(false);             // lit only when the FX light budget allows (Configure)

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

AIronProjectile* AIronProjectile::SpawnPooled(UWorld* World, const FVector& Location, const FRotator& Rotation, AActor* InOwner, APawn* InInstigator, bool bInHostile)
{
	if (!World) { return nullptr; }
	if (UArenaFXPool* Pool = UArenaFXPool::Get(World))
	{
		return Pool->AcquireProjectile(Location, Rotation, InOwner, InInstigator, bInHostile);
	}
	FActorSpawnParameters Params;
	Params.Owner = InOwner;
	Params.Instigator = InInstigator;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AIronProjectile>(AIronProjectile::StaticClass(), Location, Rotation, Params);
}

void AIronProjectile::ReactivateFromPool(const FVector& Location, const FRotator& Rotation, AActor* InOwner, APawn* InInstigator)
{
	bExploded = false;
	bActiveShot = true;
	bDestroyableByPlayer = false;
	InterceptScore = 25;
	LifeSeconds = 3.f;
	VisualProfile = EIronProjectileVisual::Ball;
	VisualRadius = 12.f;
	SetOwner(InOwner);
	SetInstigator(InInstigator);
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::ResetPhysics);
	Collision->ClearMoveIgnoreActors();
	if (InOwner) { Collision->IgnoreActorWhenMoving(InOwner, true); }
	SetActorHiddenInGame(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Movement->SetUpdatedComponent(Collision);
	Movement->bIsHomingProjectile = false;
	Movement->HomingTargetComponent = nullptr;
	Movement->HomingAccelerationMagnitude = 0.f;
	Movement->MaxSpeed = 0.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->SetComponentTickEnabled(true);
	Movement->Activate(true);
	SetLifeSpan(LifeSeconds);
}

void AIronProjectile::SetGlowEnabled(bool bWant)
{
	UArenaFXPool* Pool = UArenaFXPool::Get(this);
	if (bWant && !bHasLight)
	{
		bHasLight = !Pool || Pool->TryClaimLight();
	}
	else if (!bWant && bHasLight)
	{
		bHasLight = false;
		if (Pool) { Pool->ReturnLight(); }
	}
	Glow->SetVisibility(bHasLight);
}

void AIronProjectile::Park()
{
	bExploded = true;
	bActiveShot = false;
	SetGlowEnabled(false);
	SetLifeSpan(0.f);
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	if (UArenaFXPool* Pool = UArenaFXPool::Get(this)) { Pool->ReleaseProjectile(this); }
	else { Destroy(); }
}

void AIronProjectile::Recycle()
{
	if (!bActiveShot) { return; }
	Park();
}

void AIronProjectile::LifeSpanExpired()
{
	// Fizzle: back to the pool instead of Destroy.
	Park();
}

void AIronProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetGlowEnabled(false);
	if (UArenaFXPool* Pool = UArenaFXPool::Get(this)) { Pool->Forget(this); }
	Super::EndPlay(EndPlayReason);
}

void AIronProjectile::Configure(float InDamage, bool bInHostile, float InBlastRadius, const FLinearColor& InColor, float InRadius, bool bWantLight, EIronProjectileVisual InVisual)
{
	Damage = InDamage;
	bHostile = bInHostile;
	BlastRadius = InBlastRadius;
	Color = InColor;
	VisualRadius = InRadius;
	VisualProfile = InVisual;
	Collision->SetSphereRadius(InRadius);
	Glow->SetLightColor(InColor);
	SetGlowEnabled(bWantLight); // minigun / flamethrower shots pass false: emissive only
	ApplyVisualProfile();
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), InColor);
		// Bolts / missiles read hotter than soft orbs (film energy streak).
		const float GlowNits = (VisualProfile == EIronProjectileVisual::Ball) ? 6.f : (VisualProfile == EIronProjectileVisual::Missile ? 10.f : 14.f);
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(GlowNits));
	}
	SetLifeSpan(LifeSeconds);
}

void AIronProjectile::SetVisualProfile(EIronProjectileVisual InVisual)
{
	VisualProfile = InVisual;
	ApplyVisualProfile();
}

void AIronProjectile::ApplyVisualProfile()
{
	const float R = FMath::Max(4.f, VisualRadius);
	if (VisualProfile == EIronProjectileVisual::Bolt)
	{
		// Needle energy streak (Pass 2: thinner / longer).
		Visual->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
		Visual->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // cylinder Z -> actor forward
		const float Thick = FMath::Clamp(R * 0.38f, 2.2f, 9.f);
		const float Len = FMath::Clamp(R * 12.f, 70.f, 280.f);
		Visual->SetRelativeScale3D(FVector(Thick / 100.f, Thick / 100.f, Len / 100.f));
	}
	else if (VisualProfile == EIronProjectileVisual::Missile)
	{
		// Slim rocket body (not a glowing ball).
		Visual->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
		Visual->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		const float Thick = FMath::Clamp(R * 0.48f, 3.5f, 11.f);
		const float Len = FMath::Clamp(R * 8.5f, 55.f, 200.f);
		Visual->SetRelativeScale3D(FVector(Thick / 100.f, Thick / 100.f, Len / 100.f));
	}
	else
	{
		Visual->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Sphere));
		Visual->SetRelativeRotation(FRotator::ZeroRotator);
		Visual->SetRelativeScale3D(FVector(R * 2.f / 100.f));
	}
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
	Park();
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
		ArenaAudio::PlayAt(this, TEXT("missile_explosion"), Here, 1.f);
	}
	// City cars / pedestrians take player blasts and direct hits too (shooting overhaul).
	if (!bHostile && World) { CityLife::Damage(World, Here, FMath::Max(BlastRadius, 40.f), Damage, DirectHitActor, true); }
	else
	{
		if (DirectHitActor)
		{
			UGameplayStatics::ApplyDamage(DirectHitActor, Damage, InstigatorController, this, UDamageType::StaticClass());
		}
		AArenaBlastFX::Spawn(World, Here, 45.f, Color, 0.15f);
	}
	Park();
}
