// UArenaSaveGame: persists the arena high score, best wave, last suit and story progress
// (slot "IronManArena", user 0). Use LoadOrCreate + Write so arena and story code never
// overwrite each other's fields.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ArenaSaveGame.generated.h"

UCLASS()
class IRONMANARENA_API UArenaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static UArenaSaveGame* LoadOrCreate(const FString& SlotName);
	bool Write(const FString& SlotName);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save")
	int32 HighScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save")
	int32 BestWave = 0;

	/** Suit picked last time on the suit-select screen (suits.json id). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save")
	FName LastSuitId;

	// ---- Story ----

	/** Highest chapter index the player may start (0-based; chapters.Num() == story complete). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	int32 StoryChapterReached = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	bool bStoryComplete = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	bool bPrologueSeen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	TArray<FName> StoryUnlockedSuits;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	TArray<FName> StoryAllies;

	/** {playerName} - default "Harley". */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	FString PlayerName;

	/** {heroName} - asked for in the finale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Story")
	FString HeroName;
};
