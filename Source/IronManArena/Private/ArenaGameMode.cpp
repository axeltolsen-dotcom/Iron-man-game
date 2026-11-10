#include "ArenaGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "CityLayout.h"
#include "ArenaSmokeTest.h"
#include "WorkshopEasterEggs.h"
#include "StoryLibrary.h"
#include "StoryAutopilot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"

#include "IronManArena.h"
#include "IronManCharacter.h"
#include "ArenaPlayerController.h"
#include "ArenaSaveGame.h"
#include "ArenaEnvironment.h"
#include "ChapterSet.h"
#include "DroneEnemy.h"
#include "TankBot.h"
#include "BossTitan.h"
#include "IronProjectile.h"
#include "Pickup.h"
#include "ArenaBlastFX.h"
#include "JarvisComponent.h"
#include "SuitTypes.h"
#include "SuitUpCinematic.h"
#include "StoryDirector.h"
#include "ReactorComponent.h"

#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"

#define LOCTEXT_NAMESPACE "ArenaGameMode"

AArenaGameMode::AArenaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = AIronManCharacter::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();

	DroneClass = ADroneEnemy::StaticClass();
	TankClass = ATankBot::StaticClass();
	BossClass = ABossTitan::StaticClass();
	StoryDirectorClass = AStoryDirector::StaticClass();
}

void AArenaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	LoadHighScore(); // before any BeginPlay, so the title screen shows it
}

void AArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	BuildArenaIfNeeded();
	UStoryLibrary::LogStoryStatus();
	ArenaState = EArenaGameState::Menu;

	// Headless story test: -StoryAutoplay [-StoryAutoplayChapter=N] [-StoryAutoplayQuit] (see StoryAutopilot.h).
	if (FParse::Param(FCommandLine::Get(), TEXT("StoryAutoplay")))
	{
		int32 Chapter = 0;
		const bool bOne = FParse::Value(FCommandLine::Get(), TEXT("StoryAutoplayChapter="), Chapter);
		const int32 Last = UStoryLibrary::GetStory().Chapters.Num() - 1;
		AStoryAutopilot::Launch(GetWorld(), bOne ? Chapter - 1 : 0, bOne ? Chapter - 1 : Last, !bOne,
			FParse::Param(FCommandLine::Get(), TEXT("StoryAutoplayQuit")));
	}
	// Headless arena smoke test: -ArenaSmoke [-ArenaSmokeWaves=N] [-ArenaSmokeQuit] (see ArenaSmokeTest.h).
	if (FParse::Param(FCommandLine::Get(), TEXT("ArenaSmoke")))
	{
		int32 SmokeWaves = 1;
		FParse::Value(FCommandLine::Get(), TEXT("ArenaSmokeWaves="), SmokeWaves);
		AArenaSmokeTest::Launch(GetWorld(), SmokeWaves, FParse::Param(FCommandLine::Get(), TEXT("ArenaSmokeQuit")));
	}
	NextHulkbusterAt = HulkbusterScoreStep;
	ASuitUpCinematic::OnFinished.AddUObject(this, &AArenaGameMode::HandleSuitUpFinished);

	if (SelectedSuitIndex == INDEX_NONE)
	{
		const int32 Saved = LastSuitId.IsNone() ? INDEX_NONE : USuitLibrary::FindSuitIndex(LastSuitId);
		const int32 Default = USuitLibrary::FindSuitIndex(TEXT("mark-iii"));
		SelectedSuitIndex = Saved != INDEX_NONE ? Saved : (Default != INDEX_NONE ? Default : 0);
	}
}

void AArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ASuitUpCinematic::OnFinished.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void AArenaGameMode::SetSelectedSuitIndex(int32 SuitIndex)
{
	if (!USuitLibrary::GetAllSuits().IsValidIndex(SuitIndex)) { return; }
	SelectedSuitIndex = SuitIndex;
	LastSuitId = USuitLibrary::GetSuit(SuitIndex).Id;
	SaveHighScore(); // also stores the suit
}

bool AArenaGameMode::IsCinematicPlaying() const
{
	const AIronManCharacter* Player = GetPlayer();
	return Player && Player->IsInCinematic();
}

void AArenaGameMode::HandleSuitUpFinished(ASuitUpCinematic* Cinematic, bool bShortVersion)
{
	if (!Cinematic || Cinematic->GetWorld() != GetWorld()) { return; }
	if (!bShortVersion && ArenaState == EArenaGameState::Playing && Wave == 0)
	{
		ShowBanner(LOCTEXT("SuitUp", "SUIT UP"), LOCTEXT("SuitUpSub", "Hostiles inbound"), 1.5f);
		BreakTimer = FMath::Max(BreakTimer, 1.5f);
	}
}

// ----------------------------------------------------------------------------- arena

