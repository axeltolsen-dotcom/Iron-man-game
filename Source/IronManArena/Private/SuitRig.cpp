#include "SuitRig.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IronManArena.h"

using namespace ArenaVisuals;

namespace
{
	// Construction colours (Mark III-ish); ApplyColors replaces them from the FSuitDef.
	FLinearColor DefaultRoleColor(ESuitPaintRole Role)
	{
		switch (Role)
		{
		case ESuitPaintRole::Primary:   return SuitRed;
		case ESuitPaintRole::Secondary: return SuitGold;
		case ESuitPaintRole::Accent:    return SuitGold * 0.8f;
		case ESuitPaintRole::Emissive:  return ArcBlue;
		default:                        return FLinearColor::White;
		}
	}

	struct FPartMaker
	{
		AActor* Owner;
		TArray<FArenaPaintedPart>& Parts;

		UStaticMeshComponent* operator()(USceneComponent* Parent, const TCHAR* Name, EArenaShape Shape, FVector Loc, FVector Scale,
			ESuitPaintRole Role, float Shade = 1.f, float Emissive = 0.f, FRotator Rot = FRotator::ZeroRotator) const
		{
			const FLinearColor Color = (Role == ESuitPaintRole::Fixed) ? Thruster : DefaultRoleColor(Role) * Shade;
			UStaticMeshComponent* C = AddPart(Owner, Parent, FName(Name), Shape, Loc, Scale, Rot, Color, Emissive, Parts);
			FArenaPaintedPart& P = Parts.Last();
			P.Role = Role;
			P.Shade = Shade;
			return C;
		}
	};

	USceneComponent* MakeRoot(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		USceneComponent* C = Owner->CreateDefaultSubobject<USceneComponent>(FName(Name));
		C->SetupAttachment(Parent);
		return C;
	}

	using R = ESuitPaintRole;

	/** Fixed-colour part (wiring, bare metal) that suit recolouring leaves alone. */
	UStaticMeshComponent* FixedPart(AActor* Owner, TArray<FArenaPaintedPart>& Parts, USceneComponent* Parent, const TCHAR* Name,
		EArenaShape Shape, FVector Loc, FVector Scale, const FLinearColor& Color, float Emissive = 0.f, FRotator Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = AddPart(Owner, Parent, FName(Name), Shape, Loc, Scale, Rot, Color, Emissive, Parts);
		Parts.Last().Role = ESuitPaintRole::Fixed;
		return C;
	}

	const FLinearColor WireRed(0.45f, 0.03f, 0.02f);
	const FLinearColor WireBlack(0.02f, 0.02f, 0.02f);
	const FLinearColor Copper(0.55f, 0.25f, 0.08f);
	const FLinearColor BareSteel(0.3f, 0.3f, 0.32f);
}

// ---------------------------------------------------------------------------- suit materials
// Content/Story/suit_materials.json (U's per-suit PBR table, linear colour): paint layers map
// 1:1 onto M_CarPaint, emissive entries onto M_ArcGlow. Missing file / suit = heuristics.
namespace
{
	struct FSuitLayerMat
	{
		bool bValid = false;
		FLinearColor Color = FLinearColor::White;
		float Metallic = 0.7f, Roughness = 0.3f, ClearCoat = 1.f, ClearCoatRoughness = 0.04f, Flake = 0.35f, FlakeScale = 1.2f;
		// M_SuitMaster (Vison gap list): dielectric F0 scale and brushed-metal anisotropy.
		float Specular = 0.5f, Anisotropy = 0.f, AnisotropyAngle = 0.f;
	};
	struct FSuitGlowMat
	{
		bool bValid = false;
		FLinearColor Color = FLinearColor::White;
		float Intensity = 10.f, CoreWhite = 0.5f;
	};
	struct FSuitMaterialDef
	{
		FSuitLayerMat Layers[3]; // primary, secondary, accent
		FSuitGlowMat Eyes, Reactor, Repulsors;
		FLinearColor WearColor = FLinearColor(0.56f, 0.57f, 0.58f);
		float EdgeWear = 0.1f, Scorch = 0.f, Dirt = 0.f, PanelLineDepth = 0.f;
	};

