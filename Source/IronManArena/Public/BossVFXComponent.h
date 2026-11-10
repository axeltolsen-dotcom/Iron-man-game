// UBossVFXComponent: Niagara layer for the Obsidian Titan (merged from contrib/ultron/BossVFXComponent.*).
//
// Every system is a soft reference under /Game/FX/Boss/ (+ /Game/FX/World/ explosions). A system whose
// package does not exist is skipped silently, so until the Niagara assets are authored this component is
// a no-op and the existing AArenaBlastFX look stays as the fallback (ABossTitan keeps its BlastFX calls).
//
// Not merged from the proposal (see PROGRESS.md):
//   - Chaos fields / geometry-collection arena fracture (no GCs exist yet, and the proposal's field calls
//     did not match the 5.8 UFieldSystemComponent signatures).
//   - The "boss budget" console command (it forced fx.Niagara.QualityLevel 2 over the player's scalability).
//   - UArenaDebrisBudget (no physics debris producers in Source yet).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossVFXComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UCLASS(ClassGroup = (FX), meta = (BlueprintSpawnableComponent))
class IRONMANARENA_API UBossVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossVFXComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** ABossTitan::EnterPhase, after the gameplay work. Phase 2 shears the shoulders, phase 3 the chest plate. */
	void PlayPhaseBreak(int32 NewPhase);
	/** ABossTitan::FireVolley, once per cannon. */
	void PlayVolleyMuzzle(USceneComponent* MuzzleAttach);
	/** A hit absorbed while the boss is invulnerable (rate-limited). */
	void PlayShieldHit(const FVector& HitLocation, const FVector& HitNormal);
	/** ABossTitan::OpenVent. */
	void PlayVent(float DurationSeconds);
	/** State transitions (EBossState as uint8: Entering 0, Fighting 1, Charging 2, Dashing 3, Venting 4). */
	void SetCombatState(uint8 BossStateValue, int32 Phase);
	/** ABossTitan::OnKilled. Uses world timers so the cascade outlives the destroyed boss actor.
	 *  SlowMoSeconds < 0 uses DeathSlowMoSeconds (for when a death slow-mo lands). */
	void PlayDeathCascade(float SlowMoSeconds);

	/** True if at least one Niagara system resolved (i.e. the boss FX content exists). */
	bool HasAnySystem() const { return NumLoaded > 0; }
	/** Per-cue: the Niagara version exists, so ABossTitan can drop its BlastFX fallback for that cue. */
	bool HasPhaseFX() const { return LoadedPhaseBlast != nullptr; }
	bool HasMuzzleFX() const { return LoadedVolleyMuzzle != nullptr; }
	bool HasVentFX() const { return LoadedVentSteam != nullptr; }
	bool HasDeathFX() const { return LoadedExplosionLarge != nullptr || LoadedExplosionSmall != nullptr; }

protected:
	void SoftLoadSystems();
	UNiagaraComponent* SpawnAttachedFX(UNiagaraSystem* System, USceneComponent* AttachParent);
	USceneComponent* FindHostPart(FName PartName) const;
	void ClearTransient();

	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_PhaseBlast;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_VolleyMuzzle;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_VentSteam;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_CoreVent;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_ChargeTelegraph;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_RamHeat;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_ShieldHit;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_PlateShear;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_HoverJet;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_ExplosionLarge;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_ExplosionSmall;
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Assets") TSoftObjectPtr<UNiagaraSystem> NS_ArenaShock;

	/** contrib/jocasta combat_realism boss.death.slowMoSec. */
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Timing") float DeathSlowMoSeconds = 0.5f;
	/** Minimum gap between shield-hit sparks (seconds). */
	UPROPERTY(EditAnywhere, Category = "Boss|VFX|Timing") float ShieldHitInterval = 0.08f;

	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedPhaseBlast;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedVolleyMuzzle;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedVentSteam;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedCoreVent;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedCharge;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedRamHeat;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedShieldHit;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedPlateShear;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedHoverJet;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedExplosionLarge;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedExplosionSmall;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedArenaShock;

	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> HoverJetL;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> HoverJetR;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> CoreVentFX;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> RamHeatFX;

	int32 NumLoaded = 0;
	int32 CachedPhase = 1;
	double LastShieldHitTime = -1.0;
	FTimerHandle VentEndTimer;
};
