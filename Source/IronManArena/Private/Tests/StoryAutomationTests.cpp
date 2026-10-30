// IronManArena.Story.* automation tests: the story autopilot plays each chapter (and the whole
// story) through the real game code and checks reactor stage, suit unlock, objectives, comms
// triggers, the Vison summon (ch7 / ch9), the ch5 ally unlock and the Titan's phases.
//
// They need a running game world, so run them in -game mode, e.g. headless:
//   UnrealEditor IronManArena.uproject -game -nullrhi -nosound -unattended \
//     -ExecCmds="Automation RunTests IronManArena.Story" -TestExit="Automation Test Queue Empty" -log
// (Scripts/run_story_tests.sh does exactly that.)
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StoryAutopilot.h"
#include "StoryLibrary.h"
#include "ArenaGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace StoryTests
{
	UWorld* FindGameWorld()
	{
		if (!GEngine) { return nullptr; }
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if ((Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE) && Ctx.World()
				&& Ctx.World()->GetAuthGameMode<AArenaGameMode>())
			{
				return Ctx.World();
			}
		}
		return nullptr;
	}

	/** Waits for the autopilot, then copies its verdict into the test. */
	class FWaitForAutopilot : public IAutomationLatentCommand
	{
	public:
		FWaitForAutopilot(FAutomationTestBase* InTest, TWeakObjectPtr<AStoryAutopilot> InPilot, double InTimeout)
			: Test(InTest), Pilot(InPilot), Timeout(InTimeout) {}

		virtual bool Update() override
		{
			AStoryAutopilot* P = Pilot.Get();
			if (!P) { Test->AddError(TEXT("autopilot was destroyed before it finished")); return true; }
			if (!P->IsFinished())
			{
				if (GetCurrentRunTime() > Timeout)
				{
					Test->AddError(FString::Printf(TEXT("autopilot still running after %.0f s (real time)"), Timeout));
					P->Destroy();
					return true;
				}
				return false;
			}
			for (const FString& Line : P->GetSummary()) { Test->AddInfo(Line); }
			for (const FString& E : P->GetErrors()) { Test->AddError(E); }
			P->Destroy();
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TWeakObjectPtr<AStoryAutopilot> Pilot;
		double Timeout;
	};

	bool Launch(FAutomationTestBase* Test, int32 First, int32 Last, bool bFull, double Timeout)
	{
		UWorld* World = FindGameWorld();
		if (!World)
		{
			Test->AddError(TEXT("no game world with AArenaGameMode - run the tests with -game (see Scripts/run_story_tests.sh)"));
			return false;
		}
		if (!UStoryLibrary::GetStory().IsValid()) { Test->AddError(TEXT("story.json did not load")); return false; }
		AStoryAutopilot* Pilot = AStoryAutopilot::Launch(World, First, Last, bFull, false);
		if (!Pilot) { Test->AddError(TEXT("could not spawn the autopilot")); return false; }
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForAutopilot(Test, Pilot, Timeout));
		return true;
	}
}

// ---- story data sanity (no world needed)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoryDataTest, "IronManArena.Story.Data",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStoryDataTest::RunTest(const FString& Parameters)
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	TestTrue(TEXT("story.json loaded"), Story.IsValid());
	TestEqual(TEXT("chapter count"), Story.Chapters.Num(), 9);
	for (const FStoryChapter& C : Story.Chapters)
	{
		TestTrue(FString::Printf(TEXT("%s has objectives"), *C.Id.ToString()), C.Objectives.Num() > 0);
		for (const FStoryObjective& O : C.Objectives)
		{
			TestTrue(FString::Printf(TEXT("%s objective type '%s' known"), *C.Id.ToString(), *O.TypeName), O.Type != EStoryObjectiveType::Unknown);
		}
	}
	return true;
}

// ---- one test per chapter
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FStoryChapterTest, "IronManArena.Story.Chapter",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
void FStoryChapterTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	for (int32 i = 0; i < Story.Chapters.Num(); ++i)
	{
		OutBeautifiedNames.Add(FString::Printf(TEXT("%d_%s"), i + 1, *Story.Chapters[i].Id.ToString()));
		OutTestCommands.Add(FString::FromInt(i));
	}
}
bool FStoryChapterTest::RunTest(const FString& Parameters)
{
	const int32 Index = FCString::Atoi(*Parameters);
	return StoryTests::Launch(this, Index, Index, false, 600.0);
}

// ---- prologue -> 9 chapters -> finale (hero-name prompt) in one go
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoryFullRunTest, "IronManArena.Story.FullRun",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStoryFullRunTest::RunTest(const FString& Parameters)
{
	return StoryTests::Launch(this, 0, 8, true, 1800.0);
}

#endif // WITH_DEV_AUTOMATION_TESTS
