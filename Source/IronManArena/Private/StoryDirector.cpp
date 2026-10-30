#include "StoryDirector.h"
#include "ContribLighting.h"
#include "Sound/SoundClass.h"

#include "IronManArena.h"
#include "ArenaGameMode.h"
#include "ArenaPlayerController.h"
#include "ArenaSaveGame.h"
#include "ArenaGameUserSettings.h"
#include "ArenaBlastFX.h"
#include "IronManCharacter.h"
#include "ReactorComponent.h"
#include "JarvisComponent.h"
#include "StoryLibrary.h"
#include "StoryBeacon.h"
#include "MirrorLock.h"
#include "StoryAlly.h"
#include "StoryNamePromptWidget.h"
#include "CinematicOverlayWidget.h"
#include "SuitTypes.h"
#include "SuitUpCinematic.h"
#include "EnemyBase.h"
#include "DroneEnemy.h"
#include "TankBot.h"
#include "BossTitan.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Blueprint/UserWidget.h"
#include "ChapterSet.h"
#include "ArenaEnvironment.h"
#include "EnvironmentPreset.h"

#define LOCTEXT_NAMESPACE "StoryDirector"

namespace
{
	const FName NAME_Player(TEXT("PLAYER"));
	const FName NAME_Morgan(TEXT("MORGAN"));
	const FName NAME_Tony(TEXT("TONY"));
	const FLinearColor StoryGold(1.f, 0.78f, 0.3f);
	const FLinearColor StoryIce(0.5f, 0.85f, 1.f);

	bool Has(const FString& Haystack, const TCHAR* Needle)
	{
		return Haystack.Contains(Needle, ESearchCase::IgnoreCase);
	}

	FStoryCastMember MakeCast(const TCHAR* Speaker, EStoryBody Body, const TCHAR* MeshPath, float Scale, const FLinearColor& Tint)
	{
		FStoryCastMember M;
		M.Speaker = FName(Speaker);
		M.Body = Body;
		if (MeshPath) { M.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath)); }
		M.Scale = Scale;
		M.Tint = Tint;
		return M;
	}

	float VoiceVolume()
	{
		const UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
		// Master is applied on the audio device and the Voice slider on SC_Voice when it exists
		// (ApplyGameplaySettings); otherwise the voice slider goes on the source here.
		return S ? S->GetVoiceSourceVolume() : 1.f;
	}

	bool SubtitlesOn()
	{
		const UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
		return !S || S->bSubtitles;
	}
}

AStoryDirector::AStoryDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;

	CastList.Add(MakeCast(TEXT("TONY"), EStoryBody::Human, AStoryCharacterNPC::DefaultMannyPath, 1.f, FLinearColor(0.18f, 0.18f, 0.22f)));
	// Drop-in MetaHuman: used automatically once /Game/MetaHumans/Tony/BP_Tony exists, else the placeholder.
	CastList.Last().MetaHumanClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/MetaHumans/Tony/BP_Tony.BP_Tony_C")));
	FlashbackScore = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Music/S_FlashbackSwell.S_FlashbackSwell")));
	CastList.Add(MakeCast(TEXT("PEPPER"), EStoryBody::Human, AStoryCharacterNPC::DefaultQuinnPath, 0.97f, FLinearColor(0.75f, 0.7f, 0.62f)));
	CastList.Add(MakeCast(TEXT("MORGAN"), EStoryBody::Human, AStoryCharacterNPC::DefaultQuinnPath, 0.88f, FLinearColor(0.25f, 0.4f, 0.7f)));
	CastList.Add(MakeCast(TEXT("PLAYER"), EStoryBody::Human, AStoryCharacterNPC::DefaultMannyPath, 0.93f, FLinearColor(0.42f, 0.3f, 0.2f)));
	CastList.Add(MakeCast(TEXT("Vison"), EStoryBody::Synthezoid, nullptr, 1.02f, FLinearColor(0.45f, 0.06f, 0.1f)));
	CastList.Add(MakeCast(TEXT("JOCASTA"), EStoryBody::Synthezoid, nullptr, 0.97f, FLinearColor(0.62f, 0.72f, 0.82f)));
	CastList.Add(MakeCast(TEXT("U"), EStoryBody::Robot, nullptr, 1.f, FLinearColor(0.4f, 0.4f, 0.42f)));
	CastList.Add(MakeCast(TEXT("DUM-E"), EStoryBody::Robot, nullptr, 1.f, FLinearColor(0.45f, 0.43f, 0.38f)));
}

void AStoryDirector::BeginPlay()
{
	Super::BeginPlay();
}

void AStoryDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStory();
	Super::EndPlay(EndPlayReason);
}

AIronManCharacter* AStoryDirector::GetPlayer() const
{
	return Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

AArenaGameMode* AStoryDirector::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
}

FString AStoryDirector::Resolve(const FString& Text) const
{
	return UStoryLibrary::ResolveTokens(Text, PlayerName, HeroName);
}

// =============================================================================== progress

void AStoryDirector::LoadProgress()
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	PlayerName = Story.PlayerNameDefault.IsEmpty() ? FString(TEXT("Harley")) : Story.PlayerNameDefault;
	HeroName.Reset();
	UnlockedSuits.Reset();
	Allies.Reset();

	const AArenaGameMode* GM = GetGM();
	if (const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(GM ? GM->SaveSlotName : FString(TEXT("IronManArena"))))
	{
		if (!Save->PlayerName.IsEmpty()) { PlayerName = Save->PlayerName; }
		HeroName = Save->HeroName;
		UnlockedSuits = Save->StoryUnlockedSuits;
		Allies = Save->StoryAllies;
		AChapterSet::bHallPodLighting = Save->IsCosmeticActive(ArenaRewards::PodLighting());
	}
}

void AStoryDirector::SaveProgress(int32 ReachedChapter, bool bComplete)
{
	const AArenaGameMode* GM = GetGM();
	const FString SaveSlotId = GM ? GM->SaveSlotName : FString(TEXT("IronManArena"));
	UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotId);
	if (!Save) { return; }
	if (bSideMission) { return; } // side missions never move main progress (GrantSideReward saves)
	Save->StoryChapterReached = FMath::Max(Save->StoryChapterReached, ReachedChapter);
	Save->bStoryComplete = Save->bStoryComplete || bComplete;
	Save->bPrologueSeen = true;
	for (const FName& S : UnlockedSuits) { Save->StoryUnlockedSuits.AddUnique(S); }
	for (const FName& A : Allies) { Save->StoryAllies.AddUnique(A); }
	Save->PlayerName = PlayerName;
	Save->HeroName = HeroName;
	Save->Write(SaveSlotId);
}

// =============================================================================== flow

const FStoryBook& AStoryDirector::GetBook() const
{
	return bSideMission ? SideBook : UStoryLibrary::GetStory();
}

void AStoryDirector::StartStory(int32 InChapterIndex, bool bPlayPrologue, bool bSkipIntro)
{
	bSideMission = false;
	SideBook = FStoryBook();
	StartRun(InChapterIndex, bPlayPrologue, bSkipIntro);
}

bool AStoryDirector::StartSideMission(FName MissionId, bool bSkipIntro, bool bIgnoreAvailability)
{
	const FStoryBook& Main = UStoryLibrary::GetStory();
	const FStoryChapter* Mission = Main.FindSideMission(MissionId);
	if (!Mission || !Main.IsValid() || !GetPlayer())
	{
		UE_LOG(LogIronManArena, Warning, TEXT("Story: side mission '%s' not found (or no story / player)"), *MissionId.ToString());
		return false;
	}
	const AArenaGameMode* GM = GetGM();
	const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(GM ? GM->SaveSlotName : FString(TEXT("IronManArena")));
	const int32 Reached = Save ? Save->StoryChapterReached : 0;
	const bool bComplete = Save && Save->bStoryComplete;
	if (!bIgnoreAvailability && !UStoryLibrary::IsSideMissionAvailable(Main, *Mission, Reached, bComplete))
	{
		UE_LOG(LogIronManArena, Log, TEXT("Story: side mission %s is not available yet (chapter reached %d)"), *MissionId.ToString(), Reached);
		return false;
	}

	// Gear / reactor / difficulty follow the player's progress inside the mission's window.
	const int32 Last = Main.Chapters.Num() - 1;
	const int32 From = FMath::Clamp(UStoryLibrary::ChapterOrder(Main, Mission->AvailableFrom), 0, Last);
	const int32 Until = FMath::Clamp(UStoryLibrary::ChapterOrder(Main, Mission->AvailableUntil), From, Last);
	SideMainIndex = FMath::Clamp(FMath::Min(Reached, Until), From, Last);

	SideBook = FStoryBook();
	SideBook.Title = Mission->Title;
	SideBook.PlayerNameDefault = Main.PlayerNameDefault;
	SideBook.Speakers = Main.Speakers;
	SideBook.Chapters.Add(*Mission);
	FStoryChapter& C = SideBook.Chapters[0];
	if (C.Reactor.Note.IsEmpty()) { C.Reactor = Main.Chapters[SideMainIndex].Reactor; }
	if (C.UnlockSuit.IsNone()) { C.UnlockSuit = Main.Chapters[SideMainIndex].UnlockSuit; }
	C.UnlockAlly = NAME_None;
	bSideMission = true;
	bSideRewardGranted = false;
	UE_LOG(LogIronManArena, Log, TEXT("Story: side mission %s ('%s'), gear of chapter %d"), *C.Id.ToString(), *C.Title, SideMainIndex + 1);
	StartRun(0, false, bSkipIntro);
	return true;
}

