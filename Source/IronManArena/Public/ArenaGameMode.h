// AArenaGameMode: waves, scoring, combo, Hulkbuster milestones, high score, game flow.
//
// Flow:  Menu (paused, main menu widget)  ->  suit select  ->  StartNewGame(Suit)  ->  full
//        suit-up cinematic (waves wait for it)  ->  Playing  ->  player dies
//        -> 1.8 s -> GameOver (menu widget in game-over mode, high score saved)  ->  Restart.
//
// Waves (same formulas as the 2-D reference):
//   drones = max(2, FirstWaveDrones + (wave-1)*DronesPerWave - (boss wave ? 4 : 0))
//   tanks  = wave >= TanksFromWave ? floor(1 + (wave-TanksFromWave)*TanksPerWave) - (boss ? 1 : 0) : 0
//   every BossEvery-th wave the Obsidian Titan arrives first.
//   Max MaxAlive enemies at once; spawn gap = SpawnGap * rand(0.6..1.4) * max(0.45, 1 - wave*0.05).
//   Clearing a wave: bonus = WaveBonus * wave * (boss ? BossWaveMultiplier : 1), +10 HP
//   (+40 HP right before a boss wave), then a BreakTime pause.
// Scoring: drone 100, tank 350, boss 5000 (set on the enemy classes) x combo.
//   Each kill within ComboWindow seconds of the previous one raises the combo (max x8).
//   Every HulkbusterScoreStep points grants a Hulkbuster charge.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EnemyBase.h"
#include "ArenaGameMode.generated.h"

class AIronManCharacter;
class ABossTitan;
class UArenaSaveGame;
class ASuitUpCinematic;
class AStoryDirector;

UENUM(BlueprintType)
enum class EArenaPlayMode : uint8
{
	Arena,
	Story
};

UENUM(BlueprintType)
enum class EArenaGameState : uint8
{
	Menu,
	Playing,
	GameOver
};

