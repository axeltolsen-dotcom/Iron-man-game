#include "HelmetHUDCallouts.h"

#include "ArenaAudio.h"
#include "ArenaGameMode.h"
#include "HelmetHUDConfig.h"
#include "IronManCharacter.h"
#include "JarvisComponent.h"
#include "StoryDirector.h"
#include "SuitTypes.h"

#include "Engine/World.h"

namespace HUDCallouts
{
	// Existing SoundWaves (Content/Audio/sfx_manifest.json, all CC0 Kenney packs). Nothing new is imported.
	const FName S_WarnBeep(TEXT("hud_warning_beep"));
	const FName S_Overheat(TEXT("hud_overheat"));
	const FName S_OverheatAlarm(TEXT("hud_alarm_overheat"));
	const FName S_LowPower(TEXT("hud_low_power"));
	const FName S_LowEnergy(TEXT("hud_warning_low_energy"));
	const FName S_LowHealth(TEXT("hud_warning_low_health"));
	const FName S_TargetLock(TEXT("hud_target_lock"));
	const FName S_Select(TEXT("hud_beep_select"));
	const FName S_Confirm(TEXT("hud_beep_confirm"));
	const FName S_Vent(TEXT("reactor_overheat_vent"));
	const FName S_HitMetal(TEXT("hit_metal"));

	constexpr float PowerWarnAt = 0.30f;     // "thirty percent and falling" (LowEnergy chatter stays at 20%)
	constexpr float PowerReserveAt = 0.005f;
	constexpr float PowerRestoreAt = 0.60f;
	constexpr float DamageCriticalAt = 0.20f;  // under ArenaHUD's LowHealth (30%) so the two don't stack
	constexpr float DamageCriticalRearm = 0.50f;
	constexpr float HeavyHitFraction = 0.20f;  // = ArenaFeel hit_heavy (25 of 125 max)
	constexpr float DamageCalloutGap = 4.f;
	constexpr float BanterQuietAfterCombat = 6.f;
	constexpr float HomeworkQuietS = 55.f;

	bool IsNightKey(FName Key)
	{
		return !Key.IsNone() && Key.ToString().Contains(TEXT("night"), ESearchCase::IgnoreCase);
	}
}
using namespace HUDCallouts;

void FHelmetHUDCallouts::Reset()
{
	SfxLastPlayed.Reset();
	PendingSfx.Reset();
	PendingSay.Reset();
	HeatLevel = 0;
	bHeatCriticalReactor = false;
	bWasVenting = false;
	PowerLevel = 0;
	bDamageCriticalSaid = false;
	bWasDead = false;
	LastDamageCallout = LastCombatCallout = LastAnyCallout = -100.f;
	PendingBanter = NAME_None;
	bDroneContactSeen = false;
	LastHardLandings = -1;
	LastChapter = StartedChapter = INDEX_NONE;
	bHaveMissionState = false;
}

FName FHelmetHUDCallouts::RegionContext(int32 Region)
{
	switch (static_cast<EHUDBodyRegion>(Region))
	{
	case EHUDBodyRegion::Helmet:     return FName(TEXT("Helmet"));
	case EHUDBodyRegion::Torso:      return FName(TEXT("Torso"));
	case EHUDBodyRegion::SpineMount: return FName(TEXT("SpineMount"));
	case EHUDBodyRegion::ArmL:       return FName(TEXT("ArmL"));
	case EHUDBodyRegion::ArmR:       return FName(TEXT("ArmR"));
	case EHUDBodyRegion::LegL:
	case EHUDBodyRegion::LegR:       return FName(TEXT("Legs"));
	default:                         return NAME_None;
	}
}

void FHelmetHUDCallouts::PlaySfx(const UObject* Ctx, FName Event, float Volume, float Pitch, float PitchJitter, float MinGapS, float Now)
{
	if (!Ctx || Event.IsNone()) { return; }
	float& Last = SfxLastPlayed.FindOrAdd(Event, -100.f);
	if (Now - Last < MinGapS) { return; }
	Last = Now;
	// Duck HUD beeps ~4.5 dB under a voiced line so FRIDAY stays on top (SC_UI is not in the voice duck mix).
	const UJarvisComponent* J = UJarvisComponent::Get(Ctx);
	const float Duck = (J && J->IsVoicing()) ? 0.6f : 1.f;
	const float P = Pitch * (1.f + FMath::FRandRange(-PitchJitter, PitchJitter));
	ArenaAudio::Play2D(Ctx, Event, Volume * Duck, P);
}

