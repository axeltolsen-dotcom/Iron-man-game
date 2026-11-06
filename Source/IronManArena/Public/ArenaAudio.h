// Audio-event layer: every gameplay / UI sound goes through ArenaAudio::Play*(EventName).
// Events come from Content/Audio/sfx_manifest.json (contrib/audio/sfx_manifest.json): event -> SoundWave asset,
// sound class (SC_SFX / SC_UI / SC_Music / SC_Ambience / SC_Voice) and looping flag. Variants are grouped by base name
// ("repulsor_fire", "repulsor_fire_var1", "repulsor_fire_var2" -> "repulsor_fire"; "punch_01".."punch_08" -> "punch"),
// so Play("repulsor_fire") or Play("punch") picks a random take (never the same twice in a row).
// Missing manifest / asset = silent no-op, so the game runs on machines without the imported SFX.
// SC_Voice events are muted unless Settings > Audio > Voices is on.
#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;

namespace ArenaAudio
{
	/** Random take for an event or base name (null if unknown / not imported). */
	IRONMANARENA_API USoundBase* Find(FName Event);
	IRONMANARENA_API bool Has(FName Event);

	/** 2D (UI, HUD, music stingers). */
	IRONMANARENA_API void Play2D(const UObject* WorldContext, FName Event, float Volume = 1.f, float Pitch = 1.f);
	/** 3D one-shot with distance attenuation; PitchJitter randomises +-. */
	IRONMANARENA_API void PlayAt(const UObject* WorldContext, FName Event, const FVector& Location, float Volume = 1.f, float PitchJitter = 0.06f);
	/** Looping (or one-shot) sound attached to a component; returned component is owned by the caller (Stop / FadeOut). */
	IRONMANARENA_API UAudioComponent* Attach(FName Event, USceneComponent* AttachTo, float Volume = 1.f, bool bLoop = true);
	/** Looping 2D bed (menu music, ambience). Not auto-destroyed. */
	IRONMANARENA_API UAudioComponent* Loop2D(const UObject* WorldContext, FName Event, float Volume = 1.f);

	/** Events loaded from the manifest (0 = no manifest found). */
	IRONMANARENA_API int32 NumEvents();
}
