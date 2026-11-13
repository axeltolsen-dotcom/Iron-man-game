#include "SuitReactiveState.h"
#include "IronManArena.h"
#include "IronManCharacter.h"
#include "ReactorComponent.h"
#include "ArenaVisuals.h"
#include "ArenaEnvironment.h"
#include "ArenaGameMode.h"
#include "ArenaSaveGame.h"
#include "StoryDirector.h"
#include "SuitRig.h"
#include "ArenaCombat.h"
#include "ArenaAudio.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// =====================================================================================
// Tuning: Content/Story/suit_reactive_state.json (Jocasta), loaded once
// =====================================================================================

namespace
{
	TSharedPtr<FJsonObject> SuitReactObj(const TSharedPtr<FJsonObject>& From, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return (From.IsValid() && From->TryGetObjectField(Field, Out) && Out) ? *Out : TSharedPtr<FJsonObject>();
	}

	void SuitReactNum(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, float& InOut)
	{
		double V = 0.0;
		if (From.IsValid() && From->TryGetNumberField(Field, V)) { InOut = float(V); }
	}

	void SuitReactArray(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, float* OutVals, int32 Count)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!From.IsValid() || !From->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < Count) { return; }
		for (int32 i = 0; i < Count; ++i) { OutVals[i] = float((*Arr)[i]->AsNumber()); }
	}

	void SuitReactColor(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, FLinearColor& InOut)
	{
		float Rgb[3] = { InOut.R, InOut.G, InOut.B };
		SuitReactArray(From, Field, Rgb, 3);
		InOut = FLinearColor(FMath::Clamp(Rgb[0], 0.f, 1.f), FMath::Clamp(Rgb[1], 0.f, 1.f), FMath::Clamp(Rgb[2], 0.f, 1.f), 1.f);
	}

	SuitReactive::FTuning SuitReactLoad()
	{
		SuitReactive::FTuning T;
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/suit_reactive_state.json");
		if (FFileHelper::LoadFileToString(Text, *Path))
		{
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				T.bLoadedFromFile = true;
				SuitReactNum(Root, TEXT("updateHz"), T.UpdateHz);
				const TSharedPtr<FJsonObject> Params = SuitReactObj(Root, TEXT("params"));

				const TSharedPtr<FJsonObject> Wet = SuitReactObj(Params, TEXT("Wetness"));
				const TSharedPtr<FJsonObject> WetBuild = SuitReactObj(Wet, TEXT("build"));
				SuitReactNum(WetBuild, TEXT("rainPerSec"), T.RainPerSec);
				SuitReactNum(WetBuild, TEXT("waterContact"), T.WaterContact);
				SuitReactNum(WetBuild, TEXT("lowFlightSprayPerSec"), T.LowSprayPerSec);
				SuitReactNum(Wet, TEXT("decayPerSec"), T.WetDecayPerSec);
				SuitReactNum(Wet, TEXT("decayPerSecHot"), T.WetDecayPerSecHot);
				SuitReactNum(Wet, TEXT("decayHotAbove"), T.WetHotAbove);
				SuitReactNum(Wet, TEXT("roughnessMul"), T.WetRoughnessMul);
				SuitReactNum(Wet, TEXT("albedoDarkenPainted"), T.WetAlbedoDarken);

				const TSharedPtr<FJsonObject> Grime = SuitReactObj(Params, TEXT("GrimeLive"));
				const TSharedPtr<FJsonObject> GrimeBuild = SuitReactObj(Grime, TEXT("build"));
				SuitReactNum(GrimeBuild, TEXT("hardLanding"), T.GrimeHardLanding);
				SuitReactNum(GrimeBuild, TEXT("groundObjectivePerMin"), T.GrimeGroundPerMin);
				SuitReactNum(GrimeBuild, TEXT("nearExplosion"), T.GrimeNearExplosion);
				SuitReactNum(Grime, TEXT("upFacingMul"), T.GrimeUpFacingMul);
				SuitReactNum(Grime, TEXT("rainWashTo"), T.GrimeRainWashTo);
				SuitReactNum(Grime, TEXT("rainWashPerSec"), T.GrimeRainWashPerSec);
				SuitReactNum(Grime, TEXT("maxDarken"), T.GrimeMaxDarken);
				SuitReactNum(Grime, TEXT("maxRoughnessAdd"), T.GrimeMaxRoughnessAdd);

				const TSharedPtr<FJsonObject> ScorchBuild = SuitReactObj(SuitReactObj(Params, TEXT("ScorchLive")), TEXT("build"));
				SuitReactNum(ScorchBuild, TEXT("repulsorShot"), T.ScorchRepulsorShot);
				SuitReactNum(ScorchBuild, TEXT("nearExplosion"), T.ScorchNearExplosion);
				SuitReactNum(ScorchBuild, TEXT("tankShellHit"), T.ScorchTankShell);

				const TSharedPtr<FJsonObject> Heat = SuitReactObj(Params, TEXT("HeatTint"));
				SuitReactNum(Heat, TEXT("lagSec"), T.HeatLagSec);
				SuitReactNum(Heat, TEXT("metallicThreshold"), T.HeatMetallicThreshold);
				SuitReactNum(Heat, TEXT("emissiveAbove"), T.HeatEmissiveAbove);
				SuitReactNum(Heat, TEXT("maxEmissive"), T.HeatMaxEmissive);

				const TSharedPtr<FJsonObject> Dmg = SuitReactObj(Params, TEXT("DamageLevel"));
				SuitReactArray(Dmg, TEXT("tiers"), T.Tiers, 4);
				SuitReactArray(Dmg, TEXT("healthLostForTier"), T.TierHealthLost, 3);
				SuitReactNum(Dmg, TEXT("edgeWearAddMax"), T.EdgeWearAddMax);

				const TSharedPtr<FJsonObject> Landing = SuitReactObj(SuitReactObj(Root, TEXT("motion")), TEXT("landing"));
				SuitReactNum(Landing, TEXT("softBelowMps"), T.SoftBelowMps);
				SuitReactNum(Landing, TEXT("hardBelowMps"), T.HardBelowMps);

				const TSharedPtr<FJsonObject> Near = SuitReactObj(Root, TEXT("nearExplosion"));
				SuitReactNum(Near, TEXT("fullWithinRadiusPlusCm"), T.NearFullWithinCm);
				SuitReactNum(Near, TEXT("fadeCm"), T.NearFadeCm);
				SuitReactNum(Near, TEXT("fadeRadiusMul"), T.NearFadeRadiusMul);

				const TSharedPtr<FJsonObject> Jocasta = SuitReactObj(SuitReactObj(Root, TEXT("allyChassis")), TEXT("jocasta"));
				SuitReactColor(Jocasta, TEXT("baseColorLinear"), T.AllyBaseColor);
				SuitReactNum(Jocasta, TEXT("metallic"), T.AllyMetallic);
				SuitReactNum(Jocasta, TEXT("roughness"), T.AllyRoughness);
				SuitReactNum(Jocasta, TEXT("clearcoat"), T.AllyClearCoat);
				SuitReactNum(Jocasta, TEXT("metalFlake"), T.AllyFlake);
				const TSharedPtr<FJsonObject> Seam = SuitReactObj(Jocasta, TEXT("seamEmissive"));
				SuitReactColor(Seam, TEXT("colorLinear"), T.AllySeamColor);
				SuitReactNum(Seam, TEXT("intensity"), T.AllySeamIntensity);
			}
		}

		// Sanity clamps (a bad edit must not soak / blacken the suit instantly or never let it dry).
		T.UpdateHz = FMath::Clamp(T.UpdateHz, 1.f, 60.f);
		T.RainPerSec = FMath::Clamp(T.RainPerSec, 0.f, 2.f);
		T.WaterContact = FMath::Clamp(T.WaterContact, 0.f, 1.f);
		T.LowSprayPerSec = FMath::Clamp(T.LowSprayPerSec, 0.f, 2.f);
		T.WetDecayPerSec = FMath::Clamp(T.WetDecayPerSec, 0.001f, 2.f);
		T.WetDecayPerSecHot = FMath::Clamp(T.WetDecayPerSecHot, T.WetDecayPerSec, 4.f);
		T.WetHotAbove = FMath::Clamp(T.WetHotAbove, 0.f, 1.f);
		T.WetRoughnessMul = FMath::Clamp(T.WetRoughnessMul, 0.f, 0.95f);
		T.WetAlbedoDarken = FMath::Clamp(T.WetAlbedoDarken, 0.f, 0.6f);
		T.GrimeHardLanding = FMath::Clamp(T.GrimeHardLanding, 0.f, 0.5f);
		T.GrimeGroundPerMin = FMath::Clamp(T.GrimeGroundPerMin, 0.f, 1.f);
		T.GrimeNearExplosion = FMath::Clamp(T.GrimeNearExplosion, 0.f, 0.5f);
		T.GrimeUpFacingMul = FMath::Clamp(T.GrimeUpFacingMul, 1.f, 4.f);
		T.GrimeRainWashTo = FMath::Clamp(T.GrimeRainWashTo, 0.f, 1.f);
		T.GrimeRainWashPerSec = FMath::Clamp(T.GrimeRainWashPerSec, 0.f, 1.f);
		T.GrimeMaxDarken = FMath::Clamp(T.GrimeMaxDarken, 0.f, 0.8f);
		T.GrimeMaxRoughnessAdd = FMath::Clamp(T.GrimeMaxRoughnessAdd, 0.f, 0.8f);
		T.ScorchRepulsorShot = FMath::Clamp(T.ScorchRepulsorShot, 0.f, 0.05f);
		T.ScorchNearExplosion = FMath::Clamp(T.ScorchNearExplosion, 0.f, 0.5f);
		T.ScorchTankShell = FMath::Clamp(T.ScorchTankShell, 0.f, 0.5f);
		T.HeatLagSec = FMath::Clamp(T.HeatLagSec, 0.05f, 10.f);
		T.HeatMetallicThreshold = FMath::Clamp(T.HeatMetallicThreshold, 0.f, 1.f);
		T.HeatEmissiveAbove = FMath::Clamp(T.HeatEmissiveAbove, 0.f, 0.99f);
		T.HeatMaxEmissive = FMath::Clamp(T.HeatMaxEmissive, 0.f, 4.f);
		for (float& V : T.Tiers) { V = FMath::Clamp(V, 0.f, 1.f); }
		for (int32 i = 1; i < 4; ++i) { T.Tiers[i] = FMath::Max(T.Tiers[i], T.Tiers[i - 1]); }
		for (int32 i = 0; i < 3; ++i) { T.TierHealthLost[i] = FMath::Clamp(T.TierHealthLost[i], i > 0 ? T.TierHealthLost[i - 1] : 0.01f, 1.f); }
		T.EdgeWearAddMax = FMath::Clamp(T.EdgeWearAddMax, 0.f, 1.f);
		T.SoftBelowMps = FMath::Clamp(T.SoftBelowMps, 1.f, 50.f);
		T.HardBelowMps = FMath::Clamp(T.HardBelowMps, T.SoftBelowMps, 80.f);
		T.NearFullWithinCm = FMath::Clamp(T.NearFullWithinCm, 0.f, 3000.f);
		T.NearFadeCm = FMath::Clamp(T.NearFadeCm, 50.f, 5000.f);
		T.NearFadeRadiusMul = FMath::Clamp(T.NearFadeRadiusMul, 0.f, 10.f);
		T.AllyMetallic = FMath::Clamp(T.AllyMetallic, 0.f, 1.f);
		T.AllyRoughness = FMath::Clamp(T.AllyRoughness, 0.02f, 1.f);
		T.AllyClearCoat = FMath::Clamp(T.AllyClearCoat, 0.f, 1.f);
		T.AllyFlake = FMath::Clamp(T.AllyFlake, 0.f, 1.f);
		T.AllySeamIntensity = FMath::Clamp(T.AllySeamIntensity, 0.f, 40.f);
		UE_LOG(LogIronManArena, Log, TEXT("Suit reactive state: %s (rain %.2f/s, dry %.2f/s, landing grime %.2f, repulsor scorch %.3f, %.0f Hz)"),
			T.bLoadedFromFile ? TEXT("suit_reactive_state.json") : TEXT("built-in defaults"),
			T.RainPerSec, T.WetDecayPerSec, T.GrimeHardLanding, T.ScorchRepulsorShot, T.UpdateHz);
		return T;
	}

	bool SuitReactHasScalar(const UMaterialInstanceDynamic* MID, const TCHAR* ParamName, float& OutValue)
	{
		return MID && MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(ParamName)), OutValue);
	}

	bool SuitReactNameHas(const FString& CompName, std::initializer_list<const TCHAR*> Tokens)
	{
		for (const TCHAR* Tok : Tokens) { if (CompName.Contains(Tok)) { return true; } }
		return false;
	}
}