void FHelmetHUDCallouts::Say(const UObject* Ctx, EAIEvent Event, FName Context, float Now, bool bCombat)
{
	UJarvisComponent::Notify(Ctx, Event, Context);
	LastAnyCallout = Now;
	if (bCombat) { LastCombatCallout = Now; }
}

void FHelmetHUDCallouts::QueueBanter(FName Key, float Now, float Delay)
{
	if (!PendingBanter.IsNone()) { return; } // one at a time; the first trigger wins
	PendingBanter = Key;
	PendingBanterAt = Now + Delay;
	PendingBanterExpire = Now + Delay + 12.f;
}

void FHelmetHUDCallouts::TickBanter(const UObject* Ctx, float Now)
{
	if (PendingBanter.IsNone()) { return; }
	if (Now > PendingBanterExpire) { PendingBanter = NAME_None; return; }
	if (Now < PendingBanterAt || Now - LastCombatCallout < BanterQuietAfterCombat) { return; }
	if (UJarvisComponent* J = UJarvisComponent::Get(Ctx))
	{
		if (J->SayBanter(PendingBanter)) { LastAnyCallout = Now; PendingBanter = NAME_None; }
	}
	else
	{
		PendingBanter = NAME_None;
	}
}

void FHelmetHUDCallouts::OnBoot(const UObject* Ctx, float Now)
{
	if (Now - LastBoot < 3.f) { return; }
	LastBoot = Now;
	// Two-note power-on: a soft low tick as the visor lights, the confirm chirp as the symbology lands.
	PlaySfx(Ctx, S_Select, 0.25f, 0.82f, 0.02f, 0.f, Now);
	FPendingSfx P;
	P.Event = S_Confirm;
	P.At = Now + 0.42f;
	P.Volume = 0.35f;
	P.Pitch = 0.94f;
	PendingSfx.Add(P);
}

void FHelmetHUDCallouts::OnTopWarning(const UObject* Ctx, FName WarningId, float Now)
{
	// hud_layout.json warnings[].audio -> existing SoundWaves.
	struct FMap { const TCHAR* Id; FName Sfx; float Vol; float Pitch; };
	static const FMap Map[] = {
		{ TEXT("SuitCritical"),  S_OverheatAlarm, 0.65f, 0.9f },  // klaxon
		{ TEXT("Overheat"),      S_OverheatAlarm, 0.7f,  1.f },   // klaxon
		{ TEXT("MissileLock"),   S_TargetLock,    0.6f,  0.72f }, // lock tone (low = threat, high = ours)
		{ TEXT("ArmorCritical"), S_LowHealth,     0.6f,  1.f },   // chime_hi
		{ TEXT("VentNow"),       S_Overheat,      0.55f, 1.f },   // chime_mid
		{ TEXT("CoreDark"),      S_LowPower,      0.6f,  0.85f }, // chime_hi
		{ TEXT("LowPower"),      S_LowEnergy,     0.5f,  1.f },   // chime_low
		{ TEXT("PullUp"),        S_WarnBeep,      0.55f, 1.25f }, // chime_mid
	};
	const FString Id = WarningId.ToString();
	for (const FMap& M : Map)
	{
		if (Id == M.Id) { PlaySfx(Ctx, M.Sfx, M.Vol, M.Pitch, 0.02f, 1.2f, Now); return; }
	}
}