void AStoryDirector::GrantSideReward(const FStoryChapter& Mission)
{
	const AArenaGameMode* GMc = GetGM();
	const FString SaveSlotId = GMc ? GMc->SaveSlotName : FString(TEXT("IronManArena"));
	UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotId);
	if (!Save) { return; }
	Save->SideMissionsDone.AddUnique(Mission.Id);
	if (!Mission.RewardCodex.IsNone()) { Save->CodexGranted.AddUnique(Mission.RewardCodex); }
	if (!Mission.RewardUpgrade.IsNone()) { Save->Upgrades.AddUnique(Mission.RewardUpgrade); }
	if (!Mission.RewardCosmetic.IsNone()) { Save->Cosmetics.AddUnique(Mission.RewardCosmetic); }
	for (const FName& S : UnlockedSuits) { Save->StoryUnlockedSuits.AddUnique(S); }
	Save->Write(SaveSlotId);
	bSideRewardGranted = true;
	if (Mission.RewardCosmetic == ArenaRewards::PodLighting()) { AChapterSet::bHallPodLighting = true; }
	if (AIronManCharacter* Player = GetPlayer()) { Player->ApplyRewards(); }

	FString What;
	auto Pretty = [](FName Id)
	{
		FString T = Id.ToString().Replace(TEXT("-"), TEXT(" "));
		return T.ToUpper();
	};
	if (!Mission.RewardUpgrade.IsNone()) { What = FString::Printf(TEXT("UPGRADE: %s"), *Pretty(Mission.RewardUpgrade)); }
	if (!Mission.RewardCosmetic.IsNone()) { What += (What.IsEmpty() ? TEXT("") : TEXT("   ")) + FString::Printf(TEXT("COSMETIC: %s"), *Pretty(Mission.RewardCosmetic)); }
	if (!Mission.RewardCodex.IsNone()) { What += TEXT("   + CODEX ENTRY"); }
	UE_LOG(LogIronManArena, Log, TEXT("Story: side mission %s complete - %s"), *Mission.Id.ToString(), *What);
	if (AArenaGameMode* GM = GetGM())
	{
		GM->ShowBanner(LOCTEXT("SideDone", "SIDE MISSION COMPLETE"), FText::FromString(What), 6.f);
	}
}

void AStoryDirector::StartRun(int32 InChapterIndex, bool bPlayPrologue, bool bSkipIntro)
{
	const FStoryBook& Story = GetBook();
	AIronManCharacter* Player = GetPlayer();
	if (!Story.IsValid() || !Player)
	{
		UE_LOG(LogIronManArena, Error, TEXT("Story: no story data (Content/Story/story.json) or no player."));
		if (AArenaGameMode* GM = GetGM())
		{
			GM->ShowBanner(LOCTEXT("NoStory", "STORY DATA MISSING"), LOCTEXT("NoStorySub", "Content/Story/story.json"), 4.f);
		}
		return;
	}

	LoadProgress();
	Reports.Reset();
	Reports.SetNum(Story.Chapters.Num());
	bFinaleReached = false;
	bStoryCompleted = false;
	bHeroPromptShown = false;
	bInFinale = false;
	bInPrologue = false;

	// Player hooks (re-bound every start; StopStory removes them).
	Player->OnForcedLanding.Remove(ForcedLandingHandle);
	Player->OnHulkbusterActivated.Remove(HulkbusterHandle);
	Player->OnSummonAllyPressed.Remove(AllyHandle);
	ForcedLandingHandle = Player->OnForcedLanding.AddUObject(this, &AStoryDirector::HandleForcedLanding);
	HulkbusterHandle = Player->OnHulkbusterActivated.AddUObject(this, &AStoryDirector::HandleHulkbusterActivated);
	AllyHandle = Player->OnSummonAllyPressed.AddUObject(this, &AStoryDirector::HandleSummonAlly);

	// Morgan joins the comms as the guy in the chair (speaker lines, not a suit persona).
	if (UJarvisComponent* J = UJarvisComponent::Get(this))
	{
		J->AddSpeakerLines(UStoryLibrary::LoadCommsSpeakerLines(NAME_Morgan, TEXT("morgan.json")));
		J->SetCommsSpeaker(NAME_Morgan, MorganChatterShare);
	}

	const int32 Chapter = FMath::Clamp(InChapterIndex, 0, Story.Chapters.Num() - 1);
	if (InChapterIndex >= Story.Chapters.Num())
	{
		// Story already finished: replay the finale.
		ChapterIndex = Story.Chapters.Num() - 1;
		for (const FStoryChapter& Ch : Story.Chapters) { if (!Ch.UnlockAlly.IsNone()) { Allies.AddUnique(Ch.UnlockAlly); } }
		ApplyChapterSuit(ChapterIndex, false);
		StartFinale();
		return;
	}

	if (bPlayPrologue && Story.Prologue.Num() > 0)
	{
		ChapterIndex = Chapter;
		bInPrologue = true;
		// Flashback: the battlefield at dusk, years before Harley's story; chapter 1 cuts to the garage.
		ApplyEnvironment(TEXT("battlefield"), EnvironmentPresets::Flashback());
		FlashbackReactor = 1.f;
		PlayCutscene(Story.Prologue, FText::FromString(Story.Title), LOCTEXT("Prologue", "PROLOGUE"),
			[this, Chapter]() { BeginChapter(Chapter, false); });
	}
	else
	{
		BeginChapter(Chapter, bSkipIntro);
	}
}

void AStoryDirector::StopStory()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(FinishTimer); }
	if (Step == EStoryStep::Cutscene || Step == EStoryStep::NamePrompt)
	{
		OnCutsceneDone = nullptr;
		EndCutscene();
	}
	if (Prompt) { Prompt->OnSubmitted = nullptr; Prompt->RemoveFromParent(); Prompt = nullptr; }
	if (Beacon) { Beacon->Destroy(); Beacon = nullptr; }
	if (MirrorLock) { MirrorLock->Destroy(); MirrorLock = nullptr; }
	if (Ally) { Ally->Destroy(); Ally = nullptr; }
	ClearEnvironment();
	for (auto& Pair : Npcs) { if (Pair.Value) { Pair.Value->Destroy(); } }
	Npcs.Reset();
	ObjectiveEnemies.Reset();

	if (AIronManCharacter* Player = GetPlayer())
	{
		Player->OnForcedLanding.Remove(ForcedLandingHandle);
		Player->OnHulkbusterActivated.Remove(HulkbusterHandle);
		Player->OnSummonAllyPressed.Remove(AllyHandle);
		Player->CinematicSkipHandler = nullptr;
		if (UReactorComponent* R = Player->GetReactor()) { R->SetCoreDark(false); }
	}
	ForcedLandingHandle.Reset();
	HulkbusterHandle.Reset();
	AllyHandle.Reset();
	Step = EStoryStep::Idle;
}

void AStoryDirector::ApplyReactor(const FStoryReactor& R)
{
	if (AIronManCharacter* Player = GetPlayer())
	{
		if (UReactorComponent* Core = Player->GetReactor())
		{
			Core->ApplyStage(R.Stage, R.Mount);
			Core->SetCoreDark(false);
		}
	}
}

void AStoryDirector::ApplyChapterSuit(int32 Index, bool bAnnounce)
{
	const FStoryBook& Story = GetBook();
	AIronManCharacter* Player = GetPlayer();
	if (!Player || !Story.Chapters.IsValidIndex(Index)) { return; }

	// Every suit up to this chapter is built (also when starting from the chapter list).
	// Side missions: every suit up to the main-story chapter their gear comes from.
	const FStoryBook& Main = UStoryLibrary::GetStory();
	const int32 Upto = bSideMission ? FMath::Min(SideMainIndex, Main.Chapters.Num() - 1) : Index;
	for (int32 i = 0; i <= Upto; ++i)
	{
		const FName Id = Main.Chapters[i].UnlockSuit;
		if (!Id.IsNone() && USuitLibrary::FindSuitIndex(Id) != INDEX_NONE) { UnlockedSuits.AddUnique(Id); }
	}
	Player->SetAllowedSuits(UnlockedSuits);

	int32 SuitIdx = USuitLibrary::FindSuitIndex(Story.Chapters[Index].UnlockSuit);
	if (SuitIdx == INDEX_NONE) { SuitIdx = Player->GetSuitIndex(); }
	Player->ApplySuit(SuitIdx);
	if (UJarvisComponent* J = UJarvisComponent::Get(this)) { J->SetPersona(Player->GetSuit().AI, bAnnounce); }
}

void AStoryDirector::BeginChapter(int32 Index, bool bSkipIntro)
{
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(Index)) { FinishStory(); return; }

	ChapterIndex = Index;
	ObjectiveIndex = 0;
	bInFinale = false;
	bInPrologue = false;
	ClearHostiles();
	if (Beacon) { Beacon->Destroy(); Beacon = nullptr; }
	if (MirrorLock) { MirrorLock->Destroy(); MirrorLock = nullptr; }
	if (Ally) { Ally->Destroy(); Ally = nullptr; }
	AllyCooldownLeft = 0.f;

	const FStoryChapter& C = Story.Chapters[Index];
	{
		const FStoryBook& Main = UStoryLibrary::GetStory();
		const int32 Upto = FMath::Min(bSideMission ? SideMainIndex : Index, Main.Chapters.Num());
		for (int32 i = 0; i < Upto; ++i)
		{
			const FName A = Main.Chapters[i].UnlockAlly;
			if (!A.IsNone()) { Allies.AddUnique(A); }
		}
	}

	ApplyReactor(C.Reactor);
	ApplyChapterSuit(Index, false);
	SaveProgress(Index, false);
	{
		bool bHasBoss = false;
		for (const FStoryObjective& Obj : C.Objectives) { bHasBoss |= (Obj.Type == EStoryObjectiveType::Boss); }
		FName SetName;
		const FEnvironmentPreset Env = EnvironmentPresets::ForChapter(C.Location, C.Id.ToString(), bHasBoss, SetName);
		ApplyEnvironment(SetName, Env);
	}

	if (Reports.IsValidIndex(Index))
	{
		FStoryChapterReport& Rep = Reports[Index];
		Rep = FStoryChapterReport();
		Rep.bStarted = true;
		Rep.bIntroPlayed = !bSkipIntro && C.Intro.Num() > 0;
		if (const AIronManCharacter* P = GetPlayer())
		{
			if (const UReactorComponent* Core = P->GetReactor())
			{
				Rep.bReactorApplied = Core->HasStage();
				Rep.ReactorStage = Core->GetStage();
				Rep.ReactorMount = Core->GetMount();
			}
			Rep.SuitWorn = P->GetSuit().Id;
			Rep.bSuitAllowed = P->IsSuitAllowed(P->GetSuitIndex());
		}
	}

	if (AArenaGameMode* GM = GetGM()) { GM->SetDifficultyWave(DifficultyChapter() + 1); }
	if (AIronManCharacter* Player = GetPlayer())
	{
		Player->Heal(10000.f);
		Player->AddEnergy(10000.f);
	}

	const FText Title = bSideMission ? LOCTEXT("SideN", "SIDE MISSION") : FText::Format(LOCTEXT("ChapterN", "CHAPTER {0}"), FText::AsNumber(Index + 1));
	if (bSkipIntro || C.Intro.Num() == 0)
	{
		StartSuitUp();
	}
	else
	{
		PlayCutscene(C.Intro, Title, FText::FromString(C.Title), [this]() { StartSuitUp(); });
	}
}

