// AArenaEnvironment: a ready-made arena built from basic shapes, spawned at runtime by
// AArenaGameMode when the loaded map has no floor (e.g. the default /Engine/Maps/Entry).
// Contains: a large floor, a ring of glowing pylons, scattered cover blocks, and (optionally)
// a sun, sky atmosphere, sky light and height fog.
//
// You can also drop it into any level by hand, or ignore it completely and build your own map.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "ArenaEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;

UCLASS()
class IRONMANARENA_API AArenaEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AArenaEnvironment();

	/** Remove the lighting components (the level already has its own lights). */
	void RemoveLighting();

	/** Remove floor and props (the level already has geometry). */
	void RemoveGeometry();

	/** Half-size of the square floor in uu (floor is 2*FloorHalfSize across). */
	static constexpr float FloorHalfSize = 9000.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;
};
