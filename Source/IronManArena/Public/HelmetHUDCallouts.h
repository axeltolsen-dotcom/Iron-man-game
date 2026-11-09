// FHelmetHUDCallouts (FRIDAY): the helmet HUD's sound + voice layer.
//
// Owned by UHelmetHUDWidget (one per HUD). Every frame it watches the HUD state for edges
// (heat / power / integrity thresholds with hysteresis, venting, recovery, lock changes, hits,
// mission flow) and turns them into
//   - HUD SFX through ArenaAudio (existing SoundWaves in Content/Audio/SFX/HUD + UI, sfx_manifest.json),
//     with small pitch jitter so repeats never sound robotic, and ducked under voiced lines;
//   - suit-AI callouts through UJarvisComponent::Notify (new EAIEvent HUD events, lines in
//     Tools/data/hud_callouts.json -> Generated/AIVoiceLines.inl), and FRIDAY / MORGAN banter.
// Subtitles come from UJarvisComponent, so callouts read on screen with Settings > Voices off.
// HUD SFX are SC_UI and are never muted by the Voices switch.
//
// Anti-spam rules (on top of UJarvisComponent's per-event cooldowns and shared chatter budget):
//   any damage callout at most every 4 s; heat / power levels only re-arm after recovering
//   (hysteresis); banter waits 6 s after any damage / critical-heat / critical-power callout.
#pragma once

#include "CoreMinimal.h"
#include "HelmetHUDTypes.h"

class AIronManCharacter;
struct FHelmetHUDConfig;
enum class EAIEvent : uint8;

/** Lock changes seen by UHelmetHUDWidget::UpdateTargets this frame. */
struct FHUDLockFrame
{
	int32 NumLocked = 0;
	int32 NewLocks = 0;
	int32 AcquireStarts = 0;
	bool bNewBossLock = false;
	bool bBossLocked = false;
	/** A target that was locked is still alive but no longer locked (not killed). */
	bool bLostLock = false;
};

class IRONMANARENA_API FHelmetHUDCallouts
{
public:
	void Reset();

	/** Once per frame, after the state is known. bLive = gameplay is running (not menu / cinematic). */
	void Tick(const UObject* Ctx, const AIronManCharacter* Player, const FHelmetHUDState& State, const FHelmetHUDState& Prev,
		const FHelmetHUDConfig& Config, float Now, bool bLive);

	/** A new player hit. DamageFraction = damage / max health; Region = EHUDBodyRegion index or -1. */
	void OnPlayerHit(const UObject* Ctx, float DamageFraction, int32 Region, float Now);

	/** From UpdateTargets. */
	void OnLocks(const UObject* Ctx, const FHUDLockFrame& Locks, const FHelmetHUDState& State, const FHelmetHUDState& Prev, float Now);

	/** Slot-0 warning changed (only slot 0 plays audio, hud_spec section 9). */
	void OnTopWarning(const UObject* Ctx, FName WarningId, float Now);

	/** Visor boot sequence started. */
	void OnBoot(const UObject* Ctx, float Now);

	/** HUD SFX with pitch jitter, ducked under voiced lines and rate-limited per event. */
	void PlaySfx(const UObject* Ctx, FName Event, float Volume, float Pitch, float PitchJitter, float MinGapS, float Now);

	/** Context name used for DamageHeavy variants ("Torso", "SpineMount", "ArmL", "ArmR", "Legs", "Helmet"). */
	static FName RegionContext(int32 Region);

private:
	void Say(const UObject* Ctx, EAIEvent Event, FName Context, float Now, bool bCombat);
	void QueueBanter(FName Key, float Now, float Delay);
	void TickBanter(const UObject* Ctx, float Now);
	void TickMission(const UObject* Ctx, const AIronManCharacter* Player, const FHelmetHUDState& State, float Now);

	struct FPendingSfx { FName Event; float At = 0.f; float Volume = 1.f; float Pitch = 1.f; };
	struct FPendingSay { EAIEvent Event; FName Context; float At = 0.f; float Expire = 0.f; };

	TMap<FName, float> SfxLastPlayed;
	TArray<FPendingSfx> PendingSfx;
	TArray<FPendingSay> PendingSay;

	int32 HeatLevel = 0;          // 0 normal, 1 warning, 2 critical
	bool bHeatCriticalReactor = false;
	bool bWasVenting = false;
	int32 PowerLevel = 0;         // 0 ok, 1 warning (30%), 2 critical, 3 reserve (empty)
	bool bDamageCriticalSaid = false;
	bool bWasDead = false;

	float LastDamageCallout = -100.f;
	float LastCombatCallout = -100.f;
	float LastAnyCallout = -100.f;
	float LastBoot = -100.f;

	// Banter
	FName PendingBanter;
	float PendingBanterAt = 0.f;
	float PendingBanterExpire = 0.f;
	bool bDroneContactSeen = false;
	int32 LastHardLandings = -1;

	// Mission flow (story)
	int32 LastChapter = INDEX_NONE;
	int32 StartedChapter = INDEX_NONE;
	uint8 LastStoryStep = 0;
	uint8 LastArenaState = 0;
	bool bHaveMissionState = false;

	TWeakObjectPtr<const AIronManCharacter> LastPlayer;
};
