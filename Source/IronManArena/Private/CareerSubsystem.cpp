#include "CareerSubsystem.h"
#include "CareerEconomy.h"
#include "CareerWidgets.h"
#include "ArenaAudio.h"

#include "ArenaEnvironment.h"
#include "ArenaGameMode.h"
#include "ArenaSaveGame.h"
#include "CityHouse.h"
#include "ContribLighting.h"
#include "DowntownDistrict.h"
#include "EnvironmentPreset.h"
#include "IronManCharacter.h"
#include "ReactorComponent.h"
#include "SuitTypes.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "Career"

namespace
{
	struct FCharBase
	{
		float BaseMaxHealth = 100.f, RepulsorDamage = 1.f, BeamDps = 8.f, BeamDuration = 0.9f, BeamCost = 30.f, MaxHeat = 100.f, Boost = 1.65f;
	};
	TMap<TWeakObjectPtr<AIronManCharacter>, FCharBase>& BaseMap()
	{
		static TMap<TWeakObjectPtr<AIronManCharacter>, FCharBase> M;
		return M;
	}

	int32 TodayIndex()
	{
		const FDateTime Now = FDateTime::Now();
		return int32((Now - FDateTime(1970, 1, 1)).GetTotalDays());
	}

	void SetCVar(const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByCode); }
	}
}

UCareerSubsystem* UCareerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCareerSubsystem>() : nullptr;
}

void UCareerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

void UCareerSubsystem::Deinitialize()
{
	if (bDirty) { Save(); }
	Super::Deinitialize();
}

UWorld* UCareerSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

void UCareerSubsystem::Load()
{
	UArenaSaveGame* S = UArenaSaveGame::LoadOrCreate(SlotName);
	if (!S) { return; }
	Credits = S->Credits;
	CreditsEarned = S->CreditsEarned;
	UpgradeLevels = S->CareerUpgradeLevels;
	SuitsOwned = S->CareerSuitsOwned;
	HousesOwned = S->HousesOwned;
	HomeHouse = S->HomeHouse;
	VehiclesOwned = S->VehiclesOwned;
	bAvatarCreated = S->bAvatarCreated;
	Avatar = S->Avatar;
	if (Avatar.PilotName.IsEmpty() && !S->PlayerName.IsEmpty()) { Avatar.PilotName = S->PlayerName; }
	OutfitSlots = S->OutfitSlots;
	ClothingOwned = S->ClothingOwned;
	bBossBeaten = S->bBossBeaten;
	CasinoLossToday = S->CasinoLossToday;
	CasinoDay = S->CasinoDay;
	JobsClaimed = S->JobsClaimed;
	JobClaimUnix = S->JobClaimUnix;
	if (JobClaimUnix.Num() != JobsClaimed.Num()) { JobClaimUnix.SetNumZeroed(JobsClaimed.Num()); }
	UpgradeLevels.SetNumZeroed(int32(ECareerUpgrade::Count));
	OutfitSlots.SetNum(Career::OutfitSlots);
	// Character creator is disabled: go straight to the menu as Harley (or a previously saved pilot).
	if (!bAvatarCreated)
	{
		if (Avatar.PilotName.TrimStartAndEnd().IsEmpty()) { Avatar.PilotName = TEXT("Harley"); }
		bAvatarCreated = true;
		Save();
		UE_LOG(LogTemp, Log, TEXT("Career: default pilot %s (character creator disabled)"), *Avatar.PilotName);
	}
}

void UCareerSubsystem::Save()
{
	// Re-read the slot so fields owned by the story / side missions stay as they are on disk.
	UArenaSaveGame* S = UArenaSaveGame::LoadOrCreate(SlotName);
	if (!S) { return; }
	S->Credits = Credits;
	S->CreditsEarned = CreditsEarned;
	S->CareerUpgradeLevels = UpgradeLevels;
	S->CareerSuitsOwned = SuitsOwned;
	S->HousesOwned = HousesOwned;
	S->HomeHouse = HomeHouse;
	S->VehiclesOwned = VehiclesOwned;
	S->bAvatarCreated = bAvatarCreated;
	S->Avatar = Avatar;
	if (bAvatarCreated && !Avatar.PilotName.IsEmpty()) { S->PlayerName = Avatar.PilotName; }
	S->OutfitSlots = OutfitSlots;
	S->ClothingOwned = ClothingOwned;
	S->bBossBeaten = bBossBeaten;
	S->CasinoLossToday = CasinoLossToday;
	S->CasinoDay = CasinoDay;
	S->JobsClaimed = JobsClaimed;
	S->JobClaimUnix = JobClaimUnix;
	S->Write(SlotName);
	bDirty = false;
	SaveTimer = 0.f;
}

// ------------------------------------------------------------------ credits