void FHelmetHUDCallouts::OnPlayerHit(const UObject* Ctx, float DamageFraction, int32 Region, float Now)
{
	const float Sev = FMath::Clamp(DamageFraction / 0.25f, 0.f, 1.f);
	const bool bHeavy = DamageFraction >= HeavyHitFraction;
	// In-helmet thud: the shell rings low and dull from inside (2D, pitched down, scaled by damage).
	PlaySfx(Ctx, S_HitMetal, 0.22f + 0.5f * Sev, 0.74f, 0.05f, 0.07f, Now);
	if (bHeavy) { PlaySfx(Ctx, S_WarnBeep, 0.32f, 0.92f, 0.03f, 0.6f, Now); }

	if (Now - LastDamageCallout < DamageCalloutGap) { LastCombatCallout = Now; return; }
	LastDamageCallout = Now;
	if (bHeavy)
	{
		Say(Ctx, EAIEvent::DamageHeavy, RegionContext(Region), Now, true);
		QueueBanter(FName(TEXT("paint")), Now, 7.f);
	}
	else
	{
		Say(Ctx, EAIEvent::DamageLight, NAME_None, Now, true);
	}
}

void FHelmetHUDCallouts::OnLocks(const UObject* Ctx, const FHUDLockFrame& Locks, const FHelmetHUDState& State, const FHelmetHUDState& Prev, float Now)
{
	if (Locks.AcquireStarts > 0)
	{
		PlaySfx(Ctx, S_Select, 0.14f, 1.38f, 0.04f, 0.12f, Now); // faint seeker tick
	}
	if (Locks.NewLocks > 0)
	{
		PlaySfx(Ctx, S_Confirm, 0.34f, 1.22f, 0.03f, 0.15f, Now);
		if (Locks.bNewBossLock) { Say(Ctx, EAIEvent::LockBoss, NAME_None, Now, false); }
		else { Say(Ctx, EAIEvent::LockAcquired, NAME_None, Now, false); }
		if (Locks.NumLocked >= 3 && Locks.NumLocked - Locks.NewLocks < 3)
		{
			Say(Ctx, EAIEvent::LockMultiple, Locks.NumLocked >= 4 ? FName(TEXT("Four")) : NAME_None, Now, false);
		}
	}
	if (Locks.bLostLock && Locks.NumLocked == 0)
	{
		PlaySfx(Ctx, S_Select, 0.26f, 0.7f, 0.03f, 0.3f, Now); // falling "lost" blip
		Say(Ctx, EAIEvent::LockLost, NAME_None, Now, false);
	}
	// Boss vents (invulnerable -> open) while it is locked: the one moment worth shouting about.
	if (Locks.bBossLocked && State.bBossActive && Prev.bBossInvulnerable && !State.bBossInvulnerable)
	{
		PlaySfx(Ctx, S_TargetLock, 0.55f, 1.12f, 0.02f, 1.f, Now);
		Say(Ctx, EAIEvent::LockWeakpoint, NAME_None, Now, true);
	}
}