void AArenaGameMode::BuildArenaIfNeeded()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	AIronManCharacter* Player = GetPlayer();
	const FVector Probe = Player ? Player->GetActorLocation() : FVector::ZeroVector;

	// Is there any static geometry under the player / origin?
	FHitResult Hit;
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaFloorProbe), false);
	if (Player) { Params.AddIgnoredActor(Player); }
	// Start just above the pawn: from +50 m the probe began inside large level geometry (sky / volumes) and
	// reported a "floor" 50 m up, which lifted the whole runtime city above the player.
	bool bHasFloor = World->LineTraceSingleByObjectType(Hit, Probe + FVector(0.f, 0.f, 300.f), Probe - FVector(0.f, 0.f, 20000.f), Obj, Params);
	if (bHasFloor && (Hit.bStartPenetrating || Hit.ImpactNormal.Z < 0.5f)) { bHasFloor = false; }
	if (bHasFloor)
	{
		UE_LOG(LogIronManArena, Log, TEXT("Arena floor probe: hit %s at Z=%.0f"), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"), Hit.ImpactPoint.Z);
	}

	const bool bHasSun = static_cast<bool>(TActorIterator<ADirectionalLight>(World));

	if (bHasFloor)
	{
		ArenaCenter = FVector(Probe.X, Probe.Y, Hit.ImpactPoint.Z);
		ArenaFloorZ = Hit.ImpactPoint.Z;
	}
	else
	{
		float FloorTop = -100.f;
		if (Player && Player->GetCapsuleComponent())
		{
			FloorTop = Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f;
		}
		ArenaCenter = FVector(Probe.X, Probe.Y, FloorTop);
		ArenaFloorZ = FloorTop;
	}

	if (!bBuildArenaIfMissing || (bHasFloor && bHasSun))
	{
		return;
	}
	// L_IronManArena already places an ArenaEnvironment (floor, sun component, city). Its floor collision is off
	// while the city is shown and its sun is a component, so the probes above miss it; spawning a second
	// environment doubled the whole city (CityLayout_1), and story chapter sets only hide the first one, so the
	// live duplicate overlapped the ruins / battlefield sets (Hulkbuster NO CLEARANCE in story ch6 / ch9).
	if (TActorIterator<AArenaEnvironment> ExistingEnv(World); ExistingEnv)
	{
		UE_LOG(LogIronManArena, Log, TEXT("Arena: level already has %s - not building a runtime arena"), *ExistingEnv->GetName());
		return;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaEnvironment* Env = World->SpawnActor<AArenaEnvironment>(AArenaEnvironment::StaticClass(), FTransform(ArenaCenter), SP);
	if (!Env) { return; }
	if (bHasSun) { Env->RemoveLighting(); }
	if (bHasFloor) { Env->RemoveGeometry(); }
	UE_LOG(LogIronManArena, Log, TEXT("Built runtime arena (floor=%d, lights=%d) at Z=%.0f"), !bHasFloor, !bHasSun, ArenaFloorZ);
}

AIronManCharacter* AArenaGameMode::GetPlayer() const
{
	return Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

// ----------------------------------------------------------------------------- flow

void AArenaGameMode::ClearArena()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AEnemyBase> It(World); It; ++It) { ToDestroy.Add(*It); }
	for (TActorIterator<AIronProjectile> It(World); It; ++It) { ToDestroy.Add(*It); }
	for (TActorIterator<APickup> It(World); It; ++It) { ToDestroy.Add(*It); }
	for (TActorIterator<AArenaBlastFX> It(World); It; ++It) { ToDestroy.Add(*It); }
	for (AActor* A : ToDestroy) { if (IsValid(A)) { A->Destroy(); } }
}

void AArenaGameMode::ResetRunState()
{
	GetWorldTimerManager().ClearTimer(GameOverTimer);
	ClearArena();

	Score = 0;
	Wave = 0;
	Combo = 0;
	ComboTimeLeft = 0.f;
	Kills = 0;
	bNewHighScore = false;
	NextHulkbusterAt = HulkbusterScoreStep;
	SpawnQueue.Reset();
	bWaveActive = false;
	bWaveHasBoss = false;
	BreakTimer = 1.5f; // short breather before wave 1
	BannerTimeLeft = 0.f;
	bTankAnnouncedThisWave = false;
	bSwarmAnnouncedThisWave = false;
	bHighScoreAnnounced = false;
	ArenaState = EArenaGameState::Playing;
	bCityPlay = false;
	if (AIronManCharacter* Player = GetPlayer()) { Player->ClearPlayArea(); }
}

