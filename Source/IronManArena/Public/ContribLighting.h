// ContribLighting: applies Vison's per-chapter lighting presets (Content/Story/lighting_presets.json,
// from contrib/vison) on top of the C++ FEnvironmentPreset, merged with Vison's realism layer
// (Content/Story/postprocess_realism.json: sky, clouds, fog, Lumen and post; its _tier_overrides apply at
// Epic and Medium). The JSON names engine properties
// directly (DirectionalLight / MoonLight / SkyAtmosphere / SkyLight / ExponentialHeightFog /
// VolumetricCloud / PostProcessVolume), so they are written through UE reflection; every
// post-process key also sets its bOverride_ flag. Keys starting with "_" are documentation.
// Unknown or renamed properties are logged and skipped (never fatal).
//
// Physical light levels (decision 7:43): the presets' real sun (120000 lux TOA, 5900 K) and
// EV100 exposure are used as-is (LuxScale 1). The project's lamps (lumens) and emissives (cd/m2 via
// ArenaVisuals::EmissiveToNits) are balanced to real-world levels to match. LuxScale != 1 is kept
// only as a debugging aid (it scales sun/moon and shifts the EV100 range by log2(LuxScale)).
//
// CineCamera: ApplyCamera() pushes Filmback / LensSettings / focal length / aperture / focus and the
// cutscene PostProcessOverrides (manual physical-camera exposure) onto a UCineCameraComponent.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UCineCameraComponent;

namespace ContribLighting
{
	/** JSON preset key for an FEnvironmentPreset::Id (chapter_mapping.FEnvironmentPreset_Id_to_preset); NAME_None if unmapped. */
	IRONMANARENA_API FName PresetKeyFor(FName EnvironmentPresetId);

	/** True if lighting_presets.json loaded and has this preset key. */
	IRONMANARENA_API bool HasPreset(FName PresetKey);

	/** Number of presets loaded (0 when the file is missing). */
	IRONMANARENA_API int32 GetNumPresets();

	/** Presets merged from postprocess_realism.json (Vison's realism layer; 0 when the file is missing). */
	IRONMANARENA_API int32 GetNumRealismPresets();

	/** Preset key for an environment: the id itself, its chapter mapping, or "city" for free-play "arena". */
	IRONMANARENA_API FName KeyForEnvironment(FName EnvironmentPresetId);

	/** Apply a preset to the light/sky/fog/cloud/post-process components found on EnvActor.
	 *  Returns the number of properties written; OutSkipped counts keys that did not resolve. */
	IRONMANARENA_API int32 Apply(AActor* EnvActor, FName PresetKey, float LuxScale = 1.f, int32* OutSkipped = nullptr);

	/** Apply the preset's "CineCamera" block (cutscene camera). bAllowDOF false keeps the aperture
	 *  wide-open-free (f/22, effectively no depth of field) for the "Cinematic DOF" setting. */
	IRONMANARENA_API int32 ApplyCamera(UCineCameraComponent* Camera, FName PresetKey, bool bAllowDOF = true, int32* OutSkipped = nullptr);
}
