#include "ArenaSmokeTest.h"

#include "ArenaGameMode.h"
#include "ArenaPlayerController.h"
#include "DroneEnemy.h"
#include "EnemyBase.h"
#include "IronManArena.h"
#include "IronManCharacter.h"
#include "ReactorComponent.h"
#include "TankBot.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "HAL/PlatformMisc.h"

AArenaSmokeTest::AArenaSmokeTest()
{
	PrimaryActorTick.bCanEverTick = true;
	// The title menu pauses the game: tick anyway so Begin() can drive the menu (Drive waits for unpause).
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorHiddenInGame(true);
}

AArenaSmokeTest* AArenaSmokeTest::Launch(UWorld* World, int32 InWaves, bool bInQuit)
{
	if (!World) { return nullptr; }
	AArenaSmokeTest* T = World->SpawnActor<AArenaSmokeTest>();
	if (T)
	{
		T->Waves = FMath::Clamp(InWaves, 1, 20);
		T->bQuit = bInQuit;
		UE_LOG(LogIronManArena, Display, TEXT("ARENA SMOKE: launched on map %s (waves=%d)"), *World->GetMapName(), T->Waves);
	}
	return T;
}

AArenaGameMode* AArenaSmokeTest::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
}

AIronManCharacter* AArenaSmokeTest::GetPlayer() const
{
	return Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AArenaSmokeTest::Begin()
{
	bStarted = true;
	AArenaGameMode* GM = GetGM();
	AArenaPlayerController* PC = Cast<AArenaPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	AIronManCharacter* Player = GetPlayer();
	if (!GM || !PC || !Player) { Fail(TEXT("no game mode / controller / player pawn")); Finish(TEXT("setup")); return; }
	UE_LOG(LogIronManArena, Display, TEXT("ARENA SMOKE: player %s spawned at %s, floor Z=%.0f"), *Player->GetName(), *Player->GetActorLocation().ToCompactString(), GM->GetArenaFloorZ());
	PC->MenuStartGame();                              // title -> suit select (the menu path)
	PC->MenuConfirmSuit(GM->GetSelectedSuitIndex());  // "SUIT UP" on the selected suit
}

void AArenaSmokeTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) { return; }
	if (!bStarted)
	{
		StartDelay -= FMath::Max(DeltaSeconds, float(FApp::GetDeltaTime()));
		if (StartDelay <= 0.f) { Begin(); }
		return;
	}
	if (GetWorld() && GetWorld()->IsPaused()) { return; }
	Clock += DeltaSeconds;
	if (Clock > Timeout) { Fail(FString::Printf(TEXT("timeout after %.0f s"), Clock)); Finish(TEXT("timeout")); return; }
	Drive(DeltaSeconds);
}