void AStoryDirector::StartSuitUp()
{
	const FStoryBook& Story = GetBook();
	AIronManCharacter* Player = GetPlayer();
	if (!Player || !Story.Chapters.IsValidIndex(ChapterIndex)) { return; }
	const FStoryChapter& C = Story.Chapters[ChapterIndex];

	if (AArenaGameMode* GM = GetGM())
	{
		FString Note = C.Reactor.Note;
		if (Note.IsEmpty()) { Note = Player->GetSuit().Name.ToString(); }
		GM->ShowBanner(FText::FromString(C.Title.ToUpper()), FText::FromString(Resolve(Note)), 4.f);
	}

	Step = EStoryStep::SuitUp;
	if (!ASuitUpCinematic::Play(Player, Player->GetSuitIndex(), /*bShortVersion*/ true))
	{
		StartObjective(0);
	}
}

void AStoryDirector::FinishChapter()
{
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex)) { return; }
	const FStoryChapter& C = Story.Chapters[ChapterIndex];

	ClearHostiles();
	if (Beacon) { Beacon->Destroy(); Beacon = nullptr; }
	if (MirrorLock) { MirrorLock->Destroy(); MirrorLock = nullptr; }
	if (Ally) { Ally->Dismiss(); Ally = nullptr; }

	// Chapter 5: the makeshift reactor finally gives out - the core goes dark for the outro.
	if (C.Reactor.Stage == EReactorStage::Failed)
	{
		if (AIronManCharacter* Player = GetPlayer())
		{
			if (UReactorComponent* R = Player->GetReactor()) { R->SetCoreDark(true); }
		}
	}
	if (FStoryChapterReport* Rep = CurrentReport())
	{
		Rep->bOutroPlayed = C.Outro.Num() > 0;
		if (const AIronManCharacter* P = GetPlayer())
		{
			if (const UReactorComponent* R = P->GetReactor()) { Rep->bCoreDarkAtEnd = R->IsCoreDark(); }
		}
	}

	auto Next = [this]()
	{
		const FStoryBook& S = GetBook();
		if (!S.Chapters.IsValidIndex(ChapterIndex)) { return; }
		const FName A = S.Chapters[ChapterIndex].UnlockAlly;
		if (!A.IsNone() && !Allies.Contains(A))
		{
			Allies.AddUnique(A);
			if (AArenaGameMode* GM = GetGM())
			{
				GM->ShowBanner(LOCTEXT("AllyUnlocked", "ALLY UNLOCKED"),
					FText::Format(LOCTEXT("AllyUnlockedSub", "{0}  -  press G when the HUD says so"), UStoryLibrary::GetSpeakerLabel(A, PlayerName)), 4.f);
			}
		}
		if (FStoryChapterReport* Rep = CurrentReport())
		{
			Rep->bFinished = true;
			Rep->bAllyUnlocked = !A.IsNone() && Allies.Contains(A);
		}
		if (bSideMission)
		{
			GrantSideReward(S.Chapters[ChapterIndex]);
			Step = EStoryStep::Complete;
			GetWorldTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (AArenaGameMode* GM = GetGM()) { GM->ReturnToTitle(); }
			}), 6.5f, false);
			return;
		}
		SaveProgress(ChapterIndex + 1, false);
		if (ChapterIndex + 1 < S.Chapters.Num()) { BeginChapter(ChapterIndex + 1, false); }
		else { StartFinale(); }
	};

	if (C.Outro.Num() > 0)
	{
		PlayCutscene(C.Outro, bSideMission ? LOCTEXT("SideN", "SIDE MISSION") : FText::Format(LOCTEXT("ChapterN", "CHAPTER {0}"), FText::AsNumber(ChapterIndex + 1)), FText::FromString(C.Title), Next);
	}
	else
	{
		Next();
	}
}

void AStoryDirector::StartFinale()
{
	const FStoryBook& Story = GetBook();
	bInFinale = true;
	bFinaleReached = true;
	ClearHostiles();
	ApplyReactor(Story.Finale.Reactor);
	ApplyEnvironment(TEXT("battlefield"), EnvironmentPresets::Finale());
	if (Story.Finale.Shots.Num() == 0) { FinishStory(); return; }
	PlayCutscene(Story.Finale.Shots, LOCTEXT("Finale", "FINALE"), FText::FromString(Story.Title), [this]() { FinishStory(); });
}

void AStoryDirector::FinishStory()
{
	const FStoryBook& Story = GetBook();
	Step = EStoryStep::Complete;
	bInFinale = false;
	bStoryCompleted = true;
	SaveProgress(Story.Chapters.Num(), true);
	if (AArenaGameMode* GM = GetGM())
	{
		const FString Name = HeroName.IsEmpty() ? PlayerName : HeroName;
		GM->ShowBanner(LOCTEXT("StoryComplete", "STORY COMPLETE"), FText::FromString(Name.ToUpper()), 6.f);
	}
	GetWorldTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (AArenaGameMode* GM = GetGM()) { GM->ReturnToTitle(); }
	}), 6.5f, false);
}

void AStoryDirector::NotifyPlayerDied()
{
	if (Beacon) { Beacon->Destroy(); Beacon = nullptr; }
	if (MirrorLock) { MirrorLock->Destroy(); MirrorLock = nullptr; }
	if (Ally) { Ally->Dismiss(); Ally = nullptr; }
	ObjectiveEnemies.Reset();
	if (Step == EStoryStep::Objective || Step == EStoryStep::ObjectiveDone) { Step = EStoryStep::Idle; }
	if (UJarvisComponent* J = UJarvisComponent::Get(this))
	{
		J->SayScripted(NAME_Morgan, UStoryLibrary::GetSpeakerLabel(NAME_Morgan, PlayerName),
			FText::FromString(Resolve(TEXT("{playerName}? Talk to me! ...Okay, rebooting the suit. Again."))),
			UStoryLibrary::GetSpeakerColor(NAME_Morgan), nullptr, 90, true);
	}
}

// =============================================================================== environment

void AStoryDirector::ApplyEnvironment(FName SetName, const FEnvironmentPreset& Preset)
{
	AArenaGameMode* GM = GetGM();
	AArenaEnvironment* Env = AArenaEnvironment::Find(this);
	const bool bArena = SetName.IsNone() || SetName == TEXT("arena");
	if (EnvironmentId == Preset.Id && (ChapterSet != nullptr) == !bArena) { return; } // same look already up
	LightingKey = NAME_None;

	if (ChapterSet) { ChapterSet->Destroy(); ChapterSet = nullptr; }
	EnvironmentId = Preset.Id;
	if (Env)
	{
		Env->ApplyPreset(Preset);
		// Vison's data-driven presets (Content/Story/lighting_presets.json) override the C++ look.
		LightingKey = ContribLighting::HasPreset(Preset.Id) ? Preset.Id : ContribLighting::PresetKeyFor(Preset.Id);
		if (!LightingKey.IsNone()) { ContribLighting::Apply(Env, LightingKey); }
		Env->SetArenaDressingVisible(bArena);
		EArenaSurface Floor = EArenaSurface::Concrete;
		const FString S = SetName.ToString();
		if (S == TEXT("workshop"))         { Floor = EArenaSurface::HangarFloor; }
		else if (S == TEXT("rooftops"))    { Floor = Preset.bWet ? EArenaSurface::RoofGravelWet : EArenaSurface::RoofGravel; }
		else if (S == TEXT("city"))        { Floor = Preset.bWet ? EArenaSurface::ConcreteWet : EArenaSurface::Concrete; }
		else if (S == TEXT("ruins"))       { Floor = EArenaSurface::DebrisGround; }
		else if (S == TEXT("battlefield")) { Floor = EArenaSurface::BurnedGround; }
		Env->SetFloorSurface(Floor, FLinearColor(0.8f, 0.8f, 0.8f));
	}
	if (!bArena && GM)
	{
		const FVector Origin(GM->GetArenaCenter().X, GM->GetArenaCenter().Y, GM->GetArenaFloorZ());
		ChapterSet = AChapterSet::SpawnSet(GetWorld(), Origin, SetName, Preset);
	}
	UE_LOG(LogIronManArena, Log, TEXT("Story environment: set '%s', look '%s'%s"), *SetName.ToString(), *Preset.Id.ToString(),
		Env ? TEXT("") : TEXT(" (level has its own lighting: set only)"));
}

void AStoryDirector::ClearEnvironment()
{
	if (ChapterSet) { ChapterSet->Destroy(); ChapterSet = nullptr; }
	const UWorld* World = GetWorld();
	if (!EnvironmentId.IsNone() && World && !World->bIsTearingDown)
	{
		if (AArenaEnvironment* Env = AArenaEnvironment::Find(this))
		{
			Env->ApplyArenaLook();
			Env->SetArenaDressingVisible(true);
			Env->SetFloorSurface(EArenaSurface::Concrete, FLinearColor(0.8f, 0.8f, 0.82f));
		}
	}
	EnvironmentId = NAME_None;
}

// =============================================================================== objectives

