#include "EnvironmentPreset.h"

namespace EnvironmentPresets
{
	/** Physical light levels + EV100 exposure range (values from contrib/vison/lighting_presets.json). */
	static void Physical(FEnvironmentPreset& P, bool bSun, float MinEV, float MaxEV, float Bias)
	{
		P.SunLux = bSun ? 120000.f : 0.f;
		P.SunKelvin = 5900.f;               // the atmosphere reddens a low sun; don't double-tint
		P.MoonLux = bSun ? 0.f : 0.25f;
		P.ExposureMinEV100 = MinEV;
		P.ExposureMaxEV100 = MaxEV;
		P.ExposureBias = Bias;
	}

	FEnvironmentPreset Arena()
	{
		// Free play: Vison's "city" daylight (clear, 13:00).
		FEnvironmentPreset P;
		Physical(P, true, 14.05f, 17.05f, 0.f);
		return P;
	}

	static FEnvironmentPreset Workshop(bool bNightVariant)
	{
		// Tony's lake-house garage: warm interior practicals, cool daylight through the windows.
		FEnvironmentPreset P;
		P.Id = bNightVariant ? TEXT("workshop_night") : TEXT("workshop");
		P.SunPitch = bNightVariant ? -8.f : -28.f;
		P.SunYaw = 200.f;
		P.SunLux = bNightVariant ? 0.6f : 8.f;
		P.SunKelvin = bNightVariant ? 7800.f : 5600.f;
		P.MoonLux = bNightVariant ? 0.4f : 0.f;
		P.SkyLightIntensity = bNightVariant ? 0.35f : 1.f;
		P.bHdriSkyLight = true;
		P.FogDensity = 0.006f;
		P.FogHeightFalloff = 0.5f;
		P.FogColor = FLinearColor(0.35f, 0.45f, 0.55f);
		P.VolumetricFogExtinction = 0.6f;          // dust in the light shafts
		P.VolumetricFogAlbedo = FLinearColor(1.f, 0.95f, 0.85f);
		P.ExposureBias = bNightVariant ? 0.5f : 0.f;
		P.BloomIntensity = 0.8f;
		P.WhiteBalanceKelvin = 6000.f;             // slightly warm
		P.Contrast = 1.05f;
		P.Vignette = 0.4f;
		P.FilmGrain = 0.08f;
		P.bNight = bNightVariant;
		if (bNightVariant) { Physical(P, false, 4.01f, 5.91f, -0.3f); } else { Physical(P, true, 6.41f, 9.41f, 0.f); }
		return P;
	}

	static FEnvironmentPreset Rooftops(bool bStorm)
	{
		FEnvironmentPreset P;
		P.Id = bStorm ? TEXT("rooftops_storm") : TEXT("rooftops_dusk");
		if (bStorm)
		{
			// Siege of the Skyline: night storm, wet concrete, city glow under the clouds.
			P.SunPitch = -12.f; P.SunYaw = 300.f; P.SunLux = 0.5f; P.SunKelvin = 8000.f;
			P.MoonLux = 0.3f; P.SkyLightIntensity = 0.4f;
			P.CloudBottomKm = 0.9f; P.CloudHeightKm = 5.f;
			P.FogDensity = 0.03f; P.FogHeightFalloff = 0.08f; P.FogColor = FLinearColor(0.2f, 0.24f, 0.32f);
			P.VolumetricFogExtinction = 1.6f;
			P.ExposureBias = 1.f; P.BloomIntensity = 1.f; P.LensFlareIntensity = 0.8f;
			P.WhiteBalanceKelvin = 7200.f; P.Saturation = 0.85f; P.Contrast = 1.1f;
			P.Gain = FLinearColor(0.9f, 0.95f, 1.08f); P.Vignette = 0.5f; P.FilmGrain = 0.12f;
			P.ChromaticAberration = 0.15f;
			P.bWet = true; P.bNight = true;
			Physical(P, false, 2.58f, 4.78f, 0.f);
		}
		else
		{
			// First Flight: golden hour over the skyline, long shadows, warm haze.
			P.SunPitch = -9.f; P.SunYaw = 255.f; P.SunLux = 6.f; P.SunKelvin = 3600.f;
			P.SkyLightIntensity = 1.f;
			P.CloudBottomKm = 3.f; P.CloudHeightKm = 4.f;
			P.FogDensity = 0.012f; P.FogHeightFalloff = 0.12f; P.FogColor = FLinearColor(0.9f, 0.6f, 0.4f);
			P.ExposureBias = 0.3f; P.BloomIntensity = 0.9f; P.LensFlareIntensity = 1.f;
			P.WhiteBalanceKelvin = 6200.f; P.Saturation = 1.08f; P.Contrast = 1.05f;
			P.Gain = FLinearColor(1.05f, 1.f, 0.94f); P.Vignette = 0.35f;
			Physical(P, true, 11.05f, 14.05f, 0.3f);
		}
		return P;
	}