void AArenaSmokeTest::Drive(float DeltaSeconds)
{
	AArenaGameMode* GM = GetGM();
	AIronManCharacter* Player = GetPlayer();
	if (!GM || !Player) { Fail(TEXT("game mode or player vanished")); Finish(TEXT("lost")); return; }
	if (GM->IsCinematicPlaying()) { Player->SimulateSkipKey(); return; }
	if (GM->GetPlayMode() != EArenaPlayMode::Arena) { Fail(TEXT("not in arena mode after MenuStartGame")); Finish(TEXT("mode")); return; }

	// Player damage from enemy attacks (then top up so the run stays deterministic).
	const float H = Player->GetHealth();
	if (LastPlayerHealth >= 0.f && H < LastPlayerHealth - 0.01f) { ++HitsOnPlayer; }
	if (Player->IsDead()) { Fail(TEXT("player died")); Finish(TEXT("died")); return; }
	Player->Heal(1000.f);
	LastPlayerHealth = Player->GetHealth();

	// Track enemies: new ones, damage taken, kills.
	AEnemyBase* Nearest = nullptr;
	float NearestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		AEnemyBase* E = *It;
		if (!SeenHealth.Contains(E))
		{
			SeenHealth.Add(E, E->GetHealth());
			++EnemiesSeen;
			if (E->IsA<ADroneEnemy>()) { ++DronesSeen; }
			if (E->IsA<ATankBot>()) { ++TanksSeen; }
		}
		float& Last = SeenHealth.FindChecked(E);
		if (E->GetHealth() < Last - 0.001f && !bFallback) { ++ShotDamageEvents; }
		Last = E->GetHealth();
		if (E->IsDead()) { continue; }
		const float D = float(FVector::Dist(E->GetAimPoint(), Player->GetActorLocation()));
		if (D < NearestDist) { NearestDist = D; Nearest = E; }
	}
	for (auto It = SeenHealth.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()) { if (bFallback) { ++FallbackKills; } else { ++Kills; } It.RemoveCurrent(); }
	}

	// Play: hover a little, face the nearest enemy, hold the trigger (vent when hot).
	WaveClock += DeltaSeconds;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (Nearest && PC)
	{
		const FVector Eye = Player->GetPawnViewLocation();
		// Lead the target like a player would (repulsor bolts fly at RepulsorSpeed).
		const float LeadTime = float(FVector::Dist(Nearest->GetAimPoint(), Eye)) / FMath::Max(Player->RepulsorSpeed, 100.f);
		PC->SetControlRotation((Nearest->GetAimPoint() + Nearest->GetVelocity() * LeadTime - Eye).Rotation());
		Player->SimulateFire(true);
		DiagClock -= DeltaSeconds;
		if (DiagClock <= 0.f)
		{
			DiagClock = 5.f;
			FHitResult LosHit;
			FCollisionQueryParams LosParams(SCENE_QUERY_STAT(SmokeLos), false, Player);
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(LosHit, Eye, Nearest->GetAimPoint(), ECC_Visibility, LosParams);
			const AActor* HitActor = bBlocked ? LosHit.GetActor() : nullptr;
			UE_LOG(LogIronManArena, Display, TEXT("ARENA SMOKE: t=%.0f player %s, nearest %s at %.0f m (%s), line of sight %s"),
				Clock, *Player->GetActorLocation().ToCompactString(), *Nearest->GetName(), NearestDist / 100.f, *Nearest->GetActorLocation().ToCompactString(),
				!bBlocked ? TEXT("clear") : HitActor == Nearest ? TEXT("on target") : HitActor ? *HitActor->GetName() : TEXT("blocked"));
		}
		if (UReactorComponent* R = Player->GetReactor()) { if (R->IsOverheated()) { Player->SimulateVentKey(); } }
		if (WaveClock > FallbackAfter && !bFallback)
		{
			bFallback = true;
			UE_LOG(LogIronManArena, Warning, TEXT("ARENA SMOKE: shots did not clear wave %d in %.0f s, finishing it with direct damage"), GM->GetWave(), FallbackAfter);
		}
		if (bFallback) { UGameplayStatics::ApplyDamage(Nearest, Nearest->GetMaxHealth() * 4.f + 1000.f, PC, Player, nullptr); }
	}
	else
	{
		Player->SimulateFire(false);
	}

	// Wave bookkeeping: a wave we saw active that is no longer active (or superseded) was cleared.
	const int32 W = GM->GetWave();
	if (GM->IsWaveActive() && ActiveWave == 0) { ActiveWave = W; WaveClock = 0.f; UE_LOG(LogIronManArena, Display, TEXT("ARENA SMOKE: wave %d started at %.1f s"), W, Clock); }
	if (ActiveWave > 0 && (!GM->IsWaveActive() || W > ActiveWave))
	{
		++WavesCleared;
		bFallback = false;
		UE_LOG(LogIronManArena, Display, TEXT("ARENA SMOKE: wave %d cleared at %.1f s (enemies seen %d, kills by shots %d, fallback kills %d, hits on player %d)"),
			ActiveWave, Clock, EnemiesSeen, Kills, FallbackKills, HitsOnPlayer);
		ActiveWave = 0;
		if (WavesCleared >= Waves) { Finish(TEXT("waves cleared")); }
	}
}

void AArenaSmokeTest::Finish(const FString& Reason)
{
	if (bFinished) { return; }
	bFinished = true;
	if (AIronManCharacter* Player = GetPlayer()) { Player->SimulateFire(false); }
	if (bStarted && Failures.Num() == 0)
	{
		if (EnemiesSeen == 0) { Fail(TEXT("no enemies spawned")); }
		if (ShotDamageEvents == 0) { Fail(TEXT("player repulsor shots never damaged an enemy")); }
		if (Kills == 0) { Fail(TEXT("no enemy was killed by player shots")); }
		if (WavesCleared < Waves) { Fail(FString::Printf(TEXT("only %d / %d waves cleared"), WavesCleared, Waves)); }
	}
	for (const FString& F : Failures) { UE_LOG(LogIronManArena, Error, TEXT("ARENA SMOKE FAIL: %s"), *F); }
	UE_LOG(LogIronManArena, Display, TEXT("ARENA SMOKE RESULT %s (%s) map=%s time=%.1fs waves=%d enemies=%d drones=%d tanks=%d shotDamage=%d shotKills=%d fallbackKills=%d hitsOnPlayer=%d"),
		Failures.Num() == 0 ? TEXT("PASS") : TEXT("FAIL"), *Reason, GetWorld() ? *GetWorld()->GetMapName() : TEXT("?"), Clock, WavesCleared,
		EnemiesSeen, DronesSeen, TanksSeen, ShotDamageEvents, Kills, FallbackKills, HitsOnPlayer);
	if (bQuit)
	{
		GLog->Flush();
		FPlatformMisc::RequestExitWithStatus(false, Failures.Num() == 0 ? 0 : 1);
	}
}
