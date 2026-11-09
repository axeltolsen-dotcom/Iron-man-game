#include "CareerEconomy.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FTodBand
	{
		float NightMul = 0.45f;
		float RushMul = 1.35f;
		float MiddayMul = 1.f;
		float NightStart = 22.f;
		float NightEnd = 5.5f;
		float RushAM0 = 7.f, RushAM1 = 9.5f;
		float RushPM0 = 16.5f, RushPM1 = 19.f;
	};

	struct FEco
	{
		bool bTried = false;
		bool bOk = false;
		int32 MaxTraffic = 10;
		int32 MaxPeds = 14;
		bool bDisableSmoke = true;
		float StopChance = 0.45f;
		FTodBand Tod;
		float ShopOpen = 8.f;
		float ShopClose = 22.f;
		int32 DailyLoss = 5000;
		int32 MinBet = 10;
		int32 MaxBet = 1000;
		float CoinFlipWin = 0.48f;
		float HighLowWin = 0.47f;
		TArray<FCareerJobDef> Jobs;
	};

	FEco& Eco()
	{
		static FEco E;
		return E;
	}

	TArray<FCareerJobDef> BuiltInJobs()
	{
		return {
			{ FName(TEXT("scrap-run")), TEXT("Scrap Run"), TEXT("Haul a crate of workshop scrap across Midtown."), 350, 20.f, 0, 0, 0 },
			{ FName(TEXT("rooftop-relay")), TEXT("Rooftop Relay"), TEXT("Carry a spare reactor coil up to a tower helipad."), 500, 20.f, 0, 0, 0 },
			{ FName(TEXT("harbor-escort")), TEXT("Harbor Escort"), TEXT("Walk a shipment from the pier to Legacy Motors."), 275, 12.f, 0, 0, 0 },
			{ FName(TEXT("neon-tune")), TEXT("Neon Tune-Up"), TEXT("Fix a flickering storefront sign on Stark Street."), 200, 8.f, 0, 0, 0 },
			{ FName(TEXT("airstrip-fuel")), TEXT("Airstrip Fuel Run"), TEXT("Top off the Skylark tanks before dusk."), 650, 24.f, 150, 0, 0 },
			{ FName(TEXT("courier-lane")), TEXT("Courier Lane"), TEXT("Zip a sealed envelope through Midtown traffic."), 180, 6.f, 0, 80, 0 },
			{ FName(TEXT("plaza-sweep")), TEXT("Plaza Sweep"), TEXT("Clear drones and junk from Legacy Plaza after a wave."), 420, 14.f, 0, 0, 0 },
			{ FName(TEXT("night-watch")), TEXT("Night Watch Shift"), TEXT("Stand a quiet hour at the Arc Royale loading dock."), 300, 10.f, 0, 0, 0 },
			{ FName(TEXT("tenant-handyman")), TEXT("Tenant Handyman"), TEXT("Fix a sink in a building you own."), 240, 16.f, 0, 0, 120 },
		};
	}

	float MulAt(const FTodBand& T, float Hour)
	{
		Hour = FMath::Fmod(Hour + 24.f, 24.f);
		const bool bNight = (T.NightStart > T.NightEnd)
			? (Hour >= T.NightStart || Hour < T.NightEnd)
			: (Hour >= T.NightStart && Hour < T.NightEnd);
		if (bNight) { return T.NightMul; }
		if ((Hour >= T.RushAM0 && Hour < T.RushAM1) || (Hour >= T.RushPM0 && Hour < T.RushPM1)) { return T.RushMul; }
		return T.MiddayMul;
	}

	void Parse(FEco& E)
	{
		E.Jobs = BuiltInJobs();
		E.bOk = false;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/city_economy.json");
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
			|| !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("CareerEconomy: city_economy.json missing/invalid at %s — using built-ins"), *Path);
			return;
		}
		const TSharedPtr<FJsonObject>* Traffic = nullptr;
		if (Root->TryGetObjectField(TEXT("traffic"), Traffic) && Traffic && (*Traffic).IsValid())
		{
			(*Traffic)->TryGetNumberField(TEXT("maxCars"), E.MaxTraffic);
			(*Traffic)->TryGetNumberField(TEXT("maxPeds"), E.MaxPeds);
			(*Traffic)->TryGetBoolField(TEXT("disableDuringArenaSmoke"), E.bDisableSmoke);
			double Sc = E.StopChance;
			if ((*Traffic)->TryGetNumberField(TEXT("stopChanceAtIntersection"), Sc)) { E.StopChance = float(Sc); }
			const TSharedPtr<FJsonObject>* Tod = nullptr;
			if ((*Traffic)->TryGetObjectField(TEXT("timeOfDay"), Tod) && Tod && (*Tod).IsValid())
			{
				auto Num = [&](const TCHAR* K, float& Out) { double V = Out; if ((*Tod)->TryGetNumberField(K, V)) { Out = float(V); } };
				Num(TEXT("nightMul"), E.Tod.NightMul);
				Num(TEXT("rushMul"), E.Tod.RushMul);
				Num(TEXT("middayMul"), E.Tod.MiddayMul);
				Num(TEXT("nightStart"), E.Tod.NightStart);
				Num(TEXT("nightEnd"), E.Tod.NightEnd);
				Num(TEXT("rushMorningStart"), E.Tod.RushAM0);
				Num(TEXT("rushMorningEnd"), E.Tod.RushAM1);
				Num(TEXT("rushEveningStart"), E.Tod.RushPM0);
				Num(TEXT("rushEveningEnd"), E.Tod.RushPM1);
			}
		}
		const TSharedPtr<FJsonObject>* Shops = nullptr;
		if (Root->TryGetObjectField(TEXT("shops"), Shops) && Shops && (*Shops).IsValid())
		{
			double O = E.ShopOpen, C = E.ShopClose;
			if ((*Shops)->TryGetNumberField(TEXT("openHour"), O)) { E.ShopOpen = float(O); }
			if ((*Shops)->TryGetNumberField(TEXT("closeHour"), C)) { E.ShopClose = float(C); }
		}
		const TSharedPtr<FJsonObject>* Casino = nullptr;
		if (Root->TryGetObjectField(TEXT("casino"), Casino) && Casino && (*Casino).IsValid())
		{
			(*Casino)->TryGetNumberField(TEXT("dailyLossLimit"), E.DailyLoss);
			(*Casino)->TryGetNumberField(TEXT("minBet"), E.MinBet);
			(*Casino)->TryGetNumberField(TEXT("maxBet"), E.MaxBet);
			double Win = E.CoinFlipWin;
			if ((*Casino)->TryGetNumberField(TEXT("coinFlipWinChance"), Win)) { E.CoinFlipWin = float(Win); }
			Win = E.HighLowWin;
			if ((*Casino)->TryGetNumberField(TEXT("highLowWinChance"), Win)) { E.HighLowWin = float(Win); }
			bool bReal = false;
			if ((*Casino)->TryGetBoolField(TEXT("realMoney"), bReal) && bReal)
			{
				UE_LOG(LogTemp, Error, TEXT("CareerEconomy: casino.realMoney forced false"));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* JobArr = nullptr;
		if (Root->TryGetArrayField(TEXT("jobs"), JobArr) && JobArr && JobArr->Num() > 0)
		{
			TArray<FCareerJobDef> Parsed;
			for (const TSharedPtr<FJsonValue>& V : *JobArr)
			{
				const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
				if (!O.IsValid()) { continue; }
				FCareerJobDef J;
				FString Id;
				if (!O->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty()) { continue; }
				J.Id = FName(*Id);
				O->TryGetStringField(TEXT("name"), J.Name);
				O->TryGetStringField(TEXT("blurb"), J.Blurb);
				O->TryGetNumberField(TEXT("payout"), J.Payout);
				double Cd = J.CooldownHours;
				if (O->TryGetNumberField(TEXT("cooldownHours"), Cd)) { J.CooldownHours = float(Cd); }
				O->TryGetNumberField(TEXT("bonusIfOwnsPlane"), J.BonusIfOwnsPlane);
				O->TryGetNumberField(TEXT("bonusIfOwnsBike"), J.BonusIfOwnsBike);
				O->TryGetNumberField(TEXT("bonusIfOwnsHouse"), J.BonusIfOwnsHouse);
				if (J.Payout > 0) { Parsed.Add(J); }
			}
			if (Parsed.Num() > 0) { E.Jobs = MoveTemp(Parsed); }
		}
		E.MaxTraffic = FMath::Clamp(E.MaxTraffic, 0, 24);
		E.MaxPeds = FMath::Clamp(E.MaxPeds, 0, 32);
		E.StopChance = FMath::Clamp(E.StopChance, 0.f, 1.f);
		E.DailyLoss = FMath::Max(0, E.DailyLoss);
		E.MinBet = FMath::Max(1, E.MinBet);
		E.MaxBet = FMath::Max(E.MinBet, E.MaxBet);
		E.CoinFlipWin = FMath::Clamp(E.CoinFlipWin, 0.05f, 0.49f);
		E.HighLowWin = FMath::Clamp(E.HighLowWin, 0.05f, 0.49f);
		E.ShopOpen = FMath::Clamp(E.ShopOpen, 0.f, 23.9f);
		E.ShopClose = FMath::Clamp(E.ShopClose, 0.f, 24.f);
		E.bOk = true;
		UE_LOG(LogTemp, Log, TEXT("CareerEconomy: loaded %s v2 (%d jobs, traffic %d, peds %d)"), *Path, E.Jobs.Num(), E.MaxTraffic, E.MaxPeds);
	}
}

