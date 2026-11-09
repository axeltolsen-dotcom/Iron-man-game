#include "WantedPressure.h"
#include "CityLife.h"
#include "EnemyBase.h"
#include "EngineUtils.h"

int32 WantedPressure::GetStars(const UObject* WorldContext)
{
	if (ACityLife* Life = ACityLife::Find(WorldContext)) { return Life->GetStars(); }
	return 0;
}

WantedPressure::FMul WantedPressure::Get(const UObject* WorldContext)
{
	return ForStars(GetStars(WorldContext));
}

int32 WantedPressure::CountNonResponderAlive(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return 0; }
	int32 N = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (!It->IsDead() && !CityLife::IsResponder(*It)) { ++N; }
	}
	return N;
}