namespace SuitReactive
{
	const FTuning& Get()
	{
		static const FTuning Tuning = SuitReactLoad();
		return Tuning;
	}

	float DamageTierFor(float HealthLost, const FTuning& T)
	{
		float Tier = T.Tiers[0];
		for (int32 i = 0; i < 3; ++i)
		{
			if (HealthLost >= T.TierHealthLost[i] - KINDA_SMALL_NUMBER) { Tier = T.Tiers[i + 1]; }
		}
		return Tier;
	}

	void AddGrime(FState& S, float Amount)
	{
		if (Amount <= 0.f) { return; }
		S.GrimeLive = FMath::Clamp(S.GrimeLive + Amount, 0.f, 1.f);
		S.GrimeUnwashed = FMath::Clamp(S.GrimeUnwashed + Amount, 0.f, 1.f);
	}

	void AddScorch(FState& S, float Amount)
	{
		if (Amount <= 0.f) { return; }
		S.ScorchLive = FMath::Clamp(S.ScorchLive + Amount, 0.f, 1.f);
	}

	float NearExplosionFactor(float Distance, float BlastRadius, const FTuning& T)
	{
		const float R = FMath::Max(0.f, BlastRadius);
		const float Full = R + T.NearFullWithinCm;
		const float Fade = FMath::Max(1.f, T.NearFadeCm + T.NearFadeRadiusMul * R);
		return 1.f - FMath::Clamp((Distance - Full) / Fade, 0.f, 1.f);
	}

