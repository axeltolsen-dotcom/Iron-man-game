// FEnvironmentPreset: per-chapter lighting, sky, fog and camera "look" (post-process).
// AArenaEnvironment::ApplyPreset pushes one onto the sun / sky light / sky atmosphere /
// volumetric clouds / exponential height fog (volumetric) / unbound post-process volume.
// UEnvironmentPresets::ForChapter picks one from story.json's chapter location + id.
//
// Values are physical (Vison's lighting_presets.json conventions): the sun is 120000 lux at
// the top of the atmosphere at 5900 K (bAtmosphereSunLight: the SkyAtmosphere's transmittance does the
// dimming and reddening at low elevations), the moon 0.25 lux, exposure is histogram auto-exposure
// clamped to an EV100 range (r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True), white
// balance in Kelvin. Content/Story/lighting_presets.json (ContribLighting) overrides these at runtime;
// the C++ values are the fallback when that file is missing. They are tuned by reasoning, NOT by eye: the
// build box has no GPU. Tweak them on a real machine (Docs/REALISM.md).
#pragma once

#include "CoreMinimal.h"
#include "EnvironmentPreset.generated.h"

USTRUCT(BlueprintType)
struct IRONMANARENA_API FEnvironmentPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env") FName Id = TEXT("arena");

	// --- sun / sky ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") float SunPitch = -35.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") float SunYaw = 40.f;
	/** Directional light illuminance in lux, top of atmosphere (120000 = real sun; 0 = night). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") float SunLux = 120000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") float SunKelvin = 5900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") float SkyLightIntensity = 1.2f;
	/** Moon (second atmosphere light) in lux: 0.25 = full moon, 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") float MoonLux = 0.f;
	/** Use the imported HDRI cubemap for the sky light (indoor workshop) instead of real-time capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sun") bool bHdriSkyLight = false;

	// --- clouds / fog ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") bool bClouds = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") float CloudBottomKm = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") float CloudHeightKm = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") float FogDensity = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") float FogHeightFalloff = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.7f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") bool bVolumetricFog = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") float VolumetricFogExtinction = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Sky") FLinearColor VolumetricFogAlbedo = FLinearColor::White;

	// --- camera look (post-process) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float ExposureBias = 0.f;
	/** Histogram auto-exposure clamp, EV100. The Min clamp is what keeps nights dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float ExposureMinEV100 = 14.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float ExposureMaxEV100 = 17.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float BloomIntensity = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float LensFlareIntensity = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float WhiteBalanceKelvin = 6500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float Saturation = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float Contrast = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") FLinearColor Gain = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float Vignette = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float FilmGrain = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Look") float ChromaticAberration = 0.f;

	// --- surfaces / weather ---
	/** Wet streets (city at night, storm on the skyline). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Weather") bool bWet = false;
	/** Drifting embers / ash (ruins, battlefield). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Weather") bool bEmbers = false;
	/** Night: windows and street lights on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Env|Weather") bool bNight = false;
};

namespace EnvironmentPresets
{
	/** Free-play arena (no story). */
	IRONMANARENA_API FEnvironmentPreset Arena();

	/** Picks the set + look for a story chapter. SetId out: workshop / rooftops / city / ruins / battlefield. */
	IRONMANARENA_API FEnvironmentPreset ForChapter(FName Location, const FString& ChapterId, bool bHasBoss, FName& OutSetId);

	/** Prologue (Tony's workshop) and finale (the battlefield, dawn). */
	IRONMANARENA_API FEnvironmentPreset Prologue();
	IRONMANARENA_API FEnvironmentPreset Finale();
}
