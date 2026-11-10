// AStoryAutopilot: plays story mode by itself through the real game paths, for headless tests.
//
// It presses the same keys a player would (skip shot, take off / land, Hulkbuster, summon ally),
// walks / flies the player onto beacons, damages the objective enemies until they die (the boss
// through all its phases) and types a hero name into the finale prompt. Everything else - the
// director, objectives, comms, reactor, suits, ally - runs unmodified. When the run ends it checks
// the director's FStoryChapterReport for every chapter against Content/Story/story.json:
//   reactor stage + mount, suit worn + unlocked, all objectives completed, every start / progress /
//   complete comms line fired, the ally summoned where allyAvailable is set (Vison, ch7 + ch9),
//   unlockAlly granted, the chapter-5 core going dark, all three Titan phases, intro / outro played.
//
// Used by:
//   - the IronManArena.Story.* automation tests (Private/Tests/StoryAutomationTests.cpp)
//   - the command line: -StoryAutoplay [-StoryAutoplayChapter=N (1-based)] [-StoryAutoplayQuit]
//     (works in the packaged game; prints "StoryTest: RESULT PASS|FAIL" and exits 0 / 1 with -StoryAutoplayQuit)
// The run uses its own save slot ("StoryAutopilot"), so a player's progress is not touched.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoryAutopilot.generated.h"

class AStoryDirector;
class AIronManCharacter;
class AArenaGameMode;

UCLASS(NotBlueprintable)
class IRONMANARENA_API AStoryAutopilot : public AActor
{
	GENERATED_BODY()

public:
	AStoryAutopilot();

	/**
	 * Spawns an autopilot that plays chapters FirstChapter..LastChapter (0-based, inclusive).
	 * bFullStory: start with the prologue and keep going through the finale (FirstChapter should be 0).
	 */
	static AStoryAutopilot* Launch(UWorld* World, int32 FirstChapter, int32 LastChapter, bool bFullStory, bool bQuitWhenDone);

	/** Plays one side mission (side_missions.json id) and checks objectives, comms and that the
	 *  reward (codex + upgrade / cosmetic) lands in the save. Ignores the availability window. */
	static AStoryAutopilot* LaunchSideMission(UWorld* World, FName MissionId, bool bQuitWhenDone);

	virtual void Tick(float DeltaSeconds) override;

	/** True while any autopilot (story or side mission) is alive in a world; UI such as the first-launch
	 *  creator uses it to stay out of the way of headless runs. */
	static bool IsAnyRunning();

	bool IsFinished() const { return bFinished; }
	bool Passed() const { return bFinished && Errors.Num() == 0; }
	const TArray<FString>& GetErrors() const { return Errors; }
	const TArray<FString>& GetSummary() const { return Summary; }

	/** Game-time speed-up while the autopilot runs. */
	UPROPERTY(EditAnywhere, Category = "Autopilot")
	float TimeDilation = 4.f;

	/** Fail if one chapter takes longer than this (game seconds). */
	UPROPERTY(EditAnywhere, Category = "Autopilot")
	float ChapterTimeout = 420.f;

	/** "energy" objectives are shortened to this (game seconds) for the run. */
	UPROPERTY(EditAnywhere, Category = "Autopilot")
	float EnergySeconds = 8.f;

	/** Name typed into the finale's hero-name prompt. */
	UPROPERTY(EditAnywhere, Category = "Autopilot")
	FString TestHeroName = TEXT("Iron Lad");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Begin();
	void Drive(float DeltaSeconds);
	void DriveObjective(float DeltaSeconds);
	void Finish(const FString& Reason);
	void Evaluate();
	void Fail(const FString& Message);
	void Restore();

	AStoryDirector* GetDirector() const;
	AIronManCharacter* GetPlayer() const;
	AArenaGameMode* GetGM() const;

	int32 FirstChapter = 0;
	int32 LastChapter = 0;
	FName SideMission;
	bool bFullStory = false;
	bool bQuitWhenDone = false;

	bool bStarted = false;
	bool bFinished = false;
	bool bRestored = false;
	bool bCountedLive = false;
	float StartDelay = 0.5f;
	float RunClock = 0.f;
	float ChapterClock = 0.f;
	float SkipTimer = 0.f;
	float DamageTimer = 0.f;
	int32 WatchedChapter = INDEX_NONE;
	int32 WatchedObjective = INDEX_NONE;
	bool bSummonTried = false;
	/** Mirror Lock: alternate between the hinted plate and the ring centre. */
	float LockStepTimer = 0.f;
	bool bLockOnPlate = false;
	bool bSawPrologue = false;
	int32 MaxBossPhase = 0;
	FString SavedSlot;
	float SavedEnergySeconds = 45.f;
	float SavedMinShot = 2.5f;

	TArray<FString> Errors;
	TArray<FString> Summary;
};
