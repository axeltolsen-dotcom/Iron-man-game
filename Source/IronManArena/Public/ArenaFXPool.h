// UArenaFXPool: per-world pool for projectiles and blast FX (Ultron perf budget §5.3 / §6.2).
//   * Parked actors are hidden, collision-free and not ticking; Acquire*() reuses them instead of
//     SpawnActor/Destroy churn (game thread + GC).
//   * Live caps: 32 player shots, 24 hostile shots + missiles, 12 blasts. Past a cap the OLDEST live
//     one is recycled.
//   * FX light budget: at most 16 point lights on shots + blasts at once; the rest are emissive only.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaFXPool.generated.h"

class AIronProjectile;
class AArenaBlastFX;

UCLASS()
class IRONMANARENA_API UArenaFXPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxPlayerShots = 32;
	static constexpr int32 MaxHostileShots = 24;
	static constexpr int32 MaxBlasts = 12;
	static constexpr int32 MaxFXLights = 16;
	static constexpr int32 MaxParkedPerType = 64;

	static UArenaFXPool* Get(const UObject* WorldContext);

	/** A ready (visible, colliding) projectile at Location, from the pool or freshly spawned. */
	AIronProjectile* AcquireProjectile(const FVector& Location, const FRotator& Rotation, AActor* InOwner, APawn* InInstigator, bool bInHostile);
	void ReleaseProjectile(AIronProjectile* Shot);

	AArenaBlastFX* AcquireBlast(const FVector& Location);
	void ReleaseBlast(AArenaBlastFX* Blast);

	/** Ask for one of the FX lights. Returns false when the budget is spent. */
	bool TryClaimLight();
	void ReturnLight();

	/** Actor left the world (Destroy / level change): forget it. */
	void Forget(AActor* Actor);

	int32 NumLiveShots(bool bHostileSide) const { return (bHostileSide ? LiveHostile : LivePlayer).Num(); }
	int32 NumLitFX() const { return LitFX; }

private:
	void Prune();

	TArray<TWeakObjectPtr<AIronProjectile>> ParkedShots;
	TArray<TWeakObjectPtr<AIronProjectile>> LivePlayer;
	TArray<TWeakObjectPtr<AIronProjectile>> LiveHostile;
	TArray<TWeakObjectPtr<AArenaBlastFX>> ParkedBlasts;
	TArray<TWeakObjectPtr<AArenaBlastFX>> LiveBlasts;
	int32 LitFX = 0;
};