void AStoryDirector::StartObjective(int32 Index)
{
	const FStoryBook& Story = GetBook();
	AIronManCharacter* Player = GetPlayer();
	AArenaGameMode* GM = GetGM();
	if (!Player || !GM || !Story.Chapters.IsValidIndex(ChapterIndex)) { return; }
	const FStoryChapter& C = Story.Chapters[ChapterIndex];
	if (!C.Objectives.IsValidIndex(Index)) { FinishChapter(); return; }

	ObjectiveIndex = Index;
	Step = EStoryStep::Objective;
	if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->ObjectivesStarted; }
	ObjectiveProgress = 0.f;
	bProgressCommsFired = false;
	ObjectiveCount = 0;
	ObjectiveSpawned = 0;
	WaveNumber = 0;
	ObjectiveTimer = 0.f;
	SpawnTimer = 1.f;
	ForcedLandings = 0;
	PendingDrones = 0;
	PendingTanks = 0;
	WaveBreak = 0.f;
	ObjectiveEnemies.Reset();
	if (Beacon) { Beacon->Destroy(); Beacon = nullptr; }
	if (MirrorLock) { MirrorLock->Destroy(); MirrorLock = nullptr; }

	const FStoryObjective& O = C.Objectives[Index];
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	switch (O.Type)
	{
	case EStoryObjectiveType::Ground:
	{
		FVector L = GM->FindSpawnLocation(GroundBeaconDistance * 0.8f, GroundBeaconDistance * 1.1f, 0.f);
		L.Z = GM->GetArenaFloorZ();
		if (Has(O.Target, TEXT("mirror lock")))
		{
			// Jocasta's side mission: a lights-out ring of six pillars, solved on foot (MirrorLock.h).
			MirrorLock = GetWorld()->SpawnActor<AMirrorLock>(AMirrorLock::StaticClass(), FTransform(L), SP);
			if (MirrorLock) { MirrorLock->Arm(); break; }
			UE_LOG(LogIronManArena, Error, TEXT("Story: could not spawn the Mirror Lock, using a plain beacon"));
		}
		Beacon = GetWorld()->SpawnActor<AStoryBeacon>(AStoryBeacon::StaticClass(), FTransform(L), SP);
		if (Beacon) { Beacon->Setup(false, 300.f, StoryGold); }
		break;
	}
	case EStoryObjectiveType::Flight:
	{
		const bool bHigh = Has(O.Target, TEXT("sky")) || Has(O.Target, TEXT("cloud")) || Has(O.Target, TEXT("orbit"));
		const FVector L = GM->FindSpawnLocation(FlightBeaconDistance * 0.8f, FlightBeaconDistance * 1.1f, FlightBeaconHeight * (bHigh ? 1.6f : 1.f));
		Beacon = GetWorld()->SpawnActor<AStoryBeacon>(AStoryBeacon::StaticClass(), FTransform(L), SP);
		if (Beacon) { Beacon->Setup(true, 380.f, StoryIce); }
		break;
	}
	case EStoryObjectiveType::Wave:
	case EStoryObjectiveType::Unknown:
		StartObjectiveWave(1);
		break;
	case EStoryObjectiveType::Boss:
	{
		const FVector L = GM->FindSpawnLocation(2600.f, 3400.f, 0.f);
		if (AEnemyBase* Boss = GM->SpawnEnemyAt(GM->BossClass, L))
		{
			ObjectiveEnemies.Add(Boss);
			++ObjectiveSpawned;
		}
		UJarvisComponent::Notify(this, EAIEvent::BossIncoming);
		GM->ShowBanner(FText::FromString(O.Target.IsEmpty() ? FString(TEXT("OBSIDIAN TITAN")) : O.Target.ToUpper()), LOCTEXT("BossSub", "Bring it down"), 3.f);
		break;
	}
	case EStoryObjectiveType::Hulkbuster:
		if (Player->GetHulkbusterCharges() <= 0) { Player->GrantHulkbusterCharge(TEXT("MORGAN")); }
		SpawnObjectiveEnemy(true);
		SpawnObjectiveEnemy(true);
		SpawnObjectiveEnemy(false);
		SpawnObjectiveEnemy(false);
		break;
	case EStoryObjectiveType::Energy:
		break;
	default:
		break;
	}

	FireComms(EStoryCommsTrigger::Start);
	GM->ShowBanner(LOCTEXT("NewObjective", "OBJECTIVE"), FText::FromString(Resolve(O.Text)), 3.5f);

	if (Beacon)
	{
		BeaconStartDist = FMath::Max(1.f, FVector::Dist(Player->GetActorLocation(), Beacon->GetActorLocation()));
	}
}

void AStoryDirector::StartObjectiveWave(int32 InWave)
{
	WaveNumber = InWave;
	PendingDrones = FMath::Min(14, 3 + DifficultyChapter() + InWave * 2);
	PendingTanks = DifficultyChapter() >= 2 ? FMath::Min(4, 1 + InWave / 2 + DifficultyChapter() / 4) : (InWave > 1 ? 1 : 0);
	SpawnTimer = 0.8f;
	if (AArenaGameMode* GM = GetGM())
	{
		GM->ShowBanner(FText::Format(LOCTEXT("StoryWave", "WAVE {0}"), FText::AsNumber(InWave)), FText::GetEmpty(), 2.f);
	}
	UJarvisComponent::Notify(this, FMath::RandBool() ? EAIEvent::Intel : EAIEvent::WaveStart);
}

void AStoryDirector::SpawnObjectiveEnemy(bool bTank)
{
	AArenaGameMode* GM = GetGM();
	if (!GM) { return; }
	const FVector2D Dist = GM->SpawnDistance;
	const float Height = bTank ? 0.f : FMath::FRandRange(GM->DroneSpawnHeight.X, GM->DroneSpawnHeight.Y);
	const FVector L = GM->FindSpawnLocation(Dist.X, Dist.Y, Height);
	if (AEnemyBase* E = GM->SpawnEnemyAt(bTank ? GM->TankClass : GM->DroneClass, L))
	{
		ObjectiveEnemies.Add(E);
		++ObjectiveSpawned;
	}
}

int32 AStoryDirector::CountObjectiveAlive() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AEnemyBase>& E : ObjectiveEnemies)
	{
		if (E.IsValid() && !E->IsDead()) { ++N; }
	}
	return N;
}

void AStoryDirector::ClearHostiles()
{
	if (AArenaGameMode* GM = GetGM()) { GM->ClearCombat(); }
	ObjectiveEnemies.Reset();
}

void AStoryDirector::SetObjectiveProgress(float Fraction)
{
	ObjectiveProgress = FMath::Clamp(Fraction, 0.f, 1.f);
	if (!bProgressCommsFired && ObjectiveProgress >= 0.5f)
	{
		bProgressCommsFired = true;
		FireComms(EStoryCommsTrigger::Progress);
	}
}

void AStoryDirector::FireComms(EStoryCommsTrigger Trigger)
{
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex) || !Story.Chapters[ChapterIndex].Objectives.IsValidIndex(ObjectiveIndex)) { return; }
	UJarvisComponent* J = UJarvisComponent::Get(this);
	if (!J) { return; }
	for (const FStoryComms& Comms : Story.Chapters[ChapterIndex].Objectives[ObjectiveIndex].Comms)
	{
		if (Comms.Trigger != Trigger) { continue; }
		const FStoryLine& L = Comms.Line;
		if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->CommsFired[FMath::Clamp(int32(Trigger), 0, 2)]; }
		J->SayScripted(L.Speaker, UStoryLibrary::GetSpeakerLabel(L.Speaker, PlayerName), FText::FromString(Resolve(L.Text)),
			UStoryLibrary::GetSpeakerColor(L.Speaker), UStoryLibrary::GetLineSound(L, PlayerName, Story.PlayerNameDefault),
			Trigger == EStoryCommsTrigger::Complete ? 80 : 75, false);
	}
}

void AStoryDirector::CompleteObjective()
{
	FireComms(EStoryCommsTrigger::Complete);
	if (Beacon)
	{
		AArenaBlastFX::Spawn(GetWorld(), Beacon->GetActorLocation(), 300.f, StoryGold, 0.5f);
		Beacon->Destroy();
		Beacon = nullptr;
	}
	if (MirrorLock)
	{
		MirrorLock->SetLifeSpan(4.f);  // let the opened lock glow blue for a moment
		MirrorLock = nullptr;
	}
	ObjectiveProgress = 1.f;
	Step = EStoryStep::ObjectiveDone;
	if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->ObjectivesCompleted; }
	GapTimer = ObjectiveGapSeconds;
	if (AArenaGameMode* GM = GetGM())
	{
		GM->ShowBanner(LOCTEXT("ObjectiveDone", "OBJECTIVE COMPLETE"), FText::GetEmpty(), 2.f);
		GM->AddScore(1000 * (DifficultyChapter() + 1));
	}
}

void AStoryDirector::TickObjective(float DeltaSeconds)
{
	const FStoryBook& Story = GetBook();
	AIronManCharacter* Player = GetPlayer();
	if (!Player || Player->IsDead() || !Story.Chapters.IsValidIndex(ChapterIndex)) { return; }
	const FStoryChapter& C = Story.Chapters[ChapterIndex];
	if (!C.Objectives.IsValidIndex(ObjectiveIndex)) { return; }
	const FStoryObjective& O = C.Objectives[ObjectiveIndex];
	const int32 Need = FMath::Max(1, O.Count);
	SpawnTimer -= DeltaSeconds;

	switch (O.Type)
	{
	case EStoryObjectiveType::Ground:
	case EStoryObjectiveType::Flight:
	{
		if (MirrorLock)
		{
			SetObjectiveProgress(MirrorLock->GetProgress());
			if (MirrorLock->IsSolved())
			{
				if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->MirrorLocksSolved; }
				CompleteObjective();
			}
			break;
		}
		if (!Beacon) { CompleteObjective(); return; }
		const float D = FVector::Dist(Player->GetActorLocation(), Beacon->GetActorLocation());
		SetObjectiveProgress(1.f - D / BeaconStartDist);
		const bool bModeOk = (O.Type == EStoryObjectiveType::Ground) ? !Player->IsFlying() : Player->IsFlying();
		if (bModeOk && Beacon->IsReached(Player->GetActorLocation())) { CompleteObjective(); }
		break;
	}
	case EStoryObjectiveType::Drone:
	case EStoryObjectiveType::Tank:
	{
		const bool bTank = O.Type == EStoryObjectiveType::Tank;
		if (ObjectiveSpawned < Need && CountObjectiveAlive() < MaxObjectiveAlive && SpawnTimer <= 0.f)
		{
			SpawnObjectiveEnemy(bTank);
			SpawnTimer = bTank ? 2.2f : 0.9f;
		}
		// Safety: enemies that vanished without dying (fell out of the world) are respawned.
		if (ObjectiveSpawned >= Need && CountObjectiveAlive() == 0 && ObjectiveCount < Need)
		{
			ObjectiveSpawned = ObjectiveCount;
		}
		SetObjectiveProgress(float(ObjectiveCount) / Need);
		if (ObjectiveCount >= Need) { CompleteObjective(); }
		break;
	}
	case EStoryObjectiveType::Wave:
	case EStoryObjectiveType::Unknown:
	{
		if (WaveBreak > 0.f)
		{
			WaveBreak -= DeltaSeconds;
			if (WaveBreak <= 0.f) { StartObjectiveWave(WaveNumber + 1); }
			break;
		}
		if ((PendingDrones > 0 || PendingTanks > 0) && SpawnTimer <= 0.f && CountObjectiveAlive() < MaxObjectiveAlive + 3)
		{
			const bool bTank = PendingTanks > 0 && (PendingDrones == 0 || FMath::FRand() < 0.3f);
			SpawnObjectiveEnemy(bTank);
			if (bTank) { --PendingTanks; } else { --PendingDrones; }
			SpawnTimer = FMath::FRandRange(0.6f, 1.3f);
		}
		if (PendingDrones == 0 && PendingTanks == 0 && CountObjectiveAlive() == 0)
		{
			ObjectiveCount = WaveNumber;
			SetObjectiveProgress(float(ObjectiveCount) / Need);
			if (ObjectiveCount >= Need) { CompleteObjective(); }
			else
			{
				WaveBreak = 2.5f;
				Player->Heal(10.f);
				if (AArenaGameMode* GM = GetGM()) { GM->ShowBanner(LOCTEXT("WaveClear", "WAVE CLEARED"), FText::GetEmpty(), 1.8f); }
			}
		}
		break;
	}
	case EStoryObjectiveType::Boss:
	{
		const ABossTitan* Boss = nullptr;
		for (const TWeakObjectPtr<AEnemyBase>& E : ObjectiveEnemies)
		{
			if (const ABossTitan* B = Cast<ABossTitan>(E.Get())) { Boss = B; }
		}
		if (Boss && !Boss->IsDead() && Boss->GetMaxHealth() > 0.f)
		{
			SetObjectiveProgress(1.f - Boss->GetHealth() / Boss->GetMaxHealth());
		}
		if (ObjectiveCount >= 1 || (!Boss && ObjectiveSpawned > 0)) { CompleteObjective(); }
		break;
	}
	case EStoryObjectiveType::Hulkbuster:
	{
		const int32 Alive = CountObjectiveAlive();
		if (Alive == 0 && ObjectiveCount < Need && SpawnTimer <= 0.f)
		{
			SpawnObjectiveEnemy(true);
			SpawnObjectiveEnemy(false);
			SpawnTimer = 3.f;
		}
		if (!Player->IsHulkbusterActive() && Player->GetHulkbusterCharges() <= 0 && ObjectiveCount < Need)
		{
			Player->GrantHulkbusterCharge(TEXT("MORGAN"));
		}
		SetObjectiveProgress(float(ObjectiveCount) / Need);
		if (ObjectiveCount >= Need && (!Player->IsHulkbusterActive() || Alive == 0)) { CompleteObjective(); }
		break;
	}
	case EStoryObjectiveType::Energy:
	{
		ObjectiveTimer += DeltaSeconds;
		if (CountObjectiveAlive() < 3 && SpawnTimer <= 0.f)
		{
			SpawnObjectiveEnemy(FMath::FRand() < 0.2f);
			SpawnTimer = 2.5f;
		}
		SetObjectiveProgress(ObjectiveTimer / FMath::Max(1.f, EnergyObjectiveSeconds));
		if (ObjectiveTimer >= EnergyObjectiveSeconds) { CompleteObjective(); }
		break;
	}
	default:
		break;
	}
}

