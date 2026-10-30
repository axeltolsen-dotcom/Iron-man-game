// SuitArmorKit: U's procedural armour kit (contrib/u/armor_kit.json -> Content/Story/armor_kit.json).
//
// Builds bevel-less shell plates (extrudeShell / loftShell / revolve / superellipsoidShell / box / cylinder /
// cone / pyramid / barrelCluster) as runtime UStaticMeshes (FMeshDescription -> BuildFromMeshDescriptions,
// 2D triangulation by GeometryCore's constrained Delaunay) and attaches one UStaticMeshComponent per plate.
// Per suit: base plates -> variant groups (helmet, reactor, shoulders, backpack, wings, plating) -> style ->
// suitOverrides[id] -> mirror L->R -> panel lines (V-grooves: extrude 'pts' lines and loft 'ring' lines) -> attach.
//
// Attachment: when Owner has a USkeletalMeshComponent whose skeleton has the plate's bone (Manny), the
// plate is socketed to that bone using the REAL ref-pose joint positions (FReferenceSkeleton). Otherwise
// (the player's placeholder rig, which has no skeleton) it uses U's rigFallback: skeleton.nominalPoseCm
// joint positions, head/neck/spine/pelvis on the rig root, arms on ShoulderL/R, legs on HipL/R.
// Plates are ordinary painted parts (FArenaPaintedPart), so SuitRig::ApplyColors / FlashAll / ClassifyGlow
// treat them exactly like the old placeholder shapes (glow plates keep their Eye/Reactor/Palm keyword).
//
// Not implemented (logged in README): geometric bevels (normals are smoothed instead), seam/plane/ringR
// grooves, rivets and weld beads, sliding plates, overlap 'cut' booleans, per-segment bone-length scaling.
#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FArenaPaintedPart;
struct FSuitDef;
struct FSuitRig;

namespace SuitArmorKit
{
	/** True when Content/Story/armor_kit.json loaded with at least one plate. */
	IRONMANARENA_API bool IsLoaded();

	/** Base plates as authored (before variants and mirroring). */
	IRONMANARENA_API int32 GetNumBasePlates();

	/** Plates the suit resolves to (variants + style + overrides + mirroring), without building meshes. */
	IRONMANARENA_API int32 CountPlates(const FSuitDef& Suit);

	/** Triangles of the generated mesh for one resolved plate id (tests); -1 if the id is unknown. */
	IRONMANARENA_API int32 CountPlateTriangles(const FSuitDef& Suit, const FString& PlateId);

	/**
	 * Remove any previous kit plates from Rig / Parts and build Suit's plates (runtime; after BeginPlay).
	 * SkelMesh (optional) = Manny body to socket on. The rig's placeholder shapes (except thruster flames and
	 * the Hulkbuster plates) are hidden while plates are shown. New parts are painted, so
	 * call SuitRig::ApplyColors afterwards. Returns the number of plates built (0 = kept the placeholders).
	 */
	IRONMANARENA_API int32 Build(AActor* Owner, const FSuitDef& Suit, FSuitRig& Rig, TArray<FArenaPaintedPart>& Parts,
		USkeletalMeshComponent* SkelMesh = nullptr);

	/** Remove kit plates and show the placeholder shapes again. */
	IRONMANARENA_API void Clear(FSuitRig& Rig, TArray<FArenaPaintedPart>& Parts);
}
