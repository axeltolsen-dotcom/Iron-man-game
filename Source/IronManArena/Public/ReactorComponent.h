// UReactorComponent: the suit's power plant. Energy is the reactor's OUTPUT (regen / max energy
// multipliers); heat is its limit.
//
//  * Heat rises with beam use, specials, sustained fire and flight boost, and bleeds off after a
//    short delay (faster on the ground). At MaxHeat the reactor OVERHEATS: no weapons, no boost,
//    no energy regen until it cools below OverheatRecoverAt.
//  * Vent (X): dumps heat quickly for VentDuration seconds (no firing meanwhile), then a cooldown.
//  * A makeshift MOUNTED unit (Harley's scavenged reactor, ESuitReactor::Mounted) runs hotter.
//  * Story upgrades (Output / Cooling / Capacity, levels 0..MaxUpgradeLevel) improve it.
//  * Story stages (story.json chapter.reactor.stage): makeshift > unstable > failed (ch5) >
//    upgraded (ch7+). ApplyStage sets the heat/regen multipliers, upgrade levels and how much
//    the core flickers; the mount (spine / forearm) picks which housing the suit rig shows.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StoryTypes.h"
#include "ReactorComponent.generated.h"

UENUM(BlueprintType)
enum class EReactorUpgrade : uint8
{
	Output,    // more energy regen + max energy
	Cooling,   // faster cooling + vents
	Capacity   // higher heat ceiling
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReactorOverheatChanged, bool, bOverheated);