	void Step(FState& S, const FInputs& In, float DeltaSeconds, const FTuning& T)
	{
		const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 1.f);
		// Wetness: soak in water, build in rain / spray, otherwise dry (faster on a hot reactor).
		if (In.bWaterContact) { S.Wetness = FMath::Max(S.Wetness, T.WaterContact); }
		const float Build = (In.bRain ? T.RainPerSec : 0.f) + (In.bLowSpray ? T.LowSprayPerSec : 0.f);
		if (Build > 0.f) { S.Wetness += Build * Dt; }
		else if (!In.bWaterContact) { S.Wetness -= (In.ReactorHeat > T.WetHotAbove ? T.WetDecayPerSecHot : T.WetDecayPerSec) * Dt; }
		S.Wetness = FMath::Clamp(S.Wetness, 0.f, 1.f);

		// Grime: ground objectives build it; rain washes it down to rainWashTo of what it was when the rain started.
		if (In.bGroundObjective) { AddGrime(S, T.GrimeGroundPerMin / 60.f * Dt); }
		if (In.bRain || In.bWaterContact)
		{
			const float Floor = T.GrimeRainWashTo * S.GrimeUnwashed;
			if (S.GrimeLive > Floor) { S.GrimeLive = FMath::Max(Floor, S.GrimeLive - T.GrimeRainWashPerSec * Dt); }
		}
		else
		{
			S.GrimeUnwashed = S.GrimeLive;
		}
		S.GrimeLive = FMath::Clamp(S.GrimeLive, 0.f, 1.f);

		// Heat tint follows the reactor with a first-order lag.
		const float Heat = FMath::Clamp(In.ReactorHeat, 0.f, 1.f);
		S.HeatTint += (Heat - S.HeatTint) * (1.f - FMath::Exp(-Dt / T.HeatLagSec));
		S.HeatTint = FMath::Clamp(S.HeatTint, 0.f, 1.f);

		// Damage tiers only ever go up until repair.
		if (In.HealthFrac >= 0.f)
		{
			S.DamageLevel = FMath::Max(S.DamageLevel, DamageTierFor(1.f - FMath::Clamp(In.HealthFrac, 0.f, 1.f), T));
		}
		S.ScorchLive = FMath::Clamp(S.ScorchLive, 0.f, 1.f);
	}

	void Repair(FState& S)
	{
		S.Wetness = 0.f;
		S.GrimeLive = 0.f;
		S.GrimeUnwashed = 0.f;
		S.ScorchLive = 0.f;
		S.DamageLevel = 0.f;
	}

	bool NeedsRepair(const FState& S)
	{
		return S.GrimeLive > 0.005f || S.ScorchLive > 0.005f || S.DamageLevel > 0.005f;
	}
}

// =====================================================================================
// USuitReactiveComponent
// =====================================================================================

USuitReactiveComponent::USuitReactiveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

AIronManCharacter* USuitReactiveComponent::GetSuitOwner() const
{
	return Cast<AIronManCharacter>(GetOwner());
}

bool USuitReactiveComponent::CanWriteSave() const
{
	// Headless smoke / autoplay / automation never touch the player's wear save.
	static const bool bHeadless = FParse::Param(FCommandLine::Get(), TEXT("ArenaSmoke")) || FParse::Param(FCommandLine::Get(), TEXT("StoryAutoplay"));
	const UWorld* W = GetWorld();
	return !bHeadless && !GIsAutomationTesting && W && W->IsGameWorld();
}

void USuitReactiveComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(1.f / SuitReactive::Get().UpdateHz);
	LoadSaved();
	if (const AIronManCharacter* Char = GetSuitOwner()) { SwitchSuit(Char->GetSuit().Id); }
}

void USuitReactiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSaveDirty) { WriteSave(); }
	Super::EndPlay(EndPlayReason);
}

void USuitReactiveComponent::LoadSaved()
{
	bLoaded = true;
	const UWorld* W = GetWorld();
	const AArenaGameMode* GM = W ? W->GetAuthGameMode<AArenaGameMode>() : nullptr;
	if (const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(GM ? GM->SaveSlotName : FString(TEXT("IronManArena"))))
	{
		Wear = Save->SuitReactiveWear;
	}
}

void USuitReactiveComponent::StashCurrent()
{
	if (TrackedSuit.IsNone()) { return; }
	FSuitReactiveSave& Entry = Wear.FindOrAdd(TrackedSuit);
	Entry.Grime = State.GrimeLive;
	Entry.Scorch = State.ScorchLive;
	Entry.Damage = State.DamageLevel;
}

