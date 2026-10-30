// UArenaGameUserSettings: UGameUserSettings + the game's own options (FOV, mouse, audio, subtitles).
// Registered in Config/DefaultEngine.ini ([/Script/Engine.Engine] GameUserSettingsClassName), so
// UGameUserSettings::GetGameUserSettings() returns this class and everything is saved together in
// Saved/Config/<Platform>/GameUserSettings.ini.
//
// Defaults (windowed fullscreen ...) live in Config/DefaultGameUserSettings.ini.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ArenaGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class EArenaSoundCategory : uint8
{
	SFX,
	Voice,
	Music
};

class USoundClass;

UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class IRONMANARENA_API UArenaGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UArenaGameUserSettings();

	/** The engine's settings object as our class (null if the ini override is missing). */
	UFUNCTION(BlueprintPure, Category = "Settings")
	static UArenaGameUserSettings* GetArenaSettings();

	virtual void SetToDefaults() override;

	/** Applies the non-engine options (camera, mouse, audio) to the running game. Call after
	 *  ApplySettings(). Safe with a null world context (audio only). */
	void ApplyGameplaySettings(const UObject* WorldContext);

	/** Toggles fullscreen <-> windowed (keeps the preferred fullscreen flavour), applies and saves. */
	void ToggleFullscreen();

	/** The Sound Class at Path if that asset exists (never logs "Failed to find object"); else null. */
	static USoundClass* LoadSoundClassIfExists(const FSoftObjectPath& Path);

	/** SC_Voice (or null): voice / story lines play through it so the Voice slider applies via the sound mix. */
	USoundClass* GetVoiceSoundClass() const { return LoadSoundClassIfExists(VoiceSoundClass); }

	/** Volume for voice audio components: 1 when SC_Voice carries the slider, else the Voice slider. */
	float GetVoiceSourceVolume() const;

	/** Master * category, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetEffectiveVolume(EArenaSoundCategory Category) const;

	// ------------------------------------------------------------------ game options
	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Camera")
	float FieldOfView = 95.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Controls")
	float MouseSensitivity = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Controls")
	bool bInvertY = false;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Audio")
	float MasterVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Audio")
	float SfxVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Audio")
	float VoiceVolume = 0.7f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Audio")
	float MusicVolume = 0.8f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Accessibility")
	bool bSubtitles = true;

	/** Hardware ray tracing (Lumen HWRT + RT shadows). Needs an RT-capable GPU (RTX-class);
	 *  ignored where unsupported. Toggled at runtime via r.RayTracing.EnableOnDemand. */
	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Graphics")
	bool bRayTracing = false;

	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Graphics")
	bool bMotionBlur = true;

	/** Cinematic depth of field in cutscenes / suit-up (CineCamera). */
	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Graphics")
	bool bCinematicDOF = true;

	/** Camera shake on hits / explosions / landings (accessibility: can be turned off). */
	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Accessibility")
	bool bCameraShake = true;

	/** True when the RHI can do ray tracing at all (false on -nullrhi / old GPUs). */
	static bool IsRayTracingSupported();

	/** Pushes bRayTracing / bMotionBlur into the renderer cvars. No world needed. */
	void ApplyRenderingSettings() const;

	/** Fullscreen flavour F11 / Alt+Enter returns to (WindowedFullscreen by default). */
	UPROPERTY(config, BlueprintReadWrite, Category = "Settings|Display")
	TEnumAsByte<EWindowMode::Type> ToggleFullscreenMode = EWindowMode::WindowedFullscreen;

	/** Optional Sound Class assets; when they exist their volume follows the sliders. */
	UPROPERTY(config, EditAnywhere, Category = "Settings|Audio")
	FSoftObjectPath SfxSoundClass = FSoftObjectPath(TEXT("/Game/Audio/Classes/SC_SFX.SC_SFX"));

	UPROPERTY(config, EditAnywhere, Category = "Settings|Audio")
	FSoftObjectPath VoiceSoundClass = FSoftObjectPath(TEXT("/Game/Audio/Classes/SC_Voice.SC_Voice"));

	UPROPERTY(config, EditAnywhere, Category = "Settings|Audio")
	FSoftObjectPath MusicSoundClass = FSoftObjectPath(TEXT("/Game/Audio/Classes/SC_Music.SC_Music"));

private:
	void ApplyGameOptionDefaults();
};
