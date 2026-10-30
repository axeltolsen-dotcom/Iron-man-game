#include "SuitArmorKit.h"

#include "ArenaVisuals.h"
#include "SuitRig.h"
#include "SuitTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CompGeom/Delaunay2.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

DEFINE_LOG_CATEGORY_STATIC(LogArmorKit, Log, All);

namespace
{
	using UE::Geometry::FIndex2i;
	using UE::Geometry::FIndex3i;

	// ------------------------------------------------------------------ JSON helpers
	TSharedPtr<FJsonObject> ObjField(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		return (O.IsValid() && O->TryGetObjectField(Key, P) && P) ? *P : nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* ArrField(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return (O.IsValid() && O->TryGetArrayField(Key, A)) ? A : nullptr;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default)
	{
		double V = Default;
		if (O.IsValid() && O->HasTypedField<EJson::Number>(Key)) { O->TryGetNumberField(Key, V); }
		return V;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Default = FString())
	{
		FString V;
		return (O.IsValid() && O->TryGetStringField(Key, V)) ? V : Default;
	}

	FVector Vec3(const TSharedPtr<FJsonValue>& V, const FVector& Default = FVector::ZeroVector)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || !A || A->Num() < 3) { return Default; }
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	}

	TArray<FVector2d> Poly2(const TSharedPtr<FJsonValue>& V)
	{
		TArray<FVector2d> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || !A) { return Out; }
		for (const TSharedPtr<FJsonValue>& P : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
			if (P.IsValid() && P->TryGetArray(XY) && XY && XY->Num() >= 2) { Out.Add(FVector2d((*XY)[0]->AsNumber(), (*XY)[1]->AsNumber())); }
		}
		return Out;
	}

	TSharedPtr<FJsonObject> Clone(const TSharedPtr<FJsonObject>& Src)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		if (!Src.IsValid()) { return Out; }
		for (const auto& Pair : Src->Values)
		{
			const FString Key(*Pair.Key);
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Object && Pair.Value->TryGetObject(Child) && Child) { Out->SetObjectField(Key, Clone(*Child)); }
			else { Out->SetField(Key, Pair.Value); }
		}
		return Out;
	}

	// ------------------------------------------------------------------ resolved plate
	struct FPanelLine
	{
		TArray<FVector2d> Pts;  // extrude polyline (u,v)
		double RingZ = 0.0;     // loft ring
		bool bRing = false;
		double Width = 0.18;
		double DepthScale = 1.0;
	};

	struct FPlate
	{
		FString Id;
		FString Name;
		FString Segment;
		FString Bone;
		FString FrameBone;
		FString FallbackBone;
		FString SlotName;
		FString Side;
		bool bMirror = true;
		bool bMirrored = false;
		FVector Loc = FVector::ZeroVector;
		FRotator Rot = FRotator::ZeroRotator;
		TSharedPtr<FJsonObject> Geo;
		double Thickness = 0.8;
		double ThicknessScale = 1.0;
		double Scale = 1.0;
		double RadiusScale = 1.0;
		double RadialScale = 1.0;
		double WrapScale = 1.0;
		double Shade = 1.0;
		double GlowIntensity = -1.0;
		int32 Layer = 1;
		TArray<FPanelLine> Lines;
	};

	struct FStyle
	{
		double ThicknessScale = 1.0;
		double WidthScale = 1.0;
		double FixedWidth = -1.0;
		bool bMajorOnly = false;
		TArray<FString> Remove;
	};

	struct FKitFile
	{
		bool bLoaded = false;
		TSharedPtr<FJsonObject> Root;
		TArray<TSharedPtr<FJsonObject>> BasePlates;
		TMap<FString, FVector> Joints;      // nominal pose, left side + centre
		TMap<FString, FString> LimbChild;   // left side
	};

	const FKitFile& Kit()
	{
		static FKitFile File;
		if (File.bLoaded) { return File; }
		File.bLoaded = true;
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/armor_kit.json");
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), File.Root) || !File.Root.IsValid())
		{
			UE_LOG(LogArmorKit, Warning, TEXT("armor_kit.json not found or invalid (%s); suits keep the placeholder shapes."), *Path);
			File.Root.Reset();
			return File;
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Plates = ArrField(File.Root, TEXT("plates")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Plates)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (V.IsValid() && V->TryGetObject(O) && O) { File.BasePlates.Add(*O); }
			}
		}
		const TSharedPtr<FJsonObject> Skel = ObjField(File.Root, TEXT("skeleton"));
		if (const TSharedPtr<FJsonObject> Joints = ObjField(ObjField(Skel, TEXT("nominalPoseCm")), TEXT("joints")))
		{
			for (const auto& Pair : Joints->Values)
			{
				const FString Key(*Pair.Key);
				File.Joints.Add(Key, Vec3(Pair.Value));
			}
		}
		if (const TSharedPtr<FJsonObject> Child = ObjField(Skel, TEXT("limbChild")))
		{
			for (const auto& Pair : Child->Values)
			{
				const FString Key(*Pair.Key);
				FString Target;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(Target)) { File.LimbChild.Add(Key, Target); }
			}
		}
		UE_LOG(LogArmorKit, Log, TEXT("Loaded armour kit: %d base plates, %d nominal joints."), File.BasePlates.Num(), File.Joints.Num());
		return File;
	}

	FPlate ParsePlate(const TSharedPtr<FJsonObject>& O)
	{
		FPlate P;
		P.Id = Str(O, TEXT("id"));
		P.Name = Str(O, TEXT("name"), P.Id);
		P.Segment = Str(O, TEXT("segment"));
		P.Bone = Str(O, TEXT("bone"));
		P.FrameBone = Str(O, TEXT("frameBone"));
		P.FallbackBone = Str(O, TEXT("fallbackBone"));
		P.SlotName = Str(O, TEXT("slot"), TEXT("primary"));
		P.Side = Str(O, TEXT("side"), TEXT("C"));
		bool bMirror = true;
		if (O->TryGetBoolField(TEXT("mirror"), bMirror)) { P.bMirror = bMirror; }
		if (const TSharedPtr<FJsonObject> Off = ObjField(O, TEXT("offset")))
		{
			const TSharedPtr<FJsonValue> L = Off->TryGetField(TEXT("location"));
			P.Loc = Vec3(L);
			if (const TSharedPtr<FJsonObject> R = ObjField(Off, TEXT("rotation")))
			{
				P.Rot = FRotator(Num(R, TEXT("pitch"), 0), Num(R, TEXT("yaw"), 0), Num(R, TEXT("roll"), 0));
			}
		}
		P.Geo = Clone(ObjField(O, TEXT("geometry")));
		P.Thickness = Num(O, TEXT("thickness"), 0.8);
		P.Layer = int32(Num(O, TEXT("layer"), 1));
		P.Shade = Num(O, TEXT("shade"), 1.0);
		if (const TSharedPtr<FJsonObject> Glow = ObjField(O, TEXT("glow"))) { P.GlowIntensity = Num(Glow, TEXT("intensity"), -1.0); }
		if (const TArray<TSharedPtr<FJsonValue>>* Lines = ArrField(O, TEXT("panelLines")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Lines)
			{
				const TSharedPtr<FJsonObject>* LO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(LO) || !LO) { continue; }
				FPanelLine Line;
				Line.Width = Num(*LO, TEXT("width"), 0.18);
				Line.DepthScale = Num(*LO, TEXT("depthScale"), 1.0);
				if ((*LO)->HasField(TEXT("pts"))) { Line.Pts = Poly2((*LO)->TryGetField(TEXT("pts"))); }
				else if ((*LO)->HasTypedField<EJson::Number>(TEXT("ring"))) { Line.bRing = true; Line.RingZ = Num(*LO, TEXT("ring"), 0); }
				else { continue; } // seam / plane / ringR...: not cut (see header)
				P.Lines.Add(Line);
			}
		}
		return P;
	}

	/** One "modify[].set" block. Multipliers accumulate; geometry keys replace the geometry field. */
	void ApplySet(FPlate& P, const TSharedPtr<FJsonObject>& Set)
	{
		for (const auto& Pair : Set->Values)
		{
			const FString Key(*Pair.Key);
			if (!Pair.Value.IsValid()) { continue; }
			double D = 0.0;
			const bool bNum = Pair.Value->TryGetNumber(D);
			if (Key == TEXT("thicknessScale") && bNum) { P.ThicknessScale *= D; }
			else if (Key == TEXT("scale") && bNum) { P.Scale *= D; }
			else if (Key == TEXT("radiusScale") && bNum) { P.RadiusScale *= D; }
			else if (Key == TEXT("radialScale") && bNum) { P.RadialScale *= D; }
			else if (Key == TEXT("wrapScale") && bNum) { P.WrapScale *= D; }
			else if (Key == TEXT("thickness") && bNum) { P.Thickness = D; }
			else if (Key == TEXT("layer") && bNum) { P.Layer = int32(D); }
			else if (Key == TEXT("slot")) { Pair.Value->TryGetString(P.SlotName); }
			else if (Key == TEXT("offsetAdd")) { P.Loc += Vec3(Pair.Value); }
			else if (Key == TEXT("profileOverride")) { P.Geo->SetField(TEXT("profile"), Pair.Value); }
			else if (Key == TEXT("radii") || Key == TEXT("exponents") || Key == TEXT("circleCuts") || Key == TEXT("polyCuts")
				|| Key == TEXT("profile") || Key == TEXT("stations") || Key == TEXT("radialSegments") || Key == TEXT("size"))
			{
				P.Geo->SetField(Key, Pair.Value);
			}
			// bevelRadius / bevelSegments / facetize / cornerRadius / notes: no geometric bevel (see header).
		}
	}

	FString MirrorName(const FString& In)
	{
		FString Out = In;
		if (Out.EndsWith(TEXT("_l"))) { Out = Out.LeftChop(2) + TEXT("_r"); }
		else if (Out.EndsWith(TEXT("L"))) { Out = Out.LeftChop(1) + TEXT("R"); }
		Out.ReplaceInline(TEXT("_l."), TEXT("_r."), ESearchCase::CaseSensitive);
		return Out;
	}

	// ------------------------------------------------------------------ suit -> variant keys / style
	struct FSuitKeys
	{
		FString Helmet, Reactor, Shoulders, Backpack, Wings, Plating, Style;
		double PanelLineDepth = 0.12;
	};

	FSuitKeys KeysFor(const FSuitDef& Suit)
	{
		FSuitKeys K;
		static const TCHAR* const Helmets[] = { TEXT("classic"), TEXT("sleek"), TEXT("angular"), TEXT("bulky"), TEXT("crude") };
		static const TCHAR* const Reactors[] = { TEXT("round"), TEXT("triangle"), TEXT("none"), TEXT("mounted") };
		static const TCHAR* const Shoulders[] = { TEXT("none"), TEXT("pads"), TEXT("cannon"), TEXT("spiked") };
		static const TCHAR* const Backpacks[] = { TEXT("none"), TEXT("jets"), TEXT("missiles"), TEXT("minigun") };
		K.Helmet = Helmets[FMath::Clamp((int32)Suit.Helmet, 0, 4)];
		K.Reactor = Reactors[FMath::Clamp((int32)Suit.Reactor, 0, 3)];
		K.Shoulders = Shoulders[FMath::Clamp((int32)Suit.Shoulders, 0, 3)];
		K.Backpack = Backpacks[FMath::Clamp((int32)Suit.Backpack, 0, 3)];
		K.Wings = Suit.bWings ? TEXT("true") : TEXT("false");
		K.Plating = FString::FromInt(FMath::Clamp(Suit.Plating, 0, 2));
		// U's table carries panelLineDepth and the style (same data as suits.json / suit_materials.json).
		const TSharedPtr<FJsonObject> Row = ObjField(ObjField(Kit().Root, TEXT("suitVariantTable")), *Suit.Id.ToString());
		K.PanelLineDepth = Num(Row, TEXT("panelLineDepth"), 0.12);
		K.Style = Str(Row, TEXT("style"));
		const FString OverrideStyle = Str(ObjField(ObjField(Kit().Root, TEXT("suitOverrides")), *Suit.Id.ToString()), TEXT("style"));
		if (!OverrideStyle.IsEmpty()) { K.Style = OverrideStyle; }
		if (K.Style.IsEmpty())
		{
			K.Style = K.Helmet == TEXT("crude") ? TEXT("crude")
				: K.PanelLineDepth <= 0.06 ? TEXT("nano")
				: K.PanelLineDepth >= 0.2 ? TEXT("industrial")
				: (K.Helmet == TEXT("bulky") && K.Plating == TEXT("2")) ? TEXT("heavy") : TEXT("standard");
		}
		return K;
	}

	FStyle StyleFor(const FString& Name)
	{
		FStyle S;
		const TSharedPtr<FJsonObject> O = ObjField(ObjField(Kit().Root, TEXT("styles")), *Name);
		if (!O.IsValid()) { return S; }
		S.ThicknessScale = Num(O, TEXT("thicknessScale"), 1.0);
		S.WidthScale = Num(O, TEXT("panelLineWidthScale"), 1.0);
		S.FixedWidth = Num(O, TEXT("panelLineWidth"), -1.0);
		S.bMajorOnly = Str(O, TEXT("keepPanelLines")) == TEXT("major");
		O->TryGetStringArrayField(TEXT("remove"), S.Remove);
		return S;
	}

	void RemoveMatching(TArray<FPlate>& Plates, const TArray<FString>& Patterns)
	{
		for (const FString& Pat : Patterns)
		{
			Plates.RemoveAll([&Pat](const FPlate& P) { return P.Id.MatchesWildcard(Pat, ESearchCase::CaseSensitive); });
		}
	}

	/** add / remove / modify block (variant, or suit override). */
	void ApplyEdit(TArray<FPlate>& Plates, const TSharedPtr<FJsonObject>& Edit)
	{
		if (!Edit.IsValid()) { return; }
		TArray<FString> Remove;
		Edit->TryGetStringArrayField(TEXT("remove"), Remove);
		RemoveMatching(Plates, Remove);
		if (const TArray<TSharedPtr<FJsonValue>>* Add = ArrField(Edit, TEXT("add")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Add)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (V.IsValid() && V->TryGetObject(O) && O) { Plates.Add(ParsePlate(*O)); }
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Mods = ArrField(Edit, TEXT("modify")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Mods)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O) || !O) { continue; }
				const FString Pat = Str(*O, TEXT("match"));
				const TSharedPtr<FJsonObject> Set = ObjField(*O, TEXT("set"));
				if (Pat.IsEmpty() || !Set.IsValid()) { continue; }
				for (FPlate& P : Plates)
				{
					if (P.Id.MatchesWildcard(Pat, ESearchCase::CaseSensitive)) { ApplySet(P, Set); }
				}
			}
		}
	}

	TArray<FPlate> Resolve(const FSuitDef& Suit, FSuitKeys& OutKeys, FStyle& OutStyle)
	{
		TArray<FPlate> Plates;
		const FKitFile& File = Kit();
		if (!File.Root.IsValid()) { return Plates; }
		for (const TSharedPtr<FJsonObject>& O : File.BasePlates) { Plates.Add(ParsePlate(O)); }
		OutKeys = KeysFor(Suit);
		const TSharedPtr<FJsonObject> Variants = ObjField(File.Root, TEXT("variants"));
		const TPair<const TCHAR*, FString> Groups[] = {
			{ TEXT("helmet"), OutKeys.Helmet }, { TEXT("reactor"), OutKeys.Reactor }, { TEXT("shoulders"), OutKeys.Shoulders },
			{ TEXT("backpack"), OutKeys.Backpack }, { TEXT("wings"), OutKeys.Wings }, { TEXT("plating"), OutKeys.Plating } };
		for (const TPair<const TCHAR*, FString>& G : Groups)
		{
			ApplyEdit(Plates, ObjField(ObjField(Variants, G.Key), *G.Value));
		}
		OutStyle = StyleFor(OutKeys.Style);
		RemoveMatching(Plates, OutStyle.Remove);
		for (FPlate& P : Plates) { P.ThicknessScale *= OutStyle.ThicknessScale; }
		ApplyEdit(Plates, ObjField(ObjField(File.Root, TEXT("suitOverrides")), *Suit.Id.ToString()));

		// Mirror L -> R.
		const int32 NumAuthored = Plates.Num();
		for (int32 i = 0; i < NumAuthored; ++i)
		{
			if (Plates[i].Side != TEXT("L") || !Plates[i].bMirror) { continue; }
			FPlate R = Plates[i];
			R.Id = MirrorName(R.Id);
			R.Name = MirrorName(R.Name);
			R.Bone = MirrorName(R.Bone);
			R.FrameBone = MirrorName(R.FrameBone);
			R.FallbackBone = MirrorName(R.FallbackBone);
			R.Side = TEXT("R");
			R.bMirrored = true;
			Plates.Add(R);
		}
		return Plates;
	}

	// ------------------------------------------------------------------ mesh building
	struct FKitMesh
	{
		TArray<FVector3f> Pos;
		TArray<FVector3f> Nrm;
		TArray<FVector2f> UV;
		TArray<int32> Idx;

		int32 Add(const FVector& P, const FVector& N)
		{
			Pos.Add(FVector3f(P));
			Nrm.Add(FVector3f(N.GetSafeNormal()));
			UV.Add(FVector2f(float(P.Y / 20.0), float(-P.Z / 20.0)));
			return Pos.Num() - 1;
		}

		/** Adds the triangle so that its front face points along Desired (engine normal = (P2-P0) x (P1-P0)). */
		void Tri(int32 A, int32 B, int32 C, const FVector& Desired)
		{
			const FVector PA(Pos[A]), PB(Pos[B]), PC(Pos[C]);
			const FVector Std = FVector::CrossProduct(PB - PA, PC - PA);
			if (Std.SizeSquared() < 1e-12) { return; }
			if (FVector::DotProduct(Std, Desired) > 0.0) { Idx.Append({ A, C, B }); }
			else { Idx.Append({ A, B, C }); }
		}

		/** Triangle facing away from Ref (a point inside the solid). */
		void TriOut(int32 A, int32 B, int32 C, const FVector& Ref)
		{
			const FVector Centroid = FVector(Pos[A] + Pos[B] + Pos[C]) / 3.0;
			Tri(A, B, C, Centroid - Ref);
		}

		/** Recomputes vertex normals as area-weighted face normals (smooth shading within shared vertices). */
		void SmoothNormals()
		{
			TArray<FVector> Acc;
			Acc.SetNumZeroed(Pos.Num());
			for (int32 i = 0; i + 2 < Idx.Num(); i += 3)
			{
				const FVector P0(Pos[Idx[i]]), P1(Pos[Idx[i + 1]]), P2(Pos[Idx[i + 2]]);
				const FVector N = FVector::CrossProduct(P2 - P0, P1 - P0);
				Acc[Idx[i]] += N; Acc[Idx[i + 1]] += N; Acc[Idx[i + 2]] += N;
			}
			for (int32 v = 0; v < Pos.Num(); ++v)
			{
				if (!Acc[v].IsNearlyZero()) { Nrm[v] = FVector3f(Acc[v].GetSafeNormal()); }
			}
		}

		void Append(const FKitMesh& O, const FTransform& X)
		{
			const int32 Base = Pos.Num();
			for (int32 v = 0; v < O.Pos.Num(); ++v)
			{
				Pos.Add(FVector3f(X.TransformPosition(FVector(O.Pos[v]))));
				Nrm.Add(FVector3f(X.TransformVectorNoScale(FVector(O.Nrm[v]))));
				UV.Add(O.UV[v]);
			}
			for (int32 i : O.Idx) { Idx.Add(Base + i); }
		}
	};

	double SgnPow(double V, double P) { return FMath::Sign(V) * FMath::Pow(FMath::Abs(V), P); }

	bool InPoly(const TArray<FVector2d>& Poly, const FVector2d& P)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
		{
			if (((Poly[i].Y > P.Y) != (Poly[j].Y > P.Y))
				&& (P.X < (Poly[j].X - Poly[i].X) * (P.Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y + 1e-12) + Poly[i].X))
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	double DistToSegment(const FVector2d& P, const FVector2d& A, const FVector2d& B)
	{
		const FVector2d AB = B - A;
		const double T = FMath::Clamp(FVector2d::DotProduct(P - A, AB) / FMath::Max(AB.SquaredLength(), 1e-12), 0.0, 1.0);
		return FVector2d::Distance(P, A + AB * T);
	}

	double SignedArea(const TArray<FVector2d>& Poly)
	{
		double A = 0.0;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++) { A += Poly[j].X * Poly[i].Y - Poly[i].X * Poly[j].Y; }
		return 0.5 * A;
	}

	/** Profile (u,v) + inward depth -> plate-local point on the (optionally) cylinder-wrapped surface. */
	struct FWrap
	{
		double Ru = 0.0; // 0 = flat
		double Rv = 0.0;

		FVector Surface(double U, double V) const
		{
			FVector P(0.0, U, V);
			if (Ru > 1e-3) { const double A = U / Ru; P.Y = Ru * FMath::Sin(A); P.X += Ru * FMath::Cos(A) - Ru; }
			if (Rv > 1e-3) { const double B = V / Rv; P.Z = Rv * FMath::Sin(B); P.X += Rv * FMath::Cos(B) - Rv; }
			return P;
		}

		FVector Normal(double U, double V) const
		{
			const double H = 0.01;
			const FVector DU = Surface(U + H, V) - Surface(U - H, V);
			const FVector DV = Surface(U, V + H) - Surface(U, V - H);
			FVector N = FVector::CrossProduct(DU, DV).GetSafeNormal();
			if (N.X < 0.0) { N = -N; }
			return N.IsNearlyZero() ? FVector::ForwardVector : N;
		}
	};

	/** extrudeShell: CDT of the profile (holes as constraints, Steiner grid for the wrap, groove lines). */
	bool BuildExtrude(const FPlate& P, double Thick, double Depth, double WidthScale, double FixedWidth, bool bMajorOnly, FKitMesh& M)
	{
		TArray<FVector2d> Outer = Poly2(P.Geo->TryGetField(TEXT("profile")));
		if (Outer.Num() < 3) { return false; }
		for (FVector2d& V : Outer) { V *= P.Scale; }
		if (SignedArea(Outer) < 0.0) { Algo::Reverse(Outer); }
		TArray<TArray<FVector2d>> Holes;
		if (const TArray<TSharedPtr<FJsonValue>>* H = ArrField(P.Geo, TEXT("holes")))
		{
			for (const TSharedPtr<FJsonValue>& V : *H)
			{
				TArray<FVector2d> Hole = Poly2(V);
				for (FVector2d& X : Hole) { X *= P.Scale; }
				if (Hole.Num() >= 3) { if (SignedArea(Hole) > 0.0) { Algo::Reverse(Hole); } Holes.Add(Hole); }
			}
		}
		// Cuts that may cross the outline (reactor circle in the pecs): triangles inside are dropped.
		TArray<TArray<FVector2d>> Cuts;
		if (const TArray<TSharedPtr<FJsonValue>>* C = ArrField(P.Geo, TEXT("circleCuts")))
		{
			for (const TSharedPtr<FJsonValue>& V : *C)
			{
				const TSharedPtr<FJsonObject>* CO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(CO) || !CO) { continue; }
				const TArray<FVector2d> Ctr = Poly2(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ (*CO)->TryGetField(TEXT("center")) }));
				const double R = Num(*CO, TEXT("radius"), 0.0) * P.Scale;
				if (Ctr.Num() != 1 || R <= 0.0) { continue; }
				TArray<FVector2d> Circle;
				for (int32 i = 0; i < 32; ++i) { const double A = 2.0 * PI * i / 32.0; Circle.Add(Ctr[0] * P.Scale + FVector2d(FMath::Cos(A), FMath::Sin(A)) * R); }
				Cuts.Add(Circle);
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* C = ArrField(P.Geo, TEXT("polyCuts")))
		{
			for (const TSharedPtr<FJsonValue>& V : *C)
			{
				TArray<FVector2d> Cut = Poly2(V);
				for (FVector2d& X : Cut) { X *= P.Scale; }
				if (Cut.Num() >= 3) { Cuts.Add(Cut); }
			}
		}

		TArray<FVector2d> Verts;
		TArray<FIndex2i> Boundary;
		TArray<FIndex2i> HoleEdges;
		auto AddLoop = [&Verts](const TArray<FVector2d>& Loop, TArray<FIndex2i>& Edges)
		{
			const int32 Base = Verts.Num();
			Verts.Append(Loop);
			for (int32 i = 0; i < Loop.Num(); ++i) { Edges.Add(FIndex2i(Base + i, Base + (i + 1) % Loop.Num())); }
		};
		AddLoop(Outer, Boundary);
		for (const TArray<FVector2d>& Hole : Holes) { AddLoop(Hole, HoleEdges); }
		const int32 NumLoopVerts = Verts.Num();

		auto Inside = [&](const FVector2d& Q, double Margin)
		{
			if (!InPoly(Outer, Q)) { return false; }
			for (const TArray<FVector2d>& Hole : Holes) { if (InPoly(Hole, Q)) { return false; } }
			auto NearLoop = [&Q, Margin](const TArray<FVector2d>& Loop)
			{
				for (int32 i = 0; i < Loop.Num(); ++i) { if (DistToSegment(Q, Loop[i], Loop[(i + 1) % Loop.Num()]) < Margin) { return true; } }
				return false;
			};
			if (NearLoop(Outer)) { return false; }
			for (const TArray<FVector2d>& Hole : Holes) { if (NearLoop(Hole)) { return false; } }
			return true;
		};

		// Grooves: centre line (displaced by Depth) flanked by two lines at +-Width/2, sampled densely.
		TArray<TArray<FVector2d>> Grooves;
		TArray<double> GrooveWidth, GrooveDepth;
		for (const FPanelLine& L : P.Lines)
		{
			if (L.Pts.Num() < 2 || (bMajorOnly && L.DepthScale < 0.8)) { continue; }
			const double D = Depth * L.DepthScale;
			if (D < 0.02) { continue; } // below minDepthCut: normal map territory
			const double W = FMath::Max(FixedWidth > 0.0 ? FixedWidth : L.Width * WidthScale, 1.5 * D);
			TArray<FVector2d> Pts = L.Pts;
			for (FVector2d& X : Pts) { X *= P.Scale; }
			Grooves.Add(Pts);
			GrooveWidth.Add(W);
			GrooveDepth.Add(D);
		}
		TSet<int32> GrooveCentre;
		TMap<int32, double> CentreDepth;
		const double Margin = 0.25;
		for (int32 g = 0; g < Grooves.Num(); ++g)
		{
			const TArray<FVector2d>& Pts = Grooves[g];
			const double Half = GrooveWidth[g] * 0.5;
			for (int32 s = 0; s + 1 < Pts.Num(); ++s)
			{
				const FVector2d A = Pts[s], B = Pts[s + 1];
				const FVector2d Dir = (B - A).GetSafeNormal();
				const FVector2d Perp(-Dir.Y, Dir.X);
				const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2d::Distance(A, B) / 0.35));
				for (int32 k = 0; k <= Steps; ++k)
				{
					const FVector2d C = A + (B - A) * (double(k) / Steps);
					if (!Inside(C, Margin)) { continue; }
					CentreDepth.Add(Verts.Num(), GrooveDepth[g]);
					GrooveCentre.Add(Verts.Add(C));
					for (const double Sgn : { -1.0, 1.0 })
					{
						const FVector2d Q = C + Perp * (Half * Sgn);
						if (Inside(Q, Margin * 0.5)) { Verts.Add(Q); }
					}
				}
			}
		}
		auto NearGroove = [&](const FVector2d& Q)
		{
			for (int32 g = 0; g < Grooves.Num(); ++g)
			{
				for (int32 s = 0; s + 1 < Grooves[g].Num(); ++s)
				{
					if (DistToSegment(Q, Grooves[g][s], Grooves[g][s + 1]) < GrooveWidth[g] + 0.3) { return true; }
				}
			}
			return false;
		};

		// Steiner grid so the cylinder wrap actually curves the plate.
		FBox2d Bounds(Outer);
		const FVector2d Ext = Bounds.GetSize();
		const double Spacing = FMath::Clamp(FMath::Min(Ext.X, Ext.Y) / 8.0, 0.6, 2.0);
		for (double U = Bounds.Min.X + Spacing * 0.5; U < Bounds.Max.X; U += Spacing)
		{
			for (double V = Bounds.Min.Y + Spacing * 0.5; V < Bounds.Max.Y; V += Spacing)
			{
				const FVector2d Q(U, V);
				if (Inside(Q, Spacing * 0.35) && !NearGroove(Q)) { Verts.Add(Q); }
			}
		}
		// Cut outlines as Steiner points too, so the dropped region has a clean-ish edge.
		for (const TArray<FVector2d>& Cut : Cuts)
		{
			for (const FVector2d& Q : Cut) { if (Inside(Q, 0.2)) { Verts.Add(Q); } }
		}

		UE::Geometry::FDelaunay2 Delaunay;
		Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = true;
		TArray<FIndex2i> AllEdges = Boundary;
		AllEdges.Append(HoleEdges);
		TArray<FIndex3i> Tris;
		auto Fill = [&]() -> bool
		{
			Tris.Reset();
			const bool bTri = Delaunay.Triangulate(Verts, AllEdges);
			if (bTri) { Delaunay.GetFilledTriangles(Tris, Boundary, HoleEdges); }
			if (Tris.Num() == 0)
			{
				// Winding fill (outer CCW, holes CW), tolerant of edges the CDT could not fully constrain.
				Delaunay.GetFilledTrianglesGeneralizedWinding(Tris, Verts, AllEdges, UE::Geometry::FDelaunay2::EFillMode::NonZeroWinding);
			}
			if (Tris.Num() == 0)
			{
				UE_LOG(LogArmorKit, Warning, TEXT("Plate %s: triangulation failed (%d verts, %d edges, triangulate=%d)."), *P.Id, Verts.Num(), AllEdges.Num(), bTri ? 1 : 0);
			}
			return Tris.Num() > 0;
		};
		if (!Fill())
		{
			// Retry without the extra points (pathological grooves).
			Verts.SetNum(NumLoopVerts);
			GrooveCentre.Reset();
			CentreDepth.Reset();
			if (!Fill()) { return false; }
		}
		Tris.RemoveAll([&](const FIndex3i& T)
		{
			if (!Verts.IsValidIndex(T.A) || !Verts.IsValidIndex(T.B) || !Verts.IsValidIndex(T.C)) { return true; }
			const FVector2d Ctr = (Verts[T.A] + Verts[T.B] + Verts[T.C]) / 3.0;
			for (const TArray<FVector2d>& Cut : Cuts) { if (InPoly(Cut, Ctr)) { return true; } }
			return false;
		});

		if (Tris.Num() == 0)
		{
			UE_LOG(LogArmorKit, Warning, TEXT("Plate %s: every triangle fell inside a cut."), *P.Id);
			return false;
		}
		FWrap W;
		W.Ru = Num(P.Geo, TEXT("wrapU"), 0.0) * P.WrapScale;
		W.Rv = Num(P.Geo, TEXT("wrapV"), 0.0) * P.WrapScale;
		// Outer and inner vertices per 2D vertex.
		TArray<int32> OuterIdx, InnerIdx;
		OuterIdx.SetNum(Verts.Num());
		InnerIdx.SetNum(Verts.Num());
		for (int32 v = 0; v < Verts.Num(); ++v)
		{
			const FVector N = W.Normal(Verts[v].X, Verts[v].Y);
			const FVector S = W.Surface(Verts[v].X, Verts[v].Y);
			const double* GD = CentreDepth.Find(v);
			OuterIdx[v] = M.Add(S - N * (GD ? *GD : 0.0), N);
			InnerIdx[v] = M.Add(S - N * Thick, -N);
		}
		for (const FIndex3i& T : Tris)
		{
			const FVector N = W.Normal((Verts[T.A].X + Verts[T.B].X + Verts[T.C].X) / 3.0, (Verts[T.A].Y + Verts[T.B].Y + Verts[T.C].Y) / 3.0);
			M.Tri(OuterIdx[T.A], OuterIdx[T.B], OuterIdx[T.C], N);
			M.Tri(InnerIdx[T.A], InnerIdx[T.B], InnerIdx[T.C], -N);
		}
		M.SmoothNormals();
		// Rim walls (separate vertices: crisp edge, normals across the wall).
		auto Rim = [&](const TArray<FIndex2i>& Edges)
		{
			for (const FIndex2i& E : Edges)
			{
				const FVector2d A = Verts[E.A], B = Verts[E.B];
				const FVector2d Dir = (B - A).GetSafeNormal();
				// CCW outer loop: outward = right-hand perpendicular; holes (CW) likewise point into the hole.
				FVector2d Out2(Dir.Y, -Dir.X);
				const FVector NA = W.Normal(A.X, A.Y), NB = W.Normal(B.X, B.Y);
				const FVector SA = W.Surface(A.X, A.Y), SB = W.Surface(B.X, B.Y);
				const FVector OutDir = (W.Surface(A.X + Out2.X * 0.05, A.Y + Out2.Y * 0.05) - SA).GetSafeNormal();
				const int32 I0 = M.Add(SA, OutDir), I1 = M.Add(SB, OutDir), I2 = M.Add(SB - NB * Thick, OutDir), I3 = M.Add(SA - NA * Thick, OutDir);
				M.Tri(I0, I1, I2, OutDir);
				M.Tri(I0, I2, I3, OutDir);
			}
		};
		Rim(Boundary);
		Rim(HoleEdges);
		return M.Idx.Num() > 0;
	}

	struct FStation { double Z, Rx, Ry, N; };

	FStation SampleStations(const TArray<FStation>& S, double Z)
	{
		if (Z <= S[0].Z) { return S[0]; }
		for (int32 i = 0; i + 1 < S.Num(); ++i)
		{
			if (Z <= S[i + 1].Z)
			{
				const double T = (Z - S[i].Z) / FMath::Max(S[i + 1].Z - S[i].Z, 1e-6);
				return { Z, FMath::Lerp(S[i].Rx, S[i + 1].Rx, T), FMath::Lerp(S[i].Ry, S[i + 1].Ry, T), FMath::Lerp(S[i].N, S[i + 1].N, T) };
			}
		}
		return S.Last();
	}

	/** loftShell: superellipse sections along plate Z, arc in degrees from +X toward +Y; ring grooves. */
	bool BuildLoft(const FPlate& P, double Thick, double Depth, double WidthScale, double FixedWidth, bool bMajorOnly, FKitMesh& M)
	{
		TArray<FStation> St;
		if (const TArray<TSharedPtr<FJsonValue>>* A = ArrField(P.Geo, TEXT("stations")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (V.IsValid() && V->TryGetObject(O) && O)
				{
					St.Add({ Num(*O, TEXT("z"), 0) * P.Scale, Num(*O, TEXT("rx"), 5) * P.Scale * P.RadialScale,
						Num(*O, TEXT("ry"), 5) * P.Scale * P.RadialScale, FMath::Max(0.5, Num(*O, TEXT("n"), 2)) });
				}
			}
		}
		if (St.Num() < 2) { return false; }
		St.Sort([](const FStation& A, const FStation& B) { return A.Z < B.Z; });
		const TSharedPtr<FJsonObject> Arc = ObjField(P.Geo, TEXT("arc"));
		const double Start = Num(Arc, TEXT("start"), -180), End = Num(Arc, TEXT("end"), 180);
		const bool bClosed = FMath::Abs(End - Start) >= 359.9;
		const int32 Seg = FMath::Max(6, FMath::CeilToInt(48.0 * FMath::Abs(End - Start) / 360.0));

		// Z samples: stations, every ~2 cm, plus ring grooves (z - w/2, z, z + w/2).
		TArray<double> Zs;
		TMap<int32, double> GrooveAt; // index into Zs (after sort) is resolved below
		TArray<TPair<double, double>> Rings; // z, depth
		for (const FPanelLine& L : P.Lines)
		{
			if (!L.bRing || (bMajorOnly && L.DepthScale < 0.8)) { continue; }
			const double D = Depth * L.DepthScale;
			if (D < 0.02) { continue; }
			const double W = FMath::Max(FixedWidth > 0.0 ? FixedWidth : L.Width * WidthScale, 1.5 * D);
			const double Z = L.RingZ * P.Scale;
			if (Z <= St[0].Z + W || Z >= St.Last().Z - W) { continue; }
			Zs.Append({ Z - W * 0.5, Z + W * 0.5 });
			Rings.Add(TPair<double, double>(Z, D));
		}
		for (int32 i = 0; i + 1 < St.Num(); ++i)
		{
			const int32 Steps = FMath::Max(1, FMath::CeilToInt((St[i + 1].Z - St[i].Z) / 2.0));
			for (int32 k = 0; k < Steps; ++k) { Zs.Add(FMath::Lerp(St[i].Z, St[i + 1].Z, double(k) / Steps)); }
		}
		Zs.Add(St.Last().Z);
		for (const TPair<double, double>& R : Rings) { Zs.Add(R.Key); }
		Zs.Sort();
		for (int32 i = Zs.Num() - 1; i > 0; --i) { if (Zs[i] - Zs[i - 1] < 0.01) { Zs.RemoveAt(i); } }

		auto Point = [](const FStation& S, double ThetaDeg, double Inset) -> FVector
		{
			const double T = FMath::DegreesToRadians(ThetaDeg);
			const double E = 2.0 / S.N;
			return FVector(FMath::Max(0.1, S.Rx - Inset) * SgnPow(FMath::Cos(T), E), FMath::Max(0.1, S.Ry - Inset) * SgnPow(FMath::Sin(T), E), S.Z);
		};
		const int32 Cols = Seg + 1;
		TArray<int32> OuterGrid, InnerGrid;
		for (int32 r = 0; r < Zs.Num(); ++r)
		{
			const FStation S = SampleStations(St, Zs[r]);
			double Groove = 0.0;
			for (const TPair<double, double>& R : Rings) { if (FMath::Abs(R.Key - Zs[r]) < 0.005) { Groove = R.Value; } }
			for (int32 c = 0; c < Cols; ++c)
			{
				const double Th = FMath::Lerp(Start, End, double(c) / Seg);
				const FVector O = Point(S, Th, Groove);
				OuterGrid.Add(M.Add(O, FVector(O.X, O.Y, 0.0)));
				const FVector I = Point(S, Th, Thick);
				InnerGrid.Add(M.Add(I, -FVector(I.X, I.Y, 0.0)));
			}
		}
		auto AxisRef = [&M](int32 V) { return FVector(0.0, 0.0, M.Pos[V].Z); };
		for (int32 r = 0; r + 1 < Zs.Num(); ++r)
		{
			for (int32 c = 0; c < Seg; ++c)
			{
				const int32 A = r * Cols + c, B = A + 1, C = A + Cols, D = C + 1;
				const FVector Ref = AxisRef(OuterGrid[A]);
				M.TriOut(OuterGrid[A], OuterGrid[B], OuterGrid[D], Ref);
				M.TriOut(OuterGrid[A], OuterGrid[D], OuterGrid[C], Ref);
				const FVector InnerCtr = FVector(M.Pos[InnerGrid[A]] + M.Pos[InnerGrid[D]]) * 0.5;
				const FVector Far = InnerCtr * 3.0 - Ref * 2.0; // a point outside the inner surface
				M.TriOut(InnerGrid[A], InnerGrid[B], InnerGrid[D], Far);
				M.TriOut(InnerGrid[A], InnerGrid[D], InnerGrid[C], Far);
			}
		}
		M.SmoothNormals();
		// End rings (annuli) and, for open arcs, the two side walls.
		auto Quad = [&M](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& N)
		{
			const int32 I0 = M.Add(A, N), I1 = M.Add(B, N), I2 = M.Add(C, N), I3 = M.Add(D, N);
			M.Tri(I0, I1, I2, N);
			M.Tri(I0, I2, I3, N);
		};
		for (int32 End2 = 0; End2 < 2; ++End2)
		{
			const int32 r = End2 == 0 ? 0 : Zs.Num() - 1;
			const FVector N = End2 == 0 ? FVector(0, 0, -1) : FVector(0, 0, 1);
			for (int32 c = 0; c < Seg; ++c)
			{
				Quad(FVector(M.Pos[OuterGrid[r * Cols + c]]), FVector(M.Pos[OuterGrid[r * Cols + c + 1]]),
					FVector(M.Pos[InnerGrid[r * Cols + c + 1]]), FVector(M.Pos[InnerGrid[r * Cols + c]]), N);
			}
		}
		if (!bClosed)
		{
			for (int32 Side2 = 0; Side2 < 2; ++Side2)
			{
				const int32 c = Side2 == 0 ? 0 : Seg;
				const double Th = FMath::DegreesToRadians(Side2 == 0 ? Start : End);
				const FVector Tangent(-FMath::Sin(Th), FMath::Cos(Th), 0.0);
				const FVector N = Side2 == 0 ? -Tangent : Tangent;
				for (int32 r = 0; r + 1 < Zs.Num(); ++r)
				{
					Quad(FVector(M.Pos[OuterGrid[r * Cols + c]]), FVector(M.Pos[OuterGrid[(r + 1) * Cols + c]]),
						FVector(M.Pos[InnerGrid[(r + 1) * Cols + c]]), FVector(M.Pos[InnerGrid[r * Cols + c]]), N);
				}
			}
		}
		return M.Idx.Num() > 0;
	}

	FQuat AxisRotation(const FString& Axis)
	{
		// Local build axis is +X; rotate to the requested axis.
		if (Axis == TEXT("Y")) { return FRotator(0.f, 90.f, 0.f).Quaternion(); }
		if (Axis == TEXT("Z")) { return FRotator(90.f, 0.f, 0.f).Quaternion(); }
		return FQuat::Identity;
	}

	/** Profile [[r, h]] revolved about +X (h along X), closed at the axis. */
	void Revolve(TArray<FVector2d> Prof, int32 Segs, double SweepDeg, FKitMesh& M)
	{
		if (Prof.Num() < 2) { return; }
		if (Prof[0].X > 1e-3) { Prof.Insert(FVector2d(0.0, Prof[0].Y), 0); }
		if (Prof.Last().X > 1e-3) { Prof.Add(FVector2d(0.0, Prof.Last().Y)); }
		double HMin = Prof[0].Y, HMax = Prof[0].Y;
		for (const FVector2d& Q : Prof) { HMin = FMath::Min(HMin, Q.Y); HMax = FMath::Max(HMax, Q.Y); }
		const FVector Ref(0.5 * (HMin + HMax), 0.0, 0.0);
		const int32 Cols = Segs + 1;
		const int32 Base = M.Pos.Num();
		for (const FVector2d& Q : Prof)
		{
			for (int32 c = 0; c < Cols; ++c)
			{
				const double A = FMath::DegreesToRadians(SweepDeg * c / Segs);
				const FVector Pt(Q.Y, Q.X * FMath::Sin(A), Q.X * FMath::Cos(A));
				M.Add(Pt, Pt - Ref);
			}
		}
		for (int32 r = 0; r + 1 < Prof.Num(); ++r)
		{
			// Flat rings (equal h) need the axial direction, not the centroid, to orient.
			const bool bFlat = FMath::Abs(Prof[r].Y - Prof[r + 1].Y) < 1e-4;
			const FVector Axial(Prof[r].Y >= Ref.X ? 1.0 : -1.0, 0.0, 0.0);
			for (int32 c = 0; c < Segs; ++c)
			{
				const int32 A = Base + r * Cols + c, B = A + 1, C = A + Cols, D = C + 1;
				if (bFlat) { M.Tri(A, B, D, Axial); M.Tri(A, D, C, Axial); }
				else
				{
					const FVector Mid = FVector(M.Pos[A] + M.Pos[D]) * 0.5;
					const FVector Rad = FVector(0.0, Mid.Y, Mid.Z);
					M.Tri(A, B, D, Rad.IsNearlyZero() ? Mid - Ref : Rad);
					M.Tri(A, D, C, Rad.IsNearlyZero() ? Mid - Ref : Rad);
				}
			}
		}
	}

	bool BuildRevolve(const FPlate& P, FKitMesh& M)
	{
		TArray<FVector2d> Prof = Poly2(P.Geo->TryGetField(TEXT("profile")));
		for (FVector2d& Q : Prof) { Q.X *= P.Scale * P.RadiusScale; Q.Y *= P.Scale; }
		FKitMesh Local;
		Revolve(Prof, FMath::Clamp(int32(Num(P.Geo, TEXT("radialSegments"), 32)), 6, 64), Num(P.Geo, TEXT("sweep"), 360), Local);
		Local.SmoothNormals();
		M.Append(Local, FTransform(AxisRotation(Str(P.Geo, TEXT("axis"), TEXT("X")))));
		return M.Idx.Num() > 0;
	}

	bool BuildSuperellipsoid(const FPlate& P, FKitMesh& M)
	{
		const FVector Ctr = Vec3(P.Geo->TryGetField(TEXT("center"))) * P.Scale;
		const FVector Rad = Vec3(P.Geo->TryGetField(TEXT("radii")), FVector(10)) * P.Scale * P.RadiusScale;
		const FVector Ex = Vec3(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>()), FVector(2, 2, 0));
		double E1 = 2.0, E2 = 2.0;
		if (const TArray<TSharedPtr<FJsonValue>>* E = ArrField(P.Geo, TEXT("exponents")))
		{
			if (E->Num() >= 2) { E1 = FMath::Max(0.5, (*E)[0]->AsNumber()); E2 = FMath::Max(0.5, (*E)[1]->AsNumber()); }
		}
		(void)Ex;
		TArray<TPair<FVector, double>> Clips;
		if (const TArray<TSharedPtr<FJsonValue>>* C = ArrField(P.Geo, TEXT("clip")))
		{
			for (const TSharedPtr<FJsonValue>& V : *C)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (V.IsValid() && V->TryGetObject(O) && O)
				{
					Clips.Add(TPair<FVector, double>(Vec3((*O)->TryGetField(TEXT("n"))).GetSafeNormal(), Num(*O, TEXT("d"), 0) * P.Scale));
				}
			}
		}
		const int32 Lat = 24, Lon = 40;
		const int32 Base = M.Pos.Num();
		for (int32 i = 0; i <= Lat; ++i)
		{
			const double Eta = -HALF_PI + PI * i / Lat;
			for (int32 j = 0; j <= Lon; ++j)
			{
				const double Om = -PI + 2.0 * PI * j / Lon;
				FVector Q = Ctr + FVector(Rad.X * SgnPow(FMath::Cos(Eta), 2.0 / E1) * SgnPow(FMath::Cos(Om), 2.0 / E2),
					Rad.Y * SgnPow(FMath::Cos(Eta), 2.0 / E1) * SgnPow(FMath::Sin(Om), 2.0 / E2),
					Rad.Z * SgnPow(FMath::Sin(Eta), 2.0 / E1));
				for (const TPair<FVector, double>& Cl : Clips)
				{
					const double Over = FVector::DotProduct(Cl.Key, Q) - Cl.Value;
					if (Over > 0.0) { Q -= Cl.Key * Over; }
				}
				M.Add(Q, Q - Ctr);
			}
		}
		for (int32 i = 0; i < Lat; ++i)
		{
			for (int32 j = 0; j < Lon; ++j)
			{
				const int32 A = Base + i * (Lon + 1) + j, B = A + 1, C = A + Lon + 1, D = C + 1;
				M.TriOut(A, B, D, Ctr);
				M.TriOut(A, D, C, Ctr);
			}
		}
		M.SmoothNormals();
		return M.Idx.Num() > 0;
	}

	void BoxMesh(const FVector& Size, FKitMesh& M)
	{
		const FVector H = Size * 0.5;
		static const FVector Ns[6] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, 1), FVector(0, 0, -1) };
		for (const FVector& N : Ns)
		{
			const FVector U = FMath::Abs(N.Z) > 0.5 ? FVector(1, 0, 0) : FVector(0, 0, 1);
			const FVector V = FVector::CrossProduct(N, U);
			auto Corner = [&](double a, double b) { return (N + U * a + V * b) * H; };
			const int32 I0 = M.Add(Corner(-1, -1), N), I1 = M.Add(Corner(1, -1), N), I2 = M.Add(Corner(1, 1), N), I3 = M.Add(Corner(-1, 1), N);
			M.Tri(I0, I1, I2, N);
			M.Tri(I0, I2, I3, N);
		}
	}

	bool BuildPrimitive(const FPlate& P, const FString& Type, FKitMesh& M)
	{
		const double S = P.Scale;
		const int32 Segs = FMath::Clamp(int32(Num(P.Geo, TEXT("radialSegments"), 24)), 6, 64);
		const FQuat Axis = AxisRotation(Str(P.Geo, TEXT("axis"), TEXT("X")));
		FKitMesh Local;
		if (Type == TEXT("box"))
		{
			BoxMesh(Vec3(P.Geo->TryGetField(TEXT("size")), FVector(5)) * S, Local);
			M.Append(Local, FTransform::Identity);
			return true;
		}
		if (Type == TEXT("cylinder") || Type == TEXT("cone"))
		{
			const double R0 = (Type == TEXT("cone") ? Num(P.Geo, TEXT("radius0"), 2) : Num(P.Geo, TEXT("radius"), 2)) * S * P.RadiusScale;
			const double R1 = (Type == TEXT("cone") ? Num(P.Geo, TEXT("radius1"), 0.1) : R0 / (S * P.RadiusScale)) * S * P.RadiusScale;
			const double L = Num(P.Geo, TEXT("length"), 10) * S;
			Revolve({ FVector2d(R0, -L * 0.5), FVector2d(FMath::Max(R1, 0.01), L * 0.5) }, Segs, 360, Local);
			Local.SmoothNormals();
			M.Append(Local, FTransform(Axis));
			return true;
		}
		if (Type == TEXT("pyramid"))
		{
			const TArray<FVector2d> B = Poly2(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ P.Geo->TryGetField(TEXT("base")) }));
			const FVector2d Base2 = B.Num() == 1 ? B[0] * S : FVector2d(3, 3);
			const double Ht = Num(P.Geo, TEXT("height"), 5) * S;
			const FVector Apex(0, 0, Ht);
			const FVector C[4] = { FVector(-Base2.X / 2, -Base2.Y / 2, 0), FVector(Base2.X / 2, -Base2.Y / 2, 0), FVector(Base2.X / 2, Base2.Y / 2, 0), FVector(-Base2.X / 2, Base2.Y / 2, 0) };
			const FVector Ref(0, 0, Ht * 0.25);
			for (int32 i = 0; i < 4; ++i)
			{
				const int32 A = M.Add(C[i], C[i]), Bi = M.Add(C[(i + 1) % 4], C[(i + 1) % 4]), Ap = M.Add(Apex, FVector(0, 0, 1));
				M.TriOut(A, Bi, Ap, Ref);
			}
			const int32 Q0 = M.Add(C[0], FVector(0, 0, -1)), Q1 = M.Add(C[1], FVector(0, 0, -1)), Q2 = M.Add(C[2], FVector(0, 0, -1)), Q3 = M.Add(C[3], FVector(0, 0, -1));
			M.Tri(Q0, Q1, Q2, FVector(0, 0, -1));
			M.Tri(Q0, Q2, Q3, FVector(0, 0, -1));
			return true;
		}
		if (Type == TEXT("barrelCluster"))
		{
			const int32 Count = FMath::Clamp(int32(Num(P.Geo, TEXT("count"), 6)), 1, 16);
			const double Ring = Num(P.Geo, TEXT("ringRadius"), 2) * S;
			const double R = Num(P.Geo, TEXT("radius"), 0.6) * S;
			const double L = Num(P.Geo, TEXT("length"), 10) * S;
			for (int32 i = 0; i < Count; ++i)
			{
				FKitMesh Barrel;
				Revolve({ FVector2d(R, -L * 0.5), FVector2d(R, L * 0.5) }, 10, 360, Barrel);
				Barrel.SmoothNormals();
				const double A = 2.0 * PI * i / Count;
				Local.Append(Barrel, FTransform(FVector(0.0, Ring * FMath::Cos(A), Ring * FMath::Sin(A))));
			}
			M.Append(Local, FTransform(Axis));
			return true;
		}
		return false;
	}

	bool BuildPlateMesh(const FPlate& P, const FSuitKeys& Keys, const FStyle& Style, FKitMesh& M)
	{
		const FString Type = Str(P.Geo, TEXT("type"));
		const double Thick = FMath::Max(0.1, P.Thickness * P.ThicknessScale) * P.Scale;
		bool bOk = false;
		if (Type == TEXT("extrudeShell")) { bOk = BuildExtrude(P, Thick, Keys.PanelLineDepth, Style.WidthScale, Style.FixedWidth, Style.bMajorOnly, M); }
		else if (Type == TEXT("loftShell")) { bOk = BuildLoft(P, Thick, Keys.PanelLineDepth, Style.WidthScale, Style.FixedWidth, Style.bMajorOnly, M); }
		else if (Type == TEXT("revolve")) { bOk = BuildRevolve(P, M); }
		else if (Type == TEXT("superellipsoidShell")) { bOk = BuildSuperellipsoid(P, M); }
		else { bOk = BuildPrimitive(P, Type, M); }
		if (bOk && P.bMirrored)
		{
			// Reflect through the plate's local XZ plane (u -> -u, theta -> -theta) and flip the winding.
			for (FVector3f& V : M.Pos) { V.Y = -V.Y; }
			for (FVector3f& N : M.Nrm) { N.Y = -N.Y; }
			for (FVector2f& T : M.UV) { T.X = -T.X; }
			for (int32 i = 0; i + 2 < M.Idx.Num(); i += 3) { Swap(M.Idx[i + 1], M.Idx[i + 2]); }
		}
		return bOk && M.Idx.Num() >= 3;
	}

	UStaticMesh* MakeStaticMesh(const FKitMesh& K, const FString& DebugName)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Positions = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Signs = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		const FPolygonGroupID Group = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Group] = FName(TEXT("Plate"));
		MD.ReserveNewVertices(K.Pos.Num());
		MD.ReserveNewVertexInstances(K.Pos.Num());
		MD.ReserveNewTriangles(K.Idx.Num() / 3);
		TArray<FVertexInstanceID> Instances;
		Instances.Reserve(K.Pos.Num());
		for (int32 v = 0; v < K.Pos.Num(); ++v)
		{
			const FVertexID VID = MD.CreateVertex();
			Positions[VID] = K.Pos[v];
			const FVertexInstanceID IID = MD.CreateVertexInstance(VID);
			const FVector3f N = K.Nrm[v].GetSafeNormal();
			FVector3f T = FVector3f::CrossProduct(FVector3f(0, 0, 1), N);
			if (T.SizeSquared() < 1e-4f) { T = FVector3f::CrossProduct(FVector3f(0, 1, 0), N); }
			Normals[IID] = N;
			Tangents[IID] = T.GetSafeNormal();
			Signs[IID] = 1.f;
			UVs.Set(IID, 0, K.UV[v]);
			Instances.Add(IID);
		}
		for (int32 i = 0; i + 2 < K.Idx.Num(); i += 3)
		{
			MD.CreateTriangle(Group, TArray<FVertexInstanceID>{ Instances[K.Idx[i]], Instances[K.Idx[i + 1]], Instances[K.Idx[i + 2]] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(),
			FName(*(TEXT("SM_Armor_") + DebugName))), RF_Transient);
#if WITH_EDITORONLY_DATA
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName(TEXT("Plate")), FName(TEXT("Plate"))));
#else
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName(TEXT("Plate"))));
#endif
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bUseHashAsGuid = true;
		Params.bBuildSimpleCollision = false;
		Params.bFastBuild = true;
		Params.bCommitMeshDescription = false;
		if (!Mesh->BuildFromMeshDescriptions(TArray<const FMeshDescription*>{ &MD }, Params)) { return nullptr; }
		return Mesh;
	}

	/** Session cache: suit|plate -> mesh (rooted; a suit's plates are ~100-150 small meshes). */
	TMap<FString, UStaticMesh*>& MeshCache()
	{
		static TMap<FString, UStaticMesh*> Cache;
		return Cache;
	}

	// ------------------------------------------------------------------ frames / attachment
	FVector NominalJoint(const FString& Bone)
	{
		const FKitFile& File = Kit();
		const bool bRight = Bone.EndsWith(TEXT("_r"));
		const FString Left = bRight ? Bone.LeftChop(2) + TEXT("_l") : Bone;
		const FVector* J = File.Joints.Find(Left);
		if (!J) { return FVector(0, 0, 100); }
		return bRight ? FVector(J->X, -J->Y, J->Z) : *J;
	}

	FString LimbChildOf(const FString& Bone)
	{
		const bool bRight = Bone.EndsWith(TEXT("_r"));
		const FString Left = bRight ? Bone.LeftChop(2) + TEXT("_l") : Bone;
		const FString* C = Kit().LimbChild.Find(Left);
		if (!C || C->IsEmpty()) { return FString(); }
		return bRight ? C->LeftChop(2) + TEXT("_r") : *C;
	}

	/** Segment frame (component space) from joint positions given by JointPos. */
	FTransform SegmentFrame(const FString& Bone, const TFunctionRef<FVector(const FString&)>& JointPos)
	{
		const FVector Origin = JointPos(Bone);
		const FString Child = LimbChildOf(Bone);
		if (Child.IsEmpty())
		{
			return FTransform(Origin);
		}
		const FVector Z = (Origin - JointPos(Child)).GetSafeNormal();
		if (Z.IsNearlyZero()) { return FTransform(Origin); }
		const FMatrix R = FRotationMatrix::MakeFromZX(Z, FVector::ForwardVector);
		return FTransform(R.Rotator(), Origin);
	}

	enum class ERigPart : uint8 { Root, ShoulderL, ShoulderR, HipL, HipR };

	ERigPart RigPartFor(const FString& Bone)
	{
		const bool bRight = Bone.EndsWith(TEXT("_r"));
		static const TCHAR* const Arm[] = { TEXT("upperarm"), TEXT("lowerarm"), TEXT("hand"), TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
		static const TCHAR* const Leg[] = { TEXT("thigh"), TEXT("calf"), TEXT("foot"), TEXT("ball") };
		for (const TCHAR* A : Arm) { if (Bone.StartsWith(A)) { return bRight ? ERigPart::ShoulderR : ERigPart::ShoulderL; } }
		for (const TCHAR* L : Leg) { if (Bone.StartsWith(L)) { return bRight ? ERigPart::HipR : ERigPart::HipL; } }
		return ERigPart::Root;
	}

	struct FKitState
	{
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Plates;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Hidden;
	};

	TMap<TWeakObjectPtr<USceneComponent>, FKitState>& States()
	{
		static TMap<TWeakObjectPtr<USceneComponent>, FKitState> Map;
		return Map;
	}

	bool IsUnder(const USceneComponent* C, const USceneComponent* Ancestor)
	{
		for (const USceneComponent* P = C; P; P = P->GetAttachParent()) { if (P == Ancestor) { return true; } }
		return false;
	}
}

