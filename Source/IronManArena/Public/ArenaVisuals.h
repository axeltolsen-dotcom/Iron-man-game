// ArenaVisuals: helpers that turn engine basic shapes into placeholder art.
//
// Every actor in this project is drawn with /Engine/BasicShapes/{Cube,Sphere,Cylinder,Cone}
// so the game is playable with zero imported assets. Each part is a UStaticMeshComponent
// whose colour is set through a UMaterialInstanceDynamic at BeginPlay.
//
// Parent material:
//   * If /Game/IronManArena/Materials/M_ArenaEmissive exists (create it with
//     Scripts/create_emissive_material.py or by hand, see README) it is used. It must have a
//     Vector parameter "Color" and a Scalar parameter "EmissiveStrength"
//     (Emissive = Color * EmissiveStrength) so the arc reactor / repulsors really glow.
//   * Otherwise /Engine/BasicShapes/BasicShapeMaterial is used (it has a "Color" parameter
//     but no emissive). Glowing parts then rely on the point lights we attach to them.
//
// SWAPPING IN REAL MESHES LATER: each actor exposes bUsePlaceholderVisuals. Make a Blueprint
// subclass, untick it (hides VisualRoot) and add your own Static/Skeletal mesh component.
#pragma once

#include "CoreMinimal.h"
#include "ArenaVisuals.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AActor;

/** Which engine basic shape a placeholder part uses. */
UENUM(BlueprintType)
enum class EArenaShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone
};

/** How a suit part is coloured by FSuitDef (see SuitRig.h). Non-suit actors leave it at Fixed. */
UENUM(BlueprintType)
enum class ESuitPaintRole : uint8
{
	Fixed,      // keeps its construction colour (thruster flames, enemy parts ...)
	Primary,
	Secondary,
	Accent,
	Emissive    // eyes, reactor, palms: FSuitDef::EmissiveColor
};

/**
 * Physically based surface a part is drawn with. The materials are built headlessly by
 * Content/Python/build_realism_materials.py into /Game/Realism/Materials; when an asset is
 * missing the part falls back to M_ArenaEmissive / BasicShapeMaterial, so the game never
 * depends on them. Auto = car paint for suit parts, arc glow for emissive parts, robot metal
 * for everything else.
 */
UENUM(BlueprintType)
enum class EArenaSurface : uint8
{
	Auto,
	Plain,          // the old flat emissive material
	SuitPaint,      // MI_SuitHotRod  (M_CarPaint: clear coat, flakes, edge wear, scorch)
	SuitGold,       // MI_SuitGold
	RobotMetal,     // MI_RobotMetal
	Glass,          // M_Glass
	Glow,           // M_ArcGlow (unlit, pulsing fresnel core)
	HeatHaze,       // M_HeatHaze (refraction shimmer)
	GarageFloor,
	Asphalt,
	AsphaltWet,
	Concrete,
	ConcreteWet,
	BlockWall,
	Brick,
	BrickWet,
	MetalPlate,
	RustyMetal,
	Rubble,
	BurnedGround,
	Smoke,          // M_Smoke (soft lit translucent puffs)
	Facade          // M_Facade (procedural tower windows; EmissiveStrength lights them at night)
};

