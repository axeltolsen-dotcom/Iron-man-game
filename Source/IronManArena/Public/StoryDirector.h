// AStoryDirector: runs story mode from Content/Story/story.json (UStoryLibrary::GetStory()).
//
//   Prologue (Tony's snap, first run only)  ->  for each chapter:
//       reactor stage + suit unlock  ->  intro cutscene  ->  short suit-up  ->  objectives
//       (comms on start / 50% progress / complete)  ->  outro cutscene  ->  ally unlock, save
//   ->  Finale (hero-name prompt before the first {heroName} line, Morgan flies in Rescue)
//   ->  STORY COMPLETE, back to the title screen.
//
// Cutscenes: one ACineCameraActor framed from each shot's "camera" note (wide / close /
// low angle / aerial / crane / handheld / push-in / over-the-shoulder ...) around the cast,
// AStoryCharacterNPC actors (Manny / Quinn skeletal meshes by default, MetaHumans optional - see
// Docs/ADDING_METAHUMANS.md) and the letterbox overlay with speaker captions. Shot length =
// voice clip length (/Game/StoryAudio) or reading time, min MinShotSeconds.
// Space / LMB skips a shot, Esc / P skips the cutscene.
//
// Objectives map onto the arena systems (beacons, drones, tanks, waves, Titan, Hulkbuster,
// reactor). "energy" = survive EnergyObjectiveSeconds of combat without a POWER DEPLETED forced
// landing (a forced landing restarts the timer).
//
// The game mode spawns this actor (class configurable: AArenaGameMode::StoryDirectorClass).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoryTypes.h"
#include "StoryCharacterNPC.h"
#include "StoryDirector.generated.h"

class AArenaGameMode;
class AIronManCharacter;
class AEnemyBase;
class ACineCameraActor;
class UCinematicOverlayWidget;
class UAudioComponent;
class AStoryBeacon;
class AChapterSet;
struct FEnvironmentPreset;
class AStoryAlly;
class UStoryNamePromptWidget;

UENUM(BlueprintType)
enum class EStoryStep : uint8
{
	Idle,
	Cutscene,
	SuitUp,
	Objective,
	ObjectiveDone,
	NamePrompt,
	Complete
};

/**
 * What actually happened in one chapter of the current run (filled by the director as the
 * chapter plays; read by AStoryAutopilot / the IronManArena.Story automation tests).
 */
struct IRONMANARENA_API FStoryChapterReport
{
	bool bStarted = false;
	bool bIntroPlayed = false;
	bool bOutroPlayed = false;
	bool bFinished = false;
	/** Reactor state read back from the player's UReactorComponent after the chapter applied it. */
	bool bReactorApplied = false;
	EReactorStage ReactorStage = EReactorStage::Makeshift;
	EReactorMount ReactorMount = EReactorMount::Spine;
	bool bCoreDarkAtEnd = false;
	/** Suit the player wears in the chapter and whether the suit wheel allows it. */
	FName SuitWorn;
	bool bSuitAllowed = false;
	int32 ObjectivesStarted = 0;
	int32 ObjectivesCompleted = 0;
	/** Scripted comms lines handed to the comms AI, by trigger (start / progress / complete). */
	int32 CommsFired[3] = { 0, 0, 0 };
	int32 AllySummons = 0;
	/** UnlockAlly from story.json was added to the roster when the chapter finished. */
	bool bAllyUnlocked = false;
	int32 BossPhasesSeen = 0;
	int32 ForcedLandings = 0;
	int32 HeroPrompts = 0;
};

UCLASS(Blueprintable)
class IRONMANARENA_API AStoryDirector : public AActor
{
	GENERATED_BODY()

public:
	AStoryDirector();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Starts story mode at ChapterIndex. bPlayPrologue plays Tony's prologue first;
	 * bSkipIntro (retry after death) jumps straight to the chapter's first objective.
	 */
	void StartStory(int32 ChapterIndex, bool bPlayPrologue, bool bSkipIntro = false);

	/** Tears down cutscene / objective actors and restores the player (arena mode, title). */
	void StopStory();

	// ---- game-mode hooks
	void NotifyEnemyKilled(AEnemyBase* Enemy);
	void NotifyPlayerDied();

	// ---- HUD
	bool IsActive() const { return Step != EStoryStep::Idle && Step != EStoryStep::Complete; }
	bool IsInCutscene() const { return Step == EStoryStep::Cutscene || Step == EStoryStep::NamePrompt; }
	int32 GetChapterIndex() const { return ChapterIndex; }