namespace SuitArmorKit
{
	bool IsLoaded() { return Kit().BasePlates.Num() > 0; }

	int32 GetNumBasePlates() { return Kit().BasePlates.Num(); }

	int32 CountPlates(const FSuitDef& Suit)
	{
		FSuitKeys Keys;
		FStyle Style;
		return Resolve(Suit, Keys, Style).Num();
	}

	int32 CountPlateTriangles(const FSuitDef& Suit, const FString& PlateId)
	{
		FSuitKeys Keys;
		FStyle Style;
		for (const FPlate& P : Resolve(Suit, Keys, Style))
		{
			if (P.Id != PlateId) { continue; }
			FKitMesh M;
			return BuildPlateMesh(P, Keys, Style, M) ? M.Idx.Num() / 3 : 0;
		}
		return -1;
	}

	void Clear(FSuitRig& Rig, TArray<FArenaPaintedPart>& Parts)
	{
		FKitState* State = States().Find(Rig.Root.Get());
		if (!State) { return; }
		for (const TWeakObjectPtr<UStaticMeshComponent>& W : State->Plates)
		{
			if (UStaticMeshComponent* C = W.Get())
			{
				Parts.RemoveAll([C](const FArenaPaintedPart& P) { return P.Component == C; });
				C->DestroyComponent();
			}
		}
		for (const TWeakObjectPtr<UStaticMeshComponent>& W : State->Hidden)
		{
			if (UStaticMeshComponent* C = W.Get()) { C->SetHiddenInGame(false); }
		}
		States().Remove(Rig.Root.Get());
	}