void UCareerSubsystem::AddCredits(int32 Amount, const FText& Reason)
{
	if (Amount == 0) { return; }
	const int32 Before = Credits;
	Credits = FMath::Max(0, Credits + Amount);
	if (Amount > 0) { CreditsEarned += Amount; }
	if (!Reason.IsEmpty())
	{
		const int32 Delta = Credits - Before;
		Toast(FText::Format(LOCTEXT("CreditsToast", "{0}{1} CR  {2}"), FText::FromString(Delta >= 0 ? TEXT("+") : TEXT("")), FText::AsNumber(Delta), Reason),
			Delta >= 0 ? FLinearColor(1.f, 0.85f, 0.35f) : FLinearColor(1.f, 0.35f, 0.3f));
	}
	MarkDirty();
}

bool UCareerSubsystem::Spend(int32 Price, const FText& What)
{
	if (Price < 0 || Credits < Price)
	{
		Toast(FText::Format(LOCTEXT("NoCredits", "Not enough credits for {0} ({1} CR)"), What, FText::AsNumber(Price)), FLinearColor(1.f, 0.35f, 0.3f));
		ArenaAudio::Play2D(this, TEXT("ui_error"), 0.6f);
		return false;
	}
	Credits -= Price;
	if (Price > 0) { ArenaAudio::Play2D(this, TEXT("cash_register"), 0.7f); }
	Toast(FText::Format(LOCTEXT("Bought", "Bought {0}  -{1} CR"), What, FText::AsNumber(Price)), FLinearColor(0.5f, 1.f, 0.6f));
	MarkDirty();
	return true;
}

void UCareerSubsystem::AwardKill(int32 ScoreValue, bool bResponder)
{
	int32 C = Career::CreditsForKill(ScoreValue);
	if (bResponder) { C = FMath::Max(5, C / 2); }
	AddCredits(C);
}

// ------------------------------------------------------------------ upgrades

int32 UCareerSubsystem::GetUpgradeLevel(ECareerUpgrade U) const
{
	const int32 i = int32(U);
	return UpgradeLevels.IsValidIndex(i) ? UpgradeLevels[i] : 0;
}

bool UCareerSubsystem::BuyUpgrade(ECareerUpgrade U)
{
	const int32 L = GetUpgradeLevel(U);
	if (L >= Career::MaxUpgradeLevel) { return false; }
	const int32 Cost = Career::UpgradeCost(U, L);
	if (!Spend(Cost, FText::Format(LOCTEXT("UpgradeWhat", "{0} Mk {1}"), Career::UpgradeName(U), FText::AsNumber(L + 1)))) { return false; }
	UpgradeLevels[int32(U)] = L + 1;
	Save();
	RefreshPlayer();
	return true;
}

// ------------------------------------------------------------------ suits

bool UCareerSubsystem::IsSuitOwned(FName SuitId) const
{
	const FShopSuitDef* D = Career::FindShopSuit(SuitId);
	if (!D) { return true; }                 // not on the shop list: always free
	return D->Price <= 0 || SuitsOwned.Contains(SuitId);
}

bool UCareerSubsystem::IsSuitLockedByBoss(FName SuitId) const
{
	const FShopSuitDef* D = Career::FindShopSuit(SuitId);
	return D && D->bBossUnlock && !bBossBeaten;
}

bool UCareerSubsystem::BuySuit(FName SuitId)
{
	const FShopSuitDef* D = Career::FindShopSuit(SuitId);
	if (!D || IsSuitOwned(SuitId)) { return false; }
	if (IsSuitLockedByBoss(SuitId))
	{
		Toast(LOCTEXT("BossLock", "Beat the boss in free play to unlock this suit"), FLinearColor(1.f, 0.35f, 0.3f));
		return false;
	}
	const int32 Idx = USuitLibrary::FindSuitIndex(SuitId);
	const FText Name = Idx != INDEX_NONE ? USuitLibrary::GetSuit(Idx).Name : FText::FromName(SuitId);
	if (!Spend(D->Price, Name)) { return false; }
	SuitsOwned.AddUnique(SuitId);
	Save();
	ApplySuitLock(GetPlayer());
	return true;
}

TArray<FName> UCareerSubsystem::GetAllowedSuitIds() const
{
	TArray<FName> Out;
	for (const FSuitDef& S : USuitLibrary::GetAllSuits())
	{
		if (IsSuitOwned(S.Id)) { Out.Add(S.Id); }
	}
	return Out;
}

void UCareerSubsystem::ApplySuitLock(AIronManCharacter* Char) const
{
	if (!Char) { return; }
	if (const AArenaGameMode* GM = Char->GetWorld() ? Char->GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		if (GM->GetPlayMode() != EArenaPlayMode::Arena) { return; }
	}
	Char->SetAllowedSuits(GetAllowedSuitIds());
}

