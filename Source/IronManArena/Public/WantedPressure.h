// WantedPressure: star → fire/lead/orbit/move multipliers for CityLife responders.
// Extends ACityLife — do not invent a parallel wanted system. ULTRON.
#pragma once

#include "CoreMinimal.h"

class UObject;

namespace WantedPressure
{
	struct FMul
	{
		float FireInterval = 1.f;
		float Lead = 0.4f;
		float OrbitDist = 1.f;
		float TankMove = 1.f;
		float DroneSpring = 1.f;
		float RespawnSec = 6.f;
		int32 Cap = 2;
		int32 WantDrones = 2;
		int32 WantTanks = 0;
		int32 WantGunships = 0;
	};

	/** Stars 0 → identity (no pressure). Stars 1..5 from contrib/ultron/wanted_level/wanted_params.json. */
	inline FMul ForStars(int32 Stars)
	{
		static const FMul Table[6] = {
			{ 1.f, 0.4f, 1.f, 1.f, 1.f, 6.f, 0, 0, 0, 0 },
			{ 1.f, 0.4f, 1.f, 1.f, 1.f, 5.2f, 2, 2, 0, 0 },
			{ 0.92f, 0.55f, 0.95f, 1.f, 1.1f, 4.8f, 3, 3, 0, 0 },
			{ 0.85f, 0.7f, 0.9f, 1.1f, 1.2f, 4.4f, 5, 4, 1, 0 },
			{ 0.78f, 0.85f, 0.85f, 1.2f, 1.35f, 4.f, 6, 5, 1, 0 },
			{ 0.7f, 1.f, 0.8f, 1.3f, 1.5f, 3.75f, 7, 5, 1, 1 },
		};
		return Table[FMath::Clamp(Stars, 0, 5)];
	}

	/** Absolute scene cap from contrib/ultron/perf_budget.md. */
	constexpr int32 AbsoluteEnemyCap = 12;

	IRONMANARENA_API int32 GetStars(const UObject* WorldContext);
	IRONMANARENA_API FMul Get(const UObject* WorldContext);
	IRONMANARENA_API int32 CountNonResponderAlive(const UObject* WorldContext);
}
