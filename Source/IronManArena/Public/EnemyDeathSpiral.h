// AEnemyDeathSpiral: cosmetic drone death spiral (Jocasta combat realism, enemyDamage.drone.death).
//
// The dying drone does all its bookkeeping (kill count, combo, score, wave clear) and is destroyed
// immediately, exactly as before. This actor takes a copy of its visible basic-shape parts, keeps the
// drone's momentum, falls and spins toward the damaged side for 0.6-1.2 s, then plays the drone's
// usual death blast on impact with world geometry or at timeout. No collision, no gameplay effect,
// no new assets. At most MaxLive at once (beyond that the blast plays immediately).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "EnemyDeathSpiral.generated.h"

UCLASS(NotPlaceable, Transient)
class IRONMANARENA_API AEnemyDeathSpiral : public AActor
{
	GENERATED_BODY()

public:
	AEnemyDeathSpiral();

	static constexpr int32 MaxLive = 8;

	/**
	 * Start a spiral from SourceActor's current pose. SourceParts are the painted parts of its
	 * VisualRoot (only visible ones are copied). SpinSide -1/+1 = spin toward the left/right, 0 = random.
	 * Returns null (and the caller should play the blast itself) if nothing could be spawned.
	 */
	static AEnemyDeathSpiral* Launch(AActor* SourceActor, const TArray<FArenaPaintedPart>& SourceParts,
		const FVector& InVelocity, int32 SpinSide, float InLifeSeconds, float InBlastRadius);

	virtual void Tick(float DeltaSeconds) override;

	/** Number of live spirals in this world (tests / cap). */
	static int32 CountLive(const UWorld* World);

protected:
	void Explode();

	UPROPERTY(VisibleAnywhere, Category = "Spiral")
	TObjectPtr<USceneComponent> SpinRoot;

	UPROPERTY(Transient)
	TArray<FArenaPaintedPart> SpiralParts;

	FVector SpiralVelocity = FVector::ZeroVector;
	float SpiralAge = 0.f;
	float LifeSeconds = 0.9f;
	float BlastRadius = 220.f;
	float SpinSign = 1.f;
	float SpinYaw = 0.f;
	bool bExploded = false;
};