bool UCareerSubsystem::EquipSuit(FName SuitId)
{
	AIronManCharacter* Char = GetPlayer();
	const int32 Idx = USuitLibrary::FindSuitIndex(SuitId);
	if (!Char || Idx == INDEX_NONE || !IsSuitOwned(SuitId)) { return false; }
	ApplySuitLock(Char);
	if (!Char->RequestSuitSwap(Idx) && Char->GetSuitIndex() != Idx) { Char->ApplySuit(Idx); }
	return true;
}

void UCareerSubsystem::SetBossBeaten()
{
	if (bBossBeaten) { return; }
	bBossBeaten = true;
	Toast(LOCTEXT("UltronUnlocked", "ULTRON suit now for sale in the shop (U)"), FLinearColor(1.f, 0.3f, 0.3f));
	Save();
}

// ------------------------------------------------------------------ houses

bool UCareerSubsystem::BuyHouse(FName Id)
{
	const FCityHouseDef* D = Career::FindHouse(Id);
	if (!D || OwnsHouse(Id)) { return false; }
	if (!Spend(D->Price, FText::FromString(D->Name))) { return false; }
	HousesOwned.AddUnique(Id);
	if (HomeHouse.IsNone() || (Career::FindHouse(HomeHouse) && Career::FindHouse(HomeHouse)->Price < D->Price)) { HomeHouse = Id; }
	Save();
	if (ADowntownDistrict* Dist = ADowntownDistrict::Find(GetTickableGameObjectWorld()))
	{
		if (ACityHouse* H = Dist->FindHouse(Id)) { H->SetOwned(true); }
	}
	if (D->IncomePerMinute > 0)
	{
		Toast(FText::Format(LOCTEXT("HouseIncome", "Keys received. Passive +{0} CR/min."), FText::AsNumber(D->IncomePerMinute)), FLinearColor(0.55f, 0.95f, 0.7f));
	}
	else if (D->IncomePerMinute < 0)
	{
		Toast(FText::Format(LOCTEXT("HouseRent", "Lease signed. Rent -{0} CR/min."), FText::AsNumber(-D->IncomePerMinute)), FLinearColor(1.f, 0.75f, 0.4f));
	}
	else
	{
		Toast(LOCTEXT("HouseOwned", "Keys received. Welcome home."), FLinearColor(0.55f, 0.95f, 0.7f));
	}
	return true;
}

void UCareerSubsystem::SetHomeHouse(FName Id)
{
	if (!OwnsHouse(Id)) { return; }
	HomeHouse = Id;
	Toast(FText::Format(LOCTEXT("HomeSet", "Home set: {0}"), FText::FromString(Career::FindHouse(Id) ? Career::FindHouse(Id)->Name : TEXT("?"))));
	Save();
}

int32 UCareerSubsystem::GetPassiveIncome() const
{
	int32 Sum = 0;
	for (const FName& H : HousesOwned) { if (const FCityHouseDef* D = Career::FindHouse(H)) { Sum += D->IncomePerMinute; } }
	return Sum;
}

// ------------------------------------------------------------------ vehicles

bool UCareerSubsystem::BuyVehicle(FName Id)
{
	const FVehicleDef* D = Career::FindVehicle(Id);
	if (!D || OwnsVehicle(Id)) { return false; }
	if (HousesOwned.Num() == 0)
	{
		Toast(LOCTEXT("NeedGarage", "Buy a house first - vehicles are kept in your garage"), FLinearColor(1.f, 0.35f, 0.3f));
		return false;
	}
	if (!Spend(D->Price, FText::FromString(D->Name))) { return false; }
	VehiclesOwned.AddUnique(Id);
	Toast(LOCTEXT("VehicleHint", "Call it from your phone (N)"), FLinearColor(0.6f, 0.85f, 1.f));
	Save();
	return true;
}

// ------------------------------------------------------------------ avatar

void UCareerSubsystem::SetAvatar(const FAvatarConfig& In, bool bSave)
{
	Avatar = In;
	Avatar.PilotName = Avatar.PilotName.TrimStartAndEnd().Left(20);
	UE_LOG(LogTemp, Log, TEXT("Career: avatar applied for %s (save=%d)"), *Avatar.PilotName, bSave ? 1 : 0);
	if (bSave) { Save(); } else { MarkDirty(); }
	RefreshPlayer();
	if (UWorld* World = GetTickableGameObjectWorld())
	{
		for (TActorIterator<ACityHouse> It(World); It; ++It) { It->RefreshPilot(); }
	}
}

void UCareerSubsystem::MarkCharacterCreated()
{
	bAvatarCreated = true;
	Save();
}

// ------------------------------------------------------------------ clothing

