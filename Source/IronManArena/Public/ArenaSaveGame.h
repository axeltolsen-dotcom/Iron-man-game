// UArenaSaveGame: persists the arena high score, best wave, last suit and story progress
// (slot "IronManArena", user 0). Use LoadOrCreate + Write so arena and story code never
// overwrite each other's fields.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CareerTypes.h"
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

	// ---- Career (UCareerSubsystem): credits, shop, houses, avatar ----

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	int32 Credits = 0;

	/** Lifetime credits earned (stats). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	int32 CreditsEarned = 0;

	/** Level per ECareerUpgrade (index = enum value). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<int32> CareerUpgradeLevels;

	/** Shop suits bought (suit ids). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<FName> CareerSuitsOwned;

	/** Houses bought (Career::Houses ids). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<FName> HousesOwned;

	/** Free play starts here when set and owned (else the Legacy Plaza). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	FName HomeHouse;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	bool bAvatarCreated = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	FAvatarConfig Avatar;

	/** Clothing-store outfit slots (Career::OutfitSlots). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<FAvatarOutfit> OutfitSlots;

	/** Clothing / barber styles bought (Career::ClothingItems ids). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<FName> ClothingOwned;

	/** Dealership vehicles bought (Career::Vehicles ids). Stored at your houses' garages. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<FName> VehiclesOwned;

	/** Boss beaten in free play (unlocks the Ultron suit in the shop). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	bool bBossBeaten = false;

	/** Casino: net credits lost today (resets when CasinoDay changes; day = days since 1970, local). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	int32 CasinoLossToday = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	int32 CasinoDay = 0;

	/** Side jobs claimed (CareerEconomy::Jobs ids) whose cooldown has not expired. Parallel to JobClaimUnix. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<FName> JobsClaimed;

	/** Unix seconds when each JobsClaimed entry was last claimed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save|Career")
	TArray<int64> JobClaimUnix;

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
	inline const FName PaleBlueTrim()  { return FName(TEXT("jocasta-pale-blue-trim")); }
}
