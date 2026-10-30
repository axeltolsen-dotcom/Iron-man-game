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
#include "EnvironmentPreset.h"
#include "ArenaEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;

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

	/** Pushes a lighting / sky / fog / camera-look preset (per chapter). Safe if the level
	 *  brought its own lights (only the components that still exist are changed; the
	 *  post-process volume is always ours). */
	void ApplyPreset(const FEnvironmentPreset& Preset);

	/** Free-play look: C++ Arena() preset + the contrib "city" lighting preset. */
	void ApplyArenaLook();

	const FEnvironmentPreset& GetPreset() const { return Preset; }

	/** Show / hide the free-play arena dressing (grid lines, pylons, cover). Story chapters
	 *  hide it and bring their own set; the floor stays (it is the walkable ground). */
	void SetArenaDressingVisible(bool bVisible);

	/** Re-skin the floor (garage concrete, asphalt, rubble ...). */
	void SetFloorSurface(EArenaSurface Surface, const FLinearColor& Tint = FLinearColor::White);

	/** The environment in the world (null if the level has its own). */
	static AArenaEnvironment* Find(const UObject* WorldContext);

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

	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	/** Unbound post-process volume: exposure, bloom, lens flares, grading, grain. */
	UPROPERTY(VisibleAnywhere, Category = "Arena|Lighting")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	/** Indices into Parts that are arena-only dressing (hidden in story chapters). */
	TArray<int32> DressingParts;

	FEnvironmentPreset Preset;
};
