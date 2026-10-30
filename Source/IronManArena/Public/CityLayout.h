// ACityLayout: FRIDAY's Harbor District (contrib/friday/city_layout.json -> Content/City/city_layout.json,
// staged as a non-UFS file). A 2 x 2 km district in cm with the origin at its centre (+X east, +Y south).
//
// Built at runtime by the free-play arena (AArenaEnvironment) in place of the old 9 x 9 block grid:
//   * 2,738 buildings as Nanite cube instances (one HISM per facade style, Dum-E's facade MIs), tiers as
//     stacked boxes, ruins tilted, simple box collision; the two polygon footprints (flatiron wedge, Legacy
//     Tower octagon) as extruded prisms. Low-rise instances are distance-culled; towers never are.
//   * 404 road segments with Dum-E's lane-marked road MIs, sidewalks, intersection patches; block slabs
//     (pavement / park ground / ruins debris / piers); ground out to 2.4 km; the harbor water plane and seawall.
//   * Legacy Tower's landing pad and arc-reactor crown ring; rooftop helipads, AC units and water towers.
//   * Gameplay data: the spawn pools (drone / tank / boss / pickup / player_start, absolute positions) and
//     storyLocations (centre + playRadiusCm), read by AArenaGameMode.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityLayout.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMeshComponent;

UENUM()
enum class ECitySpawnType : uint8
{
	PlayerStart,
	Boss,
	Tank,
	Drone,
	Pickup
};

UCLASS(NotPlaceable)
class IRONMANARENA_API ACityLayout : public AActor
{
	GENERATED_BODY()

public:
	ACityLayout();

	/** True when Content/City/city_layout.json parses (cached). -NoCityLayout on the command line disables it. */
	static bool IsAvailable();

	/** Spawns the district with its street-level origin at Origin. */
	static ACityLayout* SpawnCity(UWorld* World, const FVector& Origin);

	/** First live city in the world (nullptr when free play uses the old grid). */
	static ACityLayout* Find(const UObject* WorldContext);

	/** Counts from the parsed JSON (tests): buildings, roads, blocks, spawns. */
	static void GetLayoutStats(int32& OutBuildings, int32& OutRoads, int32& OutBlocks, int32& OutSpawns);

	/** storyLocations[Location].center (world) and playRadiusCm. False when the location is unknown. */
	bool GetStoryLocation(FName Location, FVector& OutCenter, float& OutRadius) const;

	/** A player_start by id, world transform. */
	bool GetPlayerStart(FName Id, FTransform& OutTransform) const;

	/**
	 * Picks a spawn point of Type from the pool: a random one between MinDist and MaxDist (2-D) of Near, inside
	 * the play circle, not used in the last 8 s; otherwise the closest unused one beyond MinDist. World location.
	 */
	bool PickSpawn(ECitySpawnType Type, const FVector& Near, float MinDist, float MaxDist, const FVector& PlayCenter,
		float PlayRadius, FVector& OutLocation);

	/** Pickup spawn points (world) with their type name (Health / Energy / Hulkbuster / Gem) inside a circle. */
	void GetPickups(const FVector& Center, float Radius, TArray<TPair<FVector, FString>>& Out) const;

	/** Legacy Tower base (world). */
	FVector GetLandmarkLocation() const;

	int32 GetNumBuildingInstances() const { return NumBuildingInstances; }

	/** Hidden while a story chapter set is up (AArenaEnvironment::SetArenaDressingVisible). */
	void SetCityVisible(bool bVisible);

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	void BuildGround();
	void BuildRoads();
	void BuildBlocks();
	void BuildBuildings();
	void BuildLandmark();

	UHierarchicalInstancedStaticMeshComponent* MakeHism(const FString& Key, UMaterialInterface* Material, bool bCollide,
		float CullEnd, bool bShadows = true);
	UMaterialInstanceDynamic* Mid(const FString& Key, UMaterialInterface* Parent, const FLinearColor& Tint, float EmissiveNits = 0.f);
	UProceduralMeshComponent* MakeProc(const FString& Key, bool bCollide);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Hisms;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> Mids;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Pieces;

	TArray<double> SpawnLastUsed;
	int32 NumBuildingInstances = 0;
	bool bBuilt = false;
};