UCLASS(ClassGroup = (IronMan), meta = (BlueprintSpawnableComponent))
class IRONMANARENA_API UReactorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReactorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Adds heat (scaled by the mounted-unit multiplier). Ignored while venting. */
	UFUNCTION(BlueprintCallable, Category = "Reactor")
	void AddHeat(float Amount);

	/** Starts a vent if available. Returns true if venting started. */
	UFUNCTION(BlueprintCallable, Category = "Reactor")
	bool Vent();

	UFUNCTION(BlueprintCallable, Category = "Reactor")
	void ResetReactor();

	/** Story progression. Level is clamped to 0..MaxUpgradeLevel. */
	UFUNCTION(BlueprintCallable, Category = "Reactor")
	void SetUpgradeLevel(EReactorUpgrade Upgrade, int32 Level);

	UFUNCTION(BlueprintPure, Category = "Reactor")
	int32 GetUpgradeLevel(EReactorUpgrade Upgrade) const;

	/** Makeshift mounted reactor (runs hotter, see MountedHeatMultiplier). */
	void SetMountedUnit(bool bMounted) { bMountedUnit = bMounted; }

	UFUNCTION(BlueprintPure, Category = "Reactor")
	bool IsMountedUnit() const { return bMountedUnit; }

	UFUNCTION(BlueprintPure, Category = "Reactor")
	float GetHeat() const { return Heat; }

	UFUNCTION(BlueprintPure, Category = "Reactor")
	float GetMaxHeat() const;

	UFUNCTION(BlueprintPure, Category = "Reactor")
	float GetHeatFraction() const { return GetMaxHeat() > 0.f ? Heat / GetMaxHeat() : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Reactor")
	bool IsOverheated() const { return bOverheated; }

	UFUNCTION(BlueprintPure, Category = "Reactor")
	bool IsVenting() const { return VentTimeLeft > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Reactor")
	float GetVentCooldownLeft() const { return VentCooldownLeft; }

	/** Weapons / boost allowed? */
	UFUNCTION(BlueprintPure, Category = "Reactor")
	bool CanDrawPower() const { return !bOverheated && !IsVenting(); }

	/** Multiplier for ground energy regen (0 while overheated). */
	UFUNCTION(BlueprintPure, Category = "Reactor")
	float GetRegenMultiplier() const;

	/** Multiplier for max energy (Output upgrade). */
	UFUNCTION(BlueprintPure, Category = "Reactor")
	float GetCapacityMultiplier() const;

	/** Core glow colour for visuals / HUD: cool blue -> orange -> red. */
	UFUNCTION(BlueprintPure, Category = "Reactor")
	FLinearColor GetCoreColor(const FLinearColor& CoolColor) const;

	UPROPERTY(BlueprintAssignable, Category = "Reactor")
	FOnReactorOverheatChanged OnOverheatChanged;

	// ------------------------------------------------------------------ story stage
	/** Applies a story stage (also marks the unit as mounted). */
	UFUNCTION(BlueprintCallable, Category = "Reactor|Story")
	void ApplyStage(EReactorStage NewStage, EReactorMount NewMount);

	/** Back to arena behaviour (no stage modifiers, upgrades 0). */
	UFUNCTION(BlueprintCallable, Category = "Reactor|Story")
	void ClearStage();

	UFUNCTION(BlueprintPure, Category = "Reactor|Story")
	bool HasStage() const { return bHasStage; }

	UFUNCTION(BlueprintPure, Category = "Reactor|Story")
	EReactorStage GetStage() const { return Stage; }

	UFUNCTION(BlueprintPure, Category = "Reactor|Story")
	EReactorMount GetMount() const { return Mount; }

	/** 0..1 how unsteady the core glow is (stage + heat). */
	UFUNCTION(BlueprintPure, Category = "Reactor|Story")
	float GetInstability() const;

	/** Brightness multiplier for the core this frame (flicker / brownouts). */
	float SampleCoreBrightness(float TimeSeconds) const;

	/** Force the core dark (story: the reactor fails at the end of chapter 5). */
	void SetCoreDark(bool bDark) { bCoreDark = bDark; }
	bool IsCoreDark() const { return bCoreDark; }

	/** Heat multipliers per stage (on top of MountedHeatMultiplier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Story")
	float MakeshiftHeatMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Story")
	float UnstableHeatMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Story")
	float FailedHeatMultiplier = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Story")
	float UpgradedHeatMultiplier = 0.6f;

	/** Energy regen multiplier while the reactor is failing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Story")
	float FailedRegenMultiplier = 0.7f;

	/** Upgrade levels granted by the upgraded stage (Output, Cooling, Capacity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Story")
	FIntVector UpgradedLevels = FIntVector(2, 2, 1);

	// ------------------------------------------------------------------ tunables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Heat")
	float MaxHeat = 100.f;

	/** Heat per second of chest beam / laser whip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Heat")
	float HeatPerBeamSecond = 38.f;

	/** Heat per second of continuous primary fire (scaled by each weapon's fire interval). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Heat")
	float HeatPerFiringSecond = 7.f;

	/** Heat per special activation (missiles, shield, cloak ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Heat")
	float HeatPerSpecial = 14.f;

	/** Heat per second of flight boost (Shift). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Heat")
	float HeatPerBoostSecond = 20.f;

	/** Makeshift mounted reactor heat multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Heat")
	float MountedHeatMultiplier = 1.35f;

	/** Passive cooling per second once CoolingDelay has passed since the last heat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Cooling")
	float PassiveCooling = 14.f;

	/** Seconds after heating before passive cooling starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Cooling")
	float CoolingDelay = 0.8f;

	/** Cooling multiplier while standing on the ground (set by the owner each tick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Cooling")
	float GroundCoolingMultiplier = 1.6f;

	/** After an overheat, power returns below this heat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Cooling")
	float OverheatRecoverAt = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Vent")
	float VentDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Vent")
	float VentCoolingPerSecond = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Vent")
	float VentCooldown = 6.f;

	/** Per-level bonuses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Upgrades")
	int32 MaxUpgradeLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Upgrades")
	float OutputPerLevel = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Upgrades")
	float CoolingPerLevel = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor|Upgrades")
	float CapacityPerLevel = 25.f;

	/** Set by the owner every tick (ground = better cooling). */
	bool bOnGround = false;

private:
	void SetOverheated(bool bNew);
	float CoolingMultiplier() const { return 1.f + CoolingPerLevel * CoolingLevel; }

	UPROPERTY(VisibleInstanceOnly, Category = "Reactor|State")
	float Heat = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Reactor|State")
	bool bOverheated = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Reactor|State")
	int32 OutputLevel = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Reactor|State")
	int32 CoolingLevel = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Reactor|State")
	int32 CapacityLevel = 0;

	bool bMountedUnit = false;
	bool bHasStage = false;
	bool bCoreDark = false;
	EReactorStage Stage = EReactorStage::Makeshift;
	EReactorMount Mount = EReactorMount::Spine;
	float StageHeatMultiplier = 1.f;
	float StageRegenMultiplier = 1.f;
	float VentTimeLeft = 0.f;
	float VentCooldownLeft = 0.f;
	float TimeSinceHeat = 0.f;
};
