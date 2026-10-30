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

namespace ArenaVisuals
{
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
		const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
		switch (Shape)
		{
		case EArenaShape::Sphere:   Path = TEXT("/Engine/BasicShapes/Sphere.Sphere"); break;
		case EArenaShape::Cylinder: Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break;
		case EArenaShape::Cone:     Path = TEXT("/Engine/BasicShapes/Cone.Cone"); break;
		default: break;
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Mesh)
		{
			UE_LOG(LogIronManArena, Warning, TEXT("Could not load basic shape %s"), Path);
		}
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

	UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, FName Name,
		EArenaShape Shape, const FVector& Location, const FVector& Scale, const FRotator& Rotation,
		const FLinearColor& Color, float Emissive, TArray<FArenaPaintedPart>& OutParts)
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
		OutParts.Add(Part);
		return Comp;
	}

	static void ApplyColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive)
	{
		if (!MID) { return; }
		MID->SetVectorParameterValue(ColorParam, Color);
		MID->SetScalarParameterValue(EmissiveParam, Emissive); // ignored by BasicShapeMaterial
	}

	void PaintAll(UObject* Outer, TArray<FArenaPaintedPart>& Parts)
	{
		UMaterialInterface* Parent = GetParentMaterial();
		if (!Parent) { return; }
		for (FArenaPaintedPart& Part : Parts)
		{
			if (!Part.Component) { continue; }
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
		if (Part.MID) { Part.MID->SetScalarParameterValue(EmissiveParam, Emissive); }
	}

	UMaterialInstanceDynamic* PaintComponent(UObject* Outer, UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Emissive)
	{
		if (!Comp) { return nullptr; }
		UMaterialInterface* Parent = GetParentMaterial();
		if (!Parent) { return nullptr; }
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		ApplyColor(MID, Color, Emissive);
		Comp->SetMaterial(0, MID);
		return MID;
	}
}