	/** Objective panel: "CHAPTER 3 - Eyes in the Sky", objective text, progress ("2 / 4", "31 s"). */
	bool GetObjectiveInfo(FText& OutChapter, FText& OutObjective, FText& OutProgress) const;

	/** "G  SUMMON VISON" style hint; empty when no ally can be called. */
	FText GetAllyHint() const;

	/** World location of the current beacon (HUD marker), false if none. */
	bool GetBeaconLocation(FVector& Out) const;

	// ---- read-only state for the autopilot / automation tests
	EStoryStep GetStep() const { return Step; }
	int32 GetObjectiveIndex() const { return ObjectiveIndex; }
	bool IsInFinale() const { return bInFinale; }
	bool IsInPrologue() const { return bInPrologue; }
	/** Current objective (nullptr outside an objective). */
	const FStoryObjective* GetCurrentObjective() const;
	AStoryBeacon* GetBeacon() const { return Beacon; }
	AStoryAlly* GetActiveAlly() const { return Ally; }
	bool HasAlly(FName AllyId) const { return Allies.Contains(AllyId); }
	void GetObjectiveEnemies(TArray<AEnemyBase*>& Out) const;
	UStoryNamePromptWidget* GetHeroPrompt() const { return Prompt; }
	/** One entry per chapter of story.json, reset by StartStory. */
	const TArray<FStoryChapterReport>& GetChapterReports() const { return Reports; }
	bool WasFinaleReached() const { return bFinaleReached; }
	bool WasStoryCompleted() const { return bStoryCompleted; }
	/** Boss objectives: the director counts phase changes of the active Titan. */
	void NotifyBossPhase(int32 Phase);

	const FString& GetPlayerName() const { return PlayerName; }
	const FString& GetHeroName() const { return HeroName; }

	// ---- tunables
	/** Cast list: speaker -> body / mesh / anim. Defaults: Tony = Manny, Pepper / Morgan = Quinn,
	 *  Harley (PLAYER) = Manny 0.93, Vison = synthezoid, U / DUM-E = robot arms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Cast")
	TArray<FStoryCastMember> CastList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Cutscene")
	float MinShotSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Cutscene")
	float MaxShotSeconds = 14.f;

	/** Reading speed for lines without audio (characters / second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Cutscene")
	float ReadingCharsPerSecond = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Objectives")
	float GroundBeaconDistance = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Objectives")
	float FlightBeaconDistance = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Objectives")
	float FlightBeaconHeight = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Objectives")
	float EnergyObjectiveSeconds = 45.f;

	/** Enemies alive at once during kill / energy objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Objectives")
	int32 MaxObjectiveAlive = 5;

	/** Pause between objectives (lets the complete comms line play). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Objectives")
	float ObjectiveGapSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Ally")
	float AllyDuration = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Ally")
	float AllyCooldown = 45.f;

	/** Comms share of the player's own suit AI vs Morgan for generic combat chatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story|Comms")
	float MorganChatterShare = 0.5f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- flow
	void BeginChapter(int32 Index, bool bSkipIntro);

	/** Swaps the procedural chapter set + lighting/fog/post look. SetName "arena" = free-play look. */
	void ApplyEnvironment(FName SetName, const FEnvironmentPreset& Preset);
	void ClearEnvironment();

public:
	/** The current chapter set (tests). */
	AChapterSet* GetChapterSet() const { return ChapterSet; }
	FName GetEnvironmentId() const { return EnvironmentId; }
	FName GetLightingKey() const { return LightingKey; }

protected:
	void StartSuitUp();
	void StartObjective(int32 Index);
	void CompleteObjective();
	void FinishChapter();
	void StartFinale();
	void FinishStory();
	void ApplyReactor(const FStoryReactor& R);
	void ApplyChapterSuit(int32 Index, bool bAnnounce);
	void SaveProgress(int32 ReachedChapter, bool bComplete);
	void LoadProgress();

	// ---- objectives
	void TickObjective(float DeltaSeconds);
	void SetObjectiveProgress(float Fraction);
	void FireComms(EStoryCommsTrigger Trigger);
	void SpawnObjectiveEnemy(bool bTank);
	void StartObjectiveWave(int32 WaveNumber);
	int32 CountObjectiveAlive() const;
	void ClearHostiles();
	void HandleForcedLanding();
	void HandleHulkbusterActivated();
	void HandleSummonAlly();

