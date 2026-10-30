#include "ReactorComponent.h"

UReactorComponent::UReactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UReactorComponent::GetMaxHeat() const
{
	return FMath::Max(10.f, MaxHeat + CapacityPerLevel * CapacityLevel);
}

float UReactorComponent::GetRegenMultiplier() const
{
	if (bOverheated) { return 0.f; }
	return (1.f + OutputPerLevel * OutputLevel) * StageRegenMultiplier;
}

float UReactorComponent::GetCapacityMultiplier() const
{
	return (1.f + OutputPerLevel * OutputLevel) * (1.f + PermanentCapacityBonus);
}

void UReactorComponent::SetPermanentUpgrades(bool bCapacitorBank, bool bHeatSinkFins)
{
	PermanentCapacityBonus = bCapacitorBank ? FMath::Max(0.f, CapacitorBankBonus) : 0.f;
	PermanentCoolingBonus = bHeatSinkFins ? FMath::Max(0.f, HeatSinkFinsBonus) : 0.f;
}

void UReactorComponent::AddHeat(float Amount)
{
	if (Amount <= 0.f || IsVenting()) { return; }
	Heat = FMath::Min(GetMaxHeat(), Heat + Amount * (bMountedUnit ? MountedHeatMultiplier : 1.f) * StageHeatMultiplier);
	TimeSinceHeat = 0.f;
	if (Heat >= GetMaxHeat()) { SetOverheated(true); }
}

bool UReactorComponent::Vent()
{
	if (IsVenting() || VentCooldownLeft > 0.f || Heat <= 1.f) { return false; }
	VentTimeLeft = VentDuration;
	VentCooldownLeft = VentCooldown / CoolingMultiplier() + VentDuration;
	return true;
}

void UReactorComponent::ResetReactor()
{
	Heat = 0.f;
	VentTimeLeft = 0.f;
	VentCooldownLeft = 0.f;
	TimeSinceHeat = 0.f;
	SetOverheated(false);
}

void UReactorComponent::SetUpgradeLevel(EReactorUpgrade Upgrade, int32 Level)
{
	Level = FMath::Clamp(Level, 0, MaxUpgradeLevel);
	switch (Upgrade)
	{
	case EReactorUpgrade::Output:   OutputLevel = Level; break;
	case EReactorUpgrade::Cooling:  CoolingLevel = Level; break;
	case EReactorUpgrade::Capacity: CapacityLevel = Level; break;
	}
}

int32 UReactorComponent::GetUpgradeLevel(EReactorUpgrade Upgrade) const
{
	switch (Upgrade)
	{
	case EReactorUpgrade::Output:   return OutputLevel;
	case EReactorUpgrade::Cooling:  return CoolingLevel;
	case EReactorUpgrade::Capacity: return CapacityLevel;
	}
	return 0;
}

FLinearColor UReactorComponent::GetCoreColor(const FLinearColor& CoolColor) const
{
	const float F = GetHeatFraction();
	const FLinearColor Warm(1.f, 0.45f, 0.08f);
	const FLinearColor Hot(1.f, 0.06f, 0.03f);
	if (bOverheated) { return Hot; }
	return F < 0.6f ? FMath::Lerp(CoolColor, Warm, FMath::Max(0.f, F - 0.25f) / 0.35f) : FMath::Lerp(Warm, Hot, (F - 0.6f) / 0.4f);
}

void UReactorComponent::SetOverheated(bool bNew)
{
	if (bOverheated == bNew) { return; }
	bOverheated = bNew;
	OnOverheatChanged.Broadcast(bOverheated);
}

void UReactorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	VentCooldownLeft = FMath::Max(0.f, VentCooldownLeft - DeltaTime);
	TimeSinceHeat += DeltaTime;

	if (VentTimeLeft > 0.f)
	{
		VentTimeLeft -= DeltaTime;
		Heat = FMath::Max(0.f, Heat - VentCoolingPerSecond * CoolingMultiplier() * DeltaTime);
	}
	else if (TimeSinceHeat >= CoolingDelay || bOverheated)
	{
		const float Mul = CoolingMultiplier() * (bOnGround ? GroundCoolingMultiplier : 1.f);
		Heat = FMath::Max(0.f, Heat - PassiveCooling * Mul * DeltaTime);
	}

	if (bOverheated && Heat <= OverheatRecoverAt) { SetOverheated(false); }
}

void UReactorComponent::ApplyStage(EReactorStage NewStage, EReactorMount NewMount)
{
	bHasStage = true;
	bCoreDark = false;
	Stage = NewStage;
	Mount = NewMount;
	bMountedUnit = true;
	StageRegenMultiplier = 1.f;
	int32 Out = 0, Cool = 0, Cap = 0;
	switch (NewStage)
	{
	case EReactorStage::Makeshift: StageHeatMultiplier = MakeshiftHeatMultiplier; break;
	case EReactorStage::Unstable:  StageHeatMultiplier = UnstableHeatMultiplier; break;
	case EReactorStage::Failed:
		StageHeatMultiplier = FailedHeatMultiplier;
		StageRegenMultiplier = FailedRegenMultiplier;
		break;
	case EReactorStage::Upgraded:
		StageHeatMultiplier = UpgradedHeatMultiplier;
		Out = UpgradedLevels.X; Cool = UpgradedLevels.Y; Cap = UpgradedLevels.Z;
		break;
	}
	SetUpgradeLevel(EReactorUpgrade::Output, Out);
	SetUpgradeLevel(EReactorUpgrade::Cooling, Cool);
	SetUpgradeLevel(EReactorUpgrade::Capacity, Cap);
}

void UReactorComponent::ClearStage()
{
	bHasStage = false;
	bCoreDark = false;
	Stage = EReactorStage::Makeshift;
	Mount = EReactorMount::Spine;
	StageHeatMultiplier = 1.f;
	StageRegenMultiplier = 1.f;
	SetUpgradeLevel(EReactorUpgrade::Output, 0);
	SetUpgradeLevel(EReactorUpgrade::Cooling, 0);
	SetUpgradeLevel(EReactorUpgrade::Capacity, 0);
}

float UReactorComponent::GetInstability() const
{
	float Base = 0.f;
	if (bMountedUnit) { Base = 0.05f; }
	if (bHasStage)
	{
		switch (Stage)
		{
		case EReactorStage::Makeshift: Base = 0.08f; break;
		case EReactorStage::Unstable:  Base = 0.35f; break;
		case EReactorStage::Failed:    Base = 0.7f; break;
		case EReactorStage::Upgraded:  Base = 0.f; break;
		}
	}
	return FMath::Clamp(Base + 0.4f * FMath::Max(0.f, GetHeatFraction() - 0.6f) + (bOverheated ? 0.5f : 0.f), 0.f, 1.f);
}

float UReactorComponent::SampleCoreBrightness(float T) const
{
	if (bCoreDark) { return 0.03f; }
	const float I = GetInstability();
	if (I <= 0.001f) { return 1.f + 0.05f * FMath::Sin(T * 2.f); } // steady, slow breathing
	// Fast noise-ish flicker plus occasional brownouts that get longer as instability rises.
	const float Flicker = 0.5f + 0.5f * FMath::Sin(T * 37.f) * FMath::Sin(T * 23.3f + 1.7f);
	const float Brownout = (FMath::Frac(T * 0.37f + FMath::Sin(T * 0.9f)) < I * 0.18f) ? 0.15f : 1.f;
	return FMath::Max(0.05f, (1.f - I * 0.6f * Flicker) * Brownout);
}
