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

	// ---- Side missions + codex (Friday's side_missions.json / codex.json) ----

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Side")
	TArray<FName> SideMissionsDone;

	/** Codex entries granted as rewards (chapter entries are derived from StoryChapterReached). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Side")
	TArray<FName> CodexGranted;

	/** repulsor-capacitor-bank, heat-sink-fins ... (always active once earned). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Side")
	TArray<FName> Upgrades;

	/** vison-white-trim, garage-plaid-decal, hall-of-armor-pod-lighting ... */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Side")
	TArray<FName> Cosmetics;

	/** Earned cosmetics the player switched off in the codex menu. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Side")
	TArray<FName> CosmeticsDisabled;

	bool HasUpgrade(FName Id) const { return Upgrades.Contains(Id); }
	bool IsCosmeticActive(FName Id) const { return Cosmetics.Contains(Id) && !CosmeticsDisabled.Contains(Id); }
};

/** Reward ids used in code (side_missions.json "reward"). */
namespace ArenaRewards
{
	inline const FName CapacitorBank() { return FName(TEXT("repulsor-capacitor-bank")); }
	inline const FName HeatSinkFins()  { return FName(TEXT("heat-sink-fins")); }
	inline const FName WhiteTrim()     { return FName(TEXT("vison-white-trim")); }
	inline const FName PlaidDecal()    { return FName(TEXT("garage-plaid-decal")); }
	inline const FName PodLighting()   { return FName(TEXT("hall-of-armor-pod-lighting")); }
}