/** One painted placeholder part: the component plus the colour it should get at BeginPlay. */
USTRUCT(BlueprintType)
struct IRONMANARENA_API FArenaPaintedPart
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UStaticMeshComponent> Component = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor Color = FLinearColor::White;

	/** 0 = plain painted surface, >0 = glowing (only visible with M_ArenaEmissive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float Emissive = 0.f;

	/** Suit recolouring role and brightness multiplier (suit parts only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	ESuitPaintRole Role = ESuitPaintRole::Fixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float Shade = 1.f;

	/** Which PBR material to use (Auto picks by role / emissive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	EArenaSurface Surface = EArenaSurface::Auto;

	/** Runtime material instance (created in ArenaVisuals::PaintAll). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
};

namespace ArenaVisuals
{
	/**
	 * Physical emissive calibration. Code and data write "emissive units" (the suit tables' arc
	 * reactor = 25, eyes = 20, blast flash = 10 ...); the materials receive cd/m2 (nits), because
	 * exposure is physical (EV100 ~15.6 on a sunny day, where mid-grey is ~6000 nits; ~4 at night,
	 * mid-grey ~2 nits). 1 unit = ima.EmissiveNitsPerUnit nits (default 1000), so an arc reactor is
	 * ~25000 nits: hot against daylight, blooming at night. Practicals that are dim in reality
	 * (lit windows ~150 nits, screens ~200 nits, exit signs ~200 nits) are given in nits / 1000.
	 */
	IRONMANARENA_API float EmissiveToNits(float EmissiveUnits);

	/** Shared colour palette. */
	IRONMANARENA_API extern const FLinearColor SuitRed;
	IRONMANARENA_API extern const FLinearColor SuitGold;
	IRONMANARENA_API extern const FLinearColor ArcBlue;
	IRONMANARENA_API extern const FLinearColor Thruster;
	IRONMANARENA_API extern const FLinearColor EnemyGrey;
	IRONMANARENA_API extern const FLinearColor EnemyDark;
	IRONMANARENA_API extern const FLinearColor EnemyEye;

	/** Load (or find already-loaded) engine basic shape mesh. Safe in constructors. */
	IRONMANARENA_API UStaticMesh* GetShapeMesh(EArenaShape Shape);

	/** Material used as parent for all dynamic instances (see file comment). */
	IRONMANARENA_API UMaterialInterface* GetParentMaterial();

	/** True if the emissive project material was found. */
	IRONMANARENA_API bool HasEmissiveMaterial();

	/** The realistic material for a surface, or GetParentMaterial() if it was not built. */
	IRONMANARENA_API UMaterialInterface* GetSurfaceMaterial(EArenaSurface Surface);

	/** True if /Game/Realism/Materials exist (build_realism_materials.py has been run). */
	IRONMANARENA_API bool HasRealismMaterials();

	/** Resolves Auto for a part (role / emissive). */
	IRONMANARENA_API EArenaSurface ResolveSurface(const FArenaPaintedPart& Part);

	/** Sets a scalar parameter (Damage, Scorch, Wetness ...) on every part's material instance. */
	IRONMANARENA_API void SetScalarOnAll(TArray<FArenaPaintedPart>& Parts, FName Param, float Value);

	/**
	 * RUNTIME (after BeginPlay): create, register and paint a part immediately. Used by the
	 * procedural chapter sets. bSolid gives it BlockAll (WorldStatic) collision: it blocks the
	 * player and projectiles; AArenaGameMode's spawn probes ignore AChapterSet actors, so
	 * beacons / enemies are never placed on roofs.
	 */
	IRONMANARENA_API UStaticMeshComponent* AddRuntimePart(AActor* Owner, USceneComponent* Parent, EArenaShape Shape,
		const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color,
		float Emissive, EArenaSurface Surface, bool bSolid, TArray<FArenaPaintedPart>& OutParts);

	/**
	 * CONSTRUCTOR ONLY: create a placeholder part as a default subobject of Owner, attach it to
	 * Parent and record its colour in OutParts. Basic shapes are 100 uu in size, so Scale (1,1,1)
	 * gives a 1 m cube / sphere / cylinder. Collision is disabled (gameplay uses capsules/spheres).
	 */
	IRONMANARENA_API UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, FName Name,
		EArenaShape Shape, const FVector& Location, const FVector& Scale, const FRotator& Rotation,
		const FLinearColor& Color, float Emissive, TArray<FArenaPaintedPart>& OutParts,
		EArenaSurface Surface = EArenaSurface::Auto);

	/** BeginPlay: create one dynamic material instance per part and apply its colour. */
	IRONMANARENA_API void PaintAll(UObject* Outer, TArray<FArenaPaintedPart>& Parts);

	/** Temporarily override every part's colour (hit flash). Pass bRestore=true to undo. */
	IRONMANARENA_API void FlashAll(TArray<FArenaPaintedPart>& Parts, const FLinearColor& FlashColor, bool bRestore);

	/** Re-apply Part.Color / Part.Emissive to its material instance (after changing them). */
	IRONMANARENA_API void RepaintPart(FArenaPaintedPart& Part);

	/** Temporarily set a part's glow without changing its stored Emissive (palm flash ...). */
	IRONMANARENA_API void SetPartGlow(FArenaPaintedPart& Part, float Emissive);

	/** Create a standalone dynamic material for a single runtime component (FX, beam ...). */
	IRONMANARENA_API UMaterialInstanceDynamic* PaintComponent(UObject* Outer, UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Emissive, EArenaSurface Surface = EArenaSurface::Auto);
}