void AStoryDirector::NotifyEnemyKilled(AEnemyBase* Enemy)
{
	if (Step != EStoryStep::Objective || !Enemy) { return; }
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex) || !Story.Chapters[ChapterIndex].Objectives.IsValidIndex(ObjectiveIndex)) { return; }
	const FStoryObjective& O = Story.Chapters[ChapterIndex].Objectives[ObjectiveIndex];
	switch (O.Type)
	{
	case EStoryObjectiveType::Drone:
		if (Cast<ADroneEnemy>(Enemy)) { ++ObjectiveCount; }
		break;
	case EStoryObjectiveType::Tank:
		if (Cast<ATankBot>(Enemy)) { ++ObjectiveCount; }
		break;
	case EStoryObjectiveType::Boss:
		if (Cast<ABossTitan>(Enemy)) { ++ObjectiveCount; }
		break;
	default:
		break;
	}
}

void AStoryDirector::HandleForcedLanding()
{
	if (Step != EStoryStep::Objective) { return; }
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex) || !Story.Chapters[ChapterIndex].Objectives.IsValidIndex(ObjectiveIndex)) { return; }
	if (Story.Chapters[ChapterIndex].Objectives[ObjectiveIndex].Type != EStoryObjectiveType::Energy) { return; }

	++ForcedLandings;
	if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->ForcedLandings; }
	ObjectiveTimer = 0.f;
	ObjectiveProgress = 0.f;
	if (AArenaGameMode* GM = GetGM())
	{
		GM->ShowBanner(LOCTEXT("PowerDepleted", "POWER DEPLETED"), LOCTEXT("PowerDepletedSub", "Forced landing - timer reset. Watch your heat, vent with X."), 3.f);
	}
	UJarvisComponent::Notify(this, EAIEvent::Encourage);
}

void AStoryDirector::HandleHulkbusterActivated()
{
	if (Step != EStoryStep::Objective) { return; }
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex) || !Story.Chapters[ChapterIndex].Objectives.IsValidIndex(ObjectiveIndex)) { return; }
	if (Story.Chapters[ChapterIndex].Objectives[ObjectiveIndex].Type == EStoryObjectiveType::Hulkbuster) { ++ObjectiveCount; }
}

void AStoryDirector::HandleSummonAlly()
{
	if (Step != EStoryStep::Objective) { return; }
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex) || !Story.Chapters[ChapterIndex].Objectives.IsValidIndex(ObjectiveIndex)) { return; }
	const FName AllyId = Story.Chapters[ChapterIndex].Objectives[ObjectiveIndex].AllyAvailable;
	AArenaGameMode* GM = GetGM();
	if (AllyId.IsNone() || !HasAlly(AllyId) || !GM) { return; }
	if (Ally) { return; }
	if (AllyCooldownLeft > 0.f)
	{
		GM->ShowBanner(FText::Format(LOCTEXT("AllyCooldown", "{0} RECHARGING"), UStoryLibrary::GetSpeakerLabel(AllyId, PlayerName)),
			FText::Format(LOCTEXT("AllyCooldownSub", "{0} s"), FText::AsNumber(FMath::CeilToInt(AllyCooldownLeft))), 1.2f);
		return;
	}
	Ally = AStoryAlly::Summon(GetWorld(), GetPlayer(), AllyDuration, AllyId);
	if (Ally)
	{
		if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->AllySummons; }
		AllyCooldownLeft = AllyCooldown;
		GM->ShowBanner(UStoryLibrary::GetSpeakerLabel(AllyId, PlayerName), LOCTEXT("AllyOnline", "Phasing in"), 2.f);
		if (UJarvisComponent* J = UJarvisComponent::Get(this))
		{
			if (AllyId == FName(TEXT("jocasta")))
			{
				const FName JocastaSpeaker(TEXT("JOCASTA"));
				const TCHAR* SummonPath = TEXT("/Game/Audio/JOCASTA/JOCASTA_summon_1");
				USoundBase* SummonSound = FPackageName::DoesPackageExist(SummonPath)
					? LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/JOCASTA/JOCASTA_summon_1.JOCASTA_summon_1")) : nullptr;
				J->SayScripted(JocastaSpeaker, UStoryLibrary::GetSpeakerLabel(JocastaSpeaker, PlayerName),
					LOCTEXT("JocastaSummon", "I'm on your flank. Don't thank me yet."),
					UStoryLibrary::GetSpeakerColor(JocastaSpeaker), SummonSound, 70, false);
			}
			else
			{
				const FName VisonSpeaker(TEXT("Vison"));
				J->SayScripted(VisonSpeaker, UStoryLibrary::GetSpeakerLabel(VisonSpeaker, PlayerName),
					LOCTEXT("VisonSummon", "I am here. Keep your reactor cool; I will hold their attention."),
					UStoryLibrary::GetSpeakerColor(VisonSpeaker), nullptr, 70, false);
			}
		}
	}
}

// =============================================================================== test / HUD state

FStoryChapterReport* AStoryDirector::CurrentReport()
{
	return Reports.IsValidIndex(ChapterIndex) ? &Reports[ChapterIndex] : nullptr;
}

const FStoryObjective* AStoryDirector::GetCurrentObjective() const
{
	if (Step != EStoryStep::Objective && Step != EStoryStep::ObjectiveDone) { return nullptr; }
	const FStoryBook& Story = GetBook();
	if (!Story.Chapters.IsValidIndex(ChapterIndex) || !Story.Chapters[ChapterIndex].Objectives.IsValidIndex(ObjectiveIndex)) { return nullptr; }
	return &Story.Chapters[ChapterIndex].Objectives[ObjectiveIndex];
}

void AStoryDirector::GetObjectiveEnemies(TArray<AEnemyBase*>& Out) const
{
	Out.Reset();
	for (const TWeakObjectPtr<AEnemyBase>& E : ObjectiveEnemies)
	{
		if (E.IsValid() && !E->IsDead()) { Out.Add(E.Get()); }
	}
}

void AStoryDirector::NotifyBossPhase(int32 Phase)
{
	if (Step != EStoryStep::Objective) { return; }
	if (FStoryChapterReport* Rep = CurrentReport()) { Rep->BossPhasesSeen = FMath::Max(Rep->BossPhasesSeen, Phase); }
}

// =============================================================================== HUD

