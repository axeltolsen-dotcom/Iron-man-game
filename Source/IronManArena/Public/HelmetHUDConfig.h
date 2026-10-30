// FHelmetHUDConfig: helmet HUD layout data (persona palettes, thresholds, warnings, animation
// timings, element slots). This is plain C++ with no reflection.
//
// Sources, in order:
//   1. Compiled-in defaults: Private/Generated/HelmetHUDDefaults.inl, generated from
//      contrib/edith/hud_layout.json by contrib/edith/gen_hud_defaults.py.
//   2. Content/UI/hud_layout.json at runtime. Every field that is present and valid overrides
//      its default; a missing file or an invalid field keeps the default and logs a warning.
#pragma once

#include "CoreMinimal.h"

/** Easing names from hud_layout.json "animations" (the same names as EEasingFunc). */
enum class EHUDEase : uint8
{
	Linear,
	Step,
	SinusoidalIn,
	SinusoidalOut,
	SinusoidalInOut,
	EaseIn,
	EaseOut,
	EaseInOut,
	ExpoIn,
	ExpoOut,
	ExpoInOut,
	CircularIn,
	CircularOut,
	CircularInOut
};

struct IRONMANARENA_API FHUDPersonaPalette
{
	/** JSON key: JARVIS, FRIDAY, EDITH, ULTRON or WARMACHINE (the FAIPersonaDef::AudioFolder). */
	FString Key;
	FString Label;
	FLinearColor Primary = FLinearColor(0.1144f, 0.9047f, 1.f);
	FLinearColor Secondary = FLinearColor(0.0130f, 0.2747f, 1.f);
	FLinearColor Warning = FLinearColor(1.f, 0.5841f, 0.0685f);
	FLinearColor Critical = FLinearColor(1.f, 0.0437f, 0.0437f);
	FLinearColor Subtitle = FLinearColor::White;
	int32 GlyphIndex = 0;
	FString FontFamily;
	int32 LetterSpacingBias = 0;
};

struct IRONMANARENA_API FHUDAnimDef
{
	float DurationS = 0.f;
	float DelayS = 0.f;
	float LoopHz = 0.f;
	float Amplitude = 0.f;
	float FromScale = 1.f;
	float ToScale = 1.f;
	int32 Loops = 1;
	EHUDEase Ease = EHUDEase::Linear;
};

struct IRONMANARENA_API FHUDWarningDef
{
	FName Id;
	int32 Priority = 7;
	FString Text;
	FName ColorKey;
	FName Icon;
	float FlashHz = 0.f;
	float MinShowS = 1.5f;
};

struct IRONMANARENA_API FHUDGlitchTier
{
	int32 Tier = 0;
	float HealthBelow = 1.01f;
	float Glitch = 0.f;
};

struct IRONMANARENA_API FHUDFontDef
{
	FName Key;
	FString Family;
	/** Package path, e.g. /Game/UI/Fonts/F_Rajdhani (object = package name). */
	FString Asset;
};

struct IRONMANARENA_API FHUDElementDef
{
	FName Id;
	/** NAME_None for top-level elements. */
	FName Parent;
	/** C++ bind type: UWidget, UImage or UTextBlock. */
	FName BindAs;
	FVector2D AnchorMin = FVector2D::ZeroVector;
	FVector2D AnchorMax = FVector2D::ZeroVector;
	FVector2D Alignment = FVector2D::ZeroVector;
	/** For stretched anchors: left/top margin. Otherwise: position. */
	FVector2D Position = FVector2D::ZeroVector;
	/** For stretched anchors: right/bottom margin. Otherwise: size. */
	FVector2D Extent = FVector2D::ZeroVector;
	int32 ZOrder = 0;
	float Depth = 0.f;
	FName FontKey;
	FName Typeface;
	int32 FontSize = 0;
	int32 LetterSpacing = 0;
	FName Material;

	bool IsStretched() const { return AnchorMin != AnchorMax; }
};

struct IRONMANARENA_API FHelmetHUDConfig
{
	FVector2D ReferenceResolution = FVector2D(1920.f, 1080.f);

	/** Indexed by EAIPersona (Jarvis, Friday, Edith, Ultron, WarMachine). */
	TArray<FHUDPersonaPalette> Personas;

	TMap<FName, FLinearColor> Colors;
	/** Overrides for Colors when the colour-blind safe setting is on. */
	TMap<FName, FLinearColor> ColorBlind;
	TMap<FName, FHUDFontDef> Fonts;

