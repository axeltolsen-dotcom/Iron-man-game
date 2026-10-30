// Story mode data: mirrors Content/Story/story.json (Friday's script, copied from
// /workspace/ironman-3d/contrib/story.json) and Content/Story/morgan.json (Morgan's comms chatter).
//
// story.json layout (all keys optional except chapters):
//   { title, playerNameDefault, player{...}, tokens{...}, speakers[...],
//     prologue{ shots[] },
//     chapters[ { id, title, location, intro{shots[]}, outro{shots[]},
//                 objectives[ { type, target, text, count, allyAvailable,
//                               comms[ {trigger: start|progress|complete, speaker, text} ] } ],
//                 unlockSuit, unlockAlly, reactor{stage, mount, note} } ],
//     finale{ shots[], promptHeroName, reactor{...}, allySuit{speaker, suit} } }
//   shot = { camera, characters[], sfx, line{speaker, text} }
// Text tokens: {playerName} (default "Harley"), {heroName} (asked for in the finale).
#pragma once

#include "CoreMinimal.h"
#include "StoryTypes.generated.h"

UENUM(BlueprintType)
enum class EStoryObjectiveType : uint8
{
	Ground,      // reach a target on foot
	Flight,      // reach an airborne target
	Drone,       // destroy N drones
	Tank,        // destroy N tanks
	Wave,        // clear N waves
	Boss,        // defeat the boss
	Hulkbuster,  // deploy the Hulkbuster N times
	Energy,      // hold out without the reactor forcing a POWER DEPLETED landing
	Unknown
};

UENUM(BlueprintType)
enum class EStoryCommsTrigger : uint8
{
	Start,
	Progress,
	Complete
};

/** Harley's reactor across the story: makeshift > unstable > failed (ch5) > upgraded (ch7+). */
UENUM(BlueprintType)
enum class EReactorStage : uint8
{
	Makeshift,
	Unstable,
	Failed,
	Upgraded
};

UENUM(BlueprintType)
enum class EReactorMount : uint8
{
	Spine,
	Forearm
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryLine
{
	GENERATED_BODY()

	/** Stable id = audio asset name (e.g. ch1_dust_intro_03, ch4_eyes_obj1_progress). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName Id;

	/** Speaker key as written in the script (MORGAN, PLAYER, TONY, Vison, DUM-E ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName Speaker;

	/** Raw text with {playerName} / {heroName} tokens (resolve with UStoryLibrary::ResolveTokens). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Text;

	// Voice clips are loaded on demand (UStoryLibrary::GetLineSound) so the static story cache
	// never holds UObject pointers the garbage collector can't see.

	bool HasTokens() const { return Text.Contains(TEXT("{")); }
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryShot
{
	GENERATED_BODY()

	/** Director's note; also drives the scripted camera framing (wide / close-up / low angle ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FName> Characters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Sfx;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	bool bHasLine = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FStoryLine Line;
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryComms
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	EStoryCommsTrigger Trigger = EStoryCommsTrigger::Start;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FStoryLine Line;
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	EStoryObjectiveType Type = EStoryObjectiveType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString TypeName;

	/** Free text target ("breaker panel", "rooftop", "OBSIDIAN TITAN", "no-forced-landing" ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Target;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Text;

	/** Kills / waves / deployments needed (>= 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	int32 Count = 1;

	/** Ally that can be summoned during this objective (e.g. "vison"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName AllyAvailable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryComms> Comms;
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryReactor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	EReactorStage Stage = EReactorStage::Makeshift;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	EReactorMount Mount = EReactorMount::Spine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Note;
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryChapter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Title;

	/** workshop / rooftops / city / ruins (selects the arena dressing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryShot> Intro;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryShot> Outro;

	/** Suit id (suits.json) unlocked and worn in this chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName UnlockSuit;

	/** Ally unlocked by finishing this chapter (e.g. "vison" after ch5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName UnlockAlly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FStoryReactor Reactor;

	/** Optional "lore" string from story.json (archive / Easter-egg note shown in the story menu).
	 *  Reserved for lore and Vision Quest nods - see Docs/LORE.md. Empty in the shipped script. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Lore;
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryFinale
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryShot> Shots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	bool bPromptHeroName = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FStoryReactor Reactor;

	/** Who flies alongside in the finale and in which suit (MORGAN / rescue). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName AllySpeaker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName AllySuit;
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryBook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString PlayerNameDefault = TEXT("Harley");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FName> Speakers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryShot> Prologue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryChapter> Chapters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FStoryFinale Finale;

	bool IsValid() const { return Chapters.Num() > 0; }
};