void USuitReactiveComponent::WriteSave()
{
	StashCurrent();
	bSaveDirty = false;
	SaveTimer = 0.f;
	if (!CanWriteSave()) { return; }
	const UWorld* W = GetWorld();
	const AArenaGameMode* GM = W ? W->GetAuthGameMode<AArenaGameMode>() : nullptr;
	const FString SaveSlotId = GM ? GM->SaveSlotName : FString(TEXT("IronManArena"));
	// Re-read the slot so fields owned by the career / story stay as they are on disk.
	if (UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlotId))
	{
		Save->SuitReactiveWear = Wear;
		Save->Write(SaveSlotId);
	}
}

void USuitReactiveComponent::SwitchSuit(FName NewSuit)
{
	if (NewSuit == TrackedSuit) { return; }
	const bool bHadSuit = !TrackedSuit.IsNone();
	StashCurrent();
	TrackedSuit = NewSuit;
	const FSuitReactiveSave* Saved = Wear.Find(NewSuit);
	State.GrimeLive = Saved ? FMath::Clamp(Saved->Grime, 0.f, 1.f) : 0.f;
	State.GrimeUnwashed = State.GrimeLive;
	State.ScorchLive = Saved ? FMath::Clamp(Saved->Scorch, 0.f, 1.f) : 0.f;
	State.DamageLevel = Saved ? FMath::Clamp(Saved->Damage, 0.f, 1.f) : 0.f;
	if (DevDamageOverride >= 0.f) { State.DamageLevel = DevDamageOverride; } // ima.SuitDamage sticks across suit swaps
	PaintGeneration = MAX_uint32; // rebase (new suit = new deterministic damage panels)
	if (bHadSuit && bSaveDirty) { WriteSave(); }
	if (Saved && SuitReactive::NeedsRepair(State))
	{
		UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: %s restored from save (grime %.2f scorch %.2f damage %.2f)"),
			*NewSuit.ToString(), State.GrimeLive, State.ScorchLive, State.DamageLevel);
	}
}

void USuitReactiveComponent::GatherInputs(SuitReactive::FInputs& In)
{
	AIronManCharacter* Char = GetSuitOwner();
	UWorld* W = GetWorld();
	if (!Char || !W) { return; }
	const float TickStep = 1.f / SuitReactive::Get().UpdateHz;
	const AArenaEnvironment* Env = AArenaEnvironment::Find(Char);
	const bool bWetWeather = Env && Env->GetPreset().bWet;
	const FVector Here = Char->GetActorLocation();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(SuitReactiveTrace), false, Char);

	// Rain only reaches the suit in the open (re-checked once a second).
	ShelterTimer -= TickStep;
	if (bWetWeather && ShelterTimer <= 0.f)
	{
		ShelterTimer = 1.f;
		bSheltered = W->LineTraceTestByChannel(Here + FVector(0, 0, 100.f), Here + FVector(0, 0, 2500.f), ECC_Visibility, Q);
	}
	In.bRain = bWetWeather && !bSheltered;

	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	const APhysicsVolume* Vol = Move ? Move->GetPhysicsVolume() : nullptr;
	In.bWaterContact = (Move && Move->IsSwimming()) || (Vol && Vol->bWaterVolume);

	// Low fast flight over wet streets throws spray onto the boots and legs.
	const bool bFastLow = Char->IsFlying() && Char->GetVelocity().Size() > 1200.f;
	SprayTimer -= TickStep;
	if (bWetWeather && bFastLow && SprayTimer <= 0.f)
	{
		SprayTimer = 0.5f;
		bNearWetGround = W->LineTraceTestByChannel(Here, Here - FVector(0, 0, 450.f), ECC_Visibility, Q);
	}
	In.bLowSpray = bWetWeather && bFastLow && bNearWetGround;

	const AArenaGameMode* GM = W->GetAuthGameMode<AArenaGameMode>();
	const AStoryDirector* Director = GM ? GM->GetStoryDirector() : nullptr;
	const FStoryObjective* Obj = Director ? Director->GetCurrentObjective() : nullptr;
	In.bGroundObjective = Obj && Obj->Type == EStoryObjectiveType::Ground && !Char->IsFlying() && Move && Move->IsMovingOnGround();

	In.ReactorHeat = Char->Reactor ? Char->Reactor->GetHeatFraction() : 0.f;
	In.HealthFrac = (Char->MaxHealth > 0.f && !Char->bGodMode) ? Char->Health / Char->MaxHealth : -1.f;
}

void USuitReactiveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AIronManCharacter* Char = GetSuitOwner();
	UWorld* W = GetWorld();
	if (!Char || !W) { return; }
	const double Now = W->GetTimeSeconds();
	const float Dt = LastTickTime < 0.0 ? DeltaTime : float(Now - LastTickTime);
	LastTickTime = Now;

	SwitchSuit(Char->GetSuit().Id);

	// The pilot on foot (suit off) and cinematics don't weather the suit; heat / drying still run.
	SuitReactive::FInputs In;
	if (!Char->bCareerSuitOff && !Char->bInCinematic && !Char->bDead) { GatherInputs(In); }
	else { In.ReactorHeat = Char->Reactor ? Char->Reactor->GetHeatFraction() : 0.f; In.HealthFrac = -1.f; }

	const SuitReactive::FState Before = State;
	SuitReactive::Step(State, In, Dt, SuitReactive::Get());
	if (FMath::Abs(Before.GrimeLive - State.GrimeLive) > 0.f || FMath::Abs(Before.DamageLevel - State.DamageLevel) > 0.f
		|| FMath::Abs(Before.ScorchLive - State.ScorchLive) > 0.f)
	{
		bSaveDirty = true;
	}
	if (State.DamageLevel > Before.DamageLevel + 0.01f)
	{
		UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: %s damage tier %.2f"), *TrackedSuit.ToString(), State.DamageLevel);
	}

	SaveTimer += Dt;
	if (bSaveDirty && SaveTimer >= 20.f) { WriteSave(); }

	PushToMaterials(false);
	TickDamageFX(Dt, Char->HitFlashLeft > 0.f || Char->CloakLeft > 0.f);
	TickServo(Dt);
}

// =====================================================================================
// Damage tiers 2 / 3: seam flicker, seam sparks, reactor stutter (suit_reactive_realism.md)
// =====================================================================================

