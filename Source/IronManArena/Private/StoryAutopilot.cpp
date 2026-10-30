#include "StoryAutopilot.h"
#include "ChapterSet.h"
#include "SuitRig.h"
#include "ArenaSaveGame.h"

#include "IronManArena.h"
#include "ArenaGameMode.h"
#include "ArenaPlayerController.h"
#include "IronManCharacter.h"
#include "ReactorComponent.h"
#include "StoryDirector.h"
#include "StoryLibrary.h"
#include "StoryBeacon.h"
#include "MirrorLock.h"
#include "StoryAlly.h"
#include "StoryNamePromptWidget.h"
#include "SuitTypes.h"
#include "EnemyBase.h"
#include "BossTitan.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

namespace
{
	const TCHAR* AutopilotSlot = TEXT("StoryAutopilot");

	const TCHAR* StageName(EReactorStage S)
	{
		switch (S)
		{
		case EReactorStage::Makeshift: return TEXT("makeshift");
		case EReactorStage::Unstable: return TEXT("unstable");
		case EReactorStage::Failed: return TEXT("failed");
		case EReactorStage::Upgraded: return TEXT("upgraded");
		default: return TEXT("?");
		}
	}

	const TCHAR* MountName(EReactorMount M) { return M == EReactorMount::Forearm ? TEXT("forearm") : TEXT("spine"); }
}

AStoryAutopilot::AStoryAutopilot()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

AStoryAutopilot* AStoryAutopilot::Launch(UWorld* World, int32 InFirst, int32 InLast, bool bInFullStory, bool bInQuit)
{
	if (!World) { return nullptr; }
	const FStoryBook& Story = UStoryLibrary::GetStory();
	if (!Story.IsValid())
	{
		UE_LOG(LogIronManArena, Error, TEXT("StoryTest: no story data - cannot run the autopilot"));
		return nullptr;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStoryAutopilot* A = World->SpawnActor<AStoryAutopilot>(AStoryAutopilot::StaticClass(), FTransform::Identity, SP);
	if (!A) { return nullptr; }
	A->FirstChapter = FMath::Clamp(InFirst, 0, Story.Chapters.Num() - 1);
	A->LastChapter = FMath::Clamp(InLast, A->FirstChapter, Story.Chapters.Num() - 1);
	A->bFullStory = bInFullStory;
	if (bInFullStory) { A->FirstChapter = 0; A->LastChapter = Story.Chapters.Num() - 1; }
	A->bQuitWhenDone = bInQuit;
	UE_LOG(LogIronManArena, Log, TEXT("StoryTest: autopilot launched for chapters %d..%d%s"), A->FirstChapter + 1, A->LastChapter + 1,
		bInFullStory ? TEXT(" (prologue + finale)") : TEXT(""));
	return A;
}

AStoryAutopilot* AStoryAutopilot::LaunchSideMission(UWorld* World, FName MissionId, bool bInQuit)
{
	if (!World || !UStoryLibrary::GetStory().FindSideMission(MissionId))
	{
		UE_LOG(LogIronManArena, Error, TEXT("StoryTest: unknown side mission '%s'"), *MissionId.ToString());
		return nullptr;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStoryAutopilot* A = World->SpawnActor<AStoryAutopilot>(AStoryAutopilot::StaticClass(), FTransform::Identity, SP);
	if (!A) { return nullptr; }
	A->SideMission = MissionId;
	A->bQuitWhenDone = bInQuit;
	UE_LOG(LogIronManArena, Log, TEXT("StoryTest: autopilot launched for side mission %s"), *MissionId.ToString());
	return A;
}

AStoryDirector* AStoryAutopilot::GetDirector() const
{
	const AArenaGameMode* GM = GetGM();
	return GM ? GM->GetStoryDirector() : nullptr;
}

AIronManCharacter* AStoryAutopilot::GetPlayer() const
{
	return Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

AArenaGameMode* AStoryAutopilot::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
}

void AStoryAutopilot::Fail(const FString& Message)
{
	Errors.Add(Message);
	UE_LOG(LogIronManArena, Error, TEXT("StoryTest: FAIL %s"), *Message);
}

void AStoryAutopilot::Begin()
{
	AArenaGameMode* GM = GetGM();
	AArenaPlayerController* PC = Cast<AArenaPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!GM || !PC || !GetPlayer())
	{
		Fail(TEXT("no arena game mode / player controller / player pawn in this world"));
		Finish(TEXT("setup"));
		return;
	}
	bStarted = true;
	SavedSlot = GM->SaveSlotName;
	GM->SaveSlotName = AutopilotSlot;
	if (UGameplayStatics::DoesSaveGameExist(AutopilotSlot, 0)) { UGameplayStatics::DeleteGameInSlot(AutopilotSlot, 0); }
	UGameplayStatics::SetGlobalTimeDilation(this, TimeDilation);

	if (!SideMission.IsNone())
	{
		PC->HideMenu(); // as UStoryMenuWidget does via MenuStartSideMission: unpause, drop the title screen
		if (!GM->StartSideMission(SideMission, /*bIgnoreAvailability*/ true))
		{
			Fail(FString::Printf(TEXT("side mission %s did not start"), *SideMission.ToString()));
			Finish(TEXT("setup"));
			return;
		}
	}
	else
	{
		PC->MenuStartStory(FirstChapter, bFullStory);
	}
	if (AStoryDirector* D = GetDirector())
	{
		SavedEnergySeconds = D->EnergyObjectiveSeconds;
		SavedMinShot = D->MinShotSeconds;
		D->EnergyObjectiveSeconds = EnergySeconds;
	}
	else
	{
		Fail(TEXT("the game mode did not create a story director"));
		Finish(TEXT("setup"));
	}
}

void AStoryAutopilot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) { return; }
	if (!bStarted)
	{
		StartDelay -= DeltaSeconds;
		if (StartDelay <= 0.f) { Begin(); }
		return;
	}
	Drive(DeltaSeconds);
}

