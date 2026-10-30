// ArenaVisuals implementation. See header for the rationale.
#include "ArenaVisuals.h"
#include "IronManArena.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<float> CVarEmissiveNitsPerUnit(
	TEXT("ima.EmissiveNitsPerUnit"), 1000.f,
	TEXT("cd/m2 per emissive unit for every project glow (physical exposure). Applies to newly painted parts."),
	ECVF_Default);

namespace ArenaVisuals
{
	float EmissiveToNits(float EmissiveUnits)
	{
		return EmissiveUnits * FMath::Max(0.f, CVarEmissiveNitsPerUnit.GetValueOnGameThread());
	}

	const FLinearColor SuitRed   = FLinearColor(0.55f, 0.02f, 0.02f);
	const FLinearColor SuitGold  = FLinearColor(0.85f, 0.55f, 0.08f);
	const FLinearColor ArcBlue   = FLinearColor(0.35f, 0.9f, 1.0f);
	const FLinearColor Thruster  = FLinearColor(1.0f, 0.55f, 0.12f);
	const FLinearColor EnemyGrey = FLinearColor(0.18f, 0.19f, 0.22f);
	const FLinearColor EnemyDark = FLinearColor(0.05f, 0.05f, 0.07f);
	const FLinearColor EnemyEye  = FLinearColor(1.0f, 0.15f, 0.05f);

	static const TCHAR* EmissiveMaterialPackage = TEXT("/Game/IronManArena/Materials/M_ArenaEmissive");
	static const TCHAR* EmissiveMaterialObject  = TEXT("/Game/IronManArena/Materials/M_ArenaEmissive.M_ArenaEmissive");
	static const TCHAR* BasicMaterialObject     = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	static const FName ColorParam(TEXT("Color"));
	static const FName EmissiveParam(TEXT("EmissiveStrength"));

