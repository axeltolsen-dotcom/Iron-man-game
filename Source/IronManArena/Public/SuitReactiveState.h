// Jocasta's "suit reactive surface state" (contrib/jocasta/suit_reactive_realism.md, data in
// Content/Story/suit_reactive_state.json): the player suit's surface changes during play.
//
//  * Wetness     builds in rain (not under cover), in water, in low fast flight over wet streets;
//                dries at decayPerSec (faster when the reactor is hot).
//  * GrimeLive   hard landings, ground objectives, near explosions. Rain washes it down to 50%.
//                Persists per suit (save) until the workshop.
//  * ScorchLive  repulsor shots, near explosions, tank shell hits. Persists until repair.
//  * HeatTint    reactor heat with a 1.5 s lag (temper colours on bare metal, faint glow above 0.85).
//  * DamageLevel health lost, in tiers 0 / 0.33 / 0.66 / 1. Persists until repair.
//
// USuitReactiveComponent (on AIronManCharacter) ticks at updateHz (10 Hz) and pushes the values onto the
// suit's existing MIDs. M_SuitMaster already has Wetness / GrimeLive / ScorchLive / BattleDamage / HeatGlow /
// EdgeWear; materials without them (M_CarPaint, M_RobotMetal, the basic-shape fallback) get the same look
// approximated through Color / Roughness / EdgeWear / Scorch / EmissiveStrength.
// Repair: UCareerSubsystem armory pad (mansion workshop) and Dum-E's tool tray in the workshop nook.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaVisuals.h"
#include "ArenaSaveGame.h"
#include "SuitReactiveState.generated.h"

class UMaterialInstanceDynamic;
class AIronManCharacter;

namespace SuitReactive
{
	/** suit_reactive_state.json, loaded once (built-in defaults = Jocasta's values), clamped. */
	struct FTuning
	{
		bool bLoadedFromFile = false;
		float UpdateHz = 10.f;
		// Wetness
		float RainPerSec = 0.25f, WaterContact = 1.f, LowSprayPerSec = 0.15f;
		float WetDecayPerSec = 0.04f, WetDecayPerSecHot = 0.12f, WetHotAbove = 0.5f;
		float WetRoughnessMul = 0.6f, WetAlbedoDarken = 0.15f;
		// GrimeLive
		float GrimeHardLanding = 0.05f, GrimeGroundPerMin = 0.08f, GrimeNearExplosion = 0.06f;
		float GrimeUpFacingMul = 2.f, GrimeRainWashTo = 0.5f, GrimeRainWashPerSec = 0.02f;
		float GrimeMaxDarken = 0.3f, GrimeMaxRoughnessAdd = 0.25f;
		// ScorchLive
		float ScorchRepulsorShot = 0.002f, ScorchNearExplosion = 0.08f, ScorchTankShell = 0.12f;
		// HeatTint
		float HeatLagSec = 1.5f, HeatMetallicThreshold = 0.8f, HeatEmissiveAbove = 0.85f, HeatMaxEmissive = 2.f;
		// DamageLevel: Tiers[i] once the health lost reaches TierHealthLost[i - 1].
		float Tiers[4] = { 0.f, 0.33f, 0.66f, 1.f };
		float TierHealthLost[3] = { 0.25f, 0.5f, 0.75f };
		float EdgeWearAddMax = 0.3f;
		// Landing tiers (m/s): grime from "hard" and "superhero" landings (>= SoftBelowMps).
		float SoftBelowMps = 6.f, HardBelowMps = 15.f;
		// Near explosion: full effect within Radius + FullWithinCm, fades to 0 over FadeCm + FadeRadiusMul * Radius.
		float NearFullWithinCm = 300.f, NearFadeCm = 600.f, NearFadeRadiusMul = 2.f;
		// allyChassis.jocasta: satin ceramic-composite shell (not paint) + pale-blue seams.
		FLinearColor AllyBaseColor = FLinearColor(0.62f, 0.66f, 0.70f);
		float AllyMetallic = 0.1f, AllyRoughness = 0.38f, AllyClearCoat = 0.f, AllyFlake = 0.f;
		FLinearColor AllySeamColor = FLinearColor(0.55f, 0.78f, 1.f);
		float AllySeamIntensity = 6.f;
	};

	IRONMANARENA_API const FTuning& Get();

	/** Live values (all 0..1). GrimeUnwashed = grime before the current rain started washing it. */
	struct FState
	{
		float Wetness = 0.f;
		float GrimeLive = 0.f;
		float ScorchLive = 0.f;
		float HeatTint = 0.f;
		float DamageLevel = 0.f;
		float GrimeUnwashed = 0.f;
	};

