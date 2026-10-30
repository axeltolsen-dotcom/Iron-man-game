// Procedural per-chapter environment sets (C++ only, built from basic shapes + the realism
// materials): Tony's lake-house workshop, storm rooftops, the city, ruins and Titan's battlefield.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "EnvironmentPreset.h"
#include "ChapterSet.generated.h"

class ULocalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class UDecalComponent;

/** A two-joint workshop robot arm (U / Dum-E): pivots animated in Tick. */
USTRUCT()
struct FWorkshopArm
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<USceneComponent> Base;
	UPROPERTY() TObjectPtr<USceneComponent> Shoulder;
	UPROPERTY() TObjectPtr<USceneComponent> Elbow;
	UPROPERTY() TObjectPtr<USceneComponent> Wrist;
	float Phase = 0.f;
	float Speed = 1.f;
};

/** Cheap particle stand-in (embers, rain streaks, smoke puffs): moved in Tick, recycled. */
USTRUCT()
struct FSetParticle
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Comp;
	FVector Velocity = FVector::ZeroVector;
	FVector Home = FVector::ZeroVector;
	FVector BaseScale = FVector::OneVector;
	float Age = 0.f;
	float Life = 1.f;
	int32 PartIndex = INDEX_NONE;
};

UCLASS(NotBlueprintable)
class IRONMANARENA_API AChapterSet : public AActor
{
	GENERATED_BODY()

public:
	AChapterSet();

	/** Spawns a set (workshop / rooftops / city / ruins / battlefield) with its origin on the arena
	 *  floor; the preset picks the variant (night windows, wet, storm, embers). */
	static AChapterSet* SpawnSet(UWorld* World, const FVector& Origin, FName SetName, const FEnvironmentPreset& Preset);

	FName GetSetId() const { return SetId; }
	int32 GetPartCount() const { return Parts.Num(); }
	int32 GetLightCount() const { return Lights.Num(); }
	int32 GetAnimatedArmCount() const { return Arms.Num(); }
	int32 GetParticleCount() const { return Particles.Num(); }

	virtual void Tick(float DeltaSeconds) override;

	/** Max radius from the origin that the set keeps clear for gameplay (beacons / spawns). */
	static constexpr float PlayRadius = 6500.f;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	void BuildWorkshop(bool bNight);
	void BuildRooftops(bool bStorm);
	void BuildCity(bool bNight, bool bWet);
	void BuildRuins(bool bBattlefield);
	void BuildWeather();

	// --- helpers (all sizes are full extents in Unreal units, locations relative to the origin) ---
	UStaticMeshComponent* Shape(EArenaShape InShape, const FVector& Center, const FVector& Size, const FRotator& Rot,
		const FLinearColor& Color, EArenaSurface Surface, bool bSolid, float Emissive = 0.f, USceneComponent* Parent = nullptr);
	UStaticMeshComponent* Box(const FVector& Center, const FVector& Size, const FLinearColor& Color, EArenaSurface Surface,
		bool bSolid = true, const FRotator& Rot = FRotator::ZeroRotator, float Emissive = 0.f, USceneComponent* Parent = nullptr);
	UPointLightComponent* PointLight(const FVector& Loc, const FLinearColor& Color, float Lumens, float Radius, bool bShadows = false);
	USpotLightComponent* SpotLight(const FVector& Loc, const FRotator& Rot, const FLinearColor& Color, float Lumens, float Radius,
		float InnerDeg, float OuterDeg, bool bShadows);
	USceneComponent* Pivot(USceneComponent* Parent, const FVector& Loc, const FRotator& Rot = FRotator::ZeroRotator);
	void SetLastScalar(FName Param, float Value);

	void Building(const FVector2D& XY, const FVector& Size, float BaseZ, const FLinearColor& Wall, bool bNight, float LitFraction);
	void Car(const FVector& Loc, float Yaw, const FLinearColor& Paint, bool bWrecked, bool bLightsOn);
	void StreetLamp(const FVector& Loc, float Yaw, bool bOn);
	void DisplaySuit(const FVector& Loc, float Yaw, const FLinearColor& Primary, const FLinearColor& Secondary, bool bCaseLit);
	void WorkshopArm(const FVector& Loc, float Yaw, const FLinearColor& Color, float Phase, float Speed);
	void Fire(const FVector& Loc, float Size);
	void SmokeColumn(const FVector& Loc, int32 Puffs, float Size, const FLinearColor& Color);
	void Crater(const FVector& Loc, float Radius);
	void Ruin(const FVector2D& XY, float Yaw, float Width, float Height, int32 Seed);

	void TickParticles(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Set")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	UPROPERTY()
	TArray<TObjectPtr<ULocalLightComponent>> Lights;

	UPROPERTY()
	TArray<FWorkshopArm> Arms;

	UPROPERTY()
	TArray<FSetParticle> Particles;

	/** Embers: drift up and respawn. Rain: fall and respawn around the player. Smoke: rise, grow, fade. */
	TArray<int32> EmberIdx, RainIdx, SmokeIdx;

	/** Blinking aviation / status lights (Parts indices). */
	TArray<int32> BlinkParts;

	/** Flickering fire lights (+ base intensity). */
	TArray<TPair<TObjectPtr<UPointLightComponent>, float>> FireLights;

	FName SetId;
	FEnvironmentPreset Preset;
	float Clock = 0.f;
	FRandomStream Rng;
};
