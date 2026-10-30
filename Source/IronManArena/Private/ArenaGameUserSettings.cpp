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
	VoiceVolume = 1.f;
	MusicVolume = 0.8f;
	bSubtitles = true;
	bRayTracing = false;
	bMotionBlur = true;
	bCinematicDOF = true;
	bCameraShake = true;
	ToggleFullscreenMode = EWindowMode::WindowedFullscreen;
}

void UArenaGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	ApplyGameOptionDefaults();
	// The game defaults to borderless windowed fullscreen at desktop resolution.
	SetFullscreenMode(EWindowMode::WindowedFullscreen);
	SetScreenResolution(GetDesktopResolution());
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
	SetInt(TEXT("r.RayTracing.Shadows"), bRT ? 1 : 0);
	// Motion blur: 0 disables it; otherwise leave the scalability group's quality.
	if (!bMotionBlur) { SetInt(TEXT("r.MotionBlurQuality"), 0); }
	else if (IConsoleVariable* MB = CM.FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		if (MB->GetInt() == 0) { MB->Set(GetPostProcessingQuality() >= 3 ? 4 : 3, ECVF_SetByGameSetting); }
	}
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
	Override(VoiceSoundClass, VoiceVolume);
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