bool AStoryDirector::GetObjectiveInfo(FText& OutChapter, FText& OutObjective, FText& OutProgress) const
{
	const FStoryBook& Story = GetBook();
	if (!IsActive() || !Story.Chapters.IsValidIndex(ChapterIndex)) { return false; }
	const FStoryChapter& C = Story.Chapters[ChapterIndex];
	OutChapter = bInFinale ? LOCTEXT("FinaleHud", "FINALE")
		: bSideMission ? FText::Format(LOCTEXT("SideHud", "SIDE MISSION  -  {0}"), FText::FromString(C.Title.ToUpper()))
		: FText::Format(LOCTEXT("ChapterHud", "CHAPTER {0}  -  {1}"), FText::AsNumber(ChapterIndex + 1), FText::FromString(C.Title.ToUpper()));
	if ((Step != EStoryStep::Objective && Step != EStoryStep::ObjectiveDone) || !C.Objectives.IsValidIndex(ObjectiveIndex))
	{
		OutObjective = FText::GetEmpty();
		OutProgress = FText::GetEmpty();
		return true;
	}
	const FStoryObjective& O = C.Objectives[ObjectiveIndex];
	OutObjective = FText::FromString(Resolve(O.Text));
	const int32 Need = FMath::Max(1, O.Count);
	if (Step == EStoryStep::ObjectiveDone) { OutProgress = LOCTEXT("Done", "COMPLETE"); return true; }

	const AIronManCharacter* Player = GetPlayer();
	switch (O.Type)
	{
	case EStoryObjectiveType::Ground:
	case EStoryObjectiveType::Flight:
		if (MirrorLock && Player)
		{
			const int32 M = FMath::RoundToInt(FVector::Dist(Player->GetActorLocation(), MirrorLock->GetActorLocation()) / 100.f);
			OutProgress = Player->IsFlying() && M > 12
				? FText::Format(LOCTEXT("LockLand", "{0} m  -  LAND AND GO ON FOOT"), FText::AsNumber(M))
				: FText::Format(LOCTEXT("LockBlue", "{0} / 6 BLUE  -  STEP ON A PLATE TO TURN A PILLAR"), FText::AsNumber(FMath::RoundToInt(MirrorLock->GetProgress() * 6.f)));
		}
		else if (Beacon && Player)
		{
			const int32 M = FMath::RoundToInt(FVector::Dist(Player->GetActorLocation(), Beacon->GetActorLocation()) / 100.f);
			OutProgress = (O.Type == EStoryObjectiveType::Ground && Player->IsFlying())
				? FText::Format(LOCTEXT("LandAt", "{0} m  -  LAND AND GO ON FOOT"), FText::AsNumber(M))
				: (O.Type == EStoryObjectiveType::Flight && !Player->IsFlying())
					? FText::Format(LOCTEXT("FlyTo", "{0} m  -  TAKE OFF (F)"), FText::AsNumber(M))
					: FText::Format(LOCTEXT("Meters", "{0} m"), FText::AsNumber(M));
		}
		break;
	case EStoryObjectiveType::Drone:
	case EStoryObjectiveType::Tank:
		OutProgress = FText::Format(LOCTEXT("Kills", "{0} / {1}"), FText::AsNumber(ObjectiveCount), FText::AsNumber(Need));
		break;
	case EStoryObjectiveType::Wave:
	case EStoryObjectiveType::Unknown:
		OutProgress = FText::Format(LOCTEXT("Waves", "WAVE {0} / {1}"), FText::AsNumber(FMath::Max(1, WaveNumber)), FText::AsNumber(Need));
		break;
	case EStoryObjectiveType::Boss:
		OutProgress = FText::Format(LOCTEXT("BossPct", "{0}%"), FText::AsNumber(FMath::RoundToInt(ObjectiveProgress * 100.f)));
		break;
	case EStoryObjectiveType::Hulkbuster:
		OutProgress = FText::Format(LOCTEXT("Deploys", "DEPLOYED {0} / {1}  (Q)"), FText::AsNumber(ObjectiveCount), FText::AsNumber(Need));
		break;
	case EStoryObjectiveType::Energy:
		OutProgress = FText::Format(LOCTEXT("Hold", "HOLD {0} s  -  forced landings: {1}"),
			FText::AsNumber(FMath::CeilToInt(FMath::Max(0.f, EnergyObjectiveSeconds - ObjectiveTimer))), FText::AsNumber(ForcedLandings));
		break;
	default:
		break;
	}
	return true;
}

FText AStoryDirector::GetAllyHint() const
{
	const FStoryBook& Story = GetBook();
	if (Step != EStoryStep::Objective || !Story.Chapters.IsValidIndex(ChapterIndex)) { return FText::GetEmpty(); }
	const FStoryChapter& C = Story.Chapters[ChapterIndex];
	if (!C.Objectives.IsValidIndex(ObjectiveIndex)) { return FText::GetEmpty(); }
	const FName AllyId = C.Objectives[ObjectiveIndex].AllyAvailable;
	if (AllyId.IsNone() || !HasAlly(AllyId)) { return FText::GetEmpty(); }
	const FText Label = UStoryLibrary::GetSpeakerLabel(AllyId, PlayerName);
	if (Ally) { return FText::Format(LOCTEXT("AllyActive", "{0}  {1} s"), Label, FText::AsNumber(FMath::CeilToInt(Ally->GetTimeLeft()))); }
	if (AllyCooldownLeft > 0.f) { return FText::Format(LOCTEXT("AllyCd", "{0} RECHARGING  {1} s"), Label, FText::AsNumber(FMath::CeilToInt(AllyCooldownLeft))); }
	return FText::Format(LOCTEXT("AllyReady", "G  SUMMON {0}"), Label);
}

bool AStoryDirector::GetBeaconLocation(FVector& Out) const
{
	if (MirrorLock) { Out = MirrorLock->GetActorLocation(); return true; }
	if (!Beacon) { return false; }
	Out = Beacon->GetActorLocation();
	return true;
}

// =============================================================================== tick

void AStoryDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AllyCooldownLeft > 0.f) { AllyCooldownLeft = FMath::Max(0.f, AllyCooldownLeft - DeltaSeconds); }
	if (Ally && Ally->IsActorBeingDestroyed()) { Ally = nullptr; }

	switch (Step)
	{
	case EStoryStep::Cutscene:
		TickCutscene(DeltaSeconds);
		break;
	case EStoryStep::SuitUp:
	{
		const AIronManCharacter* Player = GetPlayer();
		if (Player && !Player->IsInCinematic()) { StartObjective(0); }
		break;
	}
	case EStoryStep::Objective:
		TickObjective(DeltaSeconds);
		break;
	case EStoryStep::ObjectiveDone:
		GapTimer -= DeltaSeconds;
		if (GapTimer <= 0.f) { StartObjective(ObjectiveIndex + 1); }
		break;
	default:
		break;
	}
}

// =============================================================================== cutscenes

const FStoryCastMember* AStoryDirector::FindCast(FName Speaker) const
{
	const FString S = Speaker.ToString();
	for (const FStoryCastMember& M : CastList)
	{
		if (M.Speaker.ToString().Equals(S, ESearchCase::IgnoreCase)) { return &M; }
	}
	return nullptr;
}

bool AStoryDirector::ShotUsesHumanPlayer(const FStoryShot& Shot) const
{
	// Out of the suit: prologue, the workshop chapters and anything the director's note says
	// happens at the lake house / bench / with the helmet off.
	if (!Shots.Num() || !Shot.Characters.Contains(NAME_Player)) { return false; }
	const FStoryBook& Story = GetBook();
	const FString& Note = Shot.Camera;
	if (Has(Note, TEXT("suit up")) || Has(Note, TEXT("in the suit")) || Has(Note, TEXT("armor")) || Has(Note, TEXT("armour")) || Has(Note, TEXT("thruster")) || Has(Note, TEXT("flies")) || Has(Note, TEXT("hover")))
	{
		return false;
	}
	if (Has(Note, TEXT("lake house")) || Has(Note, TEXT("workshop")) || Has(Note, TEXT("bench")) || Has(Note, TEXT("garage"))
		|| Has(Note, TEXT("helmet off")) || Has(Note, TEXT("unmasked")) || Has(Note, TEXT("kitchen")) || Has(Note, TEXT("porch"))
		|| Has(Note, TEXT("teenager")) || Has(Note, TEXT("console")) || Has(Note, TEXT("dock")))
	{
		return true;
	}
	return !bInFinale && Story.Chapters.IsValidIndex(ChapterIndex) && Story.Chapters[ChapterIndex].Location == FName(TEXT("workshop"));
}

AStoryCharacterNPC* AStoryDirector::GetOrSpawnNpc(FName Speaker, bool bSuitedOverride)
{
	const FStoryBook& Story = GetBook();
	const bool bProloguePhase = bInPrologue;
	const bool bHologram = Has(ShotCamera, TEXT("hologram")) || Has(ShotCamera, TEXT("recording")) || Has(ShotCamera, TEXT("projection"));

	FStoryCastMember M;
	if (const FStoryCastMember* Found = FindCast(Speaker)) { M = *Found; }
	else { M.Speaker = Speaker; M.Body = EStoryBody::None; }

	FName Key = Speaker;
	if (bSuitedOverride)
	{
		// Finale: Morgan flies alongside in Rescue.
		M.Body = EStoryBody::Suit;
		M.SuitId = Story.Finale.AllySuit.IsNone() ? FName(TEXT("rescue")) : Story.Finale.AllySuit;
		M.Scale = 0.92f;
		Key = FName(*(Speaker.ToString() + TEXT("_SUIT")));
	}
	else if (Speaker == NAME_Tony && bProloguePhase && !M.MetaHumanClass.IsNull()
		&& FPackageName::DoesPackageExist(M.MetaHumanClass.GetLongPackageName()))
	{
		// A real Tony MetaHuman is installed: use it as-is (Human body spawns the Blueprint).
		Key = TEXT("TONY_MH");
	}
	else if (Speaker == NAME_Tony && bProloguePhase)
	{
		// Prologue: Tony is in the Mark 85 for the snap.
		M.Body = EStoryBody::Suit;
		M.SuitId = TEXT("mark-lxxxv");
		Key = TEXT("TONY_SUIT");
	}
	else if (Speaker == NAME_Tony && bHologram)
	{
		M.Body = EStoryBody::Hologram;
		Key = TEXT("TONY_HOLO");
	}
	if (M.Body == EStoryBody::None) { return nullptr; }

	if (TObjectPtr<AStoryCharacterNPC>* Existing = Npcs.Find(Key))
	{
		if (*Existing) { return *Existing; }
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStoryCharacterNPC* Npc = GetWorld()->SpawnActor<AStoryCharacterNPC>(AStoryCharacterNPC::StaticClass(), FTransform(StageOrigin + FVector(0, 0, 100)), SP);
	if (!Npc) { return nullptr; }
	Npc->SetupCast(M);
	Npc->SetActorHiddenInGame(true);
	Npcs.Add(Key, Npc);
	return Npc;
}

void AStoryDirector::PlayCutscene(const TArray<FStoryShot>& InShots, const FText& Title, const FText& Subtitle, TFunction<void()> OnDone)
{
	AIronManCharacter* Player = GetPlayer();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!Player || !PC || InShots.Num() == 0)
	{
		if (OnDone) { OnDone(); }
		return;
	}

	ClearHostiles();
	Shots = InShots;
	OnCutsceneDone = MoveTemp(OnDone);
	CutTitle = Title;
	CutSubtitle = Subtitle;
	Step = EStoryStep::Cutscene;

	if (UJarvisComponent* J = UJarvisComponent::Get(this)) { J->StopAll(); }
	Player->SetCinematicMode(true, nullptr);
	Player->CinematicSkipHandler = [this](bool bAll) { SkipCutscene(bAll); };

	// The stage: on the floor under the player, facing the player's yaw.
	float Floor = Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (const AArenaGameMode* GM = GetGM()) { Floor = FMath::Min(Floor, GM->GetArenaFloorZ() + 400.f); }
	StageOrigin = FVector(Player->GetActorLocation().X, Player->GetActorLocation().Y, Floor);
	StageYaw = Player->GetActorRotation().Yaw;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!Cam)
	{
		Cam = GetWorld()->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FTransform(StageOrigin + FVector(-400, 0, 200)), SP);
	}
	if (Cam)
	{
		if (UCineCameraComponent* CC = Cam->GetCineCameraComponent())
		{
			CC->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
			CC->SetCurrentFocalLength(35.f);
			// Vison's per-chapter camera package (filmback, lens, aperture, manual physical exposure).
			const UArenaGameUserSettings* GUS = UArenaGameUserSettings::GetArenaSettings();
			const bool bDOF = !GUS || GUS->bCinematicDOF;
			if (!LightingKey.IsNone()) { ContribLighting::ApplyCamera(CC, LightingKey, bDOF); }
			if (bInPrologue && bDOF)
			{
				// Flashback: long lens feel, very shallow depth of field (UpdateCamera keeps focus on the subject).
				CC->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
				CC->CurrentAperture = 1.4f;
			}
		}
		PC->SetViewTargetWithBlend(Cam, 0.6f);
	}

	if (!Overlay)
	{
		Overlay = CreateWidget<UCinematicOverlayWidget>(PC, UCinematicOverlayWidget::StaticClass());
		if (Overlay) { Overlay->AddToViewport(50); }
	}
	if (Overlay)
	{
		Overlay->SetInfo(Title, Subtitle, FText::GetEmpty(), StoryGold, true);
		Overlay->SetState(1.f, 1.f, 0.f);
		Overlay->SetCaption(FText::GetEmpty(), FText::GetEmpty(), FLinearColor::White);
	}

	if (ScoreAudio) { ScoreAudio->Stop(); ScoreAudio = nullptr; }
	if (bInPrologue && !FlashbackScore.IsNull() && FPackageName::DoesPackageExist(FlashbackScore.GetLongPackageName()))
	{
		if (USoundBase* ScoreSound = FlashbackScore.LoadSynchronous())
		{
			ScoreAudio = UGameplayStatics::SpawnSound2D(this, ScoreSound, 0.45f, 1.f, 0.f, nullptr, false, true);
			if (ScoreAudio) { ScoreAudio->FadeIn(4.f, 0.45f); }
		}
	}

	BeginShot(0);
}

