#include "ArenaGameUserSettings.h"

#include "IronManCharacter.h"
#include "JarvisComponent.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Misc/PackageName.h"
#include "HAL/IConsoleManager.h"
#include "RenderUtils.h"

namespace
{
	// Transient mix carrying the category overrides (only used when Sound Class assets exist).
	TWeakObjectPtr<USoundMix> GArenaVolumeMix;
	TWeakObjectPtr<USoundMix> GArenaDuckMix;
	bool GArenaDucked = false;
}

UArenaGameUserSettings::UArenaGameUserSettings()
{
	ApplyGameOptionDefaults();
}

UArenaGameUserSettings* UArenaGameUserSettings::GetArenaSettings()
{
	return GEngine ? Cast<UArenaGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UArenaGameUserSettings::ApplyGameOptionDefaults()
{
	FieldOfView = 95.f;
	MouseSensitivity = 1.f;
	bInvertY = false;
	MasterVolume = 1.f;
	SfxVolume = 1.f;
	bVoicesEnabled = false; // voices start muted, subtitles on
	VoiceVolume = 0.7f; // voices sit under SFX by default (matches SC_Voice's 0.7 baseline)
	MusicVolume = 0.8f;
	bSubtitles = true;
	bRayTracing = false;
	bMotionBlur = true;
	bCinematicDOF = true;
	bCameraShake = true;
	CameraShakeIntensity = 1.f;
	bHudColorBlindSafe = false;
	bHudReducedMotion = false;
	HudOpacity = 1.f;
	HudScale = 1.f;
	SubtitleScale = 1.f;
	VoiceDuckSfx = 0.6f;
	VoiceDuckMusic = 0.45f;
	SettingsVersion = CurrentSettingsVersion;
	ToggleFullscreenMode = EWindowMode::WindowedFullscreen;
}

void UArenaGameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	if (SettingsVersion < 2)
	{
		// v2: voice lines were re-rendered at -20 LUFS and the default voice level dropped to 0.7.
		// Only the old untouched default (1.0) moves; a level the player chose stays.
		if (FMath::IsNearlyEqual(VoiceVolume, 1.f, 0.001f))
		{
			VoiceVolume = 0.7f;
			UE_LOG(LogTemp, Log, TEXT("ArenaGameUserSettings: migrated VoiceVolume 1.0 -> 0.7 (settings v%d)"), CurrentSettingsVersion);
		}
	}
	if (SettingsVersion < 3)
	{
		// v3: spoken voices start OFF for everyone (toggle in Settings > Audio > Voices).
		bVoicesEnabled = false;
		bSubtitles = true;
	}
	if (SettingsVersion < CurrentSettingsVersion)
	{
		SettingsVersion = CurrentSettingsVersion;
		SaveConfig();
	}
}

void UArenaGameUserSettings::SetVoiceDucking(const UObject* WorldContext, bool bDuck)
{
	if (GArenaDucked == bDuck) { return; }
	UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UArenaGameUserSettings* S = GetArenaSettings();
	if (!World || !S) { if (!bDuck) { GArenaDucked = false; } return; }
	USoundMix* Mix = GArenaDuckMix.Get();
	if (!Mix)
	{
		Mix = NewObject<USoundMix>(GetTransientPackage(), TEXT("ArenaVoiceDuckMix"));
		Mix->AddToRoot();
		Mix->FadeInTime = 0.15f;
		Mix->FadeOutTime = 0.6f;
		GArenaDuckMix = Mix;
	}
	USoundClass* Sfx = LoadSoundClassIfExists(S->SfxSoundClass);
	USoundClass* Music = LoadSoundClassIfExists(S->MusicSoundClass);
	if (!Sfx && !Music) { return; }
	GArenaDucked = bDuck;
	if (bDuck)
	{
		if (Sfx) { UGameplayStatics::SetSoundMixClassOverride(World, Mix, Sfx, FMath::Clamp(S->VoiceDuckSfx, 0.f, 1.f), 1.f, 0.15f, false); }
		if (Music) { UGameplayStatics::SetSoundMixClassOverride(World, Mix, Music, FMath::Clamp(S->VoiceDuckMusic, 0.f, 1.f), 1.f, 0.15f, false); }
		UGameplayStatics::PushSoundMixModifier(World, Mix);
	}
	else
	{
		UGameplayStatics::PopSoundMixModifier(World, Mix);
	}
}

void UArenaGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	ApplyGameOptionDefaults();
	// The game defaults to borderless windowed fullscreen at desktop resolution.
	SetFullscreenMode(EWindowMode::WindowedFullscreen);
	SetScreenResolution(GetDesktopResolution());
	SetFrameRateLimit(60.f); // locked 60 (VSync stays an option in Settings)
}

USoundClass* UArenaGameUserSettings::LoadSoundClassIfExists(const FSoftObjectPath& Path)
{
	if (Path.IsNull()) { return nullptr; }
	if (UObject* Already = Path.ResolveObject()) { return Cast<USoundClass>(Already); }
	if (!FPackageName::DoesPackageExist(Path.GetLongPackageName())) { return nullptr; }
	return Cast<USoundClass>(Path.TryLoad());
}

float UArenaGameUserSettings::GetVoiceSourceVolume() const
{
	return GetVoiceSoundClass() ? 1.f : FMath::Clamp(VoiceVolume, 0.f, 1.f);
}

float UArenaGameUserSettings::GetEffectiveVolume(EArenaSoundCategory Category) const
{
	float Cat = 1.f;
	switch (Category)
	{
	case EArenaSoundCategory::SFX:   Cat = SfxVolume; break;
	case EArenaSoundCategory::Voice: Cat = VoiceVolume; break;
	case EArenaSoundCategory::Music: Cat = MusicVolume; break;
	}
	return FMath::Clamp(MasterVolume, 0.f, 1.f) * FMath::Clamp(Cat, 0.f, 1.f);
}

void UArenaGameUserSettings::ToggleFullscreen()
{
	const EWindowMode::Type Current = GetFullscreenMode();
	if (Current == EWindowMode::Windowed)
	{
		const EWindowMode::Type Target = ToggleFullscreenMode == EWindowMode::Windowed
			? EWindowMode::WindowedFullscreen : ToggleFullscreenMode.GetValue();
		SetFullscreenMode(Target);
		if (Target == EWindowMode::WindowedFullscreen) { SetScreenResolution(GetDesktopResolution()); }
	}
	else
	{
		ToggleFullscreenMode = Current; // remember the flavour we came from
		SetFullscreenMode(EWindowMode::Windowed);
		const FIntPoint Desk = GetDesktopResolution();
		SetScreenResolution(FIntPoint(FMath::Max(1280, Desk.X * 3 / 4), FMath::Max(720, Desk.Y * 3 / 4)));
	}
	ApplySettings(false);
	SaveSettings();
}

bool UArenaGameUserSettings::IsRayTracingSupported()
{
	// IsRayTracingAllowed(): project has r.RayTracing=1 and the RHI/GPU supports it.
	return IsRayTracingAllowed();
}

void UArenaGameUserSettings::ApplyRenderingSettings() const
{
	IConsoleManager& CM = IConsoleManager::Get();
	auto SetInt = [&CM](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* V = CM.FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByGameSetting); }
	};
	const bool bRT = bRayTracing && IsRayTracingSupported();
	SetInt(TEXT("r.RayTracing.Enable"), bRT ? 1 : 0);
	SetInt(TEXT("r.Lumen.HardwareRayTracing"), bRT ? 1 : 0);
	// Never RT shadows: they take precedence over VSM and cost more (Ultron perf budget §0.2).
	SetInt(TEXT("r.RayTracing.Shadows"), 0);
	// Motion blur: 0 disables it; otherwise the PostProcessQuality tier's value from DefaultScalability.ini
	// (Vison 8:57: 3 at Medium/High, 4 at Epic). Written at game-setting priority, so it is re-derived from
	// the tier each time instead of keeping a stale value after the tier changes.
	const int32 PPQ = GetPostProcessingQuality();
	SetInt(TEXT("r.MotionBlurQuality"), !bMotionBlur ? 0 : (PPQ >= 3 ? 4 : (PPQ >= 1 ? 3 : 0)));

	// Vison's per-tier values for cvars that 5.8 does not flag ECVF_Scalability (so DefaultScalability.ini
	// cannot set them). Index: 0 = Medium and below, 1 = High, 2 = Epic and above.
	auto SetFloat = [&CM](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = CM.FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByGameSetting); }
	};
	auto TierOf = [](int32 Level) { return Level >= 3 ? 2 : (Level == 2 ? 1 : 0); };
	const int32 AA = TierOf(GetAntiAliasingQuality());
	static const float DynResMin[3] = { 50.f, 67.f, 60.f };
	static const float DynResMax[3] = { 85.f, 100.f, 100.f };
	static const float FlickerPeriod[3] = { 2.f, 2.f, 3.f };
	static const float WeightClamp[3] = { 3.f, 3.f, 4.f };
	SetFloat(TEXT("r.DynamicRes.MinScreenPercentage"), DynResMin[AA]);
	SetFloat(TEXT("r.DynamicRes.MaxScreenPercentage"), DynResMax[AA]);
	SetFloat(TEXT("r.TSR.ShadingRejection.Flickering.Period"), FlickerPeriod[AA]);
	SetFloat(TEXT("r.TSR.Velocity.WeightClampingSampleCount"), WeightClamp[AA]);
	static const float MaxRoughness[3] = { 0.25f, 0.35f, 0.4f };
	SetFloat(TEXT("r.Lumen.Reflections.MaxRoughnessToTrace"), MaxRoughness[TierOf(GetReflectionQuality())]);
	SetFloat(TEXT("r.Nanite.MaxPixelsPerEdge"), TierOf(GetViewDistanceQuality()) == 0 ? 2.f : 1.f);
}