	// ---- cutscenes
	void PlayCutscene(const TArray<FStoryShot>& InShots, const FText& Title, const FText& Subtitle, TFunction<void()> OnDone);
	void BeginShot(int32 Index);
	void EndCutscene();
	void SkipCutscene(bool bAll);
	void TickCutscene(float DeltaSeconds);
	void StageShot(const FStoryShot& Shot);
	void UpdateCamera(float Alpha);
	AStoryCharacterNPC* GetOrSpawnNpc(FName Speaker, bool bSuitedOverride);
	const FStoryCastMember* FindCast(FName Speaker) const;
	bool ShotUsesHumanPlayer(const FStoryShot& Shot) const;
	void ShowHeroPrompt();
	void SpeakLine(const FStoryLine& Line, FText& OutSpeaker, FText& OutText, FLinearColor& OutColor, float& OutDuration);

	AIronManCharacter* GetPlayer() const;
	AArenaGameMode* GetGM() const;
	FString Resolve(const FString& Text) const;

	FStoryChapterReport* CurrentReport();

	// ---- state
	TArray<FStoryChapterReport> Reports;
	bool bFinaleReached = false;
	bool bStoryCompleted = false;
	EStoryStep Step = EStoryStep::Idle;
	int32 ChapterIndex = 0;
	int32 ObjectiveIndex = 0;
	bool bInFinale = false;
	bool bInPrologue = false;
	FString PlayerName = TEXT("Harley");
	FString HeroName;
	TArray<FName> UnlockedSuits;
	TArray<FName> Allies;

	// objective
	float ObjectiveProgress = 0.f;
	bool bProgressCommsFired = false;
	int32 ObjectiveCount = 0;   // kills / waves / deployments achieved
	int32 ObjectiveSpawned = 0; // enemies spawned for kill objectives
	int32 WaveNumber = 0;
	float ObjectiveTimer = 0.f;
	float SpawnTimer = 0.f;
	float GapTimer = 0.f;
	float AllyCooldownLeft = 0.f;
	int32 ForcedLandings = 0;
	float BeaconStartDist = 1.f;
	int32 PendingDrones = 0;
	int32 PendingTanks = 0;
	float WaveBreak = 0.f;
	TArray<TWeakObjectPtr<AEnemyBase>> ObjectiveEnemies;
	FTimerHandle FinishTimer;
	FDelegateHandle ForcedLandingHandle;
	FDelegateHandle HulkbusterHandle;
	FDelegateHandle AllyHandle;

	UPROPERTY(Transient)
	TObjectPtr<AStoryBeacon> Beacon;

	UPROPERTY()
	TObjectPtr<AChapterSet> ChapterSet;

	FName EnvironmentId;
	/** lighting_presets.json key of the current look (NAME_None = C++ preset only). */
	FName LightingKey;

	UPROPERTY(Transient)
	TObjectPtr<AStoryAlly> Ally;

	// cutscene
	TArray<FStoryShot> Shots;
	TFunction<void()> OnCutsceneDone;
	int32 ShotIndex = 0;
	float ShotTime = 0.f;
	float ShotLength = 0.f;
	float SkipGuard = 0.f;
	FText CutTitle;
	FText CutSubtitle;
	FVector StageOrigin = FVector::ZeroVector;
	float StageYaw = 0.f;
	FVector ShotLookAt = FVector::ZeroVector;
	FString ShotCamera;
	bool bHeroPromptShown = false;

	// framing parsed from the shot's camera note
	float CamDist = 380.f;
	float CamHeight = 20.f;
	float CamYawOffset = 0.f;
	float CamFocal = 35.f;
	bool bCamHandheld = false;
	bool bCamPush = false;
	bool bCamOrbit = false;
	bool bCamCrane = false;

	UPROPERTY(Transient)
	TObjectPtr<ACineCameraActor> Cam;

	UPROPERTY(Transient)
	TObjectPtr<UCinematicOverlayWidget> Overlay;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudio;

	UPROPERTY(Transient)
	TObjectPtr<UStoryNamePromptWidget> Prompt;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AStoryCharacterNPC>> Npcs;

	UPROPERTY(Transient)
	TObjectPtr<AStoryCharacterNPC> SpeakingNpc;
};