namespace
{
	// Same naming rules as SuitRig's glow classifier (B13 CamelCase tokens).
	bool SuitReactGlowToken(const FString& N, const TCHAR* Tok)
	{
		const int32 Len = FCString::Strlen(Tok);
		for (int32 At = N.Find(Tok, ESearchCase::CaseSensitive); At != INDEX_NONE;
			At = N.Find(Tok, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			const int32 After = At + Len;
			if (After >= N.Len() || !FChar::IsLower(N[After])) { return true; }
		}
		return false;
	}

	/** 0 = skip (eyes, repulsors: driven elsewhere), 1 = seam / trim light, 2 = reactor core. */
	int32 SuitReactGlowKind(const FArenaPaintedPart& P)
	{
		const FString N = P.Component ? P.Component->GetName() : FString();
		if (N.Contains(TEXT("Eye")) || N.Contains(TEXT("Visor"))) { return 0; }
		if (N.Contains(TEXT("Palm")) || N.Contains(TEXT("Muzzle")) || N.Contains(TEXT("Repulsor")) || N.Contains(TEXT("Unibeam")) || N.Contains(TEXT("Thruster")) || N.Contains(TEXT("Flame"))) { return 0; }
		if (N.Contains(TEXT("Reactor")) || SuitReactGlowToken(N, TEXT("Core")) || SuitReactGlowToken(N, TEXT("Arc")) || SuitReactGlowToken(N, TEXT("Tri"))) { return 2; }
		return 1;
	}

	/** Seam light brightness at tier 2+: steady most of the time, a brownout dip once every 2 s (0.5 Hz) with a double blink. */
	float SuitReactSeamFlicker(float T)
	{
		const float Phase = FMath::Fmod(T, 2.f);
		// Windows are >= 0.15 s so the 10 Hz component tick always lands in them.
		if (Phase < 0.2f) { return 0.2f; }
		if (Phase < 0.35f) { return 0.85f; }
		if (Phase < 0.5f) { return 0.3f; }
		return 0.9f + 0.1f * FMath::Sin(T * 9.f);
	}
}

#if !UE_BUILD_SHIPPING
// Dev: preview the damage tiers without taking hits ("ima.SuitDamage 1" = critical, 0 = repaired).
static FAutoConsoleCommandWithWorldAndArgs GSuitReactDamageCmd(
	TEXT("ima.SuitDamage"),
	TEXT("Set the player suit's damage level (0, 0.33, 0.66, 1). 1 = critical: seam flicker, sparks, reactor stutter."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Level = Args.Num() > 0 ? FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 1.f) : 1.f;
		if (!World) { return; }
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (USuitReactiveComponent* Comp = Pawn ? Pawn->FindComponentByClass<USuitReactiveComponent>() : nullptr)
			{
				Comp->DevSetDamageLevel(Level);
			}
		}
	}));
#endif

void USuitReactiveComponent::DevSetDamageLevel(float Level)
{
	State.DamageLevel = FMath::Clamp(Level, 0.f, 1.f);
	DevDamageOverride = State.DamageLevel > 0.f ? State.DamageLevel : -1.f;
	if (State.DamageLevel <= 0.f && bGlowFXActive) { RestoreGlowFX(); }
	PushToMaterials(true);
	UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: %s damage level set to %.2f (ima.SuitDamage)"), *TrackedSuit.ToString(), State.DamageLevel);
}

void USuitReactiveComponent::RestoreGlowFX()
{
	AIronManCharacter* Char = GetSuitOwner();
	if (Char)
	{
		for (const FGlowFX& G : GlowFX)
		{
			TArray<FArenaPaintedPart>& Arr = G.bFP ? Char->FPParts : Char->Parts;
			if (Arr.IsValidIndex(G.Index) && Arr[G.Index].MID) { ArenaVisuals::SetPartGlow(Arr[G.Index], Arr[G.Index].Emissive); }
		}
		Char->ReactorStutterMul = 1.f;
	}
	bGlowFXActive = false;
	StutterLeft = 0.f;
}

