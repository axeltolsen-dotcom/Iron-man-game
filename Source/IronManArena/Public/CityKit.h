// CityKit: soft lookups into the CC0 city kit imported on Axel's PC (Content/CityKit/citykit_manifest.json:
// "fallbacks" / "materials" / "props", e.g. Asphalt -> /Game/CityKit/Materials/MI_City_Asphalt_Dry). Every lookup
// checks DoesPackageExist first and returns nullptr when the kit (or one asset) is missing, so callers fall back to
// the older realism materials and basic shapes.
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;

namespace CityKit
{
	/** A material by fallback key (Asphalt, AsphaltWet, Sidewalk, Curb, Brick, Metal, Roof, GlassFacade, Facade) or material name. */
	IRONMANARENA_API UMaterialInterface* Material(const TCHAR* Key);
	/** A prop mesh by manifest key (street_lamp_01, tree_small_02, painted_wooden_bench, metal_trash_can, concrete_road_barrier_02). */
	IRONMANARENA_API UStaticMesh* Prop(const TCHAR* Key);
	/** Any static mesh by long package path (no object name needed); nullptr if missing. */
	IRONMANARENA_API UStaticMesh* LoadMeshPath(const FString& PackagePath);
	/** Any material by long package path; nullptr if missing. */
	IRONMANARENA_API UMaterialInterface* LoadMaterialPath(const FString& PackagePath);
	IRONMANARENA_API bool IsKitPresent();
}