UCLASS()
class IRONMANARENA_API AArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaGameMode();

	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------------------------------------------- flow
	/**
	 * Clears the arena, resets the player and score, plays the full suit-up cinematic for
	 * SuitIndex (INDEX_NONE = last selected suit), then starts wave 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void StartNewGame(int32 SuitIndex = -1);

	/**
	 * Story mode (Content/Story/story.json): resets like StartNewGame, then hands control to the
	 * story director. ChapterIndex < 0 = continue from the saved chapter (prologue on first run).
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena|Story")
	void StartStory(int32 ChapterIndex = -1, bool bPlayPrologue = false);

	/** Side mission from side_missions.json (story mode rules, no main-story progress). */
	UFUNCTION(BlueprintCallable, Category = "Arena|Story")
	bool StartSideMission(FName MissionId, bool bIgnoreAvailability = false);

	/** Retry after death: arena mode restarts, story mode restarts the current chapter's objectives. */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void RestartCurrentMode();

	/** Leaves story mode and shows the title menu. */
	UFUNCTION(BlueprintCallable, Category = "Arena|Story")
	void ReturnToTitle();

	UFUNCTION(BlueprintPure, Category = "Arena")
	EArenaPlayMode GetPlayMode() const { return PlayMode; }

	UFUNCTION(BlueprintPure, Category = "Arena|Story")
	AStoryDirector* GetStoryDirector() const { return StoryDirector.Get(); }

	/** Story save info for the menus. */
	int32 GetStoryChapterReached() const;
	bool IsStoryComplete() const;

	/** Random spawn point MinDist..MaxDist from the player, HeightAboveFloor up, inside the arena. */
	FVector FindSpawnLocation(float MinDist, float MaxDist, float HeightAboveFloor) const { return PickSpawnLocation(MinDist, MaxDist, HeightAboveFloor); }

	/** Story: enemy stats scale with this "wave" number (AEnemyBase::InitForWave). */
	void SetDifficultyWave(int32 InWave) { Wave = FMath::Max(1, InWave); }

	float GetArenaFloorZ() const { return ArenaFloorZ; }
	FVector GetArenaCenter() const { return ArenaCenter; }

	/** Removes enemies, projectiles, pickups and FX (no score). */
	void ClearCombat() { ClearArena(); }

	/** Director class spawned for story mode (Blueprint child to restyle cast / tunables).
	 *  Config: [/Script/IronManArena.ArenaGameMode] StoryDirectorClass=/Game/...BP_StoryDirector.BP_StoryDirector_C */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Arena|Story")
	TSoftClassPtr<AStoryDirector> StoryDirectorClass;

	/** Suit picked on the suit-select screen (persisted in the save game). */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetSelectedSuitIndex() const { return SelectedSuitIndex; }

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SetSelectedSuitIndex(int32 SuitIndex);

	/** True while a suit-up cinematic runs (waves, combo timer and enemies hold). */
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsCinematicPlaying() const;

	/** Called by the player when health hits 0. */
	void OnPlayerDied();

	/** Called by AEnemyBase when it dies (before it is destroyed). */
	void OnEnemyKilled(AEnemyBase* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void AddScore(int32 Points);

	/** Big centred message on the HUD. */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void ShowBanner(const FText& Title, const FText& Subtitle, float Seconds = 2.5f);

	/** Deferred-spawns an enemy, calls InitForWave, finishes spawning. */
	AEnemyBase* SpawnEnemyAt(TSubclassOf<AEnemyBase> EnemyClass, const FVector& Location);

	// ---------------------------------------------------------------- HUD getters
	UFUNCTION(BlueprintPure, Category = "Arena")
	EArenaGameState GetArenaState() const { return ArenaState; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetScore() const { return Score; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetHighScore() const { return HighScore; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetWave() const { return Wave; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetCombo() const { return Combo; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	float GetComboTimeFraction() const { return ComboWindow > 0.f ? ComboTimeLeft / ComboWindow : 0.f; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetKills() const { return Kills; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsNewHighScore() const { return bNewHighScore; }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetPointsToNextHulkbuster() const { return FMath::Max(0, NextHulkbusterAt - Score); }
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetHostilesRemaining() const;
	UFUNCTION(BlueprintPure, Category = "Arena")
	ABossTitan* GetActiveBoss() const;

	/** Current banner (empty title = none). */
	bool GetBanner(FText& OutTitle, FText& OutSubtitle, float& OutAlpha) const;

	// ---------------------------------------------------------------- enemy classes
	/** Swap for Blueprint children to use real meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Classes")
	TSubclassOf<AEnemyBase> DroneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Classes")
	TSubclassOf<AEnemyBase> TankClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Classes")
	TSubclassOf<AEnemyBase> BossClass;

	// ---------------------------------------------------------------- waves
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	int32 FirstWaveDrones = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	int32 DronesPerWave = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	int32 TanksFromWave = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	float TanksPerWave = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	int32 BossEvery = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	float SpawnGap = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	float BreakTime = 3.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	int32 MaxAlive = 9;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	float ClearRepair = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	float PreBossRepair = 40.f;

	/** Enemies spawn this far from the player (random in range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	FVector2D SpawnDistance = FVector2D(2500.f, 3800.f);
	/** Drone spawn height above the floor (random in range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	FVector2D DroneSpawnHeight = FVector2D(400.f, 1400.f);
	/** Keep spawns within this radius of the arena centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	float ArenaRadius = 6500.f;

	// ---------------------------------------------------------------- scoring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Scoring")
	int32 WaveBonus = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Scoring")
	int32 BossWaveMultiplier = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Scoring")
	float ComboWindow = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Scoring")
	int32 ComboMax = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Scoring")
	int32 HulkbusterScoreStep = 5000;

	// ---------------------------------------------------------------- misc
	/** Seconds between death and the game-over screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float GameOverDelay = 1.8f;

	/** Build floor / lights at runtime if the map has none (lets /Engine/Maps/Entry work). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	bool bBuildArenaIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	FString SaveSlotName = TEXT("IronManArena");

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleSuitUpFinished(ASuitUpCinematic* Cinematic, bool bShortVersion);
	void BuildArenaIfNeeded();
	void StartWave(int32 NewWave);
	void TickWaves(float DeltaSeconds);
	void SpawnFromQueue(EEnemyKind Kind);
	void EnterGameOver();
	void ClearArena();
	void LoadHighScore();
	void SaveHighScore();
	FVector PickSpawnLocation(float MinDist, float MaxDist, float HeightAboveFloor) const;
	AIronManCharacter* GetPlayer() const;

	void ResetRunState();
	AStoryDirector* EnsureStoryDirector();

	EArenaGameState ArenaState = EArenaGameState::Menu;
	EArenaPlayMode PlayMode = EArenaPlayMode::Arena;

	UPROPERTY(Transient)
	TObjectPtr<AStoryDirector> StoryDirector;

	int32 Score = 0;
	int32 HighScore = 0;
	int32 BestWave = 0;
	int32 Wave = 0;
	int32 Combo = 0;
	float ComboTimeLeft = 0.f;
	int32 Kills = 0;
	int32 NextHulkbusterAt = 5000;
	bool bNewHighScore = false;

	TArray<EEnemyKind> SpawnQueue;
	float SpawnTimer = 0.f;
	float BreakTimer = 0.f;
	bool bWaveHasBoss = false;
	bool bWaveActive = false;

	FText BannerTitle;
	FText BannerSubtitle;
	float BannerTimeLeft = 0.f;
	float BannerDuration = 1.f;

	FVector ArenaCenter = FVector::ZeroVector;
	float ArenaFloorZ = 0.f;

	FTimerHandle GameOverTimer;

	int32 SelectedSuitIndex = INDEX_NONE;
	FName LastSuitId;
	bool bTankAnnouncedThisWave = false;
	bool bSwarmAnnouncedThisWave = false;
	bool bHighScoreAnnounced = false;
};