	UStaticMesh* GetShapeMesh(EArenaShape Shape)
	{
		// Prefer the project's Nanite copies (Content/Python/env_nanite.py, perf budget §3): Nanite
		// makes VSM much cheaper even for low-poly meshes. Fall back to the engine basic shapes.
		const TCHAR* Name = TEXT("Cube");
		switch (Shape)
		{
		case EArenaShape::Sphere:   Name = TEXT("Sphere"); break;
		case EArenaShape::Cylinder: Name = TEXT("Cylinder"); break;
		case EArenaShape::Cone:     Name = TEXT("Cone"); break;
		default: break;
		}
		static TMap<FName, TWeakObjectPtr<UStaticMesh>> Cache;
		const FName Key(Name);
		if (const TWeakObjectPtr<UStaticMesh>* Hit = Cache.Find(Key))
		{
			if (Hit->IsValid()) { return Hit->Get(); }
		}
		UStaticMesh* Mesh = nullptr;
		const FString NanitePackage = FString::Printf(TEXT("/Game/Env/Shapes/SM_%s"), Name);
		if (FPackageName::DoesPackageExist(NanitePackage))
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.SM_%s"), *NanitePackage, Name));
		}
		if (!Mesh)
		{
			const FString EnginePath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name);
			Mesh = LoadObject<UStaticMesh>(nullptr, *EnginePath);
			if (!Mesh) { UE_LOG(LogIronManArena, Warning, TEXT("Could not load basic shape %s"), *EnginePath); }
		}
		if (Mesh) { Cache.Add(Key, Mesh); }
		return Mesh;
	}

	bool HasEmissiveMaterial()
	{
		static int32 Cached = -1; // -1 unknown, 0 no, 1 yes
		if (Cached < 0)
		{
			Cached = FPackageName::DoesPackageExist(EmissiveMaterialPackage) ? 1 : 0;
		}
		return Cached == 1;
	}

	UMaterialInterface* GetParentMaterial()
	{
		if (HasEmissiveMaterial())
		{
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, EmissiveMaterialObject))
			{
				return M;
			}
		}
		return LoadObject<UMaterialInterface>(nullptr, BasicMaterialObject);
	}

	// ------------------------------------------------------------------ realistic surfaces

	static const TCHAR* SurfaceAssetName(EArenaSurface Surface)
	{
		switch (Surface)
		{
		case EArenaSurface::SuitPaint:    return TEXT("MI_SuitHotRod");
		case EArenaSurface::SuitGold:     return TEXT("MI_SuitGold");
		case EArenaSurface::RobotMetal:   return TEXT("MI_RobotMetal");
		case EArenaSurface::Glass:        return TEXT("M_Glass");
		case EArenaSurface::Glow:         return TEXT("M_ArcGlow");
		case EArenaSurface::HeatHaze:     return TEXT("M_HeatHaze");
		case EArenaSurface::GarageFloor:  return TEXT("MI_GarageFloor");
		case EArenaSurface::Asphalt:      return TEXT("MI_Asphalt");
		case EArenaSurface::AsphaltWet:   return TEXT("MI_Asphalt_Wet");
		case EArenaSurface::Concrete:     return TEXT("MI_Concrete");
		case EArenaSurface::ConcreteWet:  return TEXT("MI_Concrete_Wet");
		case EArenaSurface::BlockWall:    return TEXT("MI_BlockWall");
		case EArenaSurface::Brick:        return TEXT("MI_Brick");
		case EArenaSurface::BrickWet:     return TEXT("MI_Brick_Wet");
		case EArenaSurface::MetalPlate:   return TEXT("MI_MetalPlate");
		case EArenaSurface::RustyMetal:   return TEXT("MI_RustyMetal");
		case EArenaSurface::Rubble:       return TEXT("MI_Rubble");
		case EArenaSurface::BurnedGround: return TEXT("MI_BurnedGround");
		case EArenaSurface::Smoke:        return TEXT("M_Smoke");
		case EArenaSurface::Facade:       return TEXT("M_Facade");
		case EArenaSurface::HangarFloor:  return TEXT("MI_HangarFloor");
		case EArenaSurface::Steel:        return TEXT("MI_Steel");
		case EArenaSurface::RoofGravel:   return TEXT("MI_RoofGravel");
		case EArenaSurface::RoofGravelWet: return TEXT("MI_RoofGravel_Wet");
		case EArenaSurface::AsphaltRain:  return TEXT("MI_AsphaltRain");
		case EArenaSurface::RedBrick:     return TEXT("MI_RedBrick");
		case EArenaSurface::RuinedWall:   return TEXT("MI_RuinedWall");
		case EArenaSurface::DamagedFloor: return TEXT("MI_DamagedFloor");
		case EArenaSurface::DebrisGround: return TEXT("MI_DebrisGround");
		case EArenaSurface::Gravel:       return TEXT("MI_Gravel");
		case EArenaSurface::ConcreteAlt:  return TEXT("MI_ConcreteAlt");
		default: return nullptr;
		}
	}

	/** Older surface to use when a newer (Dume) instance has not been built. */
	static EArenaSurface SurfaceFallback(EArenaSurface Surface)
	{
		switch (Surface)
		{
		case EArenaSurface::HangarFloor:   return EArenaSurface::GarageFloor;
		case EArenaSurface::Steel:         return EArenaSurface::MetalPlate;
		case EArenaSurface::RoofGravel:    return EArenaSurface::Concrete;
		case EArenaSurface::RoofGravelWet: return EArenaSurface::ConcreteWet;
		case EArenaSurface::AsphaltRain:   return EArenaSurface::AsphaltWet;
		case EArenaSurface::RedBrick:      return EArenaSurface::Brick;
		case EArenaSurface::RuinedWall:    return EArenaSurface::BlockWall;
		case EArenaSurface::DamagedFloor:  return EArenaSurface::Concrete;
		case EArenaSurface::DebrisGround:  return EArenaSurface::Rubble;
		case EArenaSurface::Gravel:        return EArenaSurface::Rubble;
		case EArenaSurface::ConcreteAlt:   return EArenaSurface::Concrete;
		default: return EArenaSurface::Plain;
		}
	}

	bool HasRealismMaterials()
	{
		static int32 Cached = -1;
		if (Cached < 0)
		{
			Cached = FPackageName::DoesPackageExist(TEXT("/Game/Realism/Materials/M_CarPaint")) ? 1 : 0;
			UE_LOG(LogIronManArena, Log, TEXT("Realism materials %s (/Game/Realism/Materials)"), Cached ? TEXT("found") : TEXT("missing - using flat fallback"));
		}
		return Cached == 1;
	}

	UMaterialInterface* GetSurfaceMaterial(EArenaSurface Surface)
	{
		static TMap<uint8, TWeakObjectPtr<UMaterialInterface>> Cache;
		const TCHAR* Name = SurfaceAssetName(Surface);
		if (!Name || !HasRealismMaterials()) { return GetParentMaterial(); }
		if (const TWeakObjectPtr<UMaterialInterface>* Found = Cache.Find(uint8(Surface)))
		{
			if (Found->IsValid()) { return Found->Get(); }
		}
		const FString Package = FString(TEXT("/Game/Realism/Materials/")) + Name;
		UMaterialInterface* M = nullptr;
		if (FPackageName::DoesPackageExist(Package))
		{
			M = LoadObject<UMaterialInterface>(nullptr, *(Package + TEXT(".") + Name));
		}
		if (!M)
		{
			const EArenaSurface Fallback = SurfaceFallback(Surface);
			M = Fallback != EArenaSurface::Plain ? GetSurfaceMaterial(Fallback) : GetParentMaterial();
		}
		Cache.Add(uint8(Surface), M);
		return M;
	}

	EArenaSurface ResolveSurface(const FArenaPaintedPart& Part)
	{
		if (Part.Surface != EArenaSurface::Auto) { return Part.Surface; }
		if (Part.Emissive > 0.f) { return EArenaSurface::Glow; }
		if (Part.Role != ESuitPaintRole::Fixed) { return EArenaSurface::SuitPaint; }
		return EArenaSurface::RobotMetal;
	}

	void SetScalarOnAll(TArray<FArenaPaintedPart>& Parts, FName Param, float Value)
	{
		for (FArenaPaintedPart& Part : Parts)
		{
			if (Part.MID) { Part.MID->SetScalarParameterValue(Param, Value); }
		}
	}

	UStaticMeshComponent* AddRuntimePart(AActor* Owner, USceneComponent* Parent, EArenaShape Shape,
		const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color,
		float Emissive, EArenaSurface Surface, bool bSolid, TArray<FArenaPaintedPart>& OutParts)
	{
		if (!Owner) { return nullptr; }
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
		Comp->SetMobility(EComponentMobility::Movable);
		if (UStaticMesh* Mesh = GetShapeMesh(Shape)) { Comp->SetStaticMesh(Mesh); }
		if (Parent) { Comp->SetupAttachment(Parent); }
		Comp->SetRelativeLocation(Location);
		Comp->SetRelativeRotation(Rotation);
		Comp->SetRelativeScale3D(Scale);
		Comp->CastShadow = (Emissive <= 0.f) && Surface != EArenaSurface::Glass && Surface != EArenaSurface::HeatHaze && Surface != EArenaSurface::Smoke;
		if (bSolid)
		{
			Comp->SetCollisionProfileName(TEXT("BlockAll"));
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->RegisterComponent();
		Owner->AddInstanceComponent(Comp);

		FArenaPaintedPart Part;
		Part.Component = Comp;
		Part.Color = Color;
		Part.Emissive = Emissive;
		Part.Surface = Surface;
		if (UMaterialInterface* Mat = GetSurfaceMaterial(ResolveSurface(Part)))
		{
			Part.MID = UMaterialInstanceDynamic::Create(Mat, Owner);
			Part.MID->SetVectorParameterValue(ColorParam, Color);
			Part.MID->SetScalarParameterValue(EmissiveParam, EmissiveToNits(Emissive));
			Comp->SetMaterial(0, Part.MID);
		}
		OutParts.Add(Part);
		return Comp;
	}

	UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, FName Name,
		EArenaShape Shape, const FVector& Location, const FVector& Scale, const FRotator& Rotation,
		const FLinearColor& Color, float Emissive, TArray<FArenaPaintedPart>& OutParts, EArenaSurface Surface)
	{
		check(Owner);
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetupAttachment(Parent);
		if (UStaticMesh* Mesh = GetShapeMesh(Shape))
		{
			Comp->SetStaticMesh(Mesh);
		}
		Comp->SetRelativeLocation(Location);
		Comp->SetRelativeRotation(Rotation);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->CastShadow = (Emissive <= 0.f);

		FArenaPaintedPart Part;
		Part.Component = Comp;
		Part.Color = Color;
		Part.Emissive = Emissive;
		Part.Surface = Surface;
		OutParts.Add(Part);
		return Comp;
	}

	static void ApplyColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive)
	{
		if (!MID) { return; }
		MID->SetVectorParameterValue(ColorParam, Color);
		MID->SetScalarParameterValue(EmissiveParam, EmissiveToNits(Emissive)); // ignored by BasicShapeMaterial
	}

	void PaintAll(UObject* Outer, TArray<FArenaPaintedPart>& Parts)
	{
		for (FArenaPaintedPart& Part : Parts)
		{
			if (!Part.Component) { continue; }
			UMaterialInterface* Parent = GetSurfaceMaterial(ResolveSurface(Part));
			if (!Parent) { continue; }
			Part.MID = UMaterialInstanceDynamic::Create(Parent, Outer);
			ApplyColor(Part.MID, Part.Color, Part.Emissive);
			Part.Component->SetMaterial(0, Part.MID);
		}
	}

	void FlashAll(TArray<FArenaPaintedPart>& Parts, const FLinearColor& FlashColor, bool bRestore)
	{
		for (FArenaPaintedPart& Part : Parts)
		{
			if (bRestore) { ApplyColor(Part.MID, Part.Color, Part.Emissive); }
			else          { ApplyColor(Part.MID, FlashColor, FMath::Max(Part.Emissive, 2.f)); }
		}
	}

	void RepaintPart(FArenaPaintedPart& Part)
	{
		ApplyColor(Part.MID, Part.Color, Part.Emissive);
	}

	void SetPartGlow(FArenaPaintedPart& Part, float Emissive)
	{
		if (Part.MID) { Part.MID->SetScalarParameterValue(EmissiveParam, EmissiveToNits(Emissive)); }
	}

	UMaterialInstanceDynamic* PaintComponent(UObject* Outer, UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Emissive, EArenaSurface Surface)
	{
		if (!Comp) { return nullptr; }
		if (Surface == EArenaSurface::Auto) { Surface = Emissive > 0.f ? EArenaSurface::Glow : EArenaSurface::Plain; }
		UMaterialInterface* Parent = GetSurfaceMaterial(Surface);
		if (!Parent) { return nullptr; }
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		ApplyColor(MID, Color, Emissive);
		Comp->SetMaterial(0, MID);
		return MID;
	}
}