void AArenaGameMode::SetupCityPlay()
{
	const AArenaEnvironment* Env = AArenaEnvironment::Find(this);
	ACityLayout* City = Env ? Env->GetCityLayout() : nullptr;
	AIronManCharacter* Player = GetPlayer();
	FVector Center;
	float Radius = 0.f;
	FTransform Start;
	if (!City || !Player || !City->GetStoryLocation(TEXT("city"), Center, Radius) || !City->GetPlayerStart(TEXT("ps_plaza"), Start)) { return; }
	// city_layout.json puts ps_plaza ~657 m from the 'city' centre with a 600 m play radius; widen the ring so the
	// start (and the Legacy Plaza around it) is inside instead of being pushed back by the boundary.
	Radius = FMath::Max(Radius, float(FVector::Dist2D(Start.GetLocation(), Center)) + 5000.f);
	bCityPlay = true;
	CityPlayCenter = Center;
	CityPlayRadius = Radius;
	// A little slack past the spawn ring; the ceiling clears the Legacy tower crown.
	Player->SetPlayArea(Start, Center, Radius + 2000.f, Center.Z + 40000.f);

	TArray<TPair<FVector, FString>> Spots;
	City->GetPickups(Center, Radius, Spots);
	int32 Spawned = 0;
	for (const TPair<FVector, FString>& Spot : Spots)
	{
		EPickupType Type;
		if (Spot.Value == TEXT("Health")) { Type = EPickupType::Health; }
		else if (Spot.Value == TEXT("Energy")) { Type = EPickupType::Energy; }
		else if (Spot.Value == TEXT("Hulkbuster")) { Type = EPickupType::Hulkbuster; }
		else { continue; } // Gems belong to the story chapters.
		if (APickup::SpawnPickup(GetWorld(), Type, Spot.Key)) { ++Spawned; }
	}
	UE_LOG(LogIronManArena, Log, TEXT("City free play: radius %.0f m, %d pickups, city origin %s, start %s, centre %s."), Radius / 100.f, Spawned,
		*City->GetActorLocation().ToCompactString(), *Start.GetLocation().ToCompactString(), *Center.ToCompactString());
}

void AArenaGameMode::StartNewGame(int32 SuitIndex)
{
	if (SuitIndex != INDEX_NONE) { SetSelectedSuitIndex(SuitIndex); }
	if (StoryDirector) { StoryDirector->StopStory(); }
	PlayMode = EArenaPlayMode::Arena;
	ResetRunState();

	AIronManCharacter* Player = GetPlayer();
	if (!Player) { return; }
	SetupCityPlay(); // before ResetForNewGame so the player starts on the plaza
	Player->ResetForNewGame();
	// Dum-E's workshop easter eggs: once per world, near the start (never in smoke / story / automation runs).
	AWorkshopEasterEggs::SpawnForFreePlay(GetWorld(), Player);
	Player->SetAllowedSuits(TArray<FName>()); // every suit
	if (UReactorComponent* R = Player->GetReactor()) { R->ClearStage(); R->SetCoreDark(false); }
	if (UJarvisComponent* J = UJarvisComponent::Get(this)) { J->SetCommsSpeaker(NAME_None); }
	const int32 Suit = USuitLibrary::GetAllSuits().IsValidIndex(SelectedSuitIndex) ? SelectedSuitIndex : 0;

	// Full suit-up cinematic; waves start when it ends (TickWaves is gated on it).
	if (!ASuitUpCinematic::Play(Player, Suit, /*bShortVersion*/ false))
	{
		Player->ApplySuit(Suit);
		if (UJarvisComponent* J = UJarvisComponent::Get(this))
		{
			J->SetPersona(Player->GetSuit().AI, false);
			J->Say(EAIEvent::Start);
		}
		ShowBanner(LOCTEXT("SuitUp", "SUIT UP"), LOCTEXT("SuitUpSub", "Hostiles inbound"), 1.5f);
	}
}

AStoryDirector* AArenaGameMode::EnsureStoryDirector()
{
	if (StoryDirector) { return StoryDirector; }
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	UClass* Cls = (StoryDirectorClass.IsNull() || !FPackageName::DoesPackageExist(StoryDirectorClass.GetLongPackageName()))
		? nullptr : StoryDirectorClass.LoadSynchronous();
	if (!Cls) { Cls = AStoryDirector::StaticClass(); }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	StoryDirector = World->SpawnActor<AStoryDirector>(Cls, FTransform::Identity, SP);
	return StoryDirector;
}

void AArenaGameMode::StartStory(int32 ChapterIndex, bool bPlayPrologue)
{
	AIronManCharacter* Player = GetPlayer();
	AStoryDirector* Director = EnsureStoryDirector();
	if (!Player || !Director) { return; }

	const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotName);
	if (ChapterIndex < 0)
	{
		ChapterIndex = Save ? Save->StoryChapterReached : 0;
		bPlayPrologue = bPlayPrologue || !Save || !Save->bPrologueSeen;
	}

	Director->StopStory();
	PlayMode = EArenaPlayMode::Story;
	AWorkshopEasterEggs::RemoveAll(GetWorld());
	ResetRunState();
	Player->ResetForNewGame();
	Director->StartStory(ChapterIndex, bPlayPrologue, false);
}

