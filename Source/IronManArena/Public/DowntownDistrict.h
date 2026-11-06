// ADowntownDistrict: "Midtown", a procedural open-city district built next to FRIDAY's Harbor District for free
// play (Axel, 2026-11-05: "make it look like GTA 6 with a city"). Everything is engine basic shapes + the
// project's PBR materials (Content/Realism, Content/Env), so it needs no new assets:
//   * 9 x 9 street grid (172 m pitch, 1.55 km across): asphalt with lane lines, crosswalks, wet patches; raised
//     pavements with curbs and tactile corners; parks and plazas.
//   * ~600 buildings sized by distance from the core (glass / steel towers 80-260 m, concrete and brick mid- and
//     low-rise), podiums with storefront glass, cornices, rooftop AC units, water tanks, antennas, helipads and
//     billboards. Lit window strips, storefronts, lamp heads and billboards glow at night (SetNight).
//   * Street furniture: shape street lights (with a 24-light pool that follows the camera at night so only the
//     nearest lamps cost a real light), traffic signals, trees, parked cars in car paint; CC0 props (hydrants,
//     trash cans, benches, utility boxes) from Content/Env/Props when they are present.
//   * Three ACityHouse lots (Harbor Loft, Skyline Penthouse, Cliffside Mansion) - see CityHouse.h.
// Performance (RTX 2070 SUPER target): one HISM per material, distance culling on everything below 25 m, no
// shadows from small props, a fixed light pool. Hidden with the rest of the city during story chapters.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "DowntownDistrict.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UPointLightComponent;
class UStaticMesh;
class ACityHouse;

/** A walk-in place on the map (casino, barber, clothing store, dealership, airstrip). */
struct FCityPoi
{
	FName Type;        // casino / barber / clothing / dealership / airstrip
	FString Name;
	FVector DoorLocal = FVector::ZeroVector;
	/** Interact radius (cm) around the door / station. */
	float Radius = 600.f;
	/** Casino stations only: ECasinoGame index; else -1. */
	int32 Game = -1;
};

UCLASS(NotPlaceable)
class IRONMANARENA_API ADowntownDistrict : public AActor
{
	GENERATED_BODY()

public:
	ADowntownDistrict();

	virtual void Tick(float DeltaSeconds) override;

	/** Spawns the district west of the arena / Harbor District (EnvOrigin = AArenaEnvironment location). -NoDowntown disables. */
	static ADowntownDistrict* SpawnDistrict(UWorld* World, const FVector& EnvOrigin, bool bHarborPresent);
	static ADowntownDistrict* Find(const UObject* WorldContext);

	void SetDistrictVisible(bool bVisible);
	bool IsDistrictVisible() const { return bShown; }

	/** Night: window strips, storefronts, lamp heads, billboards and the street-light pool on; wet roads. */
	void SetNight(bool bNight, bool bWet);
	bool IsNight() const { return bNightOn; }

	/** Street-level centre of Midtown (world). */
	FVector GetCenter() const { return GetActorLocation(); }
	/** World box around the district and its houses (play area). */
	FBox GetPlayBounds() const;

	const TArray<TObjectPtr<ACityHouse>>& GetHouses() const { return Houses; }
	ACityHouse* FindHouse(FName HouseId) const;

	int32 GetNumBuildings() const { return NumBuildings; }

	/**
	 * Destructible parked cars: the nearest instanced car within Radius of WorldLoc is removed from the HISMs and
	 * handed back (transform + paint index) so ACityLife can replace it with a live ACityCar. False if none.
	 */
	bool TakeParkedCarNear(const FVector& WorldLoc, float Radius, FTransform& OutXf, int32& OutPaint);

	/** Random point on a pavement (world, street level + pavement height) within [MinDist, MaxDist] of Near. */
	bool RandomSidewalkPoint(FRandomStream& Rng, const FVector& Near, float MinDist, float MaxDist, FVector& Out) const;
	/** Road grid helpers for traffic: world X/Y of road line i (0..NumBlocks), lane centre offset. */
	float RoadLineWorldX(int32 i) const { return GetActorLocation().X - HalfExtent + i * Pitch; }
	float RoadLineWorldY(int32 i) const { return GetActorLocation().Y - HalfExtent + i * Pitch; }
	/** Street-level Z (top of the asphalt). */
	float StreetZ() const { return GetActorLocation().Z + 3.f; }
	float WalkZ() const { return GetActorLocation().Z + 18.f; }