bool UCareerSubsystem::OwnsClothing(int32 Kind, int32 Style) const
{
	const FClothingItem* I = Career::FindClothing(Kind, Style);
	return !I || I->Price <= 0 || ClothingOwned.Contains(I->Id);
}

bool UCareerSubsystem::BuyClothing(int32 Kind, int32 Style)
{
	const FClothingItem* I = Career::FindClothing(Kind, Style);
	if (!I || OwnsClothing(Kind, Style)) { return true; }
	if (!Spend(I->Price, FText::FromString(I->Name))) { return false; }
	ClothingOwned.AddUnique(I->Id);
	Save();
	return true;
}

void UCareerSubsystem::SaveOutfitSlot(int32 Index)
{
	if (!OutfitSlots.IsValidIndex(Index)) { return; }
	OutfitSlots[Index] = Avatar.Outfit;
	Toast(FText::Format(LOCTEXT("OutfitSaved", "Outfit saved to slot {0}"), FText::AsNumber(Index + 1)));
	Save();
}

void UCareerSubsystem::LoadOutfitSlot(int32 Index)
{
	if (!OutfitSlots.IsValidIndex(Index)) { return; }
	FAvatarConfig A = Avatar;
	A.Outfit = OutfitSlots[Index];
	SetAvatar(A, true);
}

// ------------------------------------------------------------------ casino

int32 UCareerSubsystem::GetCasinoLossAllowance() const
{
	const int32 Lost = CasinoDay == TodayIndex() ? CasinoLossToday : 0;
	return FMath::Max(0, CareerEconomy::CasinoDailyLossLimit() - Lost);
}

void UCareerSubsystem::SettleCasino(int32 Net, ECasinoGame Game)
{
	const int32 Today = TodayIndex();
	if (CasinoDay != Today) { CasinoDay = Today; CasinoLossToday = 0; }
	CasinoLossToday = FMath::Max(0, CasinoLossToday - Net); // wins pay back today's losses first
	if (Net != 0) { AddCredits(Net, Career::CasinoGameName(Game)); }
	Save();
}

int32 UCareerSubsystem::JobCooldownLeft(FName JobId) const
{
	const FCareerJobDef* J = CareerEconomy::FindJob(JobId);
	if (!J) { return 0; }
	const int32 Idx = JobsClaimed.IndexOfByKey(JobId);
	if (Idx == INDEX_NONE || !JobClaimUnix.IsValidIndex(Idx)) { return 0; }
	const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 ReadyAt = JobClaimUnix[Idx] + int64(J->CooldownHours * 3600.0);
	return int32(FMath::Max<int64>(0, ReadyAt - Now));
}

bool UCareerSubsystem::ClaimJob(FName JobId)
{
	const FCareerJobDef* J = CareerEconomy::FindJob(JobId);
	if (!J) { return false; }
	if (JobCooldownLeft(JobId) > 0) { return false; }
	int32 Pay = J->Payout;
	bool bOwnsPlane = false, bOwnsBike = false;
	for (const FName& Vid : VehiclesOwned)
	{
		if (const FVehicleDef* V = Career::FindVehicle(Vid))
		{
			if (V->Kind == ECityVehicleKind::Plane) { bOwnsPlane = true; }
			if (V->Kind == ECityVehicleKind::Bike) { bOwnsBike = true; }
		}
	}
	if (bOwnsPlane) { Pay += J->BonusIfOwnsPlane; }
	if (bOwnsBike) { Pay += J->BonusIfOwnsBike; }
	if (HousesOwned.Num() > 0) { Pay += J->BonusIfOwnsHouse; }
	const int32 Idx = JobsClaimed.IndexOfByKey(JobId);
	const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
	if (Idx == INDEX_NONE) { JobsClaimed.Add(JobId); JobClaimUnix.Add(Now); }
	else { JobClaimUnix[Idx] = Now; }
	AddCredits(Pay, FText::FromString(J->Name));
	Save();
	return true;
}

// ------------------------------------------------------------------ toasts

void UCareerSubsystem::Toast(const FText& Text, const FLinearColor& Color)
{
	FCareerToast T;
	T.Text = Text;
	T.Color = Color;
	T.Time = FPlatformTime::Seconds();
	Toasts.Add(T);
	if (Toasts.Num() > 6) { Toasts.RemoveAt(0); }
}

// ------------------------------------------------------------------ player

AIronManCharacter* UCareerSubsystem::GetPlayer() const
{
	UWorld* World = GetTickableGameObjectWorld();
	return World ? Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
}

void UCareerSubsystem::RefreshPlayer()
{
	if (AIronManCharacter* C = GetPlayer()) { C->RefreshCareer(); }
}

