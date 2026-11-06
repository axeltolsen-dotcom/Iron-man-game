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
#include "ContribLighting.h"
#include "ArenaSaveGame.h"
#include "SuitArmorKit.h"
#include "CityLayout.h"
#include "SuitTypes.h"
#include "MirrorLock.h"
#include "ArenaFeel.h"
#include "Engine/Scene.h"

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

// ---- side missions + codex data (Friday's side_missions.json / codex.json) and their unlock rules
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStorySideDataTest, "IronManArena.Story.SideData",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStorySideDataTest::RunTest(const FString& Parameters)
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	TestEqual(TEXT("side mission count"), Story.SideMissions.Num(), 6);
	TestEqual(TEXT("codex entry count"), Story.Codex.Num(), 45);
	const TArray<FName> Rewards = { ArenaRewards::CapacitorBank(), ArenaRewards::HeatSinkFins(), ArenaRewards::WhiteTrim(),
		ArenaRewards::PlaidDecal(), ArenaRewards::PodLighting(), ArenaRewards::PaleBlueTrim() };
	TArray<FName> Seen;
	for (const FStoryChapter& M : Story.SideMissions)
	{
		const FString Tag = M.Id.ToString();
		TestTrue(Tag + TEXT(" is flagged side"), M.bSideMission);
		TestTrue(Tag + TEXT(" has objectives"), M.Objectives.Num() > 0);
		for (const FStoryObjective& O : M.Objectives)
		{
			TestTrue(FString::Printf(TEXT("%s objective type '%s' known"), *Tag, *O.TypeName), O.Type != EStoryObjectiveType::Unknown);
		}
		const int32 From = UStoryLibrary::ChapterOrder(Story, M.AvailableFrom);
		const int32 Until = UStoryLibrary::ChapterOrder(Story, M.AvailableUntil);
		TestTrue(Tag + TEXT(" window is a real chapter range"), From >= 0 && From <= Until && Until < Story.Chapters.Num());
		if (From >= 0 && From <= Until && Until < Story.Chapters.Num())
		{
			TestFalse(Tag + TEXT(" locked before its window"), From > 0 && UStoryLibrary::IsSideMissionAvailable(Story, M, From - 1, false));
			TestTrue(Tag + TEXT(" open at the window start"), UStoryLibrary::IsSideMissionAvailable(Story, M, From, false));
			TestTrue(Tag + TEXT(" open at the window end"), UStoryLibrary::IsSideMissionAvailable(Story, M, Until, false));
			if (Until + 1 < Story.Chapters.Num())
			{
				TestFalse(Tag + TEXT(" closed after its window"), UStoryLibrary::IsSideMissionAvailable(Story, M, Until + 1, false));
			}
			TestTrue(Tag + TEXT(" open after the story is complete"), UStoryLibrary::IsSideMissionAvailable(Story, M, Until + 1, true));
		}
		if (!M.RewardCodex.IsNone())
		{
			const bool bFound = Story.Codex.ContainsByPredicate([&M](const FStoryCodexEntry& E) { return E.Id == M.RewardCodex; });
			TestTrue(Tag + TEXT(" codex reward exists"), bFound);
		}
		const FName Reward = M.RewardUpgrade.IsNone() ? M.RewardCosmetic : M.RewardUpgrade;
		TestTrue(Tag + TEXT(" reward is a known upgrade / cosmetic"), Rewards.Contains(Reward));
		Seen.AddUnique(Reward);
	}
	TestEqual(TEXT("all six rewards are handed out"), Seen.Num(), Rewards.Num());

	// Codex unlocks: nothing before the story, everything chapter-gated by the end, side entries only by grant.
	int32 AtStart = 0, AtEnd = 0, GrantOnly = 0, GrantOnlyAtEnd = 0, Granted = 0;
	const TArray<FName> None;
	for (const FStoryCodexEntry& E : Story.Codex)
	{
		TestFalse(E.Id.ToString() + TEXT(" has a body"), E.Body.IsEmpty());
		AtStart += UStoryLibrary::IsCodexUnlocked(Story, E, 0, false, false, None) ? 1 : 0;
		const bool bEnd = UStoryLibrary::IsCodexUnlocked(Story, E, Story.Chapters.Num(), true, true, None);
		AtEnd += bEnd ? 1 : 0;
		if (!E.UnlockedBy.IsNone())
		{
			++GrantOnly;
			GrantOnlyAtEnd += bEnd ? 1 : 0;
			const TArray<FName> G = { E.Id };
			Granted += UStoryLibrary::IsCodexUnlocked(Story, E, 0, false, false, G) ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("codex: %d unlocked at start, %d at story end, %d side-mission-only (%d unlock when granted)"), AtStart, AtEnd, GrantOnly, Granted));
	TestTrue(TEXT("codex: some entries locked at the start"), AtStart < Story.Codex.Num());
	TestEqual(TEXT("codex: everything but side-mission entries unlocked at story end"), AtEnd, Story.Codex.Num() - GrantOnly + GrantOnlyAtEnd);
	TestEqual(TEXT("codex: side-mission entries stay locked without the grant"), GrantOnlyAtEnd, 0);
	TestEqual(TEXT("codex: granting unlocks the side-mission entries"), Granted, GrantOnly);
	return true;
}

