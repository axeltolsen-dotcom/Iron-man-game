// AIronProjectile: every bullet in the game.
//   * Player repulsor blasts  (bHostile = false, hits AEnemyBase)
//   * Drone / boss energy bolts (bHostile = true, direct hit on the player)
//   * Tank artillery shells    (bHostile = true, gravity arc, BlastRadius > 0)
//   * Boss homing missiles     (bHostile = true, homing, can be shot down)
//
// Collision: a small query-only sphere that BLOCKS WorldStatic (floor, walls -> explode)
// and OVERLAPS Pawn (player capsule / enemy spheres -> damage). Everything else is ignored.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UMaterialInstanceDynamic;

UCLASS()
class IRONMANARENA_API AIronProjectile : public AActor
{
	GENERATED_BODY()

public:
	AIronProjectile();

	/**
	 * Set everything up right after SpawnActor.
	 * @param InDamage        damage on hit (or inside BlastRadius)
	 * @param bInHostile      true = fired by enemies, hurts the player
	 * @param InBlastRadius   0 = direct hit only, >0 = splash (uu)
	 * @param InColor         glow colour
	 * @param InRadius        visual / collision radius (uu)
	 */
	void Configure(float InDamage, bool bInHostile, float InBlastRadius, const FLinearColor& InColor, float InRadius);

	/** Give it velocity. GravityScale 0 = straight line. HomingTarget optional. */
	void Launch(const FVector& InVelocity, float GravityScale = 0.f, USceneComponent* HomingTarget = nullptr, float HomingAcceleration = 0.f);

	/** Can the player's repulsor / chest beam destroy this shot (boss missiles)? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bDestroyableByPlayer = false;

	/** Score awarded when the player shoots this down (reference game: 25). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	int32 InterceptScore = 25;

	/** Destroyed by the player's weapons: small boom, no damage. */
	void Intercept();

	bool IsHostile() const { return bHostile; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float BlastRadius = 0.f;

	/** Seconds before the shot fizzles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float LifeSeconds = 3.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

	/** Apply damage (direct or splash), spawn FX, destroy. */
	void Explode(AActor* DirectHitActor);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	bool bHostile = false;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	FLinearColor Color = FLinearColor(0.4f, 0.9f, 1.f);

	bool bExploded = false;
};
