// CareerEconomy: loads Content/Story/city_economy.json (traffic caps, TOD multipliers, shop hours,
// casino limits, side jobs). Soft overlays on hardcoded Career:: tables.
// Credits only; realMoney forced false. Dum-E living-city lane (pass 2).
#pragma once

#include "CoreMinimal.h"

struct FCareerJobDef
{
	FName Id;
	FString Name;
	FString Blurb;
	int32 Payout = 0;
	/** Real hours before the same job can be claimed again. */
	float CooldownHours = 20.f;
	int32 BonusIfOwnsPlane = 0;
	int32 BonusIfOwnsBike = 0;
	/** Extra if the player owns any house. */
	int32 BonusIfOwnsHouse = 0;
};

namespace CareerEconomy
{
	IRONMANARENA_API void EnsureLoaded();
	IRONMANARENA_API void Reload();

	IRONMANARENA_API int32 MaxTraffic();
	IRONMANARENA_API int32 MaxPeds();
	IRONMANARENA_API bool DisableLifeDuringArenaSmoke();
	/** 0..1+ multiplier for traffic/ped caps at the given hour (0..24). */
	IRONMANARENA_API float TrafficMulForHour(float Hour);
	IRONMANARENA_API float PedMulForHour(float Hour);
	/** Chance a traffic car fully stops at an intersection instead of turning through. */
	IRONMANARENA_API float StopChanceAtIntersection();

	/** Barber / clothing / dealership open window (casino always open). */
	IRONMANARENA_API float ShopOpenHour();
	IRONMANARENA_API float ShopCloseHour();
	IRONMANARENA_API bool IsShopOpenAt(float Hour);

	IRONMANARENA_API int32 CasinoDailyLossLimit();
	IRONMANARENA_API int32 CasinoMinBet();
	IRONMANARENA_API int32 CasinoMaxBet();
	IRONMANARENA_API float CoinFlipWinChance();
	/** Chance the next card favors the player's High/Low call (house edge when < 0.5). */
	IRONMANARENA_API float HighLowWinChance();

	IRONMANARENA_API const TArray<FCareerJobDef>& Jobs();
	IRONMANARENA_API const FCareerJobDef* FindJob(FName Id);
}