bool AArenaGameMode::StartSideMission(FName MissionId, bool bIgnoreAvailability)
{
	AIronManCharacter* Player = GetPlayer();
	AStoryDirector* Director = EnsureStoryDirector();
	if (!Player || !Director) { return false; }
	Director->StopStory();
	PlayMode = EArenaPlayMode::Story;
	AWorkshopEasterEggs::RemoveAll(GetWorld());
	ResetRunState();
	Player->ResetForNewGame();
	if (!Director->StartSideMission(MissionId, false, bIgnoreAvailability))
	{
		ReturnToTitle();
		return false;
	}
	return true;
}

void AArenaGameMode::RestartCurrentMode()
{
	if (AIronManCharacter* P = GetPlayer()) { P->SimulateSkipKey(); } // B08: end suit-up before restart
	if (PlayMode == EArenaPlayMode::Story && StoryDirector && StoryDirector->IsSideMission())
	{
		AIronManCharacter* Player = GetPlayer();
		const FName Mission = StoryDirector->GetSideMissionId();
		StoryDirector->StopStory();
		ResetRunState();
		if (Player) { Player->ResetForNewGame(); }
		StoryDirector->StartSideMission(Mission, /*bSkipIntro*/ true, /*bIgnoreAvailability*/ true);
		return;
	}
	if (PlayMode == EArenaPlayMode::Story && StoryDirector)
	{
		AIronManCharacter* Player = GetPlayer();
		const int32 Chapter = StoryDirector->GetChapterIndex();
		StoryDirector->StopStory();
		ResetRunState();
		if (Player) { Player->ResetForNewGame(); }
		StoryDirector->StartStory(Chapter, false, /*bSkipIntro*/ true);
		return;
	}
	StartNewGame(INDEX_NONE);
}

void AArenaGameMode::ReturnToTitle()
{
	if (StoryDirector) { StoryDirector->StopStory(); }
	GetWorldTimerManager().ClearTimer(GameOverTimer);
	ClearArena();
	PlayMode = EArenaPlayMode::Arena;
	ArenaState = EArenaGameState::Menu;
	if (AIronManCharacter* Player = GetPlayer())
	{
		Player->SimulateSkipKey(); // B08: Finish ActiveCinematic so next New Game can Play again
		Player->SetAllowedSuits(TArray<FName>());
		if (UReactorComponent* R = Player->GetReactor()) { R->ClearStage(); R->SetCoreDark(false); }
	}
	if (UJarvisComponent* J = UJarvisComponent::Get(this)) { J->SetCommsSpeaker(NAME_None); J->StopAll(); }
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowMenu(EMenuMode::Title);
	}
}

int32 AArenaGameMode::GetStoryChapterReached() const
{
	const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotName);
	return Save ? Save->StoryChapterReached : 0;
}

bool AArenaGameMode::IsStoryComplete() const
{
	const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotName);
	return Save && Save->bStoryComplete;
}

void AArenaGameMode::OnPlayerDied()
{
	if (ArenaState != EArenaGameState::Playing) { return; }
	ArenaState = EArenaGameState::GameOver;
	Combo = 0;
	ComboTimeLeft = 0.f;
	if (PlayMode == EArenaPlayMode::Story)
	{
		if (StoryDirector) { StoryDirector->NotifyPlayerDied(); }
		ShowBanner(LOCTEXT("SuitDownStory", "SUIT CRITICAL"), LOCTEXT("SuitDownStorySub", "Morgan is rebooting the suit..."), GameOverDelay);
		GetWorldTimerManager().SetTimer(GameOverTimer, this, &AArenaGameMode::EnterGameOver, GameOverDelay, false);
		return;
	}
	if (Score > HighScore)
	{
		HighScore = Score;
		bNewHighScore = true;
	}
	BestWave = FMath::Max(BestWave, Wave);
	SaveHighScore();
	ShowBanner(LOCTEXT("SuitDown", "SUIT CRITICAL"), FText::GetEmpty(), GameOverDelay);
	GetWorldTimerManager().SetTimer(GameOverTimer, this, &AArenaGameMode::EnterGameOver, GameOverDelay, false);
}

void AArenaGameMode::EnterGameOver()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowMenu(EMenuMode::GameOver);
	}
}

// ----------------------------------------------------------------------------- scoring