void USuitReactiveComponent::TickDamageFX(float Dt, bool bPaintLocked)
{
	AIronManCharacter* Char = GetSuitOwner();
	if (!Char) { return; }
	const SuitReactive::FTuning& T = SuitReactive::Get();
	const float Dmg = State.DamageLevel;
	const bool bBattered = Dmg >= T.Tiers[2] - 0.01f && Dmg > 0.f;
	const bool bCritical = Dmg >= T.Tiers[3] - 0.01f && Dmg > 0.f;
	const bool bRun = bBattered && !Char->bCareerSuitOff && !Char->bInCinematic && !Char->bDead;

	// Rebuild the glow-part list when the suit is repainted / rebuilt.
	const uint32 Gen = SuitRig::GetPaintGeneration();
	const int32 PartCount = Char->Parts.Num() + Char->FPParts.Num();
	if (Gen != GlowFXGeneration || PartCount != GlowFXPartCount)
	{
		GlowFXGeneration = Gen;
		GlowFXPartCount = PartCount;
		GlowFX.Reset();
		bGlowFXActive = false;
		auto Collect = [this, Char](const TArray<FArenaPaintedPart>& Arr, bool bFP)
		{
			for (int32 i = 0; i < Arr.Num(); ++i)
			{
				const FArenaPaintedPart& P = Arr[i];
				if (!P.Component || !P.MID || P.Emissive <= 0.f) { continue; }
				// Makeshift mounted cores are driven every frame by the character (heat colour + flicker).
				if (!bFP && (i == Char->Rig.MountedCorePart || i == Char->Rig.ForearmCorePart)) { continue; }
				if (bFP && i == Char->Rig.FPMountedCorePart) { continue; }
				const int32 Kind = SuitReactGlowKind(P);
				if (Kind == 0) { continue; }
				FGlowFX G;
				G.Index = i;
				G.bFP = bFP;
				G.bReactor = Kind == 2;
				GlowFX.Add(G);
			}
		};
		Collect(Char->Parts, false);
		Collect(Char->FPParts, true);
	}

	if (bBattered && !bRun && !bLoggedFXWait)
	{
		bLoggedFXWait = true;
		UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: damage FX waiting (suit off %d, cinematic %d, dead %d)"), Char->bCareerSuitOff ? 1 : 0, Char->bInCinematic ? 1 : 0, Char->bDead ? 1 : 0);
	}
	if (!bRun || bPaintLocked)
	{
		if (bGlowFXActive && !bPaintLocked) { RestoreGlowFX(); }
		if (!bRun) { Char->ReactorStutterMul = 1.f; }
		return;
	}
	if (!bGlowFXActive)
	{
		bGlowFXActive = true;
		SparkTimer = FMath::FRandRange(1.f, 2.f);
		StutterTimer = FMath::FRandRange(1.5f, 3.f);
		UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: %s %s damage FX on (%d glow parts)"),
			*TrackedSuit.ToString(), bCritical ? TEXT("critical (flicker + sparks + reactor stutter)") : TEXT("battered (seam flicker)"), GlowFX.Num());
	}

	SeamFlickerTime += Dt;
	const float Seam = SuitReactSeamFlicker(SeamFlickerTime);

	// Critical: the reactor stutters (3-5 quick dropouts every 2-5 s).
	float Reactor = 1.f;
	if (bCritical)
	{
		if (StutterLeft > 0.f)
		{
			StutterLeft -= Dt;
			Reactor = (int32(StutterLeft * 10.f) & 1) ? 0.15f : 0.8f; // flips every 10 Hz tick
			if (StutterLeft <= 0.f) { StutterTimer = FMath::FRandRange(2.f, 5.f); }
		}
		else if ((StutterTimer -= Dt) <= 0.f)
		{
			StutterLeft = FMath::FRandRange(0.6f, 1.f);
			Reactor = 0.15f;
		}
	}
	Char->ReactorStutterMul = Reactor;

	for (const FGlowFX& G : GlowFX)
	{
		TArray<FArenaPaintedPart>& Arr = G.bFP ? Char->FPParts : Char->Parts;
		if (!Arr.IsValidIndex(G.Index) || !Arr[G.Index].MID) { continue; }
		FArenaPaintedPart& P = Arr[G.Index];
		const float Mul = G.bReactor ? Reactor : Seam;
		ArenaVisuals::SetPartGlow(P, P.Emissive * Mul);
	}

	// Critical: a spark burst from one random seam every 2-4 s (body parts, so it reads in third person).
	if (bCritical && (SparkTimer -= Dt) <= 0.f)
	{
		SparkTimer = FMath::FRandRange(2.f, 4.f);
		TArray<int32, TInlineAllocator<64>> Seams;
		for (int32 i = 0; i < Char->Parts.Num(); ++i)
		{
			const FArenaPaintedPart& P = Char->Parts[i];
			if (P.Component && (P.Role == ESuitPaintRole::Primary || P.Role == ESuitPaintRole::Secondary)) { Seams.Add(i); }
		}
		if (Seams.Num() > 0 && GetWorld())
		{
			const UStaticMeshComponent* C = Char->Parts[Seams[FMath::RandRange(0, Seams.Num() - 1)]].Component;
			const FVector At = C->Bounds.Origin + FMath::VRand() * (C->Bounds.SphereRadius * 0.5f);
			FVector Out = At - Char->GetActorLocation();
			Out.Z = FMath::Max(Out.Z * 0.3f, 0.f) + 20.f;
			AArenaSparks::Spawn(GetWorld(), At, Out.GetSafeNormal(), FLinearColor(1.f, 0.72f, 0.35f), FMath::RandRange(5, 9), 520.f);
			ArenaAudio::Play2D(this, TEXT("repulsor_impact"), 0.12f, FMath::FRandRange(1.5f, 1.8f));
			++SparksSpawned;
			if (SparksSpawned == 1) { UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: first critical-damage seam spark")); }
		}
	}
}

void USuitReactiveComponent::TickServo(float Dt)
{
	AIronManCharacter* Char = GetSuitOwner();
	if (!Char) { return; }
	ServoCooldown -= Dt;
	const float Yaw = Char->GetActorRotation().Yaw;
	const float YawRate = bHadYaw && Dt > 0.f ? FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Yaw)) / Dt : 0.f;
	LastYaw = Yaw;
	bHadYaw = true;
	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	const bool bOnFoot = Move && Move->IsMovingOnGround() && !Char->IsFlying();
	const bool bMoving = bOnFoot && Char->GetVelocity().Size2D() > 150.f;
	const bool bStart = bMoving && !bWasMovingOnFoot;
	bWasMovingOnFoot = bMoving;
	if (Char->bCareerSuitOff || Char->bInCinematic || Char->bDead || !bOnFoot || ServoCooldown > 0.f) { return; }
	// Large joint rotation stand-ins: a fast turn on foot (> 90 deg/s) or breaking into a walk.
	if (YawRate > 90.f || bStart)
	{
		ServoCooldown = FMath::FRandRange(0.7f, 1.1f);
		const FString Id = TrackedSuit.ToString();
		const bool bHeavy = Id.Contains(TEXT("hulk")) || Id.Contains(TEXT("war-machine")) || Id.Contains(TEXT("warmachine")) || Id.Contains(TEXT("war_machine"));
		// Heavy frames: ~3 semitones down (2^(-3/12) = 0.84).
		const float Pitch = (bHeavy ? 0.84f : 1.f) * FMath::FRandRange(0.95f, 1.06f);
		ArenaAudio::Play2D(this, bHeavy ? FName(TEXT("suitup_servo_heavy")) : FName(TEXT("suitup_servo")), 0.14f, Pitch);
	}
}

void USuitReactiveComponent::NotifyLanding(float DownSpeedCmS)
{
	const SuitReactive::FTuning& T = SuitReactive::Get();
	if (DownSpeedCmS < T.SoftBelowMps * 100.f) { return; }
	// Hard landing = +hardLanding; superhero landing (>= hardBelowMps) leaves half again as much.
	const float Amount = T.GrimeHardLanding * (DownSpeedCmS >= T.HardBelowMps * 100.f ? 1.5f : 1.f);
	SuitReactive::AddGrime(State, Amount);
	bSaveDirty = true;
}

void USuitReactiveComponent::NotifyRepulsorShot()
{
	SuitReactive::AddScorch(State, SuitReactive::Get().ScorchRepulsorShot);
	bSaveDirty = true;
}

void USuitReactiveComponent::NotifyTankShellHit()
{
	SuitReactive::AddScorch(State, SuitReactive::Get().ScorchTankShell);
	bSaveDirty = true;
}

void USuitReactiveComponent::NotifyBackHit(float Strength01)
{
	SuitReactive::AddScorch(State, 0.5f * SuitReactive::Get().ScorchTankShell * FMath::Clamp(Strength01, 0.f, 1.f));
	bSaveDirty = true;
}