namespace CareerEconomy
{
	void EnsureLoaded()
	{
		FEco& E = Eco();
		if (E.bTried) { return; }
		E.bTried = true;
		Parse(E);
	}

	void Reload()
	{
		FEco& E = Eco();
		E.bTried = false;
		EnsureLoaded();
	}

	int32 MaxTraffic() { EnsureLoaded(); return Eco().MaxTraffic; }
	int32 MaxPeds() { EnsureLoaded(); return Eco().MaxPeds; }
	bool DisableLifeDuringArenaSmoke() { EnsureLoaded(); return Eco().bDisableSmoke; }
	float TrafficMulForHour(float Hour) { EnsureLoaded(); return MulAt(Eco().Tod, Hour); }
	float PedMulForHour(float Hour) { EnsureLoaded(); return MulAt(Eco().Tod, Hour); }
	float StopChanceAtIntersection() { EnsureLoaded(); return Eco().StopChance; }
	float ShopOpenHour() { EnsureLoaded(); return Eco().ShopOpen; }
	float ShopCloseHour() { EnsureLoaded(); return Eco().ShopClose; }

	bool IsShopOpenAt(float Hour)
	{
		EnsureLoaded();
		Hour = FMath::Fmod(Hour + 24.f, 24.f);
		const float O = Eco().ShopOpen, C = Eco().ShopClose;
		if (O <= C) { return Hour >= O && Hour < C; }
		return Hour >= O || Hour < C; // wraps midnight
	}

	int32 CasinoDailyLossLimit() { EnsureLoaded(); return Eco().DailyLoss; }
	int32 CasinoMinBet() { EnsureLoaded(); return Eco().MinBet; }
	int32 CasinoMaxBet() { EnsureLoaded(); return Eco().MaxBet; }
	float CoinFlipWinChance() { EnsureLoaded(); return Eco().CoinFlipWin; }
	float HighLowWinChance() { EnsureLoaded(); return Eco().HighLowWin; }
	const TArray<FCareerJobDef>& Jobs() { EnsureLoaded(); return Eco().Jobs; }

	const FCareerJobDef* FindJob(FName Id)
	{
		for (const FCareerJobDef& J : Jobs()) { if (J.Id == Id) { return &J; } }
		return nullptr;
	}
}