void AArenaGameMode::AddScore(int32 Points)
{
	if (Points <= 0 || ArenaState != EArenaGameState::Playing) { return; }
	Score += Points;
	if (Score > HighScore && HighScore > 0 && !bNewHighScore)
	{
		bNewHighScore = true;
		ShowBanner(LOCTEXT("NewHigh", "NEW HIGH SCORE"), FText::GetEmpty(), 1.5f);
		if (!bHighScoreAnnounced)
		{
			bHighScoreAnnounced = true;
			UJarvisComponent::Notify(this, EAIEvent::HighScore);
		}
	}
	while (HulkbusterScoreStep > 0 && Score >= NextHulkbusterAt)
	{
		NextHulkbusterAt += HulkbusterScoreStep;
		if (AIronManCharacter* Player = GetPlayer())
		{
			Player->GrantHulkbusterCharge(FString::Printf(TEXT("%d POINTS"), NextHulkbusterAt - HulkbusterScoreStep));
		}
	}
}

void AArenaGameMode::OnEnemyKilled(AEnemyBase* Enemy)
{
	if (!Enemy || ArenaState != EArenaGameState::Playing) { return; }
	++Kills;
	Combo = (ComboTimeLeft > 0.f) ? FMath::Min(Combo + 1, ComboMax) : 1;
	ComboTimeLeft = ComboWindow;
	AddScore(Enemy->GetScoreValue() * Combo);

	if (PlayMode == EArenaPlayMode::Story && StoryDirector) { StoryDirector->NotifyEnemyKilled(Enemy); }

	if (Cast<ABossTitan>(Enemy)) { UJarvisComponent::Notify(this, EAIEvent::BossDefeated); }
	else if (Combo == 4 || Combo == ComboMax) { UJarvisComponent::Notify(this, EAIEvent::Combo); }
}

void AArenaGameMode::ShowBanner(const FText& Title, const FText& Subtitle, float Seconds)
{
	BannerTitle = Title;
	BannerSubtitle = Subtitle;
	BannerDuration = FMath::Max(0.1f, Seconds);
	BannerTimeLeft = BannerDuration;
}

bool AArenaGameMode::GetBanner(FText& OutTitle, FText& OutSubtitle, float& OutAlpha) const
{
	if (BannerTimeLeft <= 0.f || BannerTitle.IsEmpty()) { return false; }
	OutTitle = BannerTitle;
	OutSubtitle = BannerSubtitle;
	// Fade in for 0.15 s, fade out over the last 0.4 s.
	const float Elapsed = BannerDuration - BannerTimeLeft;
	OutAlpha = FMath::Clamp(FMath::Min(Elapsed / 0.15f, BannerTimeLeft / 0.4f), 0.f, 1.f);
	return true;
}

int32 AArenaGameMode::GetHostilesRemaining() const
{
	int32 Alive = 0;
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead()) { ++Alive; }
	}
	return Alive + SpawnQueue.Num();
}

ABossTitan* AArenaGameMode::GetActiveBoss() const
{
	for (TActorIterator<ABossTitan> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead()) { return *It; }
	}
	return nullptr;
}

// ----------------------------------------------------------------------------- waves

void AArenaGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (BannerTimeLeft > 0.f) { BannerTimeLeft -= DeltaSeconds; }

	if (ArenaState != EArenaGameState::Playing) { return; }
	if (IsCinematicPlaying()) { return; } // suit-up: everything holds

	if (ComboTimeLeft > 0.f)
	{
		ComboTimeLeft -= DeltaSeconds;
		if (ComboTimeLeft <= 0.f) { ComboTimeLeft = 0.f; Combo = 0; }
	}

	if (PlayMode == EArenaPlayMode::Story) { return; } // the story director spawns enemies
	TickWaves(DeltaSeconds);
}

void AArenaGameMode::StartWave(int32 NewWave)
{
	Wave = NewWave;
	bWaveHasBoss = BossEvery > 0 && (Wave % BossEvery == 0);

	const int32 NumDrones = FMath::Max(2, FirstWaveDrones + (Wave - 1) * DronesPerWave - (bWaveHasBoss ? 4 : 0));
	const int32 NumTanks = (Wave >= TanksFromWave)
		? FMath::Max(0, FMath::FloorToInt(1.f + (Wave - TanksFromWave) * TanksPerWave) - (bWaveHasBoss ? 1 : 0))
		: 0;

	SpawnQueue.Reset();
	if (bWaveHasBoss) { SpawnQueue.Add(EEnemyKind::Boss); }
	TArray<EEnemyKind> Rest;
	for (int32 i = 0; i < NumDrones; ++i) { Rest.Add(EEnemyKind::Drone); }
	for (int32 i = 0; i < NumTanks; ++i) { Rest.Add(EEnemyKind::Tank); }
	// Shuffle the non-boss part.
	for (int32 i = Rest.Num() - 1; i > 0; --i)
	{
		Rest.Swap(i, FMath::RandRange(0, i));
	}
	SpawnQueue.Append(Rest);

	SpawnTimer = 0.6f;
	bWaveActive = true;
	EnemyWatch.Reset();
	WatchdogTimer = 0.f;
	bTankAnnouncedThisWave = false;
	bSwarmAnnouncedThisWave = false;
	UJarvisComponent::Notify(this, bWaveHasBoss ? EAIEvent::BossIncoming : EAIEvent::WaveStart);

	if (bWaveHasBoss)
	{
		ShowBanner(FText::Format(LOCTEXT("WaveBoss", "WAVE {0}"), FText::AsNumber(Wave)),
			LOCTEXT("WaveBossSub", "WARNING: OBSIDIAN TITAN INBOUND"), 3.f);
	}
	else
	{
		ShowBanner(FText::Format(LOCTEXT("Wave", "WAVE {0}"), FText::AsNumber(Wave)),
			FText::Format(LOCTEXT("WaveSub", "{0} hostiles"), FText::AsNumber(SpawnQueue.Num())), 2.2f);
	}
}