void USuitReactiveComponent::NotifyExplosion(const FVector& Epicenter, float BlastRadius)
{
	const AIronManCharacter* Char = GetSuitOwner();
	if (!Char || Char->bCareerSuitOff || Char->bInCinematic) { return; }
	const SuitReactive::FTuning& T = SuitReactive::Get();
	const float K = SuitReactive::NearExplosionFactor(FVector::Dist(Epicenter, Char->GetActorLocation()), BlastRadius, T);
	if (K <= 0.02f) { return; }
	SuitReactive::AddGrime(State, T.GrimeNearExplosion * K);
	SuitReactive::AddScorch(State, T.ScorchNearExplosion * K);
	bSaveDirty = true;
}

void USuitReactiveComponent::ReportExplosion(const UObject* WorldContext, const FVector& Epicenter, float BlastRadius)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) { return; }
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PCtl = It->Get();
		const APawn* Pawn = PCtl ? PCtl->GetPawn() : nullptr;
		if (USuitReactiveComponent* Comp = Pawn ? Pawn->FindComponentByClass<USuitReactiveComponent>() : nullptr)
		{
			Comp->NotifyExplosion(Epicenter, BlastRadius);
		}
	}
}

bool USuitReactiveComponent::RepairAndClean()
{
	const bool bWasDirty = SuitReactive::NeedsRepair(State) || State.Wetness > 0.05f;
	SuitReactive::Repair(State);
	bSaveDirty = true;
	WriteSave();
	PushToMaterials(true);
	if (bGlowFXActive) { RestoreGlowFX(); }
	if (bWasDirty) { UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: %s repaired and cleaned at the workshop"), *TrackedSuit.ToString()); }
	return bWasDirty;
}

void USuitReactiveComponent::PushToMaterials(bool bForce)
{
	AIronManCharacter* Char = GetSuitOwner();
	if (!Char) { return; }
	const uint32 Gen = SuitRig::GetPaintGeneration();
	const bool bRebase = Gen != PaintGeneration;
	PaintGeneration = Gen;
	const bool bPaintLocked = Char->HitFlashLeft > 0.f || Char->CloakLeft > 0.f;
	// A hit flash / cloak ending repaints the plain colours: push again right after.
	const bool bUnlocked = bWasPaintLocked && !bPaintLocked;
	bWasPaintLocked = bPaintLocked;
	auto Near = [](float A, float B) { return FMath::Abs(A - B) < 0.002f; };
	const bool bSame = Near(State.Wetness, LastPushed.Wetness) && Near(State.GrimeLive, LastPushed.GrimeLive)
		&& Near(State.ScorchLive, LastPushed.ScorchLive) && Near(State.HeatTint, LastPushed.HeatTint)
		&& Near(State.DamageLevel, LastPushed.DamageLevel);
	if (!bForce && !bRebase && !bUnlocked && bSame) { return; }
	LastPushed = State;
	PushParts(Char->Parts, BodyBases, bRebase, bPaintLocked);
	PushParts(Char->FPParts, FPBases, bRebase, bPaintLocked);
	if (bRebase)
	{
		int32 NumMaster = 0, NumFallback = 0, NumSubstrate = 0;
		for (const FPartBase& B : BodyBases)
		{
			if (!B.MID.IsValid()) { continue; }
			(B.bMaster ? NumMaster : NumFallback)++;
			NumSubstrate += B.bSubstrate ? 1 : 0;
		}
		UE_LOG(LogIronManArena, Log, TEXT("Suit reactive: %s on %d M_SuitMaster plates + %d approximated (fallback) parts, %d critical-tier panels"),
			*TrackedSuit.ToString(), NumMaster, NumFallback, NumSubstrate);
	}
}

void USuitReactiveComponent::PushParts(TArray<FArenaPaintedPart>& PartList, TArray<FPartBase>& BaseList, bool bRebase, bool bPaintLocked)
{
	const SuitReactive::FTuning& T = SuitReactive::Get();
	if (BaseList.Num() != PartList.Num()) { BaseList.SetNum(PartList.Num()); bRebase = true; }

	// Capture the static finish (SuitRig::ApplyColors) that the live values sit on top of.
	bool bCaptured = false;
	for (int32 i = 0; i < PartList.Num(); ++i)
	{
		FArenaPaintedPart& P = PartList[i];
		FPartBase& B = BaseList[i];
		UMaterialInstanceDynamic* M = P.MID;
		if (!M || !P.Component || P.Role == ESuitPaintRole::Emissive || P.Emissive > 0.f) { B = FPartBase(); continue; }
		// Fixed parts (undersuit / joints) are not repainted by ApplyColors: only re-read them when the MID changes.
		const bool bReread = B.MID.Get() != M || (bRebase && P.Role != ESuitPaintRole::Fixed);
		if (!bReread) { continue; }
		bCaptured = true;
		FPartBase Fresh;
		Fresh.MID = M;
		float V = 0.f;
		Fresh.bMaster = SuitReactHasScalar(M, TEXT("GrimeLive"), V);
		Fresh.bHasEdgeWear = SuitReactHasScalar(M, TEXT("EdgeWear"), V);
		if (Fresh.bHasEdgeWear) { Fresh.EdgeWear = V; }
		Fresh.bHasRoughness = SuitReactHasScalar(M, TEXT("Roughness"), V);
		if (Fresh.bHasRoughness) { Fresh.Roughness = V; }
		Fresh.bHasScorch = SuitReactHasScalar(M, TEXT("Scorch"), V);
		if (Fresh.bHasScorch) { Fresh.Scorch = V; }
		if (SuitReactHasScalar(M, TEXT("Metallic"), V)) { Fresh.Metallic = V; }
		Fresh.bHasHeatGlow = SuitReactHasScalar(M, TEXT("HeatGlow"), V);
		Fresh.bHasBattleDamage = SuitReactHasScalar(M, TEXT("BattleDamage"), V);
		Fresh.bHasHeatTint = SuitReactHasScalar(M, TEXT("HeatTint"), V);
		Fresh.bHasDamageLevel = SuitReactHasScalar(M, TEXT("DamageLevel"), V);
		// Upward-facing plates (shoulders, boot tops) gather grime faster; palms / forearm vents / chest edges scorch first.
		const FString CompName = P.Component->GetName();
		Fresh.GrimeMul = SuitReactNameHas(CompName, { TEXT("Shoulder"), TEXT("Pauldron"), TEXT("Boot"), TEXT("Foot"), TEXT("Toe"), TEXT("Collar") }) ? T.GrimeUpFacingMul : 1.f;
		Fresh.ScorchMul = SuitReactNameHas(CompName, { TEXT("Palm"), TEXT("Hand"), TEXT("Forearm"), TEXT("Gauntlet"), TEXT("Vent"), TEXT("Chest"), TEXT("FPArm") }) ? 1.6f : 0.7f;
		B = Fresh;
	}

	// Critical tier: the same two or three primary panels per suit expose the substrate (deterministic per suit id).
	if (bCaptured)
	{
		TArray<TPair<uint32, int32>> Panels;
		for (int32 i = 0; i < PartList.Num(); ++i)
		{
			BaseList[i].bSubstrate = false;
			if (BaseList[i].MID.IsValid() && PartList[i].Role == ESuitPaintRole::Primary && PartList[i].Component)
			{
				Panels.Add(TPair<uint32, int32>(HashCombine(GetTypeHash(TrackedSuit), GetTypeHash(PartList[i].Component->GetFName())), i));
			}
		}
		Panels.Sort([](const TPair<uint32, int32>& A, const TPair<uint32, int32>& Bp) { return A.Key < Bp.Key; });
		const int32 NumSubstrate = FMath::Min(Panels.Num(), 2 + int32(GetTypeHash(TrackedSuit) & 1));
		for (int32 n = 0; n < NumSubstrate; ++n) { BaseList[Panels[n].Value].bSubstrate = true; }
	}

	const float Wet = State.Wetness;
	const float Dmg = State.DamageLevel;
	const bool bCritical = Dmg >= T.Tiers[3] - 0.01f && Dmg > 0.f;
	const FLinearColor Soot(0.012f, 0.01f, 0.009f);
	const FLinearColor Substrate(0.09f, 0.09f, 0.1f);
	for (int32 i = 0; i < PartList.Num(); ++i)
	{
		FArenaPaintedPart& P = PartList[i];
		FPartBase& B = BaseList[i];
		UMaterialInstanceDynamic* M = B.MID.Get();
		if (!M || M != P.MID) { continue; }
		const float Grime = FMath::Clamp(State.GrimeLive * B.GrimeMul, 0.f, 1.f);
		const float Scorch = FMath::Clamp(State.ScorchLive * B.ScorchMul, 0.f, 1.f);
		const bool bMetal = B.Metallic > T.HeatMetallicThreshold;
		// Temper colours start early on bare metal; painted plates only glow when really hot (old HeatGlow ramp).
		const float HeatGlow = bMetal ? FMath::Clamp((State.HeatTint - 0.3f) / 0.7f, 0.f, 1.f) : FMath::Clamp((State.HeatTint - 0.6f) / 0.4f, 0.f, 1.f);
		const float Edge = FMath::Clamp(B.EdgeWear + T.EdgeWearAddMax * Dmg + (B.bSubstrate && bCritical ? 0.45f : 0.f), 0.f, 1.f);

		if (B.bMaster)
		{
			// M_SuitMaster: Jocasta's params exist (Wetness / GrimeLive / ScorchLive), BattleDamage = DamageLevel.
			M->SetScalarParameterValue(TEXT("Wetness"), Wet);
			M->SetScalarParameterValue(TEXT("GrimeLive"), Grime);
			M->SetScalarParameterValue(TEXT("ScorchLive"), Scorch);
			if (B.bHasBattleDamage) { M->SetScalarParameterValue(TEXT("BattleDamage"), Dmg); }
			if (B.bHasEdgeWear) { M->SetScalarParameterValue(TEXT("EdgeWear"), Edge); }
			if (B.bHasHeatGlow) { M->SetScalarParameterValue(TEXT("HeatGlow"), HeatGlow); }
			if (B.bHasHeatTint) { M->SetScalarParameterValue(TEXT("HeatTint"), State.HeatTint); }
			if (B.bHasDamageLevel) { M->SetScalarParameterValue(TEXT("DamageLevel"), Dmg); }
			continue;
		}

		// Other materials: approximate through the params they do have.
		if (B.bHasRoughness)
		{
			const float Rough = (B.Roughness + T.GrimeMaxRoughnessAdd * Grime + 0.35f * Scorch + 0.1f * Dmg) * (1.f - T.WetRoughnessMul * Wet);
			M->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(Rough, 0.02f, 1.f));
		}
		if (B.bHasEdgeWear) { M->SetScalarParameterValue(TEXT("EdgeWear"), Edge); }
		if (B.bHasScorch) { M->SetScalarParameterValue(TEXT("Scorch"), FMath::Clamp(B.Scorch + Scorch, 0.f, 1.f)); }
		if (B.bHasHeatGlow) { M->SetScalarParameterValue(TEXT("HeatGlow"), HeatGlow); }
		if (bPaintLocked) { continue; } // hit flash / cloak own the colour right now

		FLinearColor C = P.Color;
		C *= 1.f - T.WetAlbedoDarken * Wet * (1.f - FMath::Clamp(B.Metallic, 0.f, 1.f));
		C *= 1.f - T.GrimeMaxDarken * Grime;
		C *= 1.f - 0.12f * Dmg;
		C = C + (Soot - C) * (0.7f * Scorch);
		if (bMetal && HeatGlow > 0.f)
		{
			const float Ramp = FMath::Clamp(HeatGlow * 1.6f - 0.4f, 0.f, 1.f);
			const FLinearColor Temper = (FLinearColor(0.78f, 0.6f, 0.32f) + (FLinearColor(0.22f, 0.25f, 0.55f) - FLinearColor(0.78f, 0.6f, 0.32f)) * Ramp) * 0.55f;
			C = C + (Temper - C) * (HeatGlow * 0.4f);
		}
		if (B.bSubstrate && bCritical) { C = C + (Substrate - C) * 0.6f; }
		C.A = P.Color.A;
		M->SetVectorParameterValue(TEXT("Color"), C);

		// Faint heat emissive on bare metal above emissiveAbove (M_CarPaint glow units are strong: 20% of maxEmissive).
		if (bMetal && State.HeatTint > T.HeatEmissiveAbove)
		{
			const float Glow = T.HeatMaxEmissive * 0.2f * (State.HeatTint - T.HeatEmissiveAbove) / (1.f - T.HeatEmissiveAbove);
			ArenaVisuals::SetPartGlow(P, Glow);
			B.bHeatGlowing = true;
		}
		else if (B.bHeatGlowing)
		{
			ArenaVisuals::SetPartGlow(P, P.Emissive);
			B.bHeatGlowing = false;
		}
	}
}