// ---- play each side mission start to finish (autopilot) and check its reward lands in the save
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FStorySideMissionTest, "IronManArena.Story.SideMission",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
void FStorySideMissionTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	for (int32 i = 0; i < Story.SideMissions.Num(); ++i)
	{
		OutBeautifiedNames.Add(FString::Printf(TEXT("%d_%s"), i + 1, *Story.SideMissions[i].Id.ToString()));
		OutTestCommands.Add(Story.SideMissions[i].Id.ToString());
	}
}
bool FStorySideMissionTest::RunTest(const FString& Parameters)
{
	UWorld* World = StoryTests::FindGameWorld();
	if (!World) { AddError(TEXT("no game world with AArenaGameMode - run with -game")); return false; }
	AStoryAutopilot* Pilot = AStoryAutopilot::LaunchSideMission(World, FName(*Parameters), false);
	if (!Pilot) { AddError(TEXT("could not spawn the side-mission autopilot")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(StoryTests::FWaitForAutopilot(this, Pilot, 600.0));
	return true;
}

// ---- Jocasta's Mirror Lock (lights-out on a ring of six): solvable starts, hints, pulses
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMirrorLockTest, "IronManArena.Story.MirrorLock",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMirrorLockTest::RunTest(const FString& Parameters)
{
	using P = FMirrorLockPuzzle;
	for (int32 i = 0; i < P::NumPillars; ++i)
	{
		const uint8 M = P::MoveMask(i);
		int32 Bits = 0;
		for (int32 b = 0; b < P::NumPillars; ++b) { Bits += (M >> b) & 1; }
		TestEqual(FString::Printf(TEXT("pillar %d flips itself and two neighbours"), i + 1), Bits, 3);
		TestTrue(FString::Printf(TEXT("pillar %d flips its ring neighbours"), i + 1),
			((M >> ((i + 1) % 6)) & 1) && ((M >> ((i + 5) % 6)) & 1) && ((M >> i) & 1));
	}
	const TArray<uint8> Starts = P::ShippedStarts(4);
	AddInfo(FString::Printf(TEXT("%d shipped starting states"), Starts.Num()));
	TestTrue(TEXT("there are shipped starting states"), Starts.Num() >= 8);
	for (const uint8 S : Starts)
	{
		const int32 Need = P::MinMoves(S);
		TestTrue(FString::Printf(TEXT("start 0x%02x solvable in 1..4 moves"), S), Need >= 1 && Need <= 4);
		// Following the hint always solves it in exactly the shortest number of moves.
		P Lock; Lock.Blue = S;
		int32 Steps = 0;
		while (!Lock.IsSolved() && Steps < 8)
		{
			const int32 H = Lock.HintPillar();
			if (H == INDEX_NONE) { break; }
			const int32 Before = P::MinMoves(Lock.Blue);
			Lock.Rotate(H);
			TestEqual(FString::Printf(TEXT("start 0x%02x: hint %d is a correct next move"), S, H + 1), P::MinMoves(Lock.Blue), Before - 1);
			++Steps;
		}
		TestTrue(FString::Printf(TEXT("start 0x%02x: hints solve it"), S), Lock.IsSolved());
		TestEqual(FString::Printf(TEXT("start 0x%02x: hint path is shortest"), S), Steps, Need);
		// A pulse (a rotation) never makes the lock unsolvable.
		for (int32 i = 0; i < P::NumPillars; ++i)
		{
			TestTrue(FString::Printf(TEXT("start 0x%02x: pulse on %d stays solvable"), S, i + 1), P::MinMoves(S ^ P::MoveMask(i)) >= 0);
		}
	}
	P Solved;
	TestTrue(TEXT("default lock is solved"), Solved.IsSolved());
	TestEqual(TEXT("no hint when solved"), Solved.HintPillar(), int32(INDEX_NONE));
	return true;
}

// ---- Vison's lighting presets: 11 looks, every story environment maps onto one
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLightingPresetsTest, "IronManArena.Story.LightingPresets",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLightingPresetsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("lighting_presets.json preset count"), ContribLighting::GetNumPresets(), 11);
	TestEqual(TEXT("postprocess_realism.json presets merged"), ContribLighting::GetNumRealismPresets(), 11);
	TestEqual(TEXT("free play reuses the city look"), ContribLighting::KeyForEnvironment(FName(TEXT("arena"))), FName(TEXT("city")));
	const TCHAR* Env[] = { TEXT("workshop"), TEXT("workshop_night"), TEXT("rooftops_dusk"), TEXT("rooftops_storm"), TEXT("city_day"),
		TEXT("city_night"), TEXT("city_overcast"), TEXT("ruins"), TEXT("battlefield"), TEXT("finale_dawn") };
	for (const TCHAR* Id : Env)
	{
		const FName Key = ContribLighting::PresetKeyFor(FName(Id));
		TestTrue(FString::Printf(TEXT("%s maps to a preset (%s)"), Id, *Key.ToString()), !Key.IsNone() && ContribLighting::HasPreset(Key));
	}
	TestTrue(TEXT("free-play arena keeps the project look"), ContribLighting::PresetKeyFor(FName(TEXT("arena"))).IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLayoutTest, "IronManArena.Story.CityLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCityLayoutTest::RunTest(const FString& Parameters)
{
	int32 Buildings = 0, Roads = 0, Blocks = 0, Spawns = 0;
	ACityLayout::GetLayoutStats(Buildings, Roads, Blocks, Spawns);
	AddInfo(FString::Printf(TEXT("city_layout.json: %d buildings, %d roads, %d blocks, %d spawns"), Buildings, Roads, Blocks, Spawns));
	TestEqual(TEXT("buildings"), Buildings, 2738);
	TestEqual(TEXT("roads"), Roads, 404);
	TestEqual(TEXT("blocks"), Blocks, 196);
	TestTrue(TEXT("spawn points"), Spawns > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmorKitTest, "IronManArena.Story.ArmorKit",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArmorKitTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("armor_kit.json loaded"), SuitArmorKit::IsLoaded());
	TestEqual(TEXT("armor_kit base plates"), SuitArmorKit::GetNumBasePlates(), 73);
	int32 MinPlates = MAX_int32, MaxPlates = 0;
	for (int32 i = 0; i < USuitLibrary::GetNumSuits(); ++i)
	{
		const int32 N = SuitArmorKit::CountPlates(USuitLibrary::GetSuit(i));
		MinPlates = FMath::Min(MinPlates, N);
		MaxPlates = FMath::Max(MaxPlates, N);
	}
	AddInfo(FString::Printf(TEXT("Resolved plates per suit: %d..%d over %d suits"), MinPlates, MaxPlates, USuitLibrary::GetNumSuits()));
	TestTrue(TEXT("every suit resolves to a full plate set (mirrored)"), MinPlates >= 80);
	const int32 Mk3 = USuitLibrary::FindSuitIndex(FName(TEXT("mark-iii")));
	if (TestTrue(TEXT("mark-iii exists"), Mk3 != INDEX_NONE))
	{
		const FSuitDef& S = USuitLibrary::GetSuit(Mk3);
		const TCHAR* Ids[] = { TEXT("chest.pec_l"), TEXT("chest.pec_r"), TEXT("helmet.faceplate"), TEXT("helmet.shell"), TEXT("neck.lame_lower"), TEXT("chest.reactor_housing") };
		for (const TCHAR* Id : Ids)
		{
			const int32 Tris = SuitArmorKit::CountPlateTriangles(S, Id);
			AddInfo(FString::Printf(TEXT("%s: %d triangles"), Id, Tris));
			TestTrue(FString::Printf(TEXT("%s builds a mesh"), Id), Tris > 8);
		}
	}
	return true;
}

// Vison camera layer merge: shake data + one shake source + the photoreal grade limits.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraRealismTest, "IronManArena.Camera.Realism",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCameraRealismTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("supersonic shake is cutscene-only"), UArenaFeelSubsystem::IsCutsceneOnlyShake(TEXT("supersonic_flight")));
	TestTrue(TEXT("handheld shake is cutscene-only"), UArenaFeelSubsystem::IsCutsceneOnlyShake(TEXT("handheld_cutscene")));
	TestFalse(TEXT("landing shake is gameplay"), UArenaFeelSubsystem::IsCutsceneOnlyShake(TEXT("superhero_landing")));

	// Grade limits: CA 0 in gameplay, subtle (<= 0.25) in cutscenes; flare/grain/bloom inside Vison's camera rules.
	FPostProcessSettings G;
	G.bOverride_SceneFringeIntensity = true; G.SceneFringeIntensity = 0.3f;
	G.bOverride_FilmGrainIntensity = true; G.FilmGrainIntensity = 0.2f;
	G.bOverride_LensFlareIntensity = true; G.LensFlareIntensity = 1.f;
	G.bOverride_BloomIntensity = true; G.BloomIntensity = 1.f;
	FPostProcessSettings C = G;
	ContribLighting::ApplyPhotorealGrade(G, false);
	ContribLighting::ApplyPhotorealGrade(C, true);
	if (ContribLighting::IsPhotorealGradeEnabled())
	{
		TestEqual(TEXT("gameplay CA"), G.SceneFringeIntensity, 0.f);
		TestEqual(TEXT("cutscene CA"), C.SceneFringeIntensity, 0.25f);
		TestTrue(TEXT("gameplay grain <= 0.05"), G.FilmGrainIntensity <= 0.05f + 1e-4f);
		TestTrue(TEXT("gameplay flare <= 0.2"), G.LensFlareIntensity <= 0.2f + 1e-4f);
		TestTrue(TEXT("gameplay bloom <= 0.8"), G.BloomIntensity <= 0.8f + 1e-4f);
		TestTrue(TEXT("cutscene grain <= 0.08"), C.FilmGrainIntensity <= 0.08f + 1e-4f);
	}

	UWorld* World = StoryTests::FindGameWorld();
	UArenaFeelSubsystem* Feel = World ? UArenaFeelSubsystem::Get(World) : nullptr;
	if (!Feel) { AddWarning(TEXT("no game world: shake source checks skipped")); return true; }
	TestEqual(TEXT("camera_shake.json profiles"), Feel->NumShakeProfiles(), 10);
	const bool bWasCine = Feel->IsCinematicCamera();
	Feel->SetCinematicCamera(false);
	TestFalse(TEXT("supersonic never plays in gameplay"), Feel->PlayShake(TEXT("supersonic_flight")));
	TestTrue(TEXT("landing shake plays"), Feel->PlayShake(TEXT("superhero_landing")));
	TestEqual(TEXT("landing is the active profile"), Feel->GetActiveShake(), FName(TEXT("superhero_landing")));
	TestTrue(TEXT("landing adds trauma"), Feel->GetTrauma() > 0.5f);
	Feel->SetCinematicCamera(true);
	FRotator R; FVector L;
	Feel->GetShake(R, L);
	TestTrue(TEXT("gameplay shake muted while a cutscene owns the camera"), R.IsNearlyZero() && L.IsNearlyZero());
	Feel->SetCinematicCamera(bWasCine);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