	/** What the suit is exposed to this step. */
	struct FInputs
	{
		bool bRain = false;          // raining and not under cover
		bool bWaterContact = false;  // standing / swimming in water
		bool bLowSpray = false;      // low fast flight over wet ground
		bool bGroundObjective = false;
		float ReactorHeat = 0.f;     // 0..1
		float HealthFrac = 1.f;      // < 0 = do not update DamageLevel (suit off / not ticking health)
	};

	IRONMANARENA_API void Step(FState& S, const FInputs& In, float DeltaSeconds, const FTuning& T);
	IRONMANARENA_API float DamageTierFor(float HealthLost, const FTuning& T);
	IRONMANARENA_API void AddGrime(FState& S, float Amount);
	IRONMANARENA_API void AddScorch(FState& S, float Amount);
	/** 0..1 strength of an explosion of BlastRadius at Distance (cm). */
	IRONMANARENA_API float NearExplosionFactor(float Distance, float BlastRadius, const FTuning& T);
	/** Workshop: grime, scorch, damage and wetness to 0 (heat tint keeps following the reactor). */
	IRONMANARENA_API void Repair(FState& S);
	IRONMANARENA_API bool NeedsRepair(const FState& S);
}

UCLASS(ClassGroup = (IronMan), meta = (BlueprintSpawnableComponent))
class IRONMANARENA_API USuitReactiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuitReactiveComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Landed with DownSpeed (cm/s): grime from hard / superhero landings. */
	void NotifyLanding(float DownSpeedCmS);
	void NotifyRepulsorShot();
	void NotifyTankShellHit();
	void NotifyExplosion(const FVector& Epicenter, float BlastRadius);

	/** Any explosion in the world: grime + scorch on every player suit near it. */
	static void ReportExplosion(const UObject* WorldContext, const FVector& Epicenter, float BlastRadius);

	/** Workshop repair + clean. Returns false if the suit was already clean. Saves. */
	bool RepairAndClean();
	bool NeedsRepair() const { return SuitReactive::NeedsRepair(State); }

	const SuitReactive::FState& GetState() const { return State; }
	FName GetTrackedSuit() const { return TrackedSuit; }

	UFUNCTION(BlueprintPure, Category = "IronMan|Suit") float GetWetness() const { return State.Wetness; }
	UFUNCTION(BlueprintPure, Category = "IronMan|Suit") float GetGrime() const { return State.GrimeLive; }
	UFUNCTION(BlueprintPure, Category = "IronMan|Suit") float GetScorch() const { return State.ScorchLive; }
	UFUNCTION(BlueprintPure, Category = "IronMan|Suit") float GetDamageLevel() const { return State.DamageLevel; }

private:
	struct FPartBase
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		float EdgeWear = 0.f, Roughness = 0.5f, Metallic = 0.f, Scorch = 0.f;
		float GrimeMul = 1.f, ScorchMul = 1.f;
		bool bMaster = false;      // parent has GrimeLive (M_SuitMaster)
		bool bHasEdgeWear = false, bHasRoughness = false, bHasScorch = false, bHasHeatGlow = false, bHasBattleDamage = false;
		bool bHasHeatTint = false, bHasDamageLevel = false;
		bool bSubstrate = false;   // tier 3 exposes substrate on this panel
		bool bHeatGlowing = false; // fallback heat emissive currently applied
	};

	AIronManCharacter* GetSuitOwner() const;
	void GatherInputs(SuitReactive::FInputs& In);
	void SwitchSuit(FName NewSuit);
	void PushToMaterials(bool bForce);
	void PushParts(TArray<FArenaPaintedPart>& PartList, TArray<FPartBase>& BaseList, bool bRebase, bool bPaintLocked);
	void StashCurrent();
	void LoadSaved();
	void WriteSave();
	bool CanWriteSave() const;

	SuitReactive::FState State;
	SuitReactive::FState LastPushed;
	FName TrackedSuit;
	TMap<FName, FSuitReactiveSave> Wear;   // every suit seen this session (from the save + live)
	TArray<FPartBase> BodyBases, FPBases;
	uint32 PaintGeneration = MAX_uint32;
	float SaveTimer = 0.f;
	bool bSaveDirty = false;
	bool bLoaded = false;
	bool bWasPaintLocked = false;
	float ShelterTimer = 0.f;
	bool bSheltered = false;
	float SprayTimer = 0.f;
	bool bNearWetGround = false;
	double LastTickTime = -1.0;
};