bool UCareerSubsystem::ApplyToCharacter(AIronManCharacter* Char, FSuitDef& Look)
{
	if (!Char) { return false; }
	FCharBase& B = BaseMap().FindOrAdd(Char);
	static TSet<TWeakObjectPtr<AIronManCharacter>> Captured;
	if (!Captured.Contains(Char))
	{
		Captured.Add(Char);
		B.BaseMaxHealth = Char->BaseMaxHealth;
		B.RepulsorDamage = Char->RepulsorDamage;
		B.BeamDps = Char->BeamDamagePerSecond;
		B.BeamDuration = Char->BeamDuration;
		B.BeamCost = Char->BeamEnergyCost;
		B.MaxHeat = Char->GetReactor() ? Char->GetReactor()->MaxHeat : 100.f;
		B.Boost = Char->BoostSpeedMultiplier;
	}
	const UCareerSubsystem* C = Get(Char);
	auto Lvl = [C](ECareerUpgrade U) { return C ? C->GetUpgradeLevel(U) : 0; };
	const FShopSuitDef* Perk = Career::FindShopSuit(Look.Id);
	const FShopSuitDef NoPerk{};
	const FShopSuitDef& P = Perk ? *Perk : NoPerk;
	const bool bFreePlay = !Char->GetWorld() || !Char->GetWorld()->GetAuthGameMode<AArenaGameMode>()
		|| Char->GetWorld()->GetAuthGameMode<AArenaGameMode>()->GetPlayMode() == EArenaPlayMode::Arena;
	// Upgrades apply everywhere (they are yours); suit perks too.
	Char->BaseMaxHealth = B.BaseMaxHealth * Career::UpgradeMultiplier(ECareerUpgrade::Armor, Lvl(ECareerUpgrade::Armor)) * P.ArmorMul;
	Char->RepulsorDamage = B.RepulsorDamage * Career::UpgradeMultiplier(ECareerUpgrade::Repulsor, Lvl(ECareerUpgrade::Repulsor));
	Look.WeaponDamage *= P.DamageMul;
	const float BeamMul = Career::UpgradeMultiplier(ECareerUpgrade::Unibeam, Lvl(ECareerUpgrade::Unibeam)) * P.BeamMul;
	Char->BeamDamagePerSecond = B.BeamDps * BeamMul;
	Char->BeamDuration = B.BeamDuration * (1.f + 0.5f * (BeamMul - 1.f));
	Char->BeamEnergyCost = B.BeamCost * FMath::Max(0.5f, 1.f - 0.06f * Lvl(ECareerUpgrade::Unibeam));
	if (Char->GetReactor()) { Char->GetReactor()->MaxHeat = B.MaxHeat * Career::UpgradeMultiplier(ECareerUpgrade::HeatCapacity, Lvl(ECareerUpgrade::HeatCapacity)) * P.HeatMul; }
	Char->CareerFlightMul = Career::UpgradeMultiplier(ECareerUpgrade::Flight, Lvl(ECareerUpgrade::Flight)) * P.FlightMul;
	Char->BoostSpeedMultiplier = B.Boost * P.BoostMul;
	Char->CareerExtraMissiles = P.ExtraMissiles;

	// Silhouette + avatar paint / helmet.
	const FAvatarConfig* A = C ? &C->GetAvatar() : nullptr;
	const bool bSlim = Career::IsFeminineSuit(Look.Id) || (A && A->bSlimArmor);
	Char->CareerBodyScale = bSlim ? FVector(0.9f, 0.86f, 0.97f) : FVector::OneVector;
	if (bSlim) { Look.Plating = 0; }
	if (A && A->bCustomPaint && bFreePlay)
	{
		Look.PrimaryColor = A->SuitPrimary;
		Look.SecondaryColor = A->SuitSecondary;
		Look.AccentColor = A->SuitSecondary;
		Look.EmissiveColor = A->ReactorGlow;
	}
	if (A && A->HelmetStyle > 0) { Look.Helmet = ESuitHelmet(FMath::Clamp(A->HelmetStyle - 1, 0, 4)); }
	return true;
}

// ------------------------------------------------------------------ tick

void UCareerSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !World->IsGameWorld()) { return; }
	const double Now = FPlatformTime::Seconds();
	Toasts.RemoveAll([Now](const FCareerToast& T) { return Now - T.Time > 4.5; });
	if (!bTimeOfDaySet && AArenaEnvironment::Find(World))
	{
		bTimeOfDaySet = true;
		ApplyScalabilityPreset(ScalabilityPreset);
	}
	if (bInfiniteMoney && Credits < InfiniteCredits / 2) { Credits = InfiniteCredits; MarkDirty(); }
	if (UGameplayStatics::IsGamePaused(World)) { return; }
	TickHouses(DeltaTime);
	SaveTimer += DeltaTime;
	if (bDirty && SaveTimer > 15.f) { Save(); }
}