void FHelmetHUDCallouts::Tick(const UObject* Ctx, const AIronManCharacter* Player, const FHelmetHUDState& S, const FHelmetHUDState& Prev,
	const FHelmetHUDConfig& Config, float Now, bool bLive)
{
	if (LastPlayer.Get() != Player) { Reset(); LastPlayer = Player; }

	// Delayed SFX (boot second note) play even when the HUD is hidden.
	for (int32 i = PendingSfx.Num() - 1; i >= 0; --i)
	{
		if (Now >= PendingSfx[i].At)
		{
			const FPendingSfx P = PendingSfx[i];
			PendingSfx.RemoveAt(i);
			PlaySfx(Ctx, P.Event, P.Volume, P.Pitch, 0.01f, 0.f, Now);
		}
	}
	if (!Player || !bLive) { return; }

	// Respawn / revive: everything re-arms.
	if (S.bDead != bWasDead)
	{
		bWasDead = S.bDead;
		if (!S.bDead) { HeatLevel = 0; PowerLevel = 0; bDamageCriticalSaid = false; bWasVenting = false; }
	}
	TickMission(Ctx, Player, S, Now);
	if (S.bDead) { return; }

	// ---- Heat (HUD thresholds, hysteresis: a level re-arms only after recovering under HeatRecoverAt) ----
	if (S.bHasReactor)
	{
		const float HeatMax = FMath::Max(Config.HeatMax, 1.f);
		const float Heat = FMath::Clamp(S.HeatFraction, 0.f, 1.f) * HeatMax;
		const bool bCritical = S.bOverheated || Heat >= Config.HeatCritical;
		if (bCritical && HeatLevel < 2)
		{
			bHeatCriticalReactor = Player->GetSuit().Reactor == ESuitReactor::Mounted;
			HeatLevel = 2;
			Say(Ctx, EAIEvent::HeatCritical, bHeatCriticalReactor ? FName(TEXT("Reactor")) : NAME_None, Now, true);
		}
		else if (!bCritical && Heat >= Config.HeatWarning && HeatLevel < 1)
		{
			HeatLevel = 1;
			PlaySfx(Ctx, S_WarnBeep, 0.38f, 1.05f, 0.03f, 2.f, Now);
			Say(Ctx, EAIEvent::HeatWarning, S.bFlying ? FName(TEXT("Flight")) : NAME_None, Now, false);
		}
		if (S.bVenting && !bWasVenting)
		{
			PlaySfx(Ctx, S_Vent, 0.5f, 1.f, 0.04f, 1.f, Now);
			Say(Ctx, EAIEvent::HeatVenting, NAME_None, Now, false);
		}
		bWasVenting = S.bVenting;
		if (HeatLevel > 0 && !S.bOverheated && !S.bVenting && Heat < Config.HeatRecoverAt)
		{
			PlaySfx(Ctx, S_Select, 0.28f, 0.94f, 0.02f, 1.f, Now);
			FPendingSfx P; P.Event = S_Confirm; P.At = Now + 0.16f; P.Volume = 0.26f; P.Pitch = 1.06f; PendingSfx.Add(P); // rising "all clear"
			Say(Ctx, EAIEvent::HeatRecovered, NAME_None, Now, false);
			if (HeatLevel >= 2 && bHeatCriticalReactor) { QueueBanter(FName(TEXT("reactor")), Now, 4.f); }
			HeatLevel = 0;
			bHeatCriticalReactor = false;
		}
	}

	// ---- Power ----
	const float Power = FMath::Clamp(S.PowerFraction, 0.f, 1.f);
	int32 PowerNow = 0;
	if (Power <= PowerReserveAt) { PowerNow = 3; }
	else if (Power <= Config.PowerCritical) { PowerNow = 2; }
	else if (Power <= PowerWarnAt) { PowerNow = 1; }
	if (PowerNow > PowerLevel)
	{
		if (PowerNow == 3)
		{
			PlaySfx(Ctx, S_LowPower, 0.6f, 0.8f, 0.02f, 1.5f, Now);
			Say(Ctx, EAIEvent::PowerReserve, NAME_None, Now, true);
		}
		else if (PowerNow == 2)
		{
			PlaySfx(Ctx, S_LowPower, 0.55f, 1.f, 0.02f, 1.5f, Now);
			Say(Ctx, EAIEvent::PowerCritical, NAME_None, Now, true);
		}
		else
		{
			Say(Ctx, EAIEvent::PowerWarning, NAME_None, Now, false);
			QueueBanter(FName(TEXT("lowpower")), Now, 5.f);
		}
		PowerLevel = PowerNow;
	}
	else if (PowerLevel > 0 && Power >= PowerRestoreAt)
	{
		PlaySfx(Ctx, S_Confirm, 0.3f, 1.f, 0.03f, 1.f, Now);
		Say(Ctx, EAIEvent::PowerRestored, NAME_None, Now, false);
		PowerLevel = 0;
	}

	// ---- Integrity ----
	const float Health = FMath::Clamp(S.HealthFraction, 0.f, 1.f);
	if (!bDamageCriticalSaid && Health < DamageCriticalAt)
	{
		bDamageCriticalSaid = true;
		Say(Ctx, EAIEvent::DamageCritical, NAME_None, Now, true);
	}
	else if (bDamageCriticalSaid && Health > DamageCriticalRearm)
	{
		bDamageCriticalSaid = false;
	}

	// ---- Banter triggers (story + FRIDAY + Morgan on comms; UJarvisComponent::SayBanter checks that) ----
	if (S.bStoryMode)
	{
		if (!bDroneContactSeen)
		{
			for (const FHUDTarget& T : S.Targets)
			{
				if (T.Type == EHUDThreatType::Drone && T.bOnScreen && T.DistanceM < 80.f)
				{
					bDroneContactSeen = true;
					QueueBanter(FName(TEXT("heartrate")), Now, 2.5f);
					break;
				}
			}
		}
		const int32 Landings = Player->GetHardLandingCount();
		if (LastHardLandings >= 0 && Landings > LastHardLandings) { QueueBanter(FName(TEXT("landing")), Now, 1.2f); }
		LastHardLandings = Landings;

		const bool bThreatsOnScreen = S.Targets.ContainsByPredicate([](const FHUDTarget& T) { return T.bOnScreen && T.Type != EHUDThreatType::Ally && T.Type != EHUDThreatType::Pickup && T.Type != EHUDThreatType::Beacon; });
		if (!bThreatsOnScreen && Now - LastAnyCallout > HomeworkQuietS && Now - LastCombatCallout > HomeworkQuietS)
		{
			QueueBanter(FName(TEXT("homework")), Now, 0.f);
			LastAnyCallout = Now; // try once per quiet stretch
		}
	}
	TickBanter(Ctx, Now);
}