	FLinearColor ReadColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A && A->Num() >= 3)
		{
			return FLinearColor(float((*A)[0]->AsNumber()), float((*A)[1]->AsNumber()), float((*A)[2]->AsNumber()), 1.f);
		}
		return Default;
	}

	float ReadNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Default)
	{
		double V = 0.0;
		return (O.IsValid() && O->TryGetNumberField(Field, V)) ? float(V) : Default;
	}

	FSuitGlowMat ReadGlow(const TSharedPtr<FJsonObject>& Emissive, const TCHAR* Field)
	{
		FSuitGlowMat G;
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (Emissive.IsValid() && Emissive->TryGetObjectField(Field, O) && O && O->IsValid()) // null entries = no such light
		{
			G.bValid = true;
			G.Color = ReadColor(*O, TEXT("colorLinear"), G.Color);
			G.Intensity = ReadNum(*O, TEXT("intensity"), G.Intensity);
			G.CoreWhite = ReadNum(*O, TEXT("coreWhite"), G.CoreWhite);
		}
		return G;
	}

	const TMap<FName, FSuitMaterialDef>& GetSuitMaterials()
	{
		static TMap<FName, FSuitMaterialDef> Table;
		static bool bLoaded = false;
		if (bLoaded) { return Table; }
		bLoaded = true;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/suit_materials.json");
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogIronManArena, Warning, TEXT("Suit materials: %s missing or invalid - using built-in finishes"), *Path);
			return Table;
		}
		const TSharedPtr<FJsonObject>* Suits = nullptr;
		if (!Root->TryGetObjectField(TEXT("suits"), Suits) || !Suits) { return Table; }
		for (const auto& Pair : (*Suits)->Values)
		{
			const TSharedPtr<FJsonObject> S = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			if (!S.IsValid()) { continue; }
			FSuitMaterialDef Def;
			const TSharedPtr<FJsonObject>* Layers = nullptr;
			if (S->TryGetObjectField(TEXT("layers"), Layers) && Layers)
			{
				const TCHAR* Names[3] = { TEXT("primary"), TEXT("secondary"), TEXT("accent") };
				for (int32 i = 0; i < 3; ++i)
				{
					const TSharedPtr<FJsonObject>* L = nullptr;
					if (!(*Layers)->TryGetObjectField(Names[i], L) || !L) { continue; }
					FSuitLayerMat& M = Def.Layers[i];
					M.bValid = true;
					M.Color = ReadColor(*L, TEXT("baseColorLinear"), M.Color);
					M.Metallic = ReadNum(*L, TEXT("metallic"), M.Metallic);
					M.Roughness = ReadNum(*L, TEXT("roughness"), M.Roughness);
					M.ClearCoat = ReadNum(*L, TEXT("clearcoat"), M.ClearCoat);
					M.ClearCoatRoughness = ReadNum(*L, TEXT("clearcoatRoughness"), M.ClearCoatRoughness);
					M.Flake = ReadNum(*L, TEXT("metalFlake"), M.Flake);
					M.FlakeScale = ReadNum(*L, TEXT("flakeScale"), M.FlakeScale);
					M.Specular = FMath::Clamp(ReadNum(*L, TEXT("specular"), M.Specular), 0.f, 1.f);
					// Brushed finishes get a default anisotropy unless the table sets one.
					FString Finish;
					(*L)->TryGetStringField(TEXT("finish"), Finish);
					const float AnisoDefault = (Finish.Contains(TEXT("brush")) && M.Metallic > 0.5f) ? 0.4f : 0.f;
					M.Anisotropy = FMath::Clamp(ReadNum(*L, TEXT("anisotropy"), AnisoDefault), -1.f, 1.f);
					M.AnisotropyAngle = ReadNum(*L, TEXT("anisotropyAngle"), 0.f);
				}
			}
			const TSharedPtr<FJsonObject>* Wear = nullptr;
			if (S->TryGetObjectField(TEXT("wear"), Wear) && Wear)
			{
				Def.EdgeWear = ReadNum(*Wear, TEXT("edgeWear"), Def.EdgeWear);
				Def.Scorch = ReadNum(*Wear, TEXT("scorch"), Def.Scorch);
				Def.Dirt = ReadNum(*Wear, TEXT("dirt"), Def.Dirt);
				Def.WearColor = ReadColor(*Wear, TEXT("substrateColorLinear"), Def.WearColor);
			}
			const TSharedPtr<FJsonObject>* Em = nullptr;
			if (S->TryGetObjectField(TEXT("emissive"), Em) && Em)
			{
				Def.Eyes = ReadGlow(*Em, TEXT("eyes"));
				Def.Reactor = ReadGlow(*Em, TEXT("reactor"));
				Def.Repulsors = ReadGlow(*Em, TEXT("repulsors"));
			}
			Def.PanelLineDepth = ReadNum(S, TEXT("panelLineDepth"), 0.f);
			Table.Add(FName(*Pair.Key), Def);
		}
		UE_LOG(LogIronManArena, Log, TEXT("Suit materials: %d suits from %s"), Table.Num(), *Path);
		return Table;
	}

	enum class EGlowKind : uint8 { Eyes, Reactor, Repulsor, Other };

	EGlowKind ClassifyGlow(const FArenaPaintedPart& P)
	{
		const FString N = P.Component ? P.Component->GetName() : FString();
		if (N.Contains(TEXT("Eye")) || N.Contains(TEXT("Visor"))) { return EGlowKind::Eyes; }
		if (N.Contains(TEXT("Palm")) || N.Contains(TEXT("Muzzle")) || N.Contains(TEXT("Repulsor")) || N.Contains(TEXT("Unibeam"))) { return EGlowKind::Repulsor; }
		if (N.Contains(TEXT("Reactor")) || N.Contains(TEXT("Core")) || N.Contains(TEXT("Arc")) || N.Contains(TEXT("Tri"))) { return EGlowKind::Reactor; }
		return EGlowKind::Other;
	}
}

namespace SuitRig
{
	bool HasSuitMaterial(FName SuitId) { return GetSuitMaterials().Contains(SuitId); }
	int32 GetSuitMaterialCount() { return GetSuitMaterials().Num(); }
}

