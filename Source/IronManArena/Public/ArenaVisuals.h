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

	/** Runtime material instance (created in ArenaVisuals::PaintAll). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
};

namespace ArenaVisuals
{
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

	/**
	 * CONSTRUCTOR ONLY: create a placeholder part as a default subobject of Owner, attach it to
	 * Parent and record its colour in OutParts. Basic shapes are 100 uu in size, so Scale (1,1,1)
	 * gives a 1 m cube / sphere / cylinder. Collision is disabled (gameplay uses capsules/spheres).
	 */
	IRONMANARENA_API UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, FName Name,
		EArenaShape Shape, const FVector& Location, const FVector& Scale, const FRotator& Rotation,
		const FLinearColor& Color, float Emissive, TArray<FArenaPaintedPart>& OutParts);

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
		const FLinearColor& Color, float Emissive);
}
