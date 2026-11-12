// IronManArena.Suit.ReactiveState: Jocasta's reactive surface state math (build / decay / tiers / repair).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SuitReactiveState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuitReactiveStateTest, "IronManArena.Suit.ReactiveState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
	void SuitReactRun(SuitReactive::FState& S, const SuitReactive::FInputs& In, float Seconds, const SuitReactive::FTuning& T)
	{
		const int32 Steps = FMath::RoundToInt(Seconds / 0.1f);
		for (int32 i = 0; i < Steps; ++i) { SuitReactive::Step(S, In, 0.1f, T); }
	}
}

bool FSuitReactiveStateTest::RunTest(const FString& Parameters)
{
	const SuitReactive::FTuning& T = SuitReactive::Get();
	TestTrue(TEXT("suit_reactive_state.json loaded"), T.bLoadedFromFile);
	TestEqual(TEXT("rainPerSec"), T.RainPerSec, 0.25f, 1e-4f);
	TestEqual(TEXT("decayPerSec"), T.WetDecayPerSec, 0.04f, 1e-4f);
	TestEqual(TEXT("hardLanding grime"), T.GrimeHardLanding, 0.05f, 1e-4f);
	TestEqual(TEXT("repulsorShot scorch"), T.ScorchRepulsorShot, 0.002f, 1e-5f);
	TestEqual(TEXT("updateHz"), T.UpdateHz, 10.f, 1e-3f);
	TestEqual(TEXT("tier 3"), T.Tiers[3], 1.f, 1e-4f);

	// Wetness: 2 s of rain = 0.5, then 5 s dry = 0.3; a hot reactor dries it 3x faster; water soaks it.
	SuitReactive::FState S;
	SuitReactive::FInputs Rain; Rain.bRain = true; Rain.HealthFrac = -1.f;
	SuitReactive::FInputs Dry; Dry.HealthFrac = -1.f;
	SuitReactRun(S, Rain, 2.f, T);
	TestEqual(TEXT("rain 2 s"), S.Wetness, 0.5f, 0.011f);
	SuitReactRun(S, Dry, 5.f, T);
	TestEqual(TEXT("dry 5 s"), S.Wetness, 0.3f, 0.011f);
	SuitReactive::FInputs HotDry = Dry; HotDry.ReactorHeat = 0.9f;
	SuitReactive::FState Hot; Hot.Wetness = 0.6f;
	SuitReactRun(Hot, HotDry, 2.f, T);
	TestTrue(TEXT("hot reactor dries faster"), Hot.Wetness < 0.6f - 0.2f);
	SuitReactive::FInputs Water = Dry; Water.bWaterContact = true;
	SuitReactRun(S, Water, 0.1f, T);
	TestEqual(TEXT("water contact soaks"), S.Wetness, 1.f, 1e-4f);
	SuitReactRun(S, Rain, 30.f, T);
	TestTrue(TEXT("wetness clamped"), S.Wetness <= 1.f);

	// Grime: landings add, rain washes down to 50% (never below), ground objectives build per minute.
	SuitReactive::FState G;
	for (int32 i = 0; i < 8; ++i) { SuitReactive::AddGrime(G, T.GrimeHardLanding); }
	TestEqual(TEXT("8 hard landings"), G.GrimeLive, 0.4f, 1e-4f);
	SuitReactRun(G, Dry, 1.f, T);
	TestEqual(TEXT("grime does not decay dry"), G.GrimeLive, 0.4f, 1e-4f);
	SuitReactRun(G, Rain, 60.f, T);
	TestEqual(TEXT("rain washes grime to 50%"), G.GrimeLive, 0.2f, 1e-3f);
	SuitReactRun(G, Dry, 0.1f, T);
	SuitReactive::FInputs Ground = Dry; Ground.bGroundObjective = true;
	SuitReactRun(G, Ground, 60.f, T);
	TestEqual(TEXT("ground objective +0.08 / min"), G.GrimeLive, 0.28f, 0.005f);

	// Scorch persists; near explosions scale with distance.
	SuitReactive::FState Sc;
	for (int32 i = 0; i < 100; ++i) { SuitReactive::AddScorch(Sc, T.ScorchRepulsorShot); }
	TestEqual(TEXT("100 repulsor shots"), Sc.ScorchLive, 0.2f, 1e-3f);
	SuitReactRun(Sc, Rain, 30.f, T);
	TestEqual(TEXT("scorch persists in rain"), Sc.ScorchLive, 0.2f, 1e-3f);
	TestEqual(TEXT("explosion at the edge = full"), SuitReactive::NearExplosionFactor(200.f, 200.f, T), 1.f, 1e-4f);
	TestEqual(TEXT("explosion far away = none"), SuitReactive::NearExplosionFactor(10000.f, 200.f, T), 0.f, 1e-4f);
	const float Mid = SuitReactive::NearExplosionFactor(200.f + T.NearFullWithinCm + 0.5f * (T.NearFadeCm + T.NearFadeRadiusMul * 200.f), 200.f, T);
	TestEqual(TEXT("explosion falloff half way"), Mid, 0.5f, 1e-3f);

	// Heat tint lags reactor heat by lagSec (1 - 1/e after one lag).
	SuitReactive::FState H;
	SuitReactive::FInputs Heat = Dry; Heat.ReactorHeat = 1.f;
	SuitReactRun(H, Heat, T.HeatLagSec, T);
	TestEqual(TEXT("heat tint after one lag"), H.HeatTint, 1.f - FMath::Exp(-1.f), 0.02f);

	// Damage tiers from health lost; they only go up until repair.
	SuitReactive::FState D;
	SuitReactive::FInputs Hp = Dry;
	Hp.HealthFrac = 0.9f;  SuitReactRun(D, Hp, 0.1f, T); TestEqual(TEXT("10% lost = fresh"), D.DamageLevel, T.Tiers[0], 1e-4f);
	Hp.HealthFrac = 0.7f;  SuitReactRun(D, Hp, 0.1f, T); TestEqual(TEXT("30% lost = scuffed"), D.DamageLevel, T.Tiers[1], 1e-4f);
	Hp.HealthFrac = 1.0f;  SuitReactRun(D, Hp, 0.1f, T); TestEqual(TEXT("healing keeps the tier"), D.DamageLevel, T.Tiers[1], 1e-4f);
	Hp.HealthFrac = 0.45f; SuitReactRun(D, Hp, 0.1f, T); TestEqual(TEXT("55% lost = battered"), D.DamageLevel, T.Tiers[2], 1e-4f);
	Hp.HealthFrac = 0.2f;  SuitReactRun(D, Hp, 0.1f, T); TestEqual(TEXT("80% lost = critical"), D.DamageLevel, T.Tiers[3], 1e-4f);
	Hp.HealthFrac = -1.f;  SuitReactRun(D, Hp, 0.1f, T); TestEqual(TEXT("no health input keeps the tier"), D.DamageLevel, T.Tiers[3], 1e-4f);

	// Workshop repair clears everything persistent.
	D.GrimeLive = 0.5f; D.ScorchLive = 0.4f; D.Wetness = 0.7f;
	TestTrue(TEXT("needs repair"), SuitReactive::NeedsRepair(D));
	SuitReactive::Repair(D);
	TestFalse(TEXT("repaired"), SuitReactive::NeedsRepair(D));
	TestEqual(TEXT("repair dries"), D.Wetness, 0.f);
	TestEqual(TEXT("repair clears damage"), D.DamageLevel, 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