namespace SuitRig
{
	void Build(AActor* Owner, USceneComponent* Root, FSuitRig& Rig, TArray<FArenaPaintedPart>& OutParts)
	{
		check(Owner && Root);
		FPartMaker Part{ Owner, OutParts };
		Rig.Root = Root;

		// --- Torso ---
		Part(Root, TEXT("Pelvis"), EArenaShape::Cube, FVector(0, 0, 100), FVector(0.26f, 0.36f, 0.14f), R::Secondary);
		Part(Root, TEXT("Torso"), EArenaShape::Cube, FVector(0, 0, 132), FVector(0.28f, 0.42f, 0.42f), R::Primary);
		Part(Root, TEXT("Abs"), EArenaShape::Cube, FVector(14, 0, 117), FVector(0.04f, 0.26f, 0.16f), R::Accent);
		Part(Root, TEXT("ChestPlateL"), EArenaShape::Cube, FVector(14, -11, 143), FVector(0.03f, 0.16f, 0.14f), R::Primary, 1.3f);
		Part(Root, TEXT("ChestPlateR"), EArenaShape::Cube, FVector(14, 11, 143), FVector(0.03f, 0.16f, 0.14f), R::Primary, 1.3f);
		Rig.ArcReactor = Part(Root, TEXT("ArcReactor"), EArenaShape::Cylinder, FVector(16, 0, 140), FVector(0.12f, 0.12f, 0.03f),
			R::Emissive, 1.f, 25.f, FRotator(-90.f, 0.f, 0.f));
		// Triangle reactor: a cone seen from the front is a triangle; point down like the Mark VI.
		Rig.TriReactor = Part(Root, TEXT("TriReactor"), EArenaShape::Cone, FVector(15.5f, 0, 141), FVector(0.17f, 0.03f, 0.13f),
			R::Emissive, 1.f, 25.f, FRotator(180.f, 90.f, 0.f));
		Part(Root, TEXT("Neck"), EArenaShape::Cylinder, FVector(0, 0, 157), FVector(0.1f, 0.1f, 0.06f), R::Accent);
		// Small inverted triangular collar light (Sketchfab Mark bust); glow.reactor via "Tri" in the name.
		Part(Root, TEXT("CollarTri"), EArenaShape::Cone, FVector(12.f, 0.f, 154.f), FVector(0.05f, 0.04f, 0.035f),
			R::Emissive, 1.f, 28.f, FRotator(180.f, 90.f, 0.f));

		// --- Helmets (index = ESuitHelmet) ---
		auto Eyes = [&Part](USceneComponent* P, const TCHAR* L, const TCHAR* Rn, float X, float Z, float W)
		{
			Part(P, L, EArenaShape::Cube, FVector(X, -4.5f, Z), FVector(0.01f, W, 0.015f), R::Emissive, 1.f, 20.f);
			Part(P, Rn, EArenaShape::Cube, FVector(X, 4.5f, Z), FVector(0.01f, W, 0.015f), R::Emissive, 1.f, 20.f);
		};
		Rig.Helmets.SetNum(5);
		{
			USceneComponent* H = MakeRoot(Owner, Root, TEXT("HelmetClassic"));
			Part(H, TEXT("HClassicShell"), EArenaShape::Cube, FVector(0, 0, 172), FVector(0.22f, 0.2f, 0.24f), R::Primary);
			Part(H, TEXT("HClassicFace"), EArenaShape::Cube, FVector(11, 0, 169), FVector(0.03f, 0.16f, 0.19f), R::Secondary);
			Part(H, TEXT("HClassicEarL"), EArenaShape::Cylinder, FVector(0, -10.5f, 172), FVector(0.07f, 0.07f, 0.02f), R::Secondary, 1.f, 0.f, FRotator(0, 0, 90));
			Part(H, TEXT("HClassicEarR"), EArenaShape::Cylinder, FVector(0, 10.5f, 172), FVector(0.07f, 0.07f, 0.02f), R::Secondary, 1.f, 0.f, FRotator(0, 0, 90));
			Eyes(H, TEXT("HClassicEyeL"), TEXT("HClassicEyeR"), 13.f, 174.f, 0.05f);
			Rig.Helmets[(int32)ESuitHelmet::Classic] = H;
		}
		{
			USceneComponent* H = MakeRoot(Owner, Root, TEXT("HelmetSleek"));
			Part(H, TEXT("HSleekShell"), EArenaShape::Sphere, FVector(0, 0, 173), FVector(0.25f, 0.22f, 0.27f), R::Primary);
			Part(H, TEXT("HSleekFace"), EArenaShape::Sphere, FVector(5, 0, 170), FVector(0.17f, 0.17f, 0.2f), R::Secondary);
			Eyes(H, TEXT("HSleekEyeL"), TEXT("HSleekEyeR"), 13.2f, 174.f, 0.05f);
			Rig.Helmets[(int32)ESuitHelmet::Sleek] = H;
		}
		{
			USceneComponent* H = MakeRoot(Owner, Root, TEXT("HelmetAngular"));
			Part(H, TEXT("HAngShell"), EArenaShape::Cube, FVector(0, 0, 172), FVector(0.22f, 0.18f, 0.24f), R::Primary);
			Part(H, TEXT("HAngFace"), EArenaShape::Cube, FVector(11, 0, 168), FVector(0.03f, 0.15f, 0.2f), R::Secondary);
			Part(H, TEXT("HAngFinL"), EArenaShape::Cube, FVector(-3, -6, 186), FVector(0.2f, 0.02f, 0.07f), R::Primary, 1.2f, 0.f, FRotator(18, 0, -12));
			Part(H, TEXT("HAngFinR"), EArenaShape::Cube, FVector(-3, 6, 186), FVector(0.2f, 0.02f, 0.07f), R::Primary, 1.2f, 0.f, FRotator(18, 0, 12));
			Part(H, TEXT("HAngChin"), EArenaShape::Cone, FVector(10, 0, 156), FVector(0.07f, 0.1f, 0.08f), R::Secondary, 1.f, 0.f, FRotator(180, 0, 0));
			Part(H, TEXT("HAngCheekL"), EArenaShape::Cone, FVector(9, -9, 162), FVector(0.05f, 0.05f, 0.09f), R::Primary, 0.8f, 0.f, FRotator(160, 0, 25));
			Part(H, TEXT("HAngCheekR"), EArenaShape::Cone, FVector(9, 9, 162), FVector(0.05f, 0.05f, 0.09f), R::Primary, 0.8f, 0.f, FRotator(160, 0, -25));
			// Narrow, slanted "angry" eyes.
			Part(H, TEXT("HAngEyeL"), EArenaShape::Cube, FVector(13, -4.5f, 175), FVector(0.01f, 0.055f, 0.012f), R::Emissive, 1.f, 25.f, FRotator(0, 0, -14));
			Part(H, TEXT("HAngEyeR"), EArenaShape::Cube, FVector(13, 4.5f, 175), FVector(0.01f, 0.055f, 0.012f), R::Emissive, 1.f, 25.f, FRotator(0, 0, 14));
			Rig.Helmets[(int32)ESuitHelmet::Angular] = H;
		}
		{
			USceneComponent* H = MakeRoot(Owner, Root, TEXT("HelmetBulky"));
			Part(H, TEXT("HBulkShell"), EArenaShape::Cube, FVector(0, 0, 173), FVector(0.27f, 0.25f, 0.27f), R::Primary);
			Part(H, TEXT("HBulkFace"), EArenaShape::Cube, FVector(13.5f, 0, 170), FVector(0.03f, 0.2f, 0.2f), R::Secondary);
			Part(H, TEXT("HBulkJaw"), EArenaShape::Cube, FVector(9, 0, 160), FVector(0.14f, 0.23f, 0.06f), R::Primary, 0.8f);
			Eyes(H, TEXT("HBulkEyeL"), TEXT("HBulkEyeR"), 15.5f, 175.f, 0.06f);
			Rig.Helmets[(int32)ESuitHelmet::Bulky] = H;
		}
		{
			USceneComponent* H = MakeRoot(Owner, Root, TEXT("HelmetCrude"));
			Part(H, TEXT("HCrudeBucket"), EArenaShape::Cylinder, FVector(0, 0, 172), FVector(0.24f, 0.24f, 0.27f), R::Primary);
			Part(H, TEXT("HCrudeBand"), EArenaShape::Cylinder, FVector(0, 0, 181), FVector(0.255f, 0.255f, 0.025f), R::Secondary);
			Part(H, TEXT("HCrudeMouth"), EArenaShape::Cube, FVector(12, 0, 164), FVector(0.01f, 0.08f, 0.006f), R::Secondary, 0.5f);
			Eyes(H, TEXT("HCrudeEyeL"), TEXT("HCrudeEyeR"), 12.2f, 175.f, 0.035f);
			Rig.Helmets[(int32)ESuitHelmet::Crude] = H;
		}

		// --- Arms: pivots at the shoulders, arm hangs along -Z. Pitching the pivot +90 points it forward.
		Rig.ShoulderL = MakeRoot(Owner, Root, TEXT("ShoulderL"));
		Rig.ShoulderL->SetRelativeLocation(FVector(0, -18, 150)) /* U armor_kit: upperarm_l y=-18 */;
		Rig.ShoulderR = MakeRoot(Owner, Root, TEXT("ShoulderR"));
		Rig.ShoulderR->SetRelativeLocation(FVector(0, 18, 150));
		for (int32 Side = 0; Side < 2; ++Side)
		{
			USceneComponent* S = Side == 0 ? Rig.ShoulderL.Get() : Rig.ShoulderR.Get();
			const bool L = Side == 0;
			Part(S, L ? TEXT("ShoulderCapL") : TEXT("ShoulderCapR"), EArenaShape::Sphere, FVector(0, 0, 0), FVector(0.2f), R::Primary);
			Part(S, L ? TEXT("UpperArmL") : TEXT("UpperArmR"), EArenaShape::Cube, FVector(0, 0, -18), FVector(0.13f, 0.13f, 0.34f), R::Primary);
			Part(S, L ? TEXT("ForearmL") : TEXT("ForearmR"), EArenaShape::Cube, FVector(0, 0, -50), FVector(0.12f, 0.12f, 0.3f), R::Secondary);
			Part(S, L ? TEXT("HandL") : TEXT("HandR"), EArenaShape::Cube, FVector(0, 0, -70), FVector(0.11f, 0.1f, 0.11f), R::Primary);
			UStaticMeshComponent* Palm = Part(S, L ? TEXT("PalmL") : TEXT("PalmR"), EArenaShape::Sphere, FVector(0, 0, -77), FVector(0.07f), R::Emissive, 1.f, 15.f);
			UStaticMeshComponent* Flame = Part(S, L ? TEXT("FlameHandL") : TEXT("FlameHandR"), EArenaShape::Cone, FVector(0, 0, -95),
				FVector(0.1f, 0.1f, 0.3f), R::Fixed, 1.f, 20.f, FRotator(180.f, 0.f, 0.f));
			(L ? Rig.PalmL : Rig.PalmR) = Palm;
			(L ? Rig.FlameHandL : Rig.FlameHandR) = Flame;
		}

		// --- Forearm-mounted reactor (left forearm, outer side): ring housing + core + wiring ---
		{
			USceneComponent* F = MakeRoot(Owner, Rig.ShoulderL, TEXT("ForearmReactor"));
			Rig.ForearmReactor = F;
			FixedPart(Owner, OutParts, F, TEXT("FRBand1"), EArenaShape::Cylinder, FVector(0, 0, -42), FVector(0.135f, 0.135f, 0.03f), BareSteel);
			FixedPart(Owner, OutParts, F, TEXT("FRBand2"), EArenaShape::Cylinder, FVector(0, 0, -58), FVector(0.135f, 0.135f, 0.03f), BareSteel);
			Part(F, TEXT("FRHousing"), EArenaShape::Cylinder, FVector(0, -7.5f, -50), FVector(0.1f, 0.1f, 0.035f), R::Secondary, 0.6f, 0.f, FRotator(0.f, 0.f, 90.f));
			Rig.ForearmCorePart = OutParts.Num();
			Part(F, TEXT("FRCore"), EArenaShape::Sphere, FVector(0, -9.5f, -50), FVector(0.06f, 0.035f, 0.06f), R::Emissive, 1.f, 25.f);
			FixedPart(Owner, OutParts, F, TEXT("FRWire1"), EArenaShape::Cylinder, FVector(2, -7, -36), FVector(0.01f, 0.01f, 0.16f), Copper);
			FixedPart(Owner, OutParts, F, TEXT("FRWire2"), EArenaShape::Cylinder, FVector(-2, -7, -36), FVector(0.01f, 0.01f, 0.16f), WireRed);
			F->SetVisibility(false, true);
		}

		// --- Legs ---
		Rig.HipL = MakeRoot(Owner, Root, TEXT("HipL"));
		Rig.HipL->SetRelativeLocation(FVector(0, -9.5, 95)) /* U armor_kit: thigh_l y=-9.5 */;
		Rig.HipR = MakeRoot(Owner, Root, TEXT("HipR"));
		Rig.HipR->SetRelativeLocation(FVector(0, 9.5, 95));
		for (int32 Side = 0; Side < 2; ++Side)
		{
			USceneComponent* H = Side == 0 ? Rig.HipL.Get() : Rig.HipR.Get();
			const bool L = Side == 0;
			Part(H, L ? TEXT("ThighL") : TEXT("ThighR"), EArenaShape::Cube, FVector(0, 0, -24), FVector(0.19f, 0.17f, 0.45f), R::Primary);
			Part(H, L ? TEXT("KneeL") : TEXT("KneeR"), EArenaShape::Sphere, FVector(3, 0, -48), FVector(0.14f), R::Accent);
			Part(H, L ? TEXT("ShinL") : TEXT("ShinR"), EArenaShape::Cube, FVector(0, 0, -70), FVector(0.17f, 0.15f, 0.4f), R::Primary);
			Part(H, L ? TEXT("BootL") : TEXT("BootR"), EArenaShape::Cube, FVector(4, 0, -90), FVector(0.28f, 0.15f, 0.1f), R::Primary, 0.8f);
			UStaticMeshComponent* Flame = Part(H, L ? TEXT("FlameBootL") : TEXT("FlameBootR"), EArenaShape::Cone, FVector(0, 0, -115),
				FVector(0.14f, 0.14f, 0.45f), R::Fixed, 1.f, 20.f, FRotator(180.f, 0.f, 0.f));
			(L ? Rig.FlameBootL : Rig.FlameBootR) = Flame;
		}

		// --- Shoulder attachments (index = ESuitShoulders; None has no parts) ---
		Rig.ShoulderSets.SetNum(4);
		{
			USceneComponent* S = MakeRoot(Owner, Root, TEXT("ShoulderPads"));
			Part(S, TEXT("PadL"), EArenaShape::Cube, FVector(0, -31, 161), FVector(0.26f, 0.2f, 0.07f), R::Secondary, 1.f, 0.f, FRotator(0, 0, -15));
			Part(S, TEXT("PadR"), EArenaShape::Cube, FVector(0, 31, 161), FVector(0.26f, 0.2f, 0.07f), R::Secondary, 1.f, 0.f, FRotator(0, 0, 15));
			Rig.ShoulderSets[(int32)ESuitShoulders::Pads] = S;
		}
		{
			USceneComponent* S = MakeRoot(Owner, Root, TEXT("ShoulderCannon"));
			Part(S, TEXT("CannonMount"), EArenaShape::Cube, FVector(-4, 22, 169), FVector(0.16f, 0.12f, 0.1f), R::Primary, 0.7f);
			Part(S, TEXT("CannonBarrel"), EArenaShape::Cylinder, FVector(10, 22, 177), FVector(0.08f, 0.08f, 0.42f), R::Secondary, 0.6f, 0.f, FRotator(-90, 0, 0));
			Part(S, TEXT("CannonMuzzle"), EArenaShape::Cylinder, FVector(31, 22, 177), FVector(0.05f, 0.05f, 0.02f), R::Emissive, 1.f, 10.f, FRotator(-90, 0, 0));
			Rig.ShoulderSets[(int32)ESuitShoulders::Cannon] = S;
		}
		{
			USceneComponent* S = MakeRoot(Owner, Root, TEXT("ShoulderSpikes"));
			Part(S, TEXT("SpikeL1"), EArenaShape::Cone, FVector(0, -32, 165), FVector(0.1f, 0.1f, 0.24f), R::Primary, 1.15f, 0.f, FRotator(0, 0, -40));
			Part(S, TEXT("SpikeR1"), EArenaShape::Cone, FVector(0, 32, 165), FVector(0.1f, 0.1f, 0.24f), R::Primary, 1.15f, 0.f, FRotator(0, 0, 40));
			Part(S, TEXT("SpikeL2"), EArenaShape::Cone, FVector(-7, -26, 166), FVector(0.07f, 0.07f, 0.17f), R::Secondary, 1.f, 0.f, FRotator(-15, 0, -20));
			Part(S, TEXT("SpikeR2"), EArenaShape::Cone, FVector(-7, 26, 166), FVector(0.07f, 0.07f, 0.17f), R::Secondary, 1.f, 0.f, FRotator(-15, 0, 20));
			Rig.ShoulderSets[(int32)ESuitShoulders::Spiked] = S;
		}

		// --- Backpacks (index = ESuitBackpack) ---
		Rig.Backpacks.SetNum(4);
		{
			USceneComponent* B = MakeRoot(Owner, Root, TEXT("BackJets"));
			Part(B, TEXT("JetL"), EArenaShape::Cylinder, FVector(-18, -8, 135), FVector(0.09f, 0.09f, 0.32f), R::Secondary, 0.8f);
			Part(B, TEXT("JetR"), EArenaShape::Cylinder, FVector(-18, 8, 135), FVector(0.09f, 0.09f, 0.32f), R::Secondary, 0.8f);
			Part(B, TEXT("JetGlowL"), EArenaShape::Cylinder, FVector(-18, -8, 118), FVector(0.07f, 0.07f, 0.02f), R::Emissive, 1.f, 15.f);
			Part(B, TEXT("JetGlowR"), EArenaShape::Cylinder, FVector(-18, 8, 118), FVector(0.07f, 0.07f, 0.02f), R::Emissive, 1.f, 15.f);
			Rig.Backpacks[(int32)ESuitBackpack::Jets] = B;
		}
		{
			USceneComponent* B = MakeRoot(Owner, Root, TEXT("BackMissiles"));
			Part(B, TEXT("PodBox"), EArenaShape::Cube, FVector(-20, 0, 142), FVector(0.12f, 0.34f, 0.3f), R::Primary, 0.7f);
			Part(B, TEXT("PodTipL"), EArenaShape::Cone, FVector(-20, -10, 161), FVector(0.07f, 0.07f, 0.09f), R::Secondary);
			Part(B, TEXT("PodTipM"), EArenaShape::Cone, FVector(-20, 0, 161), FVector(0.07f, 0.07f, 0.09f), R::Secondary);
			Part(B, TEXT("PodTipR"), EArenaShape::Cone, FVector(-20, 10, 161), FVector(0.07f, 0.07f, 0.09f), R::Secondary);
			Rig.Backpacks[(int32)ESuitBackpack::Missiles] = B;
		}
		{
			USceneComponent* B = MakeRoot(Owner, Root, TEXT("BackMinigun"));
			Part(B, TEXT("GunMount"), EArenaShape::Cube, FVector(-15, -20, 166), FVector(0.14f, 0.12f, 0.12f), R::Primary, 0.6f);
			Part(B, TEXT("GunBarrel"), EArenaShape::Cylinder, FVector(6, -24, 178), FVector(0.08f, 0.08f, 0.5f), R::Secondary, 0.5f, 0.f, FRotator(-90, 0, 0));
			Part(B, TEXT("GunRing"), EArenaShape::Cylinder, FVector(24, -24, 178), FVector(0.1f, 0.1f, 0.04f), R::Accent, 1.f, 0.f, FRotator(-90, 0, 0));
			Rig.Backpacks[(int32)ESuitBackpack::Minigun] = B;
		}

		// --- Makeshift mounted arc reactor (spine housing, exposed wiring, glowing core) ---
		{
			USceneComponent* M = MakeRoot(Owner, Root, TEXT("MountedReactor"));
			Rig.MountedReactor = M;
			FixedPart(Owner, OutParts, M, TEXT("MRFrame"), EArenaShape::Cube, FVector(-19, 0, 135), FVector(0.06f, 0.22f, 0.36f), BareSteel);
			Part(M, TEXT("MRHousing"), EArenaShape::Cylinder, FVector(-25, 0, 140), FVector(0.2f, 0.2f, 0.09f), R::Secondary, 0.6f, 0.f, FRotator(90.f, 0.f, 0.f));
			Rig.MountedCorePart = OutParts.Num();
			Part(M, TEXT("MRCore"), EArenaShape::Sphere, FVector(-30, 0, 140), FVector(0.11f, 0.11f, 0.11f), R::Emissive, 1.f, 30.f);
			// Cage bars over the core.
			FixedPart(Owner, OutParts, M, TEXT("MRCageV"), EArenaShape::Cube, FVector(-31, 0, 140), FVector(0.01f, 0.015f, 0.2f), BareSteel);
			FixedPart(Owner, OutParts, M, TEXT("MRCageH"), EArenaShape::Cube, FVector(-31, 0, 140), FVector(0.01f, 0.2f, 0.015f), BareSteel);
			// Exposed wiring looping to the shoulders and hips (taped-together look).
			FixedPart(Owner, OutParts, M, TEXT("MRWireL1"), EArenaShape::Cylinder, FVector(-22, -11, 155), FVector(0.015f, 0.015f, 0.22f), WireRed, 0.f, FRotator(0.f, 0.f, -35.f));
			FixedPart(Owner, OutParts, M, TEXT("MRWireR1"), EArenaShape::Cylinder, FVector(-22, 11, 155), FVector(0.015f, 0.015f, 0.22f), WireBlack, 0.f, FRotator(0.f, 0.f, 35.f));
			FixedPart(Owner, OutParts, M, TEXT("MRWireL2"), EArenaShape::Cylinder, FVector(-21, -9, 118), FVector(0.015f, 0.015f, 0.24f), WireBlack, 0.f, FRotator(0.f, 0.f, 25.f));
			FixedPart(Owner, OutParts, M, TEXT("MRWireR2"), EArenaShape::Cylinder, FVector(-21, 9, 118), FVector(0.015f, 0.015f, 0.24f), Copper, 0.f, FRotator(0.f, 0.f, -25.f));
			FixedPart(Owner, OutParts, M, TEXT("MRCoil"), EArenaShape::Cylinder, FVector(-24, 0, 124), FVector(0.07f, 0.07f, 0.05f), Copper);
			FixedPart(Owner, OutParts, M, TEXT("MRStrapTop"), EArenaShape::Cube, FVector(-2, 0, 155), FVector(0.34f, 0.3f, 0.025f), WireBlack);
			FixedPart(Owner, OutParts, M, TEXT("MRStrapLow"), EArenaShape::Cube, FVector(-2, 0, 122), FVector(0.34f, 0.44f, 0.025f), WireBlack);
		}

		// --- Wings ---
		Rig.Wings = MakeRoot(Owner, Root, TEXT("Wings"));
		Part(Rig.Wings, TEXT("WingL"), EArenaShape::Cube, FVector(-19, -32, 150), FVector(0.05f, 0.55f, 0.14f), R::Primary, 1.f, 0.f, FRotator(0, 15, -22));
		Part(Rig.Wings, TEXT("WingR"), EArenaShape::Cube, FVector(-19, 32, 150), FVector(0.05f, 0.55f, 0.14f), R::Primary, 1.f, 0.f, FRotator(0, -15, 22));
		Part(Rig.Wings, TEXT("WingTrimL"), EArenaShape::Cube, FVector(-21, -34, 143), FVector(0.03f, 0.5f, 0.03f), R::Emissive, 1.f, 8.f, FRotator(0, 15, -22));
		Part(Rig.Wings, TEXT("WingTrimR"), EArenaShape::Cube, FVector(-21, 34, 143), FVector(0.03f, 0.5f, 0.03f), R::Emissive, 1.f, 8.f, FRotator(0, -15, 22));

		// --- Hulkbuster extra armour (hidden until Q) ---
		Rig.HulkbusterPlates = MakeRoot(Owner, Root, TEXT("HulkbusterPlates"));
		Part(Rig.HulkbusterPlates, TEXT("BusterChest"), EArenaShape::Cube, FVector(4, 0, 135), FVector(0.36f, 0.56f, 0.4f), R::Primary, 0.9f);
		Part(Rig.HulkbusterPlates, TEXT("BusterPadL"), EArenaShape::Cube, FVector(0, -36, 158), FVector(0.3f, 0.26f, 0.18f), R::Secondary);
		Part(Rig.HulkbusterPlates, TEXT("BusterPadR"), EArenaShape::Cube, FVector(0, 36, 158), FVector(0.3f, 0.26f, 0.18f), R::Secondary);
		Part(Rig.HulkbusterPlates, TEXT("BusterReactor"), EArenaShape::Cylinder, FVector(23, 0, 140), FVector(0.16f, 0.16f, 0.03f), R::Emissive, 1.f, 25.f, FRotator(-90.f, 0.f, 0.f));
	}