void AArenaGameMode::TickWaves(float DeltaSeconds)
{
	if (!bWaveActive)
	{
		BreakTimer -= DeltaSeconds;
		if (BreakTimer <= 0.f) { StartWave(Wave + 1); }
		return;
	}

	TickStuckWatchdog(DeltaSeconds);

	// Spawning.
	if (SpawnQueue.Num() > 0)
	{
		SpawnTimer -= DeltaSeconds;
		if (SpawnTimer <= 0.f)
		{
			int32 Alive = 0;
			for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It) { if (!It->IsDead()) { ++Alive; } }
			if (Alive < MaxAlive)
			{
				const EEnemyKind Kind = SpawnQueue[0];
				SpawnQueue.RemoveAt(0);
				SpawnFromQueue(Kind);
				if (!bSwarmAnnouncedThisWave && Kind == EEnemyKind::Drone && Alive + 1 >= 6)
				{
					bSwarmAnnouncedThisWave = true;
					UJarvisComponent::Notify(this, EAIEvent::DroneSwarm);
				}
				SpawnTimer = SpawnGap * FMath::FRandRange(0.6f, 1.4f) * FMath::Max(0.45f, 1.f - Wave * 0.05f);
				if (Kind == EEnemyKind::Boss) { SpawnTimer += 2.5f; } // let the boss make an entrance
			}
			else
			{
				SpawnTimer = 0.5f;
			}
		}
		return;
	}

	// Wave clear?
	if (GetHostilesRemaining() == 0)
	{
		bWaveActive = false;
		const int32 Bonus = WaveBonus * Wave * (bWaveHasBoss ? BossWaveMultiplier : 1);
		AddScore(Bonus);
		const bool bNextIsBoss = BossEvery > 0 && ((Wave + 1) % BossEvery == 0);
		if (AIronManCharacter* Player = GetPlayer())
		{
			Player->Heal(bNextIsBoss ? PreBossRepair : ClearRepair);
		}
		ShowBanner(FText::Format(LOCTEXT("Clear", "WAVE {0} CLEARED"), FText::AsNumber(Wave)),
			FText::Format(LOCTEXT("ClearSub", "+{0} bonus"), FText::AsNumber(Bonus)), 2.5f);
		BreakTimer = BreakTime;
	}
}