void UArenaGameUserSettings::ApplyGameplaySettings(const UObject* WorldContext)
{
	ApplyRenderingSettings();

	// --- audio: master on the main audio device; categories on our own sources / sound classes ---
	if (GEngine)
	{
		if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
		{
			Device->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
		}
	}

	UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) { return; }

	// Sound Class assets (/Game/Audio/Classes, made by Content/Python/setup_sound_classes.py); skipped if absent.
	USoundMix* Mix = GArenaVolumeMix.Get();
	if (!Mix)
	{
		Mix = NewObject<USoundMix>(GetTransientPackage(), TEXT("ArenaVolumeMix"));
		Mix->AddToRoot(); // lives for the session; tiny
		GArenaVolumeMix = Mix;
	}
	bool bAnyClass = false;
	auto Override = [World, Mix, &bAnyClass](const FSoftObjectPath& Path, float Volume)
	{
		if (USoundClass* Class = LoadSoundClassIfExists(Path))
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, FMath::Clamp(Volume, 0.f, 1.f), 1.f, 0.f, true);
			bAnyClass = true;
		}
	};
	Override(SfxSoundClass, SfxVolume);
	// SC_Voice already plays at VoiceClassBaseVolume, so the mix scales relative to it: the slider
	// value is the absolute voice level (0.7 default = asset baseline, 1.0 = full).
	{
		constexpr float VoiceClassBaseVolume = 0.7f;
		if (USoundClass* Class = LoadSoundClassIfExists(VoiceSoundClass))
		{
			const float Base = FMath::Max(0.05f, Class->Properties.Volume > 0.f ? Class->Properties.Volume : VoiceClassBaseVolume);
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, FMath::Clamp(VoiceVolume, 0.f, 1.f) / Base, 1.f, 0.f, true);
			bAnyClass = true;
		}
	}
	Override(MusicSoundClass, MusicVolume);
	if (bAnyClass)
	{
		// Pop first so repeated applies keep the mix's push count at one.
		UGameplayStatics::PopSoundMixModifier(World, Mix);
		UGameplayStatics::PushSoundMixModifier(World, Mix);
	}

	// AI voice: through SC_Voice when it exists (volume 1 on the source), else the slider on the source.
	if (UJarvisComponent* J = UJarvisComponent::Get(World)) { J->SetVoiceVolume(GetVoiceSourceVolume()); }

	// --- camera / mouse on the player ---
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (AIronManCharacter* Player = Cast<AIronManCharacter>(PC->GetPawn()))
		{
			Player->SetCameraFOV(FieldOfView);
			Player->SetLookSettings(FMath::Clamp(MouseSensitivity, 0.05f, 5.f), bInvertY);
		}
	}
}

bool UArenaGameUserSettings::AreVoicesEnabled()
{
	const UArenaGameUserSettings* S = GetArenaSettings();
	return S ? S->bVoicesEnabled : false;
}
