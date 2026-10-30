// Data-binding types for UHelmetHUDWidget (the first-person helmet HUD).
// This header is authoritative; hud_spec.md section 15.3 is the design draft it was built from.
#pragma once

#include "CoreMinimal.h"
#include "SuitTypes.h"   // EAIPersona
#include "StoryTypes.h"  // EReactorStage
#include "HelmetHUDTypes.generated.h"

UENUM(BlueprintType)
enum class EHUDThreatType : uint8
{
	Drone,
	Tank,
	Boss,
	Projectile,
	Ally,
	Pickup,
	Beacon
};

UENUM(BlueprintType)
enum class EHUDLockState : uint8
{
	None,
	Acquire,
	Tracking,
	Locked,
	Lost
};

/** Body-diagram regions; the order matches the Region_* widgets and FHelmetHUDState::RegionIntegrity. */
UENUM(BlueprintType)
enum class EHUDBodyRegion : uint8
{
	Helmet,
	Torso,
	SpineMount,
	ArmL,
	ArmR,
	LegL,
	LegR,
	Count UMETA(Hidden)
};

/** A world target projected into HUD space. */
USTRUCT(BlueprintType)
struct IRONMANARENA_API FHUDTarget
{
	GENERATED_BODY()

	/** Stable per-target id (e.g. the actor's GetUniqueID()). Lock progress accumulates per id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	int32 TargetId = 0;

	/** Position in widget (slate) units, relative to the HUD's top-left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	FVector2D ScreenPos = FVector2D::ZeroVector;

	/** Bracket size in widget units (min 24). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	FVector2D ScreenSize = FVector2D(40.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	bool bOnScreen = true;

	/** Off-screen arrow direction, 0 = up, clockwise. Used when bOnScreen is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	float EdgeAngleDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	float DistanceM = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	float HeightDeltaM = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	EHUDThreatType Type = EHUDThreatType::Drone;

	/** Producer lock state. Leave as None to let the widget run its own lock timer (cone + range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	EHUDLockState Lock = EHUDLockState::None;

	/** 0..1. Only used when Lock != None. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	float LockProgress = 0.f;

	/** 1..6 missile lock number, 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	int32 LockIndex = 0;

	/** Angle from the view centre in degrees (for the widget's own lock cone test). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	float OffAxisDeg = 0.f;
};

/** A radar contact relative to the player. */
USTRUCT(BlueprintType)
struct IRONMANARENA_API FHUDRadarBlip
{
	GENERATED_BODY()

	/** Metres, heading-up: +Y is forward, +X is right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	FVector2D LocalXY = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	float HeightDeltaM = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	EHUDThreatType Type = EHUDThreatType::Drone;
};

/** Everything the helmet HUD shows in one frame. Fill it with UHelmetHUDWidget::FillStateFromGame or yourself. */
USTRUCT(BlueprintType)
struct IRONMANARENA_API FHelmetHUDState
{
	GENERATED_BODY()

	// ---- Mode -----------------------------------------------------------------------------------
	/** false hides the whole helmet HUD (third person, Hulkbuster cockpit, cinematic, menu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	bool bVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	bool bFlying = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	bool bBoosting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	bool bStoryMode = false;

	/** Show score/combo/wave in story mode too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	bool bShowScoreInStory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	bool bDead = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	EAIPersona Persona = EAIPersona::Jarvis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	FText SuitName;

	/** Suit AI voice envelope 0..1 (drives the glyph Speaking param). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Mode")
	float SpeakingEnvelope = 0.f;

	// ---- Motion / parallax ----------------------------------------------------------------------
	/** Yaw, pitch rate in deg/s. Leave at zero to let the widget derive it from HeadingDeg/PitchDeg deltas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Motion")
	FVector2D CameraAngularVelDeg = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Motion")
	FVector2D HeadBobPx = FVector2D::ZeroVector;

	/** Non-zero for one frame on a hit: screen-space direction the hit came from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Motion")
	FVector2D ImpactDir2D = FVector2D::ZeroVector;

	// ---- Flight ---------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float SpeedMs = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float AltitudeM = 0.f;

	/** Above ground level. Negative = unknown (box hidden). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float AglM = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float VerticalSpeedMs = 0.f;

	/** 0..360, 0 = north (+X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float HeadingDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float PitchDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	float RollDeg = 0.f;

	/** Story beacon bearings (deg) for heading-tape carets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Flight")
	TArray<float> BeaconHeadingsDeg;

	// ---- Reactor and energy ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	float PowerFraction = 1.f;

	/** Energy change per second as a fraction of max (negative = draining). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	float PowerRatePerS = 0.f;

	/** Heat 0..1 of the reactor's max heat (thresholds.heat values are on a 0..max scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	float HeatFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	bool bOverheated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	bool bVenting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	float VentCooldownLeft = 0.f;

	/** Unstable-core jitter 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	float Instability = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	bool bCoreDark = false;

	/** false = no story reactor (arena mode): the heat gauge is hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	bool bHasReactor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	EReactorStage ReactorStage = EReactorStage::Makeshift;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Reactor")
	FLinearColor CoreColor = FLinearColor::White;

	// ---- Integrity ------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Integrity")
	float HealthFraction = 1.f;

	/** Indexed by EHUDBodyRegion (7 entries). Missing entries fall back to HealthFraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Integrity")
	TArray<float> RegionIntegrity;

	/** EHUDBodyRegion index hit this frame, -1 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Integrity")
	int32 LastHitRegion = -1;

	// ---- Combat ---------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	FText WeaponName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	FText SpecialName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	float SpecialCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	bool bBeamActive = false;

	/** Timed effect (beam, cloak, ...) name and seconds left; empty name = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	FText ActiveEffectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	float ActiveEffectTimeLeft = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	bool bHulkbusterActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	float HulkbusterTimeLeft = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	int32 HulkbusterCharges = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	TArray<FHUDTarget> Targets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	TArray<FHUDRadarBlip> Blips;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	float RadarRangeM = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	bool bIncomingMissile = false;

	/** Raise the PullUp warning (e.g. flying, descending fast, low AGL). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Combat")
	bool bPullUp = false;

	// ---- Boss / objectives / score --------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	bool bBossActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	FText BossName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	float BossFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	int32 BossPhase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	bool bBossInvulnerable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	FText ObjectiveChapter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	FText ObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	FText ObjectiveProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	int32 Combo = 0;

	/** 0..1 combo window left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	float ComboTimeFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Game")
	int32 Wave = 0;

	// ---- Comms and subtitles --------------------------------------------------------------------
	/** Non-persona speaker on comms (e.g. "MORGAN"); NAME_None closes the portrait. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	FName CommsSpeaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	FText CommsSpeakerLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	float CommsEnvelope = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	float CommsSignalLoss = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	FText SubtitleSpeaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	FText SubtitleLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	FLinearColor SubtitleColor = FLinearColor::White;

	/** 0..1 producer fade (UJarvisComponent::GetSubtitle OutAlpha). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	float SubtitleAlpha = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|Comms")
	bool bSubtitlesEnabled = true;

	// ---- FX -------------------------------------------------------------------------------------
	/** EMP / ULTRON takeover glitch 0..1, max()'d with the health tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD|FX")
	float GlitchExternal = 0.f;
};