void AArenaGameMode::TickStuckWatchdog(float DeltaSeconds)
{
	WatchdogTimer -= DeltaSeconds;
	if (WatchdogTimer > 0.f) { return; }
	const float Step = 1.f;
	WatchdogTimer = Step;

	UWorld* World = GetWorld();
	if (!World) { return; }
	const AIronManCharacter* Player = GetPlayer();
	const FVector PlayCenter = bCityPlay ? CityPlayCenter : ArenaCenter;
	const float PlayRadius = bCityPlay ? CityPlayRadius : ArenaRadius;
	const float KillZ = World->GetWorldSettings() ? World->GetWorldSettings()->KillZ : -HALF_WORLD_MAX;
	const float StuckAfter = 20.f;      // seconds without moving or being hurt
	const float FarFromPlayer = 2500.f; // a still enemy right next to the player is just fighting

	TSet<TWeakObjectPtr<AEnemyBase>> Seen;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* E = *It;
		if (!E || E->IsDead() || !IsValid(E)) { continue; }
		Seen.Add(E);
		const FVector Loc = E->GetActorLocation();
		FEnemyWatch* W = EnemyWatch.Find(E);
		if (!W)
		{
			FEnemyWatch Fresh;
			Fresh.LastLocation = Loc;
			Fresh.LastHealth = E->GetHealth();
			EnemyWatch.Add(E, Fresh);
			continue;
		}

		const bool bBoss = E->GetKind() == EEnemyKind::Boss;
		const bool bOutOfBounds = Loc.Z < KillZ + 500.f || Loc.Z < ArenaFloorZ - 3000.f
			|| (PlayRadius > 0.f && FVector::Dist2D(Loc, PlayCenter) > PlayRadius * 1.5f + 2000.f);
		const bool bMoved = FVector::DistSquared(Loc, W->LastLocation) > FMath::Square(60.f);
		const bool bHurt = E->GetHealth() < W->LastHealth - KINDA_SMALL_NUMBER;
		const bool bNearPlayer = Player && FVector::Dist(Loc, Player->GetActorLocation()) < FarFromPlayer;
		W->IdleSeconds = (bMoved || bHurt || bNearPlayer || bBoss) ? 0.f : W->IdleSeconds + Step;
		W->LastLocation = Loc;
		W->LastHealth = E->GetHealth();

		if (!bOutOfBounds && W->IdleSeconds < StuckAfter) { continue; }

		if (W->Relocations < 2)
		{
			const bool bDrone = E->GetKind() == EEnemyKind::Drone;
			const FVector To = bBoss ? PickSpawnLocation(2000.f, 2400.f, 2100.f)
				: PickSpawnLocation(SpawnDistance.X, SpawnDistance.Y,
					bDrone ? FMath::FRandRange(DroneSpawnHeight.X, DroneSpawnHeight.Y) : 160.f);
			E->SetActorLocation(To, false, nullptr, ETeleportType::TeleportPhysics);
			++W->Relocations;
			W->IdleSeconds = 0.f;
			W->LastLocation = To;
			UE_LOG(LogTemp, Log, TEXT("Wave watchdog: relocated %s (%s, try %d)"), *E->GetName(),
				bOutOfBounds ? TEXT("out of bounds") : TEXT("stuck"), W->Relocations);
		}
		else
		{
			// Still stuck after two relocations: remove it so the wave can end (no score, no drops).
			UE_LOG(LogTemp, Log, TEXT("Wave watchdog: culled %s"), *E->GetName());
			EnemyWatch.Remove(E);
			Seen.Remove(E);
			E->Destroy();
		}
	}

	for (auto WIt = EnemyWatch.CreateIterator(); WIt; ++WIt)
	{
		if (!WIt.Key().IsValid() || !Seen.Contains(WIt.Key())) { WIt.RemoveCurrent(); }
	}
}

FVector AArenaGameMode::PickSpawnLocation(float MinDist, float MaxDist, float HeightAboveFloor) const
{
	const AIronManCharacter* Player = GetPlayer();
	const FVector PlayCenter = bCityPlay ? CityPlayCenter : ArenaCenter;
	const float PlayRadius = bCityPlay ? CityPlayRadius : ArenaRadius;
	const FVector Around = Player ? Player->GetActorLocation() : PlayCenter;

	// Floor snap ignores chapter sets (roofs, towers, ruins must not lift beacons / enemies onto their tops).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSpawnProbe), false);
	for (TActorIterator<AChapterSet> It(GetWorld()); It; ++It) { Params.AddIgnoredActor(*It); }
	// B19: ...but the final spot must not be inside any static geometry, chapter-set props included.
	FCollisionQueryParams ClearParams(SCENE_QUERY_STAT(ArenaSpawnClear), false);
	const float ClearRadius = FMath::Clamp(HeightAboveFloor - 20.f, 40.f, 120.f);
	const FCollisionShape ClearShape = FCollisionShape::MakeSphere(ClearRadius);

	FVector Best = PlayCenter;
	int32 BestSpotRank = -1; // spot rank, not the game score
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::FRandRange(MinDist, MaxDist);
		FVector P(Around.X + FMath::Cos(Angle) * Dist, Around.Y + FMath::Sin(Angle) * Dist, ArenaFloorZ);
		FVector2D Flat(P.X - PlayCenter.X, P.Y - PlayCenter.Y);
		const bool bInside = Flat.Size() <= PlayRadius;
		if (!bInside)
		{
			// Pull back inside the arena; accepted only if nothing better turns up.
			Flat = Flat.GetSafeNormal() * PlayRadius;
			P.X = PlayCenter.X + Flat.X;
			P.Y = PlayCenter.Y + Flat.Y;
		}

		// Snap to the actual floor if there is one.
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 6000.f), P - FVector(0.f, 0.f, 6000.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			P.Z = Hit.ImpactPoint.Z;
		}
		P.Z += HeightAboveFloor;

		const bool bClear = !GetWorld()->OverlapAnyTestByObjectType(P, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_WorldStatic), ClearShape, ClearParams);
		// Prefer clear + inside, then clear, then inside (old behaviour), then anything.
		const int32 SpotRank = (bClear ? 2 : 0) + (bInside ? 1 : 0);
		if (SpotRank > BestSpotRank) { Best = P; BestSpotRank = SpotRank; }
		if (SpotRank == 3) { break; }
	}
	if (BestSpotRank < 2)
	{
		UE_LOG(LogIronManArena, Verbose, TEXT("Spawn probe: no clear spot in 12 tries, using %s"), *Best.ToString());
	}
	return Best;
}

