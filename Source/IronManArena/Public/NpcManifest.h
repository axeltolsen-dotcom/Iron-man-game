// NpcManifest: data for the city's people (pedestrians, the pilot's voice). Read from
// Content/NPC/npc_manifest.json (staged next to the /Game/NPC assets another worker produces; also accepted at
// Content/NPC/Data/npc_manifest.json). Tolerant schema - every field is optional:
// {
//   "bodies":  [ { "id": "quinn", "mesh": "/Game/NPC/Bodies/SKM_Quinn", "animBlueprint": "/Game/.../ABP_X",
//                  "feminine": true, "weight": 1, "scaleMin": 0.92, "scaleMax": 1.06 } ],
//   "anims":   { "walk": ["/Game/NPC/Anims/Walk_01", ...], "run": [...], "idle": [...], "panic": [...], "talk": [...] },
//   "voices":  [ { "id": "en_US-amy", "name": "Amy", "gender": "f", "preview": "/Game/NPC/Voices/en_US-amy/greeting_1",
//                  "lines": { "idle": ["/Game/NPC/Voices/en_US-amy/idle_1", ...], "panic": [...] } } ]
//              (voices may also be plain id strings; lines are then found by the convention
//               /Game/NPC/Voices/<VoiceId>/<Category>_<n>, n = 1..12)
// }
// Also accepts the real schema: "animCategories" {cat:[paths]} and "voiceSets" [{id, gender:"male"/"female", note,
// categories{cat:[paths]}}] (paths may 404 until the import scripts run - each is checked with DoesPackageExist).
// Bark categories: idle, see_ironman, panic, hit, car_explode (alias explosion), thanks (alias greeting);
// police drone PA uses voice id police_pa.
// Anything missing falls back (engine mannequins / basic-shape people, silent NPCs, placeholder voices).
#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UAnimSequenceBase;
class USkeletalMesh;
class UAnimInstance;

struct FNpcBodyDef
{
	FString Id;
	FString MeshPath;
	FString AnimBlueprintPath;
	bool bFeminine = false;
	float Weight = 1.f;
	float ScaleMin = 0.94f;
	float ScaleMax = 1.06f;
};

struct FNpcVoiceDef
{
	FName Id;
	FString Name;
	bool bFeminine = false;
	bool bPlaceholder = false;
	FString PreviewPath;
	TMap<FName, TArray<FString>> Lines; // explicit; convention lookup fills in the rest lazily
};

namespace NpcManifest
{
	IRONMANARENA_API bool IsLoaded();
	IRONMANARENA_API const TArray<FNpcBodyDef>& Bodies();
	IRONMANARENA_API const TArray<FNpcVoiceDef>& Voices();
	IRONMANARENA_API const FNpcVoiceDef* FindVoice(FName Id);
	/** Animation paths by category (walk / run / idle / panic / talk). */
	IRONMANARENA_API const TArray<FString>& Anims(FName Category);

	/** Loads a sound for voice + category (random line); nullptr if none exists. */
	IRONMANARENA_API USoundBase* PickLine(FName VoiceId, FName Category, FRandomStream& Rng);
	/** The voice's preview line (preview path, greeting or idle). */
	IRONMANARENA_API USoundBase* PreviewLine(FName VoiceId);

	IRONMANARENA_API USoundBase* LoadSound(const FString& Path);
	IRONMANARENA_API UAnimSequenceBase* LoadAnim(const FString& Path);
	IRONMANARENA_API USkeletalMesh* LoadSkeletalMesh(const FString& Path);
	IRONMANARENA_API UClass* LoadAnimClass(const FString& Path);
}
