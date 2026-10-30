// FSuitRig: the placeholder armour built from engine basic shapes, shared by the player
// (AIronManCharacter) and the suit-select preview (ASuitPreviewActor).
//
//  * SuitRig::Build        CONSTRUCTOR ONLY. Creates the body (torso, arm/leg pivots, palms,
//                          reactor, thruster flames, Hulkbuster plates) plus every part VARIANT:
//                          5 helmets, round/triangle reactor, shoulder pads/cannon/spikes,
//                          jet/missile/minigun backpacks, wings.
//  * SuitRig::BuildGauntlets  CONSTRUCTOR ONLY. First-person forearms + hands attached to a camera.
//  * SuitRig::ApplyLook    Runtime (after ArenaVisuals::PaintAll). Recolours every part by its
//                          ESuitPaintRole from an FSuitDef and shows only that suit's variants.
#pragma once

#include "CoreMinimal.h"
#include "ArenaVisuals.h"
#include "SuitTypes.h"
#include "SuitRig.generated.h"

class USceneComponent;
class UStaticMeshComponent;

USTRUCT()
struct IRONMANARENA_API FSuitRig
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> ShoulderL = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> ShoulderR = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> HipL = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> HipR = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PalmL = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PalmR = nullptr;

	/** Round reactor (also the anchor for the chest beam and the reactor light). */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ArcReactor = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TriReactor = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FlameBootL = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FlameBootR = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FlameHandL = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FlameHandR = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> HulkbusterPlates = nullptr;

	/** Variant roots, indexed by the enum value (nullptr where the variant has no parts). */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Helmets;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> ShoulderSets;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Backpacks;

	UPROPERTY()
	TObjectPtr<USceneComponent> Wings = nullptr;

	// --- first-person gauntlets (BuildGauntlets) ---
	UPROPERTY()
	TObjectPtr<USceneComponent> FPRoot = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> FPArmL = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> FPArmR = nullptr;

	/** Hulkbuster fist plates (children of FPArmL / FPArmR). */
	UPROPERTY()
	TObjectPtr<USceneComponent> FPBusterL = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> FPBusterR = nullptr;

	/** Indices into the gauntlet parts array of the two palm glows. */
	/** Makeshift spine-mounted arc reactor (ESuitReactor::Mounted). */
	UPROPERTY()
	TObjectPtr<USceneComponent> MountedReactor = nullptr;

	/** First-person forearm readout of the mounted reactor (left gauntlet). */
	UPROPERTY()
	TObjectPtr<USceneComponent> FPMountedReactor = nullptr;

	/** Third-person forearm housing (story ch7+: Tony's ring around Morgan's core, left forearm). */
	UPROPERTY()
	TObjectPtr<USceneComponent> ForearmReactor = nullptr;

	/** Part indices of the glowing cores (body parts array / gauntlet parts array). */
	int32 MountedCorePart = INDEX_NONE;
	int32 ForearmCorePart = INDEX_NONE;
	int32 FPMountedCorePart = INDEX_NONE;

	int32 FPPalmPartL = INDEX_NONE;
	int32 FPPalmPartR = INDEX_NONE;
};

namespace SuitRig
{
	/** Feet at Root Z = 0, facing +X, ~185 cm tall. Call from the owner's constructor. */
	IRONMANARENA_API void Build(AActor* Owner, USceneComponent* Root, FSuitRig& Rig, TArray<FArenaPaintedPart>& OutParts);

	/** First-person forearms/hands in front of Camera. Call from the owner's constructor. */
	IRONMANARENA_API void BuildGauntlets(AActor* Owner, USceneComponent* Camera, FSuitRig& Rig, TArray<FArenaPaintedPart>& OutParts);

	/** Recolour Parts from Suit (parts must already be painted). */
	IRONMANARENA_API void ApplyColors(const FSuitDef& Suit, TArray<FArenaPaintedPart>& Parts);

	/**
	 * Show the body with only Suit's variants (bVisible=false hides everything).
	 * bHulkbuster shows the Hulkbuster plates.
	 */
	IRONMANARENA_API void ApplyVisibility(const FSuitDef& Suit, FSuitRig& Rig, bool bVisible, bool bHulkbuster);

	/**
	 * Overrides the mounted-reactor visibility after ApplyVisibility: story mode shows Harley's
	 * reactor on every suit, on the spine or (bForearm) the left forearm.
	 */
	IRONMANARENA_API void ApplyMountedReactor(FSuitRig& Rig, bool bShow, bool bForearm);

	/** XY body scale for the plating level (0 slim, 1 standard, 2 heavy). */
	IRONMANARENA_API float PlatingScale(int32 Plating);
}