	void BuildGauntlets(AActor* Owner, USceneComponent* Camera, FSuitRig& Rig, TArray<FArenaPaintedPart>& OutParts)
	{
		check(Owner && Camera);
		FPartMaker Part{ Owner, OutParts };
		// Camera space: X forward, Y right, Z up. Forearms come in from the lower corners.
		Rig.FPRoot = MakeRoot(Owner, Camera, TEXT("FPGauntlets"));
		Rig.FPArmL = MakeRoot(Owner, Rig.FPRoot, TEXT("FPArmL"));
		Rig.FPArmL->SetRelativeLocation(FVector(8, -24, -30));
		Rig.FPArmR = MakeRoot(Owner, Rig.FPRoot, TEXT("FPArmR"));
		Rig.FPArmR->SetRelativeLocation(FVector(8, 24, -30));

		for (int32 Side = 0; Side < 2; ++Side)
		{
			const bool L = Side == 0;
			USceneComponent* A = L ? Rig.FPArmL.Get() : Rig.FPArmR.Get();
			const float S = L ? -1.f : 1.f;
			Part(A, L ? TEXT("FPForearmL") : TEXT("FPForearmR"), EArenaShape::Cube, FVector(18, 0, 0), FVector(0.34f, 0.1f, 0.1f), R::Secondary);
			Part(A, L ? TEXT("FPCuffL") : TEXT("FPCuffR"), EArenaShape::Cube, FVector(31, 0, 0), FVector(0.06f, 0.115f, 0.115f), R::Primary, 1.2f);
			Part(A, L ? TEXT("FPHandL") : TEXT("FPHandR"), EArenaShape::Cube, FVector(40, 0, 0.5f), FVector(0.12f, 0.1f, 0.075f), R::Primary);
			Part(A, L ? TEXT("FPFingersL") : TEXT("FPFingersR"), EArenaShape::Cube, FVector(48.5f, 0, 2.5f), FVector(0.06f, 0.095f, 0.04f), R::Primary, 0.85f, 0.f, FRotator(-25, 0, 0));
			Part(A, L ? TEXT("FPThumbL") : TEXT("FPThumbR"), EArenaShape::Cube, FVector(42, -S * 5.5f, 1), FVector(0.06f, 0.025f, 0.03f), R::Accent, 1.f, 0.f, FRotator(0, S * 30, 0));
			Part(A, L ? TEXT("FPArmGlowL") : TEXT("FPArmGlowR"), EArenaShape::Cube, FVector(18, 0, 5.1f), FVector(0.2f, 0.015f, 0.005f), R::Emissive, 1.f, 6.f);
			// Palm repulsor: faces forward-up out of the hand (glows when firing).
			Part(A, L ? TEXT("FPPalmL") : TEXT("FPPalmR"), EArenaShape::Sphere, FVector(44, 0, 4.5f), FVector(0.045f, 0.045f, 0.02f), R::Emissive, 1.f, 4.f);
			(L ? Rig.FPPalmPartL : Rig.FPPalmPartR) = OutParts.Num() - 1;
		}

		// Mounted-reactor readout strapped on top of the left forearm (core + wires), first person.
		{
			USceneComponent* M = MakeRoot(Owner, Rig.FPArmL, TEXT("FPMountedReactor"));
			Rig.FPMountedReactor = M;
			FixedPart(Owner, OutParts, M, TEXT("FPMRBase"), EArenaShape::Cube, FVector(16, 0, 6.5f), FVector(0.12f, 0.08f, 0.025f), BareSteel);
			Part(M, TEXT("FPMRHousing"), EArenaShape::Cylinder, FVector(16, 0, 8.5f), FVector(0.07f, 0.07f, 0.025f), R::Secondary, 0.6f);
			Rig.FPMountedCorePart = OutParts.Num();
			Part(M, TEXT("FPMRCore"), EArenaShape::Sphere, FVector(16, 0, 10), FVector(0.045f, 0.045f, 0.03f), R::Emissive, 1.f, 25.f);
			FixedPart(Owner, OutParts, M, TEXT("FPMRWire1"), EArenaShape::Cylinder, FVector(8, -2, 7), FVector(0.008f, 0.008f, 0.14f), WireRed, 0.f, FRotator(-90.f, 0.f, 0.f));
			FixedPart(Owner, OutParts, M, TEXT("FPMRWire2"), EArenaShape::Cylinder, FVector(8, 2.5f, 7), FVector(0.008f, 0.008f, 0.14f), Copper, 0.f, FRotator(-88.f, 6.f, 0.f));
			FixedPart(Owner, OutParts, M, TEXT("FPMRTape"), EArenaShape::Cube, FVector(24, 0, 6), FVector(0.03f, 0.105f, 0.105f), WireBlack);
		}

		// Hulkbuster fists: big blocky gauntlets around the hands (hidden unless Hulkbuster).
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const bool L = Side == 0;
			USceneComponent* A = MakeRoot(Owner, L ? Rig.FPArmL.Get() : Rig.FPArmR.Get(), L ? TEXT("FPBusterL") : TEXT("FPBusterR"));
			Part(A, L ? TEXT("FPBusterFistL") : TEXT("FPBusterFistR"), EArenaShape::Cube, FVector(42, 0, 0), FVector(0.22f, 0.2f, 0.17f), R::Primary, 0.9f);
			Part(A, L ? TEXT("FPBusterKnuckleL") : TEXT("FPBusterKnuckleR"), EArenaShape::Cube, FVector(53, 0, 1), FVector(0.03f, 0.21f, 0.12f), R::Secondary);
			Part(A, L ? TEXT("FPBusterGuardL") : TEXT("FPBusterGuardR"), EArenaShape::Cube, FVector(22, 0, 0), FVector(0.3f, 0.2f, 0.18f), R::Secondary, 0.9f);
			(L ? Rig.FPBusterL : Rig.FPBusterR) = A;
		}
	}

	static bool GWhiteTrim = false;

	void SetWhiteTrim(bool bEnable) { GWhiteTrim = bEnable; }
	bool IsWhiteTrim() { return GWhiteTrim; }

	static bool GPaleBlueTrim = false;
	void SetPaleBlueTrim(bool bEnable) { GPaleBlueTrim = bEnable; }
	bool IsPaleBlueTrim() { return GPaleBlueTrim; }

	static void ApplyColorsBase(const FSuitDef& Suit, TArray<FArenaPaintedPart>& Parts);

	void ApplyColors(const FSuitDef& Suit, TArray<FArenaPaintedPart>& Parts)
	{
		ApplyColorsBase(Suit, Parts);
		if (!GWhiteTrim && !GPaleBlueTrim) { return; }
		// Cosmetic reward "vison-white-trim" (side mission Synthezoid Signal): accent trim in
		// Vison's pearl white - a satin ceramic-like finish, not chrome.
		// "jocasta-pale-blue-trim" (The Second Signal) uses the same satin finish in pale blue.
		const FLinearColor Pearl = GPaleBlueTrim ? FLinearColor(0.50f, 0.70f, 0.86f) : FLinearColor(0.80f, 0.80f, 0.77f);
		for (FArenaPaintedPart& P : Parts)
		{
			if (P.Role != ESuitPaintRole::Accent) { continue; }
			P.Color = Pearl * FMath::Clamp(P.Shade, 0.6f, 1.1f);
			P.Color.A = 1.f;
			if (P.MID)
			{
				P.MID->SetScalarParameterValue(TEXT("Metallic"), 0.f); // pearl / pale-blue trim is a dielectric (Vison row 3)
				P.MID->SetScalarParameterValue(TEXT("Roughness"), 0.32f);
				P.MID->SetScalarParameterValue(TEXT("ClearCoat"), 0.8f);
				P.MID->SetScalarParameterValue(TEXT("FlakeAmount"), 0.f);
			}
			RepaintPart(P);
		}
	}

	static void ApplyColorsBase(const FSuitDef& Suit, TArray<FArenaPaintedPart>& Parts)
	{
		if (const FSuitMaterialDef* Def = GetSuitMaterials().Find(Suit.Id))
		{
			// Data-driven finish (U's suit_materials.json). Dirt has no material input: it darkens
			// the base colour by up to 30% and adds up to +0.25 roughness. Panel line depth (cm)
			// x 4 drives the procedural panel-line normal strength.
			const float DirtDarken = 1.f - 0.3f * FMath::Clamp(Def->Dirt, 0.f, 1.f);
			const float DirtRough = 0.25f * FMath::Clamp(Def->Dirt, 0.f, 1.f);
			for (FArenaPaintedPart& P : Parts)
			{
				int32 Layer = INDEX_NONE;
				switch (P.Role)
				{
				case ESuitPaintRole::Primary:   Layer = 0; break;
				case ESuitPaintRole::Secondary: Layer = 1; break;
				case ESuitPaintRole::Accent:    Layer = 2; break;
				case ESuitPaintRole::Emissive:
				{
					const EGlowKind Kind = ClassifyGlow(P);
					const FSuitGlowMat* G = Kind == EGlowKind::Eyes ? &Def->Eyes
						: Kind == EGlowKind::Repulsor ? &Def->Repulsors
						: &Def->Reactor; // reactor cores and other suit lights (jets, trims) follow the reactor
					if (!G->bValid && Kind == EGlowKind::Other && Def->Eyes.bValid) { G = &Def->Eyes; }
					if (G->bValid)
					{
						P.Color = G->Color;
						// Repulsor strength is driven by firing; eyes / reactor take the table intensity.
						if (Kind == EGlowKind::Eyes || Kind == EGlowKind::Reactor) { P.Emissive = G->Intensity; }
						if (P.MID) { P.MID->SetScalarParameterValue(TEXT("CoreWhite"), G->CoreWhite); }
					}
					else
					{
						// Null entry: the suit has no such light (Mark I palms, Arachnid core) - keep it dark.
						P.Color = FLinearColor(0.02f, 0.02f, 0.02f);
						P.Emissive = 0.f;
					}
					P.Color.A = 1.f;
					RepaintPart(P);
					continue;
				}
				default: continue;
				}
				const FSuitLayerMat& M = Def->Layers[Layer].bValid ? Def->Layers[Layer] : Def->Layers[0];
				P.Color = M.Color * P.Shade * DirtDarken;
				P.Color.A = 1.f;
				if (P.MID)
				{
					P.MID->SetScalarParameterValue(TEXT("Metallic"), M.Metallic);
					P.MID->SetScalarParameterValue(TEXT("Roughness"), FMath::Min(1.f, M.Roughness + DirtRough));
					P.MID->SetScalarParameterValue(TEXT("ClearCoat"), M.ClearCoat);
					P.MID->SetScalarParameterValue(TEXT("ClearCoatRoughness"), M.ClearCoatRoughness);
					P.MID->SetScalarParameterValue(TEXT("FlakeAmount"), M.Flake);
					P.MID->SetScalarParameterValue(TEXT("FlakeScale"), M.FlakeScale);
					P.MID->SetScalarParameterValue(TEXT("Specular"), M.Specular);       // M_SuitMaster only (ignored elsewhere)
					P.MID->SetScalarParameterValue(TEXT("Anisotropy"), M.Anisotropy);
					P.MID->SetScalarParameterValue(TEXT("AnisotropyAngle"), M.AnisotropyAngle);
					P.MID->SetScalarParameterValue(TEXT("EdgeWear"), Def->EdgeWear);
					P.MID->SetScalarParameterValue(TEXT("Scorch"), Def->Scorch);
					P.MID->SetScalarParameterValue(TEXT("Dirt"), Def->Dirt);
					// PanelNormalStrength: M_CarPaint has no such parameter (U fix #1); panel lines are geometry now.
					P.MID->SetVectorParameterValue(TEXT("WearColor"), Def->WearColor);
				}
				RepaintPart(P);
			}
			return;
		}

		// Paint finish for M_CarPaint (ignored by the flat fallback material). Saturated suits
		// get hot-rod clear coat with flakes; desaturated ones (Mark I iron, stealth, Ultron
		// chrome) read as bare metal. The Mark I is welded scrap: rough, worn, scorched.
		const FLinearColor HSV = Suit.PrimaryColor.LinearRGBToHSV();
		const bool bMarkI = Suit.Id == FName(TEXT("mark-i"));
		const bool bBareMetal = bMarkI || HSV.G < 0.25f;
		const float Coat     = bMarkI ? 0.f : (bBareMetal ? 0.35f : 1.f);
		const float Rough    = bMarkI ? 0.62f : (bBareMetal ? 0.28f : 0.3f);
		const float Flakes   = bBareMetal ? 0.05f : 0.4f;
		const float Wear     = bMarkI ? 0.5f : 0.1f;
		const float Scorch   = bMarkI ? 0.3f : 0.f;
		for (FArenaPaintedPart& P : Parts)
		{
			if (P.MID && P.Role != ESuitPaintRole::Fixed && P.Role != ESuitPaintRole::Emissive)
			{
				const bool bGoldish = P.Role != ESuitPaintRole::Primary && !bBareMetal;
				P.MID->SetScalarParameterValue(TEXT("ClearCoat"), Coat);
				P.MID->SetScalarParameterValue(TEXT("Roughness"), bGoldish ? 0.22f : Rough);
				P.MID->SetScalarParameterValue(TEXT("Metallic"), bGoldish || bBareMetal ? 1.f : 0.7f);
				P.MID->SetScalarParameterValue(TEXT("FlakeAmount"), Flakes);
				P.MID->SetScalarParameterValue(TEXT("EdgeWear"), Wear);
				P.MID->SetScalarParameterValue(TEXT("Scorch"), Scorch);
			}
		}
		for (FArenaPaintedPart& P : Parts)
		{
			switch (P.Role)
			{
			case ESuitPaintRole::Primary:   P.Color = Suit.PrimaryColor * P.Shade; break;
			case ESuitPaintRole::Secondary: P.Color = Suit.SecondaryColor * P.Shade; break;
			case ESuitPaintRole::Accent:    P.Color = Suit.AccentColor * P.Shade; break;
			case ESuitPaintRole::Emissive:  P.Color = Suit.EmissiveColor; break;
			default: continue;
			}
			P.Color.A = 1.f;
			RepaintPart(P);
		}
	}

	void ApplyVisibility(const FSuitDef& Suit, FSuitRig& Rig, bool bVisible, bool bHulkbuster)
	{
		if (!Rig.Root) { return; }
		Rig.Root->SetVisibility(bVisible, true);
		if (!bVisible) { return; }

		for (int32 i = 0; i < Rig.Helmets.Num(); ++i)
		{
			if (Rig.Helmets[i]) { Rig.Helmets[i]->SetVisibility(i == (int32)Suit.Helmet, true); }
		}
		for (int32 i = 0; i < Rig.ShoulderSets.Num(); ++i)
		{
			if (Rig.ShoulderSets[i]) { Rig.ShoulderSets[i]->SetVisibility(i == (int32)Suit.Shoulders, true); }
		}
		for (int32 i = 0; i < Rig.Backpacks.Num(); ++i)
		{
			if (Rig.Backpacks[i]) { Rig.Backpacks[i]->SetVisibility(i == (int32)Suit.Backpack, true); }
		}
		if (Rig.Wings) { Rig.Wings->SetVisibility(Suit.bWings, true); }
		if (Rig.ArcReactor) { Rig.ArcReactor->SetVisibility(Suit.Reactor == ESuitReactor::Round, false); }
		if (Rig.TriReactor) { Rig.TriReactor->SetVisibility(Suit.Reactor == ESuitReactor::Triangle, false); }
		if (Rig.MountedReactor) { Rig.MountedReactor->SetVisibility(Suit.Reactor == ESuitReactor::Mounted, true); }
		if (Rig.ForearmReactor) { Rig.ForearmReactor->SetVisibility(false, true); }
		if (Rig.HulkbusterPlates) { Rig.HulkbusterPlates->SetVisibility(bHulkbuster, true); }
	}

	void ApplyMountedReactor(FSuitRig& Rig, bool bShow, bool bForearm)
	{
		const bool bBody = Rig.Root && Rig.Root->IsVisible();
		if (Rig.MountedReactor) { Rig.MountedReactor->SetVisibility(bBody && bShow && !bForearm, true); }
		if (Rig.ForearmReactor) { Rig.ForearmReactor->SetVisibility(bBody && bShow && bForearm, true); }
	}

	float PlatingScale(int32 Plating)
	{
		return Plating <= 0 ? 0.92f : (Plating >= 2 ? 1.1f : 1.f);
	}
}