void AArenaGameMode::SpawnFromQueue(EEnemyKind Kind)
{
	// In the city, FRIDAY's authored spawn points (absolute, z final) replace the random ring.
	const AArenaEnvironment* Env = bCityPlay ? AArenaEnvironment::Find(this) : nullptr;
	if (ACityLayout* City = Env ? Env->GetCityLayout() : nullptr)
	{
		const AIronManCharacter* Player = GetPlayer();
		const FVector Near = Player ? Player->GetActorLocation() : CityPlayCenter;
		FVector Loc;
		bool bFound = false;
		switch (Kind)
		{
		case EEnemyKind::Drone:
			bFound = City->PickSpawn(ECitySpawnType::Drone, Near, SpawnDistance.X, FMath::Max(SpawnDistance.Y, 20000.f), CityPlayCenter, CityPlayRadius, Loc);
			break;
		case EEnemyKind::Tank:
			bFound = City->PickSpawn(ECitySpawnType::Tank, Near, SpawnDistance.X, FMath::Max(SpawnDistance.Y, 20000.f), CityPlayCenter, CityPlayRadius, Loc);
			break;
		case EEnemyKind::Boss:
			bFound = City->PickSpawn(ECitySpawnType::Boss, Near, 2000.f, 25000.f, CityPlayCenter, CityPlayRadius, Loc);
			if (bFound && FVector::Dist2D(Loc, Near) > 25000.f) { bFound = false; }
			break;
		}
		if (bFound)
		{
			SpawnEnemyAt(Kind == EEnemyKind::Drone ? DroneClass : (Kind == EEnemyKind::Tank ? TankClass : BossClass), Loc);
			if (Kind == EEnemyKind::Tank && !bTankAnnouncedThisWave)
			{
				bTankAnnouncedThisWave = true;
				UJarvisComponent::Notify(this, EAIEvent::TankSpotted);
			}
			return;
		}
	}
	switch (Kind)
	{
	case EEnemyKind::Drone:
		SpawnEnemyAt(DroneClass, PickSpawnLocation(SpawnDistance.X, SpawnDistance.Y, FMath::FRandRange(DroneSpawnHeight.X, DroneSpawnHeight.Y)));
		break;
	case EEnemyKind::Tank:
		SpawnEnemyAt(TankClass, PickSpawnLocation(SpawnDistance.X, SpawnDistance.Y, 160.f));
		if (!bTankAnnouncedThisWave)
		{
			bTankAnnouncedThisWave = true;
			UJarvisComponent::Notify(this, EAIEvent::TankSpotted);
		}
		break;
	case EEnemyKind::Boss:
		SpawnEnemyAt(BossClass, PickSpawnLocation(2000.f, 2400.f, 2100.f));
		break;
	}
}

AEnemyBase* AArenaGameMode::SpawnEnemyAt(TSubclassOf<AEnemyBase> EnemyClass, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || !*EnemyClass) { return nullptr; }

	FRotator Facing = FRotator::ZeroRotator;
	if (const AIronManCharacter* Player = GetPlayer())
	{
		Facing = FRotator(0.f, (Player->GetActorLocation() - Location).Rotation().Yaw, 0.f);
	}
	const FTransform Xform(Facing, Location);
	AEnemyBase* Enemy = World->SpawnActorDeferred<AEnemyBase>(EnemyClass, Xform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy) { return nullptr; }
	Enemy->InitForWave(FMath::Max(1, Wave));
	Enemy->FinishSpawning(Xform);

	AArenaBlastFX::Spawn(World, Location, Enemy->GetHitRadius() * 1.5f, FLinearColor(1.f, 0.2f, 0.1f), 0.35f);
	return Enemy;
}

// ----------------------------------------------------------------------------- save

void AArenaGameMode::LoadHighScore()
{
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		if (const UArenaSaveGame* Save = Cast<UArenaSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
		{
			HighScore = Save->HighScore;
			BestWave = Save->BestWave;
			LastSuitId = Save->LastSuitId;
		}
	}
}

void AArenaGameMode::SaveHighScore()
{
	// Load first so the story fields written by AStoryDirector survive.
	UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotName);
	if (!Save) { return; }
	Save->HighScore = HighScore;
	Save->BestWave = BestWave;
	Save->LastSuitId = LastSuitId;
	if (!Save->Write(SaveSlotName))
	{
		UE_LOG(LogIronManArena, Warning, TEXT("Failed to save high score to slot %s"), *SaveSlotName);
	}
}

#undef LOCTEXT_NAMESPACE