void UCareerSubsystem::TickHouses(float DeltaTime)
{
	AIronManCharacter* Char = GetPlayer();
	// Passive income (penthouse / mansion), once a minute.
	IncomeTimer += DeltaTime;
	if (IncomeTimer >= 60.f)
	{
		IncomeTimer = 0.f;
		if (const int32 Inc = GetPassiveIncome())
		{
			AddCredits(Inc, Inc >= 0 ? LOCTEXT("PropIncome", "property income") : LOCTEXT("PropRent", "property rent"));
		}
	}
	if (!Char || Char->IsDead()) { return; }
	// Suit perk self-repair (nanotech / Rescue).
	if (const FShopSuitDef* P = Career::FindShopSuit(Char->GetSuit().Id))
	{
		if (P->RepairPerSecond > 0.f && Char->GetHealth() < Char->GetMaxHealth()) { Char->Heal(P->RepairPerSecond * DeltaTime); }
	}
	// Mansion armory: stand on the pad to repair + recharge.
	ArmoryToastCooldown = FMath::Max(0.f, ArmoryToastCooldown - DeltaTime);
	if (ADowntownDistrict* Dist = ADowntownDistrict::Find(Char))
	{
		for (const FName& H : HousesOwned)
		{
			ACityHouse* House = Dist->FindHouse(H);
			if (!House || !House->HasArmory()) { continue; }
			if (FVector::Dist(House->GetArmoryLocation(), Char->GetActorLocation()) < 520.f)
			{
				if (Char->GetHealth() < Char->GetMaxHealth() || Char->GetEnergy() < Char->GetMaxEnergy())
				{
					Char->Heal(35.f * DeltaTime);
					Char->AddEnergy(40.f * DeltaTime);
					if (ArmoryToastCooldown <= 0.f) { Toast(LOCTEXT("Armory", "ARMORY: repairing + recharging"), FLinearColor(0.4f, 0.85f, 1.f)); ArmoryToastCooldown = 5.f; }
				}
			}
		}
	}
}

// ------------------------------------------------------------------ look / time of day

void UCareerSubsystem::SetTimeOfDay(float Hour)
{
	TimeOfDay = FMath::Fmod(FMath::Fmod(Hour, 24.f) + 24.f, 24.f);
	bTimeOfDaySet = true;
	UWorld* World = GetTickableGameObjectWorld();
	AArenaEnvironment* Env = World ? AArenaEnvironment::Find(World) : nullptr;
	if (!Env) { return; }
	// Sun elevation: 0 at 06:00 / 18:00, 72 deg at 12:00 (sub-tropical Leonida in summer), negative at night.
	const float Elev = 72.f * FMath::Sin((TimeOfDay - 6.f) / 12.f * PI);
	const float Yaw = 95.f + (TimeOfDay - 6.f) / 12.f * 170.f;       // rises over the sea (east), sets inland (west)
	const bool bNight = Elev < -4.f;
	const bool bGolden = !bNight && Elev < 16.f;
	FEnvironmentPreset P;
	if (bNight)
	{
		FName SetId;
		P = EnvironmentPresets::ForChapter(TEXT("city"), TEXT("ch4_eyes"), false, SetId);
	}
	else
	{
		P = EnvironmentPresets::Arena();
	}
	P.bNight = bNight;
	Env->ApplyPreset(P);
	// The Vison JSON layer writes its own sun rotation - put the clock's sun back.
	if (UDirectionalLightComponent* Sun = Env->GetSun())
	{
		Sun->SetRelativeRotation(FRotator(bNight ? 30.f : -FMath::Max(Elev, -3.f), Yaw, 0.f));
		if (!bNight) { Sun->SetIntensity(120000.f); }
		Sun->SetVolumetricScatteringIntensity(bGolden ? 2.0f : 1.f);
	}
	if (UExponentialHeightFogComponent* Fog = Env->FindComponentByClass<UExponentialHeightFogComponent>())
	{
		// GTA VI look-dev: warm coastal haze at golden hour, light humid haze by day, teal-blue at night.
		if (bGolden) { Fog->SetFogDensity(0.028f); Fog->SetFogHeightFalloff(0.12f); Fog->SetFogInscatteringColor(FLinearColor(0.85f, 0.55f, 0.38f)); Fog->SetVolumetricFogExtinctionScale(0.8f); }
		else if (!bNight) { Fog->SetFogDensity(0.014f); Fog->SetFogHeightFalloff(0.16f); Fog->SetFogInscatteringColor(FLinearColor(0.55f, 0.65f, 0.8f)); }
		else { Fog->SetFogDensity(0.018f); Fog->SetFogInscatteringColor(FLinearColor(0.08f, 0.12f, 0.2f)); }
	}
	if (UPostProcessComponent* PP = Env->GetPostProcess())
	{
		FPostProcessSettings& S = PP->Settings;
		// Leonida grade: warm highlights, teal-lifted shadows, gentle contrast, no grain, light vignette.
		S.bOverride_WhiteTemp = true;        S.WhiteTemp = bNight ? 6300.f : bGolden ? 7400.f : 6800.f;
		S.bOverride_ColorContrast = true;    S.ColorContrast = FVector4(1.f, 1.f, 1.f, bNight ? 1.06f : 1.04f);
		S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = bNight ? FVector4(1.02f, 0.98f, 1.04f, 1.f) : FVector4(1.04f, 1.0f, 0.93f, 1.f);
		S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(0.93f, 1.0f, 1.07f, 1.f);
		S.bOverride_ColorOffsetShadows = true; S.ColorOffsetShadows = FVector4(0.f, 0.003f, 0.006f, 0.f);
		S.bOverride_ColorSaturation = true;  S.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.f);
		S.bOverride_ColorSaturationMidtones = true; S.ColorSaturationMidtones = FVector4(1.f, 1.f, 1.f, 1.08f);
		S.bOverride_BloomIntensity = true;   S.BloomIntensity = bNight ? 0.8f : bGolden ? 0.7f : 0.5f;
		S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = 0.f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.22f;
		S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
		S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = bGolden ? 0.3f : 0.15f;
		if (bGolden)
		{
			// Sun near the horizon: ground light drops ~10x; widen the exposure range down so it stays bright and warm.
			S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = Elev < 4.f ? 8.5f : 11.f;
			S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = Elev < 4.f ? 12.f : 14.5f;
			S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.3f;
		}
		// Lumen budget (GTX/RTX 20-series at 1080p): quality 1 keeps ~60 fps; Epic preset raises it.
		const float LumenQ = ScalabilityPreset >= 3 ? 2.f : 1.f;
		S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = LumenQ;
		S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = LumenQ;
		S.bOverride_LumenReflectionQuality = true; S.LumenReflectionQuality = LumenQ;
		S.bOverride_LumenSceneDetail = true; S.LumenSceneDetail = 1.f;
		ContribLighting::ApplyPhotorealGrade(S, false);
	}
	if (ADowntownDistrict* Dist = ADowntownDistrict::Find(World)) { Dist->SetNight(bNight || Elev < 2.f, bNight); }
}