	// thresholds.heat (heat units, MaxHeat = HeatMax)
	float HeatMax = 100.f;
	float HeatCaution = 60.f;
	float HeatWarning = 80.f;
	float HeatCritical = 95.f;
	float HeatOverheat = 100.f;
	float HeatRecoverAt = 35.f;
	float VentDurationS = 1.2f;
	float VentCooldownS = 6.f;
	// thresholds.power / integrity (fractions)
	float PowerLow = 0.2f;
	float PowerCritical = 0.1f;
	float IntegrityDamaged = 0.7f;
	float IntegrityCritical = 0.35f;
	float IntegrityOffline = 0.05f;
	TArray<FHUDGlitchTier> GlitchTiers;
	// thresholds.lock
	float LockConeDeg = 12.f;
	float LockAcquireRangeM = 180.f;
	float LockTimeRepulsorS = 0.6f;
	float LockTimeMissileS = 1.2f;
	float LockLoseGraceS = 0.25f;
	int32 MaxBrackets = 24;
	int32 MaxLocks = 6;
	int32 MaxOffscreen = 8;
	float OffscreenEllipseInset = 0.06f;
	// thresholds.radar
	float RadarRangeGroundM = 120.f;
	float RadarRangeFlightM = 300.f;
	float RadarRangeBossM = 500.f;
	float RadarElevationMarkerM = 10.f;
	float RadarSweepPeriodS = 1.5f;
	// thresholds.tapes
	float SpeedWindowHalf = 50.f;
	float SpeedMinor = 2.f;
	float SpeedMajor = 10.f;
	float SpeedPxPerUnit = 3.6f;
	float MachFrom = 0.3f;
	float AltWindowHalf = 100.f;
	float AltMinor = 5.f;
	float AltMajor = 20.f;
	float AltPxPerUnit = 1.8f;
	float AglBoxBelowM = 60.f;
	float HeadingWindowHalf = 45.f;
	float HeadingMinor = 5.f;
	float HeadingMajor = 10.f;
	float HeadingPxPerUnit = 6.4f;
	float LadderStepDeg = 10.f;
	float LadderPxPerDeg = 8.f;
	float LadderVisibleDeg = 25.f;
	float FlashMaxHz = 3.f;

	TArray<FHUDWarningDef> Warnings;
	int32 WarningMaxVisible = 3;
	float WarningRefreshExtendsS = 1.f;
	float WarningDemoteSlideMs = 160.f;
	float WarningLowerDim = 0.7f;

	// parallax
	FVector2D ParallaxMaxOffsetPx = FVector2D(28.f, 18.f);
	float ParallaxPxPerDegPerSec = 0.12f;
	float LagHalfLifeNearMs = 55.f;
	float LagHalfLifeMidMs = 85.f;
	float LagHalfLifeFarMs = 120.f;
	float ImpactKickPx = 10.f;
	float ImpactRecoverMs = 220.f;

	/** visorWarp.params: retainer effect material scalars (BarrelK1, ChromaticEdgePx, ...). */
	TMap<FName, float> VisorWarp;
	TMap<FName, FHUDAnimDef> Anims;
	/** animations.bootSequence.stagger in seconds, indexed L0_Visor, L1_Near, L2_Mid, L3_Far. */
	TArray<float> BootStaggerS;
	TArray<FHUDElementDef> Elements;

	/** Accessibility defaults. */
	float SubtitleBackgroundOpacity = 0.55f;

	bool bLoadedFromFile = false;
	FString SourcePath;

	/** Compiled-in defaults (they match hud_layout.json). */
	static FHelmetHUDConfig MakeDefaults();

	/** Defaults overlaid with the JSON at Path. Issues are logged to LogIronManArena. */
	static FHelmetHUDConfig Load(const FString& Path);

	/** Content/UI/hud_layout.json. */
	static FString DefaultPath();

	FLinearColor GetColor(FName Key, bool bColorBlindSafe, const FLinearColor& Fallback = FLinearColor::White) const;
	const FHUDAnimDef& GetAnim(FName Key) const;
	const FHUDElementDef* FindElement(FName Id) const;
	const FHUDWarningDef* FindWarning(FName Id) const;
	const FHUDPersonaPalette& GetPersona(int32 PersonaIndex) const;
	/** Glitch value for a health fraction (thresholds.glitchTiers). */
	float GlitchForHealth(float HealthFrac, int32* OutTier = nullptr) const;
	/** Layer index (0 = L0_Visor/L4_FX, 1 = L1_Near, 2 = L2_Mid, 3 = L3_Far) from the layers zRange table. */
	static int32 LayerForZ(int32 ZOrder);
};

/** Evaluates an easing curve at T in [0,1]. */
IRONMANARENA_API float HelmetHUDEase(EHUDEase Ease, float T);
/** Parses an EEasingFunc-style name; Linear when unknown. */
IRONMANARENA_API EHUDEase HelmetHUDParseEase(const FString& Name);
