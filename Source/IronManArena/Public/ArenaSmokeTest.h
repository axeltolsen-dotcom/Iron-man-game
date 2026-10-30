// Headless arena smoke test for the packaged game:
//   IronManArena -ArenaSmoke [-ArenaSmokeWaves=N] [-ArenaSmokeQuit] -nullrhi -nosound -unattended
// Starts Arena mode from the menu path, skips the suit-up, then plays like a player would: faces
// the nearest enemy and holds the real repulsor trigger (projectiles, not direct damage). Records
// enemies spawned, enemy hits that landed on the player, player shots that damaged enemies and
// kills. If the shots have not cleared a wave after FallbackAfter seconds it finishes the wave with
// direct damage (logged, and counted separately). Player health is topped up so the run is
// deterministic. Logs "ARENA SMOKE RESULT PASS|FAIL ..." and, with -ArenaSmokeQuit, exits 0 / 1.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaSmokeTest.generated.h"

class AArenaGameMode;
class AIronManCharacter;
class AEnemyBase;

UCLASS(NotBlueprintable)
class IRONMANARENA_API AArenaSmokeTest : public AActor
{
	GENERATED_BODY()

public:
	AArenaSmokeTest();

	static AArenaSmokeTest* Launch(UWorld* World, int32 InWaves, bool bInQuit);

	virtual void Tick(float DeltaSeconds) override;

	bool IsFinished() const { return bFinished; }
	bool Passed() const { return bFinished && Failures.Num() == 0; }

	float FallbackAfter = 75.f;
	float DiagClock = 0.f;
	float Timeout = 400.f;

private:
	void Begin();
	void Drive(float DeltaSeconds);
	void Finish(const FString& Reason);
	void Fail(const FString& Msg) { Failures.Add(Msg); }
	AArenaGameMode* GetGM() const;
	AIronManCharacter* GetPlayer() const;

	TMap<TWeakObjectPtr<AEnemyBase>, float> SeenHealth;
	TArray<FString> Failures;
	int32 Waves = 1;
	bool bQuit = false;
	bool bStarted = false;
	bool bFinished = false;
	float StartDelay = 1.5f;
	float Clock = 0.f;
	float WaveClock = 0.f;
	int32 ActiveWave = 0;
	int32 WavesCleared = 0;
	int32 EnemiesSeen = 0;
	int32 DronesSeen = 0;
	int32 TanksSeen = 0;
	int32 ShotDamageEvents = 0;
	int32 Kills = 0;
	int32 FallbackKills = 0;
	int32 HitsOnPlayer = 0;
	float LastPlayerHealth = -1.f;
	bool bFallback = false;
};