	static FEnvironmentPreset City(const FString& ChapterId)
	{
		FEnvironmentPreset P;
		const bool bNight = ChapterId.Contains(TEXT("eyes"));     // ch4 "Eyes in the Dark"
		const bool bLegacy = ChapterId.Contains(TEXT("legacy"));  // ch8: overcast morning
		P.Id = bNight ? TEXT("city_night") : (bLegacy ? TEXT("city_overcast") : TEXT("city_day"));
		if (bNight)
		{
			P.SunPitch = -6.f; P.SunYaw = 120.f; P.SunLux = 0.25f; P.SunKelvin = 9000.f;
			P.MoonLux = 0.25f; P.SkyLightIntensity = 0.25f;
			P.CloudBottomKm = 1.5f;
			P.FogDensity = 0.025f; P.FogHeightFalloff = 0.15f; P.FogColor = FLinearColor(0.15f, 0.2f, 0.3f);
			P.VolumetricFogExtinction = 1.4f;
			P.ExposureBias = 1.2f; P.BloomIntensity = 1.1f; P.LensFlareIntensity = 1.f;
			P.WhiteBalanceKelvin = 5200.f;     // sodium street lights read orange, shadows teal
			P.Saturation = 0.95f; P.Contrast = 1.12f; P.Vignette = 0.5f; P.FilmGrain = 0.15f;
			P.ChromaticAberration = 0.2f;
			P.bWet = true; P.bNight = true;
			Physical(P, false, 3.03f, 5.13f, -0.3f);
		}
		else if (bLegacy)
		{
			P.SunPitch = -40.f; P.SunYaw = 60.f; P.SunLux = 4.f; P.SunKelvin = 6800.f;
			P.SkyLightIntensity = 1.3f; P.CloudBottomKm = 1.2f; P.CloudHeightKm = 6.f;
			P.FogDensity = 0.02f; P.FogHeightFalloff = 0.1f; P.FogColor = FLinearColor(0.6f, 0.63f, 0.68f);
			P.Saturation = 0.9f; P.Contrast = 1.02f; P.Vignette = 0.3f; P.bWet = true;
			Physical(P, true, 11.58f, 13.58f, 0.3f);
		}
		else
		{
			P.SunPitch = -50.f; P.SunYaw = 30.f; P.SunLux = 10.f; P.SunKelvin = 5800.f;
			P.FogDensity = 0.008f; P.FogHeightFalloff = 0.15f;
			P.Saturation = 1.02f; P.Contrast = 1.05f;
			Physical(P, true, 14.05f, 17.05f, 0.f);
		}
		return P;
	}

