// ACityHouse: a property you can buy with credits (Career::Houses): Harbor Loft (small walk-up apartment), Skyline
// Penthouse (glass box + pool + helipad on a 220 m tower) and Cliffside Mansion (Malibu-style cliff house with a
// workshop and an armory bay). Built from engine basic shapes + the realism materials at runtime.
//   * A tall light beam marks it on the skyline: green = for sale, blue = owned (UCareerHUDWidget also draws a
//     screen marker with name / price / distance).
//   * Owned: windows lit, your pilot (APilotAvatar, the avatar creator's look) waits by the door, it becomes a
//     fast-travel / spawn point; the mansion's armory pad repairs, recharges and cools the suit.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "CityHouse.generated.h"

class UPointLightComponent;
class APilotAvatar;

UENUM()
enum class ECityHouseKind : uint8
{
	Loft,
	Penthouse,
	Mansion
};

UCLASS(NotPlaceable)
class IRONMANARENA_API ACityHouse : public AActor
{
	GENERATED_BODY()

public:
	ACityHouse();

	/** Builds the geometry (call once, right after spawning). bOnWater: the mansion's cliff rises from the sea (-3 m). */
	void Setup(FName InHouseId, ECityHouseKind InKind, bool bOnWater);

	FName GetHouseId() const { return HouseId; }
	ECityHouseKind GetKind() const { return Kind; }

	/** Screen marker anchor (world). */
	FVector GetMarkerLocation() const;
	/** Where fast travel / home spawn puts the player (world). */
	FTransform GetSpawnTransform() const;
	bool HasArmory() const { return bArmory; }
	FVector GetArmoryLocation() const;

	/** Owned look (beam colour, lights, the pilot by the door). */
	void SetOwned(bool bOwned);
	bool IsOwned() const { return bIsOwned; }
	void SetNight(bool bNight);
	/** Re-dress the pilot after an avatar change. */
	void RefreshPilot();

	void SetHouseVisible(bool bVisible);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UStaticMeshComponent* Part(EArenaShape Shape, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color,
		EArenaSurface Surface, bool bSolid = true, float Emissive = 0.f, const FRotator& Rot = FRotator::ZeroRotator);
	void BuildLoft();
	void BuildPenthouse();
	void BuildMansion(bool bOnWater);
	void BuildBeam(const FVector& Base, float Height);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<FArenaPaintedPart> Parts;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> InteriorLight;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> ArmoryLight;
	UPROPERTY(Transient) TObjectPtr<APilotAvatar> Pilot;

	/** Indices into Parts: beam (recoloured), interior glow (night / owned), armory glow. */
	TArray<int32> BeamParts;
	TArray<int32> WindowParts;
	TArray<int32> ArmoryParts;

	FName HouseId;
	ECityHouseKind Kind = ECityHouseKind::Loft;
	FVector MarkerLocal = FVector::ZeroVector;
	FTransform SpawnLocal;
	FVector ArmoryLocal = FVector::ZeroVector;
	FVector PilotLocal = FVector::ZeroVector;
	float PilotYaw = 0.f;
	bool bArmory = false;
	bool bIsOwned = false;
	bool bNightOn = false;
};
