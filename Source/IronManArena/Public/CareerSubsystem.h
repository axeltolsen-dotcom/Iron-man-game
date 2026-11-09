// UCareerSubsystem: the free-play "career" - credits, the suit shop (upgrades + suit roster), houses, vehicles,
// avatar / outfits / barber / clothing, casino limits, time of day, scalability and fast travel. One per game
// instance; persisted in the IronManArena save slot (career fields only - every write re-reads the slot first so
// story / side-mission saves are never clobbered).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CareerTypes.h"
#include "CareerSubsystem.generated.h"

class AIronManCharacter;
class ADowntownDistrict;
class AArenaEnvironment;

struct FCareerToast
{
	FText Text;
	FLinearColor Color = FLinearColor::White;
	double Time = 0.0;
};

UCLASS()
class IRONMANARENA_API UCareerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UCareerSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCareerSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	/** Writes the career fields into the save slot now. */
	void Save();
	/** True when no save exists yet / the character was never created (first launch). */
	bool NeedsCharacterSetup() const { return !bAvatarCreated; }

	// ---- credits
	int32 GetCredits() const { return Credits; }
	int32 GetCreditsEarned() const { return CreditsEarned; }
	/** Adds (or with a negative amount removes, never below 0) credits; shows a toast when Reason is set. */
	void AddCredits(int32 Amount, const FText& Reason = FText::GetEmpty());
	bool CanAfford(int32 Price) const { return Credits >= Price; }
	bool Spend(int32 Price, const FText& What);
	/** Credits for a kill (GM). bResponder: wanted-level responders pay half. */
	void AwardKill(int32 ScoreValue, bool bResponder);

	// ---- upgrades
	int32 GetUpgradeLevel(ECareerUpgrade U) const;
	bool BuyUpgrade(ECareerUpgrade U);

	// ---- suits
	bool IsSuitOwned(FName SuitId) const;
	bool IsSuitLockedByBoss(FName SuitId) const;
	bool BuySuit(FName SuitId);
	/** Owned shop suits + every non-catalogue suit (free play suit lock). */
	TArray<FName> GetAllowedSuitIds() const;
	/** Applies the free-play suit lock to the player (story mode keeps its own). */
	void ApplySuitLock(AIronManCharacter* Char) const;
	bool EquipSuit(FName SuitId);
	void SetBossBeaten();
	bool IsBossBeaten() const { return bBossBeaten; }

	// ---- houses
	bool OwnsHouse(FName Id) const { return HousesOwned.Contains(Id); }
	bool BuyHouse(FName Id);
	FName GetHomeHouse() const { return HomeHouse; }
	void SetHomeHouse(FName Id);
	const TArray<FName>& GetHousesOwned() const { return HousesOwned; }
	/** Credits per minute from owned houses. */
	int32 GetPassiveIncome() const;

	// ---- vehicles
	bool OwnsVehicle(FName Id) const { return VehiclesOwned.Contains(Id); }
	bool BuyVehicle(FName Id);
	const TArray<FName>& GetVehiclesOwned() const { return VehiclesOwned; }

	// ---- avatar
	const FAvatarConfig& GetAvatar() const { return Avatar; }
	/** Stores the avatar, refreshes the suit / pilot everywhere (and saves when bSave). */
	void SetAvatar(const FAvatarConfig& In, bool bSave = true);
	void MarkCharacterCreated();
	FString GetPilotName() const { return Avatar.PilotName.IsEmpty() ? FString(TEXT("Harley")) : Avatar.PilotName; }

	// ---- clothing / barber
	bool OwnsClothing(int32 Kind, int32 Style) const;
	bool BuyClothing(int32 Kind, int32 Style);
	const FAvatarOutfit* GetOutfitSlot(int32 Index) const { return OutfitSlots.IsValidIndex(Index) ? &OutfitSlots[Index] : nullptr; }
	void SaveOutfitSlot(int32 Index);
	void LoadOutfitSlot(int32 Index);

	// ---- casino (credits only)
	/** Net credits you may still lose today. */
	int32 GetCasinoLossAllowance() const;
	/** Settles a round: Net > 0 win, < 0 loss. */
	void SettleCasino(int32 Net, ECasinoGame Game);

	// ---- side jobs (phone board; credits only)
	/** Seconds remaining before JobId can be claimed again (0 = ready). */
	int32 JobCooldownLeft(FName JobId) const;
	/** Pays out the job if off cooldown; returns false if still cooling or unknown. */
	bool ClaimJob(FName JobId);

	// ---- toasts (HUD)
	void Toast(const FText& Text, const FLinearColor& Color = FLinearColor(1.f, 0.85f, 0.35f));
	const TArray<FCareerToast>& GetToasts() const { return Toasts; }

	// ---- world: time of day, look, scalability, travel, god mode
	/** 0..24. Picks day / golden hour / night lighting, sun angle, Leonida grade, city night lights. */
	void SetTimeOfDay(float Hour);
	float GetTimeOfDay() const { return TimeOfDay; }
	/** Re-applies the time of day (after an environment preset change). */
	void ReapplyLook() { SetTimeOfDay(TimeOfDay); }
	void SetPhotoreal(bool bOn);
	bool IsPhotoreal() const;
	/** 0 Low, 1 Medium, 2 "2070 SUPER" (High, 1080p60 Lumen), 3 Epic, 4 Cinematic. */
	void ApplyScalabilityPreset(int32 Preset);
	int32 GetScalabilityPreset() const { return ScalabilityPreset; }
	static FText ScalabilityName(int32 Preset);
	void SetGodMode(bool bOn);
	bool IsGodMode() const { return bGodMode; }

	// ---- private dev tools (no-ops unless UCareerInput::IsDevMenuAllowed(): Editor / dev builds, or Saved/AllowDevMenu.txt) ----
	void DevSetInfiniteMoney(bool bOn);
	bool IsInfiniteMoney() const { return bInfiniteMoney; }
	void DevSetCredits(int32 Amount);
	void DevGiveAllSuits();
	void DevUnlockAllProperty(); // houses, vehicles, clothing
	static constexpr int32 InfiniteCredits = 999999999;

	bool TravelToCity();
	bool TravelToArena();
	bool TravelToHouse(FName Id);
	bool TravelTo(const FVector& Location, float Yaw);

	/** Career stats on top of the suit for the character (called from AIronManCharacter::ApplySuit). Returns true when
	 *  Look was changed (perks / paint / helmet / armour silhouette). */
	static bool ApplyToCharacter(AIronManCharacter* Char, FSuitDef& Look);

	/** Re-applies the career to the player (after a purchase). */
	void RefreshPlayer();
	AIronManCharacter* GetPlayer() const;

private:
	void Load();
	void MarkDirty() { bDirty = true; }
	void TickHouses(float DeltaTime);

	int32 Credits = 0;
	int32 CreditsEarned = 0;
	TArray<int32> UpgradeLevels;
	TArray<FName> SuitsOwned;
	TArray<FName> HousesOwned;
	FName HomeHouse;
	TArray<FName> VehiclesOwned;
	bool bAvatarCreated = false;
	FAvatarConfig Avatar;
	TArray<FAvatarOutfit> OutfitSlots;
	TArray<FName> ClothingOwned;
	bool bBossBeaten = false;
	int32 CasinoLossToday = 0;
	int32 CasinoDay = 0;
	TArray<FName> JobsClaimed;
	TArray<int64> JobClaimUnix;

	TArray<FCareerToast> Toasts;
	bool bDirty = false;
	float SaveTimer = 0.f;
	float IncomeTimer = 0.f;
	float ArmoryToastCooldown = 0.f;
	float TimeOfDay = 13.f;
	bool bTimeOfDaySet = false;
	int32 ScalabilityPreset = 2;
	bool bGodMode = false;
	bool bInfiniteMoney = false;
	FString SlotName = TEXT("IronManArena");
};