	int32 Build(AActor* Owner, const FSuitDef& Suit, FSuitRig& Rig, TArray<FArenaPaintedPart>& Parts, USkeletalMeshComponent* SkelMesh)
	{
		if (!Owner || !Rig.Root) { return 0; }
		Clear(Rig, Parts);
		FSuitKeys Keys;
		FStyle Style;
		const TArray<FPlate> Plates = Resolve(Suit, Keys, Style);
		if (Plates.Num() == 0) { return 0; }

		// Real Manny joints when a skeleton is given: component-space ref pose from FReferenceSkeleton.
		TMap<FString, FTransform> RefCS;
		if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
		{
			const FReferenceSkeleton& Ref = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
			const TArray<FTransform>& Local = Ref.GetRefBonePose();
			TArray<FTransform> CS;
			CS.SetNum(Local.Num());
			for (int32 i = 0; i < Local.Num(); ++i)
			{
				const int32 Parent = Ref.GetParentIndex(i);
				CS[i] = Parent == INDEX_NONE ? Local[i] : Local[i] * CS[Parent];
				RefCS.Add(Ref.GetBoneName(i).ToString(), CS[i]);
			}
		}
		const bool bSkeleton = RefCS.Num() > 0;
		auto JointPos = [&RefCS, bSkeleton](const FString& Bone) -> FVector
		{
			if (bSkeleton) { if (const FTransform* T = RefCS.Find(Bone)) { return T->GetLocation(); } }
			return NominalJoint(Bone);
		};

		USceneComponent* RigParts[] = { Rig.Root.Get(), Rig.ShoulderL.Get(), Rig.ShoulderR.Get(), Rig.HipL.Get(), Rig.HipR.Get() };
		FKitState State;
		TArray<FArenaPaintedPart> NewParts;
		int32 Failed = 0;
		const FString SuitKey = Suit.Id.ToString();
		for (const FPlate& P : Plates)
		{
			const FString CacheKey = SuitKey + TEXT("|") + P.Id;
			UStaticMesh* Mesh = MeshCache().FindRef(CacheKey);
			if (!Mesh)
			{
				FKitMesh K;
				if (!BuildPlateMesh(P, Keys, Style, K)) { ++Failed; continue; }
				Mesh = MakeStaticMesh(K, P.Id.Replace(TEXT("."), TEXT("_")));
				if (!Mesh) { ++Failed; continue; }
				Mesh->AddToRoot();
				MeshCache().Add(CacheKey, Mesh);
			}

			// Bone: kit bone, else its fallback (skeleton mode); frameBone defines the authoring frame.
			FString Bone = P.Bone;
			if (bSkeleton && !RefCS.Contains(Bone) && !P.FallbackBone.IsEmpty()) { Bone = P.FallbackBone; }
			const FString FrameBone = P.FrameBone.IsEmpty() ? Bone : P.FrameBone;
			const FTransform Frame = SegmentFrame(FrameBone, JointPos);
			FVector Loc = P.bMirrored ? FVector(P.Loc.X, -P.Loc.Y, P.Loc.Z) : P.Loc;
			const FRotator Rot = P.bMirrored ? FRotator(P.Rot.Pitch, -P.Rot.Yaw, -P.Rot.Roll) : P.Rot;
			// Layering: each layer above 1 sits layerGap (0.15 cm) further out along the plate normal.
			Loc += Rot.RotateVector(FVector::ForwardVector) * (0.15 * FMath::Max(0, P.Layer - 1));
			const FTransform PlateCS = FTransform(Rot, Loc) * Frame;

			const FName CompName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), FName(*P.Name));
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, CompName);
			C->SetStaticMesh(Mesh);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			C->bCastDynamicShadow = true;
			if (bSkeleton && RefCS.Contains(Bone))
			{
				C->SetupAttachment(SkelMesh, FName(*Bone));
				C->SetRelativeTransform(PlateCS.GetRelativeTransform(RefCS[Bone]));
			}
			else
			{
				USceneComponent* Parent = RigParts[(int32)RigPartFor(Bone)];
				if (!Parent) { Parent = Rig.Root.Get(); }
				// Rig pivots are at rest when unrotated; plates are placed relative to the pivot location.
				const FTransform ParentRest(Parent == Rig.Root.Get() ? FVector::ZeroVector : Parent->GetRelativeLocation());
				C->SetupAttachment(Parent);
				C->SetRelativeTransform(PlateCS.GetRelativeTransform(ParentRest));
			}
			C->RegisterComponent();
			Owner->AddInstanceComponent(C);

			FArenaPaintedPart Part;
			Part.Component = C;
			Part.Shade = float(P.Shade);
			Part.Surface = EArenaSurface::SuitPaint;
			if (P.SlotName == TEXT("primary")) { Part.Role = ESuitPaintRole::Primary; }
			else if (P.SlotName == TEXT("secondary")) { Part.Role = ESuitPaintRole::Secondary; }
			else if (P.SlotName == TEXT("accent")) { Part.Role = ESuitPaintRole::Accent; }
			else if (P.SlotName.StartsWith(TEXT("glow.")))
			{
				Part.Role = ESuitPaintRole::Emissive;
				Part.Surface = EArenaSurface::Auto;
				const double Def = P.SlotName == TEXT("glow.eyes") ? 20.0 : (P.SlotName == TEXT("glow.repulsors") ? 15.0 : 25.0);
				Part.Emissive = float(P.GlowIntensity > 0.0 ? P.GlowIntensity : Def);
				Part.Color = Suit.EmissiveColor;
			}
			else
			{
				Part.Role = ESuitPaintRole::Fixed;
				Part.Color = P.SlotName == TEXT("fixed.sole") ? FLinearColor(0.05f, 0.05f, 0.055f)
					: P.SlotName == TEXT("fixed.baresteel") ? FLinearColor(0.3f, 0.3f, 0.32f) : FLinearColor(0.03f, 0.03f, 0.035f);
				Part.Surface = P.SlotName == TEXT("fixed.baresteel") ? EArenaSurface::Steel : EArenaSurface::RobotMetal;
			}
			NewParts.Add(Part);
			State.Plates.Add(C);
		}
		if (NewParts.Num() == 0)
		{
			UE_LOG(LogArmorKit, Warning, TEXT("Armour kit: no plates built for %s (%d failed); keeping placeholders."), *SuitKey, Failed);
			return 0;
		}
		ArenaVisuals::PaintAll(Owner, NewParts);

		// Hide the placeholder shapes the plates replace. Kept: thruster flames, Hulkbuster plates and
		// the first-person gauntlets.
		for (FArenaPaintedPart& Old : Parts)
		{
			UStaticMeshComponent* C = Old.Component;
			if (!C || !IsUnder(C, Rig.Root.Get())) { continue; }
			if (C == Rig.FlameBootL || C == Rig.FlameBootR || C == Rig.FlameHandL || C == Rig.FlameHandR) { continue; }
			if ((Rig.HulkbusterPlates && IsUnder(C, Rig.HulkbusterPlates)) || (Rig.FPRoot && IsUnder(C, Rig.FPRoot))) { continue; }
			if (C->bHiddenInGame) { continue; }
			C->SetHiddenInGame(true);
			State.Hidden.Add(C);
		}
		Parts.Append(NewParts);
		States().Add(Rig.Root.Get(), State);
		UE_LOG(LogArmorKit, Log, TEXT("Armour kit: %s -> %d plates (%s style, panel depth %.2f cm, %s joints)%s."), *SuitKey, NewParts.Num(),
			*Keys.Style, Keys.PanelLineDepth, bSkeleton ? TEXT("skeleton") : TEXT("nominal rig"),
			Failed > 0 ? *FString::Printf(TEXT(", %d failed"), Failed) : TEXT(""));
		return NewParts.Num();
	}
}