	/** Walk-in places (doors, casino stations). World location = GetActorTransform().TransformPosition(DoorLocal). */
	const TArray<FCityPoi>& GetPois() const { return Pois; }
	FVector PoiWorld(const FCityPoi& P) const { return GetActorTransform().TransformPosition(P.DoorLocal); }
	/** City airstrip: runway start (world) and heading yaw; rooftop helipads (world). */
	FTransform GetAirstripStart() const { return FTransform(FRotator(0.f, AirstripYaw, 0.f), GetActorTransform().TransformPosition(AirstripLocal)); }
	const TArray<FVector>& GetHelipadsLocal() const { return HelipadsLocal; }
	/** Kerbside delivery spot near a world location (for cars / bikes), facing along the road. */
	FTransform GetRoadsideSpotNear(const FVector& WorldLoc) const;

	/** Car paint colours used by parked / traffic cars. */
	static const TArray<FLinearColor>& CarPaints();
	/** Material for a car paint index (shared with ACityCar). */
	UMaterialInterface* CarPaintMaterial(int32 Paint);

	/** Grid constants (cm). */
	static constexpr int32 NumBlocks = 9;
	static constexpr float BlockSize = 15000.f;  // lot area inside the pavements
	static constexpr float RoadWidth = 1400.f;   // asphalt curb to curb
	static constexpr float WalkWidth = 400.f;    // pavement each side
	static constexpr float Pitch = BlockSize + RoadWidth + 2.f * WalkWidth; // 17200
	static constexpr float HalfExtent = NumBlocks * Pitch * 0.5f;

protected:
	virtual void BeginPlay() override;

	void Build();
	void BuildGround();
	void BuildStreets();
	void BuildBlocks();
	void BuildBuilding(FRandomStream& Rng, const FVector2D& Center, const FVector2D& Size, float DistBlocks);
	void BuildStreetFurniture();
	void BuildCars();
	void BuildHouses();
	void FlushBatches();

	UHierarchicalInstancedStaticMeshComponent* Hism(const FString& Key, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide, float CullEnd, bool bShadows);
	UMaterialInstanceDynamic* Mid(const FString& Key, UMaterialInterface* Parent, const FLinearColor& Tint, float EmissiveUnits = 0.f);
	/** Queues one instance of a basic shape (Size in cm, Center = shape centre) for HISM Key. */
	int32 Add(const FString& Key, EArenaShape Shape, UMaterialInterface* Mat, bool bCollide, float CullEnd, bool bShadows,
		const FVector& Center, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator);
	UProceduralMeshComponent* MakeProc(const FString& Key, bool bCollide);

	/** Emissive MID that is dark by day and NightUnits (emissive units) at night. */
	UMaterialInstanceDynamic* NightMid(const FString& Key, const FLinearColor& Tint, float NightUnits);
	TArray<TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, float>> NightEmissive;

	/** Material for a facade style (glass_a, glass_b, office, concrete_a ...). */
	UMaterialInterface* FacadeMaterial(const FString& Style, float& OutNightNits);

	UPROPERTY() TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient) TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Hisms;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> Mids;
	UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> Pieces;
	UPROPERTY(Transient) TArray<TObjectPtr<ACityHouse>> Houses;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> LightPool;

	struct FBatch
	{
		EArenaShape Shape = EArenaShape::Cube;
		UMaterialInterface* Mat = nullptr;
		bool bCollide = false;
		float Cull = 0.f;
		bool bShadows = true;
		TArray<FTransform> Xf;
	};
	TMap<FString, FBatch> Batches;

	/** HISMs that only show at night (window strips, lamp heads ...). */
	TArray<FString> NightOnlyKeys;
	/** Facade MIDs and their lit-window luminance (nits) at night. */
	TArray<TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, float>> NightFacades;
	/** Road / pavement MIDs that take "Wetness". */
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> WetMids;
	/** Street-lamp heads (world) for the light pool. */
	TArray<FVector> LampHeads;
	struct FParkedCar
	{
		FTransform Xf;   // local
		int32 Paint = 0;
		bool bGone = false;
		TArray<TPair<FString, int32>> Inst;
	};
	TArray<FParkedCar> ParkedCars;
	void AddCarShapes(const FTransform& LocalXf, int32 Paint, TArray<TPair<FString, int32>>* OutInst);

	/** Lots kept free for the houses (local XY centre). */
	TArray<FVector2D> ReservedLots;

	void BuildCasino(const FVector2D& Center);
	void BuildShop(const FVector2D& Center, FName Type, const FString& Name, const FLinearColor& Neon);
	void BuildAirstrip();
	TArray<FCityPoi> Pois;
	FVector AirstripLocal = FVector::ZeroVector;
	float AirstripYaw = 0.f;
	TArray<FVector> HelipadsLocal;

	bool bHarbor = false;
	bool bBuilt = false;
	bool bShown = true;
	bool bNightOn = false;
	float PoolTimer = 0.f;
	int32 NumBuildings = 0;
	FBox LocalBounds = FBox(ForceInit);
};