void AStoryDirector::SpeakLine(const FStoryLine& Line, FText& OutSpeaker, FText& OutText, FLinearColor& OutColor, float& OutDuration)
{
	const FStoryBook& Story = GetBook();
	OutSpeaker = UStoryLibrary::GetSpeakerLabel(Line.Speaker, PlayerName);
	const FString Text = Resolve(Line.Text);
	OutText = FText::FromString(Text);
	OutColor = UStoryLibrary::GetSpeakerColor(Line.Speaker);

	if (VoiceAudio) { VoiceAudio->Stop(); VoiceAudio = nullptr; }
	USoundBase* Sound = UStoryLibrary::GetLineSound(Line, PlayerName, Story.PlayerNameDefault);
	float SoundLen = 0.f;
	if (Sound)
	{
		VoiceAudio = UGameplayStatics::CreateSound2D(this, Sound, VoiceVolume(), 1.f, 0.f, nullptr, false, false);
		if (VoiceAudio)
		{
			if (const UArenaGameUserSettings* Settings = UArenaGameUserSettings::GetArenaSettings())
			{
				if (USoundClass* VoiceClass = Settings->GetVoiceSoundClass()) { VoiceAudio->SoundClassOverride = VoiceClass; }
			}
			VoiceAudio->Play();
		}
		SoundLen = Sound->GetDuration();
		if (SoundLen > 60.f) { SoundLen = 0.f; } // looping / unknown
	}
	const float Reading = 1.2f + Text.Len() / FMath::Max(4.f, ReadingCharsPerSecond);
	OutDuration = SoundLen > 0.f ? SoundLen + 0.45f : Reading;
	if (!SubtitlesOn() && Sound)
	{
		OutSpeaker = FText::GetEmpty();
		OutText = FText::GetEmpty();
	}
}

void AStoryDirector::BeginShot(int32 Index)
{
	if (!Shots.IsValidIndex(Index)) { EndCutscene(); return; }
	const FStoryBook& Story = GetBook();
	const FStoryShot& Shot = Shots[Index];
	ShotIndex = Index;

	// Finale: ask for the hero name right before the first line that uses it.
	if (bInFinale && !bHeroPromptShown && Shot.bHasLine && Shot.Line.Text.Contains(TEXT("{heroName}"))
		&& (Story.Finale.bPromptHeroName || HeroName.IsEmpty()))
	{
		ShowHeroPrompt();
		return;
	}

	ShotTime = 0.f;
	ShotCamera = Resolve(Shot.Camera);
	StageShot(Shot);

	FText Speaker, Text;
	FLinearColor Color = FLinearColor::White;
	float Len = 2.6f;
	if (Shot.bHasLine)
	{
		SpeakLine(Shot.Line, Speaker, Text, Color, Len);
	}
	else if (VoiceAudio)
	{
		VoiceAudio->Stop();
		VoiceAudio = nullptr;
	}
	if (!Shot.bHasLine && !Shot.Sfx.IsEmpty() && SubtitlesOn())
	{
		Text = FText::FromString(FString::Printf(TEXT("[%s]"), *Shot.Sfx));
		Color = FLinearColor(0.7f, 0.7f, 0.75f);
	}
	ShotLength = FMath::Clamp(Len, MinShotSeconds, MaxShotSeconds);
	// The flashback breathes: every shot holds a little past its line.
	if (bInPrologue) { ShotLength = FMath::Clamp(Len + 1.5f, 4.5f, MaxShotSeconds + 2.f); }
	if (Overlay) { Overlay->SetCaption(Speaker, Text, Color); }
	UpdateCamera(0.f);
}

void AStoryDirector::StageShot(const FStoryShot& Shot)
{
	const FStoryBook& Story = GetBook();
	AIronManCharacter* Player = GetPlayer();
	if (!Player) { return; }

	for (auto& Pair : Npcs)
	{
		if (Pair.Value) { Pair.Value->SetActorHiddenInGame(true); Pair.Value->SetSpeaking(false); }
	}
	SpeakingNpc = nullptr;

	const bool bHumanPlayer = ShotUsesHumanPlayer(Shot);
	const bool bPawnInShot = Shot.Characters.Contains(NAME_Player) && !bHumanPlayer;
	Player->SetBodyHiddenForCinematic(!bPawnInShot);

	// Who stands where: the player (pawn or Harley) at the stage origin, everyone else in a
	// shallow arc in front of them, facing back.
	const FVector Fwd = FRotator(0.f, StageYaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, StageYaw + 90.f, 0.f).Vector();
	TArray<AStoryCharacterNPC*> Others;
	AStoryCharacterNPC* HarleyNpc = nullptr;
	for (const FName& Who : Shot.Characters)
	{
		if (Who == NAME_Player)
		{
			if (bHumanPlayer) { HarleyNpc = GetOrSpawnNpc(NAME_Player, false); }
			continue;
		}
		const bool bSuited = bInFinale && !Story.Finale.AllySpeaker.IsNone() && Who == Story.Finale.AllySpeaker
			&& !Has(ShotCamera, TEXT("chair")) && !Has(ShotCamera, TEXT("workshop"));
		if (AStoryCharacterNPC* N = GetOrSpawnNpc(Who, bSuited)) { Others.AddUnique(N); }
	}

	auto Place = [this](AStoryCharacterNPC* N, const FVector& Foot, float Yaw)
	{
		const float Half = N->GetCapsuleComponent() ? N->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
		N->SetActorLocationAndRotation(Foot + FVector(0, 0, Half), FRotator(0.f, Yaw, 0.f));
		N->SetActorHiddenInGame(false);
	};
	if (HarleyNpc) { Place(HarleyNpc, StageOrigin, StageYaw); }
	const bool bAnyoneAtOrigin = HarleyNpc || bPawnInShot;
	for (int32 i = 0; i < Others.Num(); ++i)
	{
		const float Side = (i - (Others.Num() - 1) * 0.5f) * 150.f;
		const FVector Foot = bAnyoneAtOrigin ? StageOrigin + Fwd * 260.f + Right * Side : StageOrigin + Right * Side;
		Place(Others[i], Foot, bAnyoneAtOrigin ? StageYaw + 180.f - Side * 0.08f : StageYaw + 180.f);
	}

	// Look-at: the speaker, else the group, else an establishing point.
	const FName Speaker = Shot.bHasLine ? Shot.Line.Speaker : NAME_None;
	ShotLookAt = FVector::ZeroVector;
	if (Speaker == NAME_Player && bPawnInShot)
	{
		ShotLookAt = Player->GetActorLocation() + FVector(0, 0, 60.f);
	}
	else if (Speaker == NAME_Player && HarleyNpc)
	{
		SpeakingNpc = HarleyNpc;
	}
	else if (!Speaker.IsNone())
	{
		for (AStoryCharacterNPC* N : Others)
		{
			if (N->GetSpeaker() == Speaker) { SpeakingNpc = N; break; }
		}
	}
	if (SpeakingNpc)
	{
		SpeakingNpc->SetSpeaking(true);
		ShotLookAt = SpeakingNpc->GetFocusPoint();
	}
	if (ShotLookAt.IsZero())
	{
		TArray<FVector> Points;
		if (bPawnInShot) { Points.Add(Player->GetActorLocation() + FVector(0, 0, 50.f)); }
		if (HarleyNpc) { Points.Add(HarleyNpc->GetFocusPoint()); }
		for (AStoryCharacterNPC* N : Others) { Points.Add(N->GetFocusPoint()); }
		if (Points.Num() > 0)
		{
			FVector Sum = FVector::ZeroVector;
			for (const FVector& P : Points) { Sum += P; }
			ShotLookAt = Sum / Points.Num();
		}
		else
		{
			ShotLookAt = StageOrigin + Fwd * 300.f + FVector(0, 0, 220.f);
		}
	}

	// Framing from the director's note.
	const FString& C = ShotCamera;
	const bool bGroup = (bPawnInShot || HarleyNpc ? 1 : 0) + Others.Num() > 1;
	CamDist = bGroup ? 520.f : 380.f;
	CamHeight = 20.f;
	CamFocal = 35.f;
	CamYawOffset = ((ShotIndex % 3) - 1) * 28.f;
	bCamHandheld = Has(C, TEXT("handheld")) || Has(C, TEXT("shaky")) || Has(C, TEXT("shake"));
	bCamPush = Has(C, TEXT("push")) || Has(C, TEXT("dolly")) || Has(C, TEXT("slow zoom"));
	bCamOrbit = Has(C, TEXT("orbit")) || Has(C, TEXT("pan")) || Has(C, TEXT("circles")) || Has(C, TEXT("swivel"));
	bCamCrane = Has(C, TEXT("crane"));
	if (Has(C, TEXT("extreme close")) || Has(C, TEXT("macro"))) { CamDist = 90.f; CamFocal = 70.f; }
	else if (Has(C, TEXT("close"))) { CamDist = 150.f; CamFocal = 50.f; }
	else if (Has(C, TEXT("medium")) || Has(C, TEXT("two-shot")) || Has(C, TEXT("two shot"))) { CamDist = 420.f; CamFocal = 35.f; }
	if (Has(C, TEXT("wide")) || Has(C, TEXT("establishing")) || Has(C, TEXT("skyline")) || Has(C, TEXT("battlefield")))
	{
		CamDist = 1100.f; CamHeight = 260.f; CamFocal = 20.f;
	}
	if (Has(C, TEXT("aerial")) || Has(C, TEXT("overhead")) || Has(C, TEXT("bird")) || bCamCrane)
	{
		CamDist = FMath::Max(CamDist, 800.f); CamHeight = 750.f; CamFocal = 22.f;
	}
	if (Has(C, TEXT("low angle")) || Has(C, TEXT("low-angle")) || Has(C, TEXT("from below"))) { CamHeight = -70.f; CamDist *= 0.85f; }
	if (Has(C, TEXT("over-the-shoulder")) || Has(C, TEXT("over the shoulder"))) { CamYawOffset = 65.f; CamDist = 230.f; CamHeight = 25.f; }
	if (Has(C, TEXT("rack focus"))) { CamFocal = 60.f; }
}