void AStoryAutopilot::Drive(float DeltaSeconds)
{
	AStoryDirector* D = GetDirector();
	AIronManCharacter* Player = GetPlayer();
	if (!D || !Player) { Fail(TEXT("director or player vanished")); Finish(TEXT("lost")); return; }

	RunClock += DeltaSeconds;
	ChapterClock += DeltaSeconds;

	// Stay alive: this run tests the story flow, not the player's aim.
	if (Player->IsDead())
	{
		Fail(FString::Printf(TEXT("the player died in chapter %d"), D->GetChapterIndex() + 1));
		Finish(TEXT("death"));
		return;
	}
	Player->Heal(10000.f);
	if (!D->IsInPrologue()) { Player->AddEnergy(10000.f); }
	if (D->IsInPrologue()) { bSawPrologue = true; }

	// Chapter bookkeeping.
	if (D->GetChapterIndex() != WatchedChapter && !D->IsInPrologue())
	{
		if (WatchedChapter != INDEX_NONE)
		{
			UE_LOG(LogIronManArena, Log, TEXT("StoryTest: chapter %d -> %d after %.1f game s"), WatchedChapter + 1, D->GetChapterIndex() + 1, ChapterClock);
		}
		WatchedChapter = D->GetChapterIndex();
		WatchedObjective = INDEX_NONE;
		ChapterClock = 0.f;
	}

	// Done?
	const bool bPastLast = D->GetChapterIndex() > LastChapter;
	if (!SideMission.IsNone())
	{
		if (D->WasSideRewardGranted()) { Finish(TEXT("side mission complete")); return; }
	}
	else if (bFullStory)
	{
		if (D->WasStoryCompleted()) { Finish(TEXT("story complete")); return; }
	}
	else if (bPastLast || D->IsInFinale() || D->WasStoryCompleted())
	{
		Finish(TEXT("chapter range done"));
		return;
	}
	if (ChapterClock > ChapterTimeout)
	{
		const FStoryObjective* O = D->GetCurrentObjective();
		Fail(FString::Printf(TEXT("chapter %d timed out after %.0f game s (step %d, objective %d '%s')"), D->GetChapterIndex() + 1, ChapterClock,
			int32(D->GetStep()), D->GetObjectiveIndex() + 1, O ? *O->TypeName : TEXT("-")));
		Finish(TEXT("timeout"));
		return;
	}

	SkipTimer -= DeltaSeconds;
	switch (D->GetStep())
	{
	case EStoryStep::Cutscene:
		// Shot by shot (Space), so every shot is staged and the finale's name prompt comes up.
		if (SkipTimer <= 0.f) { Player->SimulateSkipKey(); SkipTimer = 0.45f; }
		break;
	case EStoryStep::NamePrompt:
		if (UStoryNamePromptWidget* Prompt = D->GetHeroPrompt())
		{
			UE_LOG(LogIronManArena, Log, TEXT("StoryTest: hero-name prompt shown, typing '%s'"), *TestHeroName);
			Prompt->SubmitName(TestHeroName);
		}
		break;
	case EStoryStep::SuitUp:
		if (Player->IsInCinematic() && SkipTimer <= 0.f) { Player->SimulateSkipKey(); SkipTimer = 0.3f; }
		break;
	case EStoryStep::Objective:
		DriveObjective(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AStoryAutopilot::DriveObjective(float DeltaSeconds)
{
	AStoryDirector* D = GetDirector();
	AIronManCharacter* Player = GetPlayer();
	const FStoryObjective* O = D ? D->GetCurrentObjective() : nullptr;
	if (!O || !Player) { return; }

	if (D->GetObjectiveIndex() != WatchedObjective)
	{
		WatchedObjective = D->GetObjectiveIndex();
		bSummonTried = false;
		MaxBossPhase = 0;
		UE_LOG(LogIronManArena, Log, TEXT("StoryTest: chapter %d objective %d: %s %s"), D->GetChapterIndex() + 1, WatchedObjective + 1, *O->TypeName, *O->Target);
	}
	if (Player->IsInCinematic())
	{
		// A suit swap / Hulkbuster cinematic in the middle of an objective.
		if (SkipTimer <= 0.f) { Player->SimulateSkipKey(); SkipTimer = 0.3f; }
		return;
	}

	// The ally key, once per objective, as soon as the HUD offers it.
	if (!O->AllyAvailable.IsNone() && D->HasAlly(O->AllyAvailable) && !D->GetActiveAlly() && !bSummonTried)
	{
		bSummonTried = true;
		Player->SimulateAllyKey();
		UE_LOG(LogIronManArena, Log, TEXT("StoryTest: pressed G (summon %s): %s"), *O->AllyAvailable.ToString(), D->GetActiveAlly() ? TEXT("ally phased in") : TEXT("no ally"));
	}

	switch (O->Type)
	{
	case EStoryObjectiveType::Ground:
	case EStoryObjectiveType::Flight:
	{
		const bool bAir = O->Type == EStoryObjectiveType::Flight;
		Player->SimulateFlightKey(bAir);
		if (AMirrorLock* Lock = D->GetMirrorLock())
		{
			// Solve it like a player: walk onto the hinted pillar's plate, step off, repeat.
			if (Player->IsFlying()) { break; }
			LockStepTimer -= DeltaSeconds;
			if (LockStepTimer > 0.f) { break; }
			LockStepTimer = 0.4f;
			const float Half = Player->GetCapsuleComponent() ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			const int32 Hint = Lock->GetHintPillar();
			bLockOnPlate = !bLockOnPlate && Hint != INDEX_NONE;
			const FVector Spot = bLockOnPlate ? Lock->GetPlateLocation(Hint) : Lock->GetActorLocation() + FVector(0.f, 0.f, 0.f);
			Player->SetActorLocation(Spot + FVector(0.f, 0.f, Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement()) { Move->Velocity = FVector::ZeroVector; }
			if (bLockOnPlate) { UE_LOG(LogIronManArena, Log, TEXT("StoryTest: mirror lock: step on pillar %d (%d moves left)"), Hint + 1, Lock->GetMovesLeft()); }
			break;
		}
		if (const AStoryBeacon* Beacon = D->GetBeacon())
		{
			if (bAir != Player->IsFlying()) { break; }
			const float Half = Player->GetCapsuleComponent() ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			const FVector Target = Beacon->GetActorLocation() + FVector(0.f, 0.f, bAir ? 0.f : Half + 5.f);
			Player->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement()) { Move->Velocity = FVector::ZeroVector; }
		}
		break;
	}
	case EStoryObjectiveType::Hulkbuster:
		if (!Player->IsHulkbusterActive() && Player->GetHulkbusterCharges() > 0) { Player->SimulateHulkbusterKey(); }
		[[fallthrough]];
	default:
	{
		// Combat objectives: damage what the objective spawned until it dies.
		DamageTimer -= DeltaSeconds;
		if (DamageTimer > 0.f) { break; }
		DamageTimer = 0.25f;
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		TArray<AEnemyBase*> Enemies;
		D->GetObjectiveEnemies(Enemies);
		for (AEnemyBase* E : Enemies)
		{
			if (!E || E->IsDead()) { continue; }
			if (const ABossTitan* Boss = Cast<ABossTitan>(E))
			{
				MaxBossPhase = FMath::Max(MaxBossPhase, Boss->GetPhase());
				UGameplayStatics::ApplyDamage(E, E->GetMaxHealth() * 0.12f, PC, Player, nullptr);
			}
			else
			{
				UGameplayStatics::ApplyDamage(E, E->GetMaxHealth() * 4.f + 1000.f, PC, Player, nullptr);
			}
		}
		break;
	}
	}
}

void AStoryAutopilot::Finish(const FString& Reason)
{
	if (bFinished) { return; }
	UE_LOG(LogIronManArena, Log, TEXT("StoryTest: run ended (%s) after %.1f game s"), *Reason, RunClock);
	if (bStarted) { Evaluate(); }
	bFinished = true;
	Restore();

	if (Errors.Num() == 0)
	{
		if (!SideMission.IsNone()) { UE_LOG(LogIronManArena, Log, TEXT("StoryTest: RESULT PASS (side mission %s)"), *SideMission.ToString()); }
		else { UE_LOG(LogIronManArena, Log, TEXT("StoryTest: RESULT PASS (chapters %d..%d%s)"), FirstChapter + 1, LastChapter + 1, bFullStory ? TEXT(", prologue + finale") : TEXT("")); }
	}
	else
	{
		UE_LOG(LogIronManArena, Error, TEXT("StoryTest: RESULT FAIL (%d problems)"), Errors.Num());
	}
	if (bQuitWhenDone)
	{
		FPlatformMisc::RequestExitWithStatus(false, Errors.Num() == 0 ? 0 : 1);
	}
}

void AStoryAutopilot::Restore()
{
	if (bRestored) { return; }
	bRestored = true;
	if (GetWorld()) { UGameplayStatics::SetGlobalTimeDilation(this, 1.f); }
	AArenaGameMode* GM = GetGM();
	if (AStoryDirector* D = GetDirector())
	{
		D->EnergyObjectiveSeconds = SavedEnergySeconds;
		D->MinShotSeconds = SavedMinShot;
	}
	if (GM && bStarted)
	{
		GM->ReturnToTitle();
		GM->SaveSlotName = SavedSlot;
		if (UGameplayStatics::DoesSaveGameExist(AutopilotSlot, 0)) { UGameplayStatics::DeleteGameInSlot(AutopilotSlot, 0); }
	}
}

void AStoryAutopilot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Restore();
	Super::EndPlay(EndPlayReason);
}

void AStoryAutopilot::Evaluate()
{
	const AStoryDirector* D = GetDirector();
	if (!D) { Fail(TEXT("no director to evaluate")); return; }
	const bool bSide = !SideMission.IsNone();
	const FStoryBook& Story = bSide ? D->GetBook() : UStoryLibrary::GetStory();
	if (bSide)
	{
		if (!D->IsSideMission() || D->GetSideMissionId() != SideMission) { Fail(TEXT("director is not running the side mission")); return; }
		if (!D->WasSideRewardGranted()) { Fail(TEXT("side mission reward was not granted")); }
		const FStoryChapter& M = Story.Chapters[0];
		const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(AutopilotSlot);
		if (!Save) { Fail(TEXT("no save to check the reward")); }
		else
		{
			if (!Save->SideMissionsDone.Contains(M.Id)) { Fail(TEXT("save: mission not marked done")); }
			if (!M.RewardCodex.IsNone() && !Save->CodexGranted.Contains(M.RewardCodex)) { Fail(TEXT("save: codex reward missing")); }
			if (!M.RewardUpgrade.IsNone() && !Save->HasUpgrade(M.RewardUpgrade)) { Fail(TEXT("save: upgrade reward missing")); }
			if (!M.RewardCosmetic.IsNone() && !Save->Cosmetics.Contains(M.RewardCosmetic)) { Fail(TEXT("save: cosmetic reward missing")); }
			if (Save->StoryChapterReached != 0) { Fail(FString::Printf(TEXT("side mission moved main progress to %d"), Save->StoryChapterReached)); }
			const FString Line = FString::Printf(TEXT("side %s reward: codex %s upgrade %s cosmetic %s (saved)"), *M.Id.ToString(),
				*M.RewardCodex.ToString(), *M.RewardUpgrade.ToString(), *M.RewardCosmetic.ToString());
			Summary.Add(Line);
			UE_LOG(LogIronManArena, Log, TEXT("StoryTest: %s"), *Line);
		}
		if (const AIronManCharacter* P = GetPlayer())
		{
			const UReactorComponent* R = P->GetReactor();
			if (M.RewardUpgrade == ArenaRewards::CapacitorBank() && R && R->GetPermanentCapacityBonus() <= 0.f) { Fail(TEXT("capacitor bank not applied to the reactor")); }
			if (M.RewardUpgrade == ArenaRewards::HeatSinkFins() && R && R->GetPermanentCoolingBonus() <= 0.f) { Fail(TEXT("heat-sink fins not applied to the reactor")); }
			if (M.RewardCosmetic == ArenaRewards::WhiteTrim() && !SuitRig::IsWhiteTrim()) { Fail(TEXT("white trim not applied")); }
			if (M.RewardCosmetic == ArenaRewards::PaleBlueTrim() && !SuitRig::IsPaleBlueTrim()) { Fail(TEXT("pale blue trim not applied")); }
			if (M.RewardCosmetic == ArenaRewards::PodLighting() && !AChapterSet::bHallPodLighting) { Fail(TEXT("pod lighting flag not set")); }
		}
		FirstChapter = LastChapter = 0;
	}
	const TArray<FStoryChapterReport>& Reports = D->GetChapterReports();
	if (Reports.Num() != Story.Chapters.Num())
	{
		Fail(FString::Printf(TEXT("director has %d chapter reports, story.json has %d chapters"), Reports.Num(), Story.Chapters.Num()));
		return;
	}

	for (int32 c = FirstChapter; c <= LastChapter; ++c)
	{
		const FStoryChapter& C = Story.Chapters[c];
		const FStoryChapterReport& R = Reports[c];
		const int32 ErrorsBefore = Errors.Num();
		const FString Tag = FString::Printf(TEXT("ch%d %s"), c + 1, *C.Id.ToString());

		if (!R.bStarted) { Fail(Tag + TEXT(": never started")); continue; }
		if (!R.bFinished) { Fail(Tag + TEXT(": did not finish")); }

		// Reactor.
		if (!R.bReactorApplied) { Fail(Tag + TEXT(": reactor stage was not applied")); }
		else if (R.ReactorStage != C.Reactor.Stage || R.ReactorMount != C.Reactor.Mount)
		{
			Fail(FString::Printf(TEXT("%s: reactor %s/%s, story.json says %s/%s"), *Tag, StageName(R.ReactorStage), MountName(R.ReactorMount),
				StageName(C.Reactor.Stage), MountName(C.Reactor.Mount)));
		}
		if (C.Reactor.Stage == EReactorStage::Failed && R.bFinished && !R.bCoreDarkAtEnd) { Fail(Tag + TEXT(": failed reactor did not go dark at the end")); }

		// Suit.
		if (!C.UnlockSuit.IsNone())
		{
			if (USuitLibrary::FindSuitIndex(C.UnlockSuit) == INDEX_NONE) { Fail(Tag + FString::Printf(TEXT(": unlockSuit '%s' is not in suits.json"), *C.UnlockSuit.ToString())); }
			else if (R.SuitWorn != C.UnlockSuit) { Fail(Tag + FString::Printf(TEXT(": wore '%s', expected '%s'"), *R.SuitWorn.ToString(), *C.UnlockSuit.ToString())); }
			else if (!R.bSuitAllowed) { Fail(Tag + TEXT(": chapter suit is not unlocked on the suit wheel")); }
		}

		// Objectives + comms.
		int32 Expected[3] = { 0, 0, 0 };
		bool bAllyObjective = false;
		bool bBoss = false;
		for (const FStoryObjective& O : C.Objectives)
		{
			for (const FStoryComms& Comms : O.Comms) { ++Expected[FMath::Clamp(int32(Comms.Trigger), 0, 2)]; }
			bAllyObjective |= !O.AllyAvailable.IsNone();
			bBoss |= O.Type == EStoryObjectiveType::Boss;
		}
		if (R.ObjectivesCompleted != C.Objectives.Num())
		{
			Fail(FString::Printf(TEXT("%s: %d / %d objectives completed"), *Tag, R.ObjectivesCompleted, C.Objectives.Num()));
		}
		static const TCHAR* TriggerNames[3] = { TEXT("start"), TEXT("progress"), TEXT("complete") };
		for (int32 t = 0; t < 3; ++t)
		{
			if (R.CommsFired[t] != Expected[t])
			{
				Fail(FString::Printf(TEXT("%s: %d / %d '%s' comms lines fired"), *Tag, R.CommsFired[t], Expected[t], TriggerNames[t]));
			}
		}

		// Ally.
		if (bAllyObjective && R.AllySummons < 1) { Fail(Tag + TEXT(": allyAvailable is set but the ally was never summoned")); }
		const bool bMirrorLock = C.Objectives.ContainsByPredicate([](const FStoryObjective& Obj)
			{ return Obj.Type == EStoryObjectiveType::Ground && Obj.Target.Contains(TEXT("mirror lock"), ESearchCase::IgnoreCase); });
		if (bMirrorLock && R.MirrorLocksSolved < 1) { Fail(Tag + TEXT(": the Mirror Lock was never solved")); }
		if (!C.UnlockAlly.IsNone() && R.bFinished && !R.bAllyUnlocked) { Fail(Tag + FString::Printf(TEXT(": unlockAlly '%s' was not granted"), *C.UnlockAlly.ToString())); }

		// Boss phases.
		if (bBoss && R.BossPhasesSeen < 3) { Fail(FString::Printf(TEXT("%s: boss reached phase %d, expected 3"), *Tag, R.BossPhasesSeen)); }

		// Cutscenes.
		if (C.Intro.Num() > 0 && !R.bIntroPlayed) { Fail(Tag + TEXT(": intro cutscene skipped")); }
		if (C.Outro.Num() > 0 && R.bFinished && !R.bOutroPlayed) { Fail(Tag + TEXT(": outro cutscene skipped")); }

		const FString Line = FString::Printf(TEXT("ch%d %-18s %s  reactor %s/%s  suit %s  objectives %d/%d  comms %d/%d/%d  ally summons %d%s%s"),
			c + 1, *C.Id.ToString(), Errors.Num() == ErrorsBefore ? TEXT("PASS") : TEXT("FAIL"), StageName(R.ReactorStage), MountName(R.ReactorMount),
			*R.SuitWorn.ToString(), R.ObjectivesCompleted, C.Objectives.Num(), R.CommsFired[0], R.CommsFired[1], R.CommsFired[2], R.AllySummons,
			bBoss ? *FString::Printf(TEXT("  boss phases %d"), R.BossPhasesSeen) : TEXT(""),
			C.UnlockAlly.IsNone() ? TEXT("") : *FString::Printf(TEXT("  unlocked %s"), *C.UnlockAlly.ToString()));
		Summary.Add(Line);
		UE_LOG(LogIronManArena, Log, TEXT("StoryTest: %s"), *Line);
	}

	if (bFullStory)
	{
		if (Story.Prologue.Num() > 0 && !bSawPrologue) { Fail(TEXT("prologue did not play")); }
		if (!D->WasFinaleReached()) { Fail(TEXT("finale not reached")); }
		if (!D->WasStoryCompleted()) { Fail(TEXT("story not completed")); }
		if (Story.Finale.bPromptHeroName)
		{
			if (Reports.Last().HeroPrompts < 1) { Fail(TEXT("finale hero-name prompt never appeared")); }
			else if (D->GetHeroName() != TestHeroName) { Fail(FString::Printf(TEXT("hero name is '%s', typed '%s'"), *D->GetHeroName(), *TestHeroName)); }
		}
		const FString Line = FString::Printf(TEXT("prologue %s, finale %s, hero name '%s', complete %s"), bSawPrologue ? TEXT("played") : TEXT("-"),
			D->WasFinaleReached() ? TEXT("played") : TEXT("-"), *D->GetHeroName(), D->WasStoryCompleted() ? TEXT("yes") : TEXT("no"));
		Summary.Add(Line);
		UE_LOG(LogIronManArena, Log, TEXT("StoryTest: %s"), *Line);
	}
}
