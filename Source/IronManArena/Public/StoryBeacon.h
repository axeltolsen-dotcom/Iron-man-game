// AStoryBeacon: objective marker for "ground" / "flight" story objectives.
// A glowing ring + light column the player reaches on foot (ground) or in the air (flight).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "StoryBeacon.generated.h"

class UPointLightComponent;

UCLASS()
class IRONMANARENA_API AStoryBeacon : public AActor
{
	GENERATED_BODY()

public:
	AStoryBeacon();

	virtual void Tick(float DeltaSeconds) override;

	/** bAir: hovering ring (fly through it); otherwise a ground pad with a tall column. */
	void Setup(bool bAir, float InRadius, const FLinearColor& Color);

	/** True when Location is inside the trigger radius. */
	bool IsReached(const FVector& Location) const;

	float GetRadius() const { return Radius; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USceneComponent> Spinner;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	float Radius = 320.f;
	bool bAirBeacon = false;
	float Time = 0.f;
};