void AStoryDirector::UpdateCamera(float Alpha)
{
	if (!Cam) { return; }
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Ease = FMath::InterpEaseInOut(0.f, 1.f, A, 2.f);
	float Dist = CamDist;
	float Height = CamHeight;
	float Yaw = StageYaw + 90.f + CamYawOffset;
	if (bCamPush) { Dist *= FMath::Lerp(1.25f, 0.85f, Ease); }
	if (bCamOrbit) { Yaw += FMath::Lerp(-18.f, 18.f, Ease); }
	if (bCamCrane) { Height *= FMath::Lerp(1.4f, 0.75f, Ease); }
	else { Dist *= FMath::Lerp(1.04f, 0.98f, Ease); } // gentle drift so no shot is static

	FVector Pos = ShotLookAt - FRotator(0.f, Yaw, 0.f).Vector() * Dist + FVector(0, 0, Height);
	Pos.Z = FMath::Max(Pos.Z, StageOrigin.Z + 25.f);
	FRotator Rot = (ShotLookAt - Pos).Rotation();
	if (bCamHandheld)
	{
		const float T = ShotTime;
		Pos += FVector(FMath::Sin(T * 1.7f) * 4.f, FMath::Sin(T * 2.3f + 1.f) * 4.f, FMath::Sin(T * 3.1f) * 3.f);
		Rot.Pitch += FMath::Sin(T * 2.9f) * 0.8f;
		Rot.Yaw += FMath::Sin(T * 1.9f + 2.f) * 0.8f;
		Rot.Roll += FMath::Sin(T * 1.3f) * 0.6f;
	}
	Cam->SetActorLocationAndRotation(Pos, Rot);
	if (UCineCameraComponent* CC = Cam->GetCineCameraComponent())
	{
		CC->SetCurrentFocalLength(CamFocal);
		if (CC->FocusSettings.FocusMethod == ECameraFocusMethod::Manual)
		{
			CC->FocusSettings.ManualFocusDistance = FMath::Max(50.f, FVector::Dist(Pos, ShotLookAt));
		}
	}
}

void AStoryDirector::TickCutscene(float DeltaSeconds)
{
	ShotTime += DeltaSeconds;
	UpdateCamera(ShotLength > 0.f ? ShotTime / ShotLength : 1.f);
	if (Overlay)
	{
		// Chapter title card over the first shot only.
		const float TitleAlpha = ShotIndex == 0 ? FMath::Clamp(FMath::Min(ShotTime / 0.4f, (3.5f - ShotTime) / 0.6f), 0.f, 1.f) : 0.f;
		Overlay->SetState(1.f, TitleAlpha, 0.f);
	}
	if (bInPrologue) { TickFlashback(DeltaSeconds); }
	if (ShotTime >= ShotLength) { BeginShot(ShotIndex + 1); }
}

void AStoryDirector::TickFlashback(float DeltaSeconds)
{
	const float A = ShotLength > 0.f ? FMath::Clamp(ShotTime / ShotLength, 0.f, 1.f) : 1.f;
	const FString& C = ShotCamera;
	// Target reactor level from the director's note; it never comes back once it has started to die.
	float Target = FlashbackReactor;
	if (Has(C, TEXT("single blue point")) || Has(C, TEXT("shrinking"))) { Target = FMath::Lerp(0.35f, 0.f, A); }
	else if (Has(C, TEXT("reactor light fading")) || Has(C, TEXT("fading"))) { Target = FMath::Lerp(0.7f, 0.35f, A); }
	else if (Has(C, TEXT("slumped"))) { Target = 0.75f; }
	else if (ShotIndex > 0 && FlashbackReactor <= 0.01f) { Target = 0.f; }
	FlashbackReactor = FMath::Min(FlashbackReactor, Target);

	// Flicker: a heartbeat stutter that worsens as the core dies, with short dropouts.
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : ShotTime;
	const bool bStutter = Has(C, TEXT("stutter")) || Has(C, TEXT("flicker")) || FlashbackReactor < 0.99f;
	float Flicker = 1.f;
	if (bStutter && FlashbackReactor > 0.f)
	{
		Flicker = 0.78f + 0.22f * FMath::Sin(T * 29.f) * FMath::Sin(T * 6.3f);
		if (FMath::Frac(T * 1.7f) < 0.07f + 0.25f * (1.f - FlashbackReactor)) { Flicker *= 0.15f; }
	}
	const float Damage = ShotIndex == 0 ? 0.6f : 0.85f; // already battered; worse after the snap
	for (auto& Pair : Npcs)
	{
		if (Pair.Value && Pair.Key == FName(TEXT("TONY_SUIT"))) { Pair.Value->SetWounded(Damage, FlashbackReactor * Flicker); }
	}

	// "Years later" title card on the closing shot; the score swells, then fades with the picture.
	if (Overlay && Has(C, TEXT("years later")))
	{
		Overlay->SetInfo(LOCTEXT("YearsLater", "YEARS LATER"), LOCTEXT("YearsLaterSub", "Rural Tennessee"), FText::GetEmpty(), StoryGold, true);
		Overlay->SetState(1.f, FMath::Clamp(FMath::Min(ShotTime / 0.8f, (ShotLength - ShotTime) / 0.8f), 0.f, 1.f), 0.f);
		if (ScoreAudio && ShotLength - ShotTime < 2.5f) { ScoreAudio->FadeOut(2.5f, 0.f); ScoreAudio = nullptr; }
	}
	(void)DeltaSeconds;
}

void AStoryDirector::SkipCutscene(bool bAll)
{
	if (Step != EStoryStep::Cutscene) { return; }
	if (bAll) { EndCutscene(); return; }
	if (ShotTime > 0.35f) { BeginShot(ShotIndex + 1); }
}

void AStoryDirector::ShowHeroPrompt()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) { bHeroPromptShown = true; BeginShot(ShotIndex); return; }
	Step = EStoryStep::NamePrompt;
	if (FStoryChapterReport* Rep = CurrentReport()) { ++Rep->HeroPrompts; }
	if (VoiceAudio) { VoiceAudio->Stop(); VoiceAudio = nullptr; }
	if (Overlay) { Overlay->SetCaption(FText::GetEmpty(), FText::GetEmpty(), FLinearColor::White); }

	Prompt = CreateWidget<UStoryNamePromptWidget>(PC, UStoryNamePromptWidget::StaticClass());
	if (!Prompt) { bHeroPromptShown = true; Step = EStoryStep::Cutscene; BeginShot(ShotIndex); return; }
	Prompt->Setup(LOCTEXT("HeroTitle", "WHAT DOES THE WORLD CALL YOU?"),
		FText::FromString(Resolve(TEXT("Morgan: \"The press needs a name, {playerName}. Pick a good one. No pressure.\""))),
		HeroName.IsEmpty() ? FString(TEXT("Iron Lad")) : HeroName, 20);
	Prompt->OnSubmitted = [this](const FString& Name)
	{
		HeroName = Name;
		bHeroPromptShown = true;
		SaveProgress(ChapterIndex + 1, false);
		// (Submit() already moved this callback out of Prompt->OnSubmitted before calling it.)
		if (Prompt) { Prompt->RemoveFromParent(); Prompt = nullptr; }
		if (APlayerController* P = UGameplayStatics::GetPlayerController(this, 0))
		{
			P->SetInputMode(FInputModeGameOnly());
			P->SetShowMouseCursor(false);
		}
		Step = EStoryStep::Cutscene;
		BeginShot(ShotIndex);
	};
	Prompt->AddToViewport(60);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Prompt->TakeWidget());
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	Prompt->FocusInput(PC);
}

void AStoryDirector::EndCutscene()
{
	if (VoiceAudio) { VoiceAudio->Stop(); VoiceAudio = nullptr; }
	if (ScoreAudio) { ScoreAudio->FadeOut(1.5f, 0.f); ScoreAudio = nullptr; }
	for (auto& Pair : Npcs) { if (Pair.Value) { Pair.Value->Destroy(); } }
	Npcs.Reset();
	SpeakingNpc = nullptr;
	if (Overlay) { Overlay->RemoveFromParent(); Overlay = nullptr; }
	Shots.Reset();

	if (AIronManCharacter* Player = GetPlayer())
	{
		Player->CinematicSkipHandler = nullptr;
		Player->SetBodyHiddenForCinematic(false);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) { PC->SetViewTargetWithBlend(Player, 0.5f); }
		Player->SetCinematicMode(false, nullptr);
	}
	if (Cam) { Cam->Destroy(); Cam = nullptr; }
	Step = EStoryStep::Idle;

	TFunction<void()> Done = MoveTemp(OnCutsceneDone);
	OnCutsceneDone = nullptr;
	if (Done) { Done(); }
}

#undef LOCTEXT_NAMESPACE