void UCareerSubsystem::SetPhotoreal(bool bOn)
{
	SetCVar(TEXT("ima.PhotorealGrade"), bOn ? TEXT("1") : TEXT("0"));
	ReapplyLook();
	Toast(bOn ? LOCTEXT("PhotoOn", "Photoreal grade ON") : LOCTEXT("PhotoOff", "Photoreal grade OFF"), FLinearColor(0.6f, 0.85f, 1.f));
}

bool UCareerSubsystem::IsPhotoreal() const { return ContribLighting::IsPhotorealGradeEnabled(); }

FText UCareerSubsystem::ScalabilityName(int32 Preset)
{
	switch (Preset)
	{
	case 0: return LOCTEXT("ScLow", "Low");
	case 1: return LOCTEXT("ScMed", "Medium");
	case 2: return LOCTEXT("Sc2070", "2070 SUPER (High, 1080p60)");
	case 3: return LOCTEXT("ScEpic", "Epic");
	default: return LOCTEXT("ScCine", "Cinematic");
	}
}

void UCareerSubsystem::ApplyScalabilityPreset(int32 Preset)
{
	ScalabilityPreset = FMath::Clamp(Preset, 0, 4);
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetOverallScalabilityLevel(ScalabilityPreset);
		GUS->ApplySettings(false);
	}
	// Tuned on paper for an RTX 2070 SUPER at 1080p: TSR at 77 % internal res, software Lumen, VSM bias,
	// cheaper volumetric fog grid. Epic / Cinematic restore native resolution.
	SetCVar(TEXT("r.AntiAliasingMethod"), TEXT("4"));
	SetCVar(TEXT("r.ScreenPercentage"), ScalabilityPreset <= 0 ? TEXT("60") : ScalabilityPreset == 1 ? TEXT("67") : ScalabilityPreset == 2 ? TEXT("77") : TEXT("100"));
	SetCVar(TEXT("r.Lumen.HardwareRayTracing"), TEXT("0"));
	SetCVar(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"), ScalabilityPreset <= 2 ? TEXT("1.0") : TEXT("-0.5"));
	SetCVar(TEXT("r.VolumetricFog.GridPixelSize"), ScalabilityPreset <= 2 ? TEXT("16") : TEXT("8"));
	SetCVar(TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"), ScalabilityPreset <= 2 ? TEXT("20") : TEXT("16"));
	SetCVar(TEXT("r.Nanite.MaxPixelsPerEdge"), ScalabilityPreset <= 2 ? TEXT("2") : TEXT("1"));
	ReapplyLook();
}

void UCareerSubsystem::SetGodMode(bool bOn)
{
	bGodMode = bOn;
	if (AIronManCharacter* C = GetPlayer()) { C->bGodMode = bOn; if (bOn) { C->Heal(10000.f); } }
	Toast(bOn ? LOCTEXT("GodOn", "God mode ON") : LOCTEXT("GodOff", "God mode OFF"), FLinearColor(0.6f, 0.85f, 1.f));
}

// ------------------------------------------------------------------ travel

bool UCareerSubsystem::TravelTo(const FVector& Location, float Yaw)
{
	AIronManCharacter* C = GetPlayer();
	if (!C) { return false; }
	C->TeleportTo(Location, FRotator(0.f, Yaw, 0.f), false, true);
	if (AController* PC = C->GetController()) { PC->SetControlRotation(FRotator(-10.f, Yaw, 0.f)); }
	return true;
}

bool UCareerSubsystem::TravelToCity()
{
	ADowntownDistrict* Dist = ADowntownDistrict::Find(GetTickableGameObjectWorld());
	if (!Dist) { Toast(LOCTEXT("NoCity", "The city is not loaded (free play only)"), FLinearColor(1.f, 0.35f, 0.3f)); return false; }
	const FVector Spot = Dist->GetRoadsideSpotNear(Dist->GetActorLocation() + FVector(0.f, -9000.f, 0.f)).GetLocation();
	return TravelTo(Spot + FVector(0, 0, 150.f), 90.f);
}

bool UCareerSubsystem::TravelToArena()
{
	const UWorld* World = GetTickableGameObjectWorld();
	const AArenaGameMode* GM = World ? World->GetAuthGameMode<AArenaGameMode>() : nullptr;
	if (!GM) { return false; }
	return TravelTo(GM->GetArenaCenter() + FVector(0.f, 0.f, GM->GetArenaFloorZ() - GM->GetArenaCenter().Z + 200.f), 0.f);
}

bool UCareerSubsystem::TravelToHouse(FName Id)
{
	ADowntownDistrict* Dist = ADowntownDistrict::Find(GetTickableGameObjectWorld());
	ACityHouse* H = Dist ? Dist->FindHouse(Id) : nullptr;
	if (!H || !OwnsHouse(Id)) { return false; }
	const FTransform T = H->GetSpawnTransform();
	return TravelTo(T.GetLocation(), T.Rotator().Yaw);
}


// ------------------------------------------------------------------------------------------------ dev tools (gated)

void UCareerSubsystem::DevSetInfiniteMoney(bool bOn)
{
	if (!UCareerInput::IsDevMenuAllowed()) { return; }
	bInfiniteMoney = bOn;
	if (bOn) { Credits = InfiniteCredits; }
	Save();
	Toast(bOn ? LOCTEXT("InfOn", "DEV: infinite money ON") : LOCTEXT("InfOff", "DEV: infinite money OFF"));
}

void UCareerSubsystem::DevSetCredits(int32 Amount)
{
	if (!UCareerInput::IsDevMenuAllowed()) { return; }
	Credits = FMath::Clamp(Amount, 0, InfiniteCredits);
	Save();
}

void UCareerSubsystem::DevGiveAllSuits()
{
	if (!UCareerInput::IsDevMenuAllowed()) { return; }
	for (const FShopSuitDef& D : Career::ShopSuits()) { SuitsOwned.AddUnique(D.SuitId); }
	for (int32 i = 0; i < USuitLibrary::GetNumSuits(); ++i) { SuitsOwned.AddUnique(USuitLibrary::GetSuit(i).Id); }
	bBossBeaten = true;
	Save();
	ApplySuitLock(GetPlayer());
	Toast(LOCTEXT("AllSuits", "DEV: every suit unlocked"));
}

void UCareerSubsystem::DevUnlockAllProperty()
{
	if (!UCareerInput::IsDevMenuAllowed()) { return; }
	for (const FCityHouseDef& H : Career::Houses()) { HousesOwned.AddUnique(H.Id); }
	if (HomeHouse.IsNone() && HousesOwned.Num()) { HomeHouse = HousesOwned[0]; }
	for (const FVehicleDef& V : Career::Vehicles()) { VehiclesOwned.AddUnique(V.Id); }
	for (int32 Kind = 0; Kind < 8; ++Kind)
	{
		for (int32 St = 0; St < Career::ClothingStyleCount(Kind); ++St)
		{
			if (const FClothingItem* I = Career::FindClothing(Kind, St)) { ClothingOwned.AddUnique(I->Id); }
		}
	}
	Save();
	Toast(LOCTEXT("AllProps", "DEV: all houses, vehicles and clothes unlocked"));
}

#undef LOCTEXT_NAMESPACE
