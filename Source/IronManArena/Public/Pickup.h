// APickup: health pack, energy cell, Hulkbuster beacon, score gem.
// Values from the 2-D reference (contrib/pickups.js):
//   Health +25 HP (14 s), Energy +30 (14 s), Hulkbuster beacon +1 charge (20 s), Gem +100 pts (10 s)
// Pickups fall to the floor, bob and spin, get magnet-pulled toward the player when close,
// and blink before expiring.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "Pickup.generated.h"

class USphereComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EPickupType : uint8
{
	Health,
	Energy,
	Hulkbuster,
	Gem
};

UCLASS()
class IRONMANARENA_API APickup : public AActor
{
	GENERATED_BODY()

public:
	APickup();

	/** Spawn a pickup of the given type (lifetime / value / colour chosen from Type). */
	static APickup* SpawnPickup(UWorld* World, EPickupType Type, const FVector& Location);

	virtual void Tick(float DeltaSeconds) override;

	/** Changes look + defaults. Call before BeginPlay (SpawnPickup does it via deferred spawn). */
	void SetPickupType(EPickupType InType);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	EPickupType Type = EPickupType::Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Values")
	float HealthAmount = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Values")
	float EnergyAmount = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Values")
	int32 GemScore = 100;

	/** Seconds before the pickup disappears (0 = forever). Set per type in SetPickupType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float Lifetime = 14.f;

	/** Within this distance the pickup flies toward the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float MagnetRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float MagnetSpeed = 1800.f;

	/** Collect sphere radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float CollectRadius = 90.f;

	/** Can't be collected for this long after spawning (lets the pop show). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float ArmTime = 0.35f;

	/** Falling speed until it reaches the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float FallSpeed = 600.f;

	/** Hover this high above the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float HoverHeight = 70.f;

	/** Untick in a Blueprint subclass to hide the basic-shape look and use your own mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	bool bUsePlaceholderVisuals = true;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryCollect(AActor* OtherActor);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	float Age = 0.f;
	float FloorZ = 0.f;
	bool bHasFloor = false;
	bool bCollected = false;
};