	static FEnvironmentPreset Ruins(bool bBattlefield)
	{
		FEnvironmentPreset P;
		P.Id = bBattlefield ? TEXT("battlefield") : TEXT("ruins");
		P.SunPitch = bBattlefield ? -14.f : -22.f;
		P.SunYaw = bBattlefield ? 170.f : 140.f;
		P.SunLux = bBattlefield ? 4.f : 6.f;
		P.SunKelvin = bBattlefield ? 2800.f : 4500.f;        // smoke-filtered, orange
		P.SkyLightIntensity = 0.8f;
		P.CloudBottomKm = 1.f; P.CloudHeightKm = 3.f;
		P.FogDensity = bBattlefield ? 0.045f : 0.03f;
		P.FogHeightFalloff = 0.06f;
		P.FogColor = bBattlefield ? FLinearColor(0.7f, 0.4f, 0.22f) : FLinearColor(0.55f, 0.48f, 0.4f);
		P.VolumetricFogExtinction = bBattlefield ? 2.f : 1.3f;
		P.VolumetricFogAlbedo = FLinearColor(0.9f, 0.8f, 0.7f);
		P.ExposureBias = 0.2f; P.BloomIntensity = 0.9f; P.LensFlareIntensity = 0.7f;
		P.WhiteBalanceKelvin = 6800.f; P.Saturation = 0.85f; P.Contrast = 1.12f;
		P.Gain = bBattlefield ? FLinearColor(1.08f, 0.96f, 0.86f) : FLinearColor(1.02f, 0.98f, 0.92f);
		P.Vignette = 0.5f; P.FilmGrain = 0.15f; P.ChromaticAberration = 0.1f;
		P.bEmbers = true;
		if (bBattlefield) { Physical(P, true, 8.55f, 11.05f, -0.2f); } else { Physical(P, true, 11.08f, 14.08f, 0.f); }
		return P;
	}

	FEnvironmentPreset ForChapter(FName Location, const FString& ChapterId, bool bHasBoss, FName& OutSetId)
	{
		const FString L = Location.ToString().ToLower();
		if (L == TEXT("workshop"))
		{
			OutSetId = TEXT("workshop");
			return Workshop(ChapterId.Contains(TEXT("ghost")));   // ch9 "Ghost in the Machine": night
		}
		if (L == TEXT("rooftops"))
		{
			OutSetId = TEXT("rooftops");
			return Rooftops(ChapterId.Contains(TEXT("siege")) || ChapterId.Contains(TEXT("skyline")) || bHasBoss);
		}
		if (L == TEXT("city"))
		{
			OutSetId = TEXT("city");
			return City(ChapterId);
		}
		if (L == TEXT("ruins") || L == TEXT("battlefield"))
		{
			// "Titan's battlefield": the ruins chapter where the Obsidian Titan fights (ch5).
			const bool bBattle = bHasBoss || L == TEXT("battlefield");
			OutSetId = bBattle ? TEXT("battlefield") : TEXT("ruins");
			return Ruins(bBattle);
		}
		OutSetId = TEXT("arena");
		return Arena();
	}

	FEnvironmentPreset Prologue()
	{
		FEnvironmentPreset P = Workshop(false);
		P.Id = TEXT("prologue"); // own id so Vison's "prologue" lighting preset applies
		P.SunPitch = 4.f;        // 04:50 civil twilight, sun just below the horizon
		Physical(P, true, 4.91f, 6.91f, 0.f);
		return P;
	}

	FEnvironmentPreset Flashback()
	{
		// Dusk over the battlefield, years ago: a low sun through the smoke pall, ash and embers in the air.
		FEnvironmentPreset P = Ruins(true);
		P.Id = TEXT("flashback_dusk");
		P.SunPitch = -3.f; P.SunYaw = 255.f;   // 3 deg above the western horizon
		Physical(P, true, 7.6f, 10.1f, -0.4f);
		P.FogDensity = 0.06f; P.VolumetricFogExtinction = 2.4f;
		P.FogColor = FLinearColor(0.62f, 0.4f, 0.28f);
		// Muted, memory-like grade: low saturation, lifted contrast, warm gain, heavier grain and vignette.
		P.Saturation = 0.62f; P.Contrast = 1.06f;
		P.Gain = FLinearColor(1.05f, 0.97f, 0.9f);
		P.Vignette = 0.65f; P.FilmGrain = 0.22f; P.ChromaticAberration = 0.15f;
		P.BloomIntensity = 1.1f;
		P.bEmbers = true;
		return P;
	}

	FEnvironmentPreset Finale()
	{
		// Dawn over the battlefield: the fight is over, warm light breaks through.
		FEnvironmentPreset P = Ruins(true);
		P.Id = TEXT("finale_dawn");
		P.SunPitch = -3.f; P.SunYaw = 90.f;
		Physical(P, true, 9.58f, 12.08f, 0.3f);
		P.FogDensity = 0.02f; P.VolumetricFogExtinction = 1.f;
		P.Saturation = 1.f; P.Gain = FLinearColor(1.06f, 1.f, 0.92f); P.FilmGrain = 0.08f;
		P.bEmbers = false;
		return P;
	}
}