void FHelmetHUDCallouts::TickMission(const UObject* Ctx, const AIronManCharacter* Player, const FHelmetHUDState& S, float Now)
{
	const UWorld* World = Player->GetWorld();
	const AArenaGameMode* GM = World ? World->GetAuthGameMode<AArenaGameMode>() : nullptr;
	if (!GM) { return; }
	const AStoryDirector* Story = GM->GetStoryDirector();
	const uint8 ArenaState = static_cast<uint8>(GM->GetArenaState());
	const uint8 Step = Story ? static_cast<uint8>(Story->GetStep()) : 0;
	const int32 Chapter = Story ? Story->GetChapterIndex() : INDEX_NONE;

	if (bHaveMissionState)
	{
		// Fail: the run ends (after the Death line, which interrupts with priority 100).
		if (LastArenaState == static_cast<uint8>(EArenaGameState::Playing) && ArenaState == static_cast<uint8>(EArenaGameState::GameOver))
		{
			FPendingSay P; P.Event = EAIEvent::MissionFail; P.At = Now + 2.5f; P.Expire = Now + 10.f; PendingSay.Add(P);
		}
		if (Story)
		{
			const bool bChapterDone = (LastChapter != INDEX_NONE && Chapter > LastChapter)
				|| (Step == static_cast<uint8>(EStoryStep::Complete) && LastStoryStep != Step);
			if (bChapterDone)
			{
				Say(Ctx, EAIEvent::MissionWin, S.HealthFraction < 0.3f ? FName(TEXT("Close")) : NAME_None, Now, false);
			}
		}
	}
	// Start: the first objective of a chapter goes live (after its cutscene / suit-up).
	if (Story && Step == static_cast<uint8>(EStoryStep::Objective) && Chapter != StartedChapter && !Story->IsInCutscene())
	{
		StartedChapter = Chapter;
		const bool bNight = IsNightKey(Story->GetLightingKey()) || IsNightKey(Story->GetEnvironmentId());
		FPendingSay P; P.Event = EAIEvent::MissionStart; P.Context = bNight ? FName(TEXT("Night")) : NAME_None;
		P.At = Now + 1.2f; P.Expire = Now + 12.f; PendingSay.Add(P);
	}
	LastArenaState = ArenaState;
	LastStoryStep = Step;
	LastChapter = Chapter;
	bHaveMissionState = true;

	for (int32 i = PendingSay.Num() - 1; i >= 0; --i)
	{
		if (Now > PendingSay[i].Expire) { PendingSay.RemoveAt(i); continue; }
		if (Now >= PendingSay[i].At)
		{
			const FPendingSay P = PendingSay[i];
			PendingSay.RemoveAt(i);
			Say(Ctx, P.Event, P.Context, Now, false);
		}
	}
}
