#include "PilotAvatar.h"
#include "HAL/IConsoleManager.h"

#include "NpcManifest.h"
#include "StoryCharacterNPC.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

namespace
{
	FLinearColor ColorAt(const TArray<FLinearColor>& L, int32 Index)
	{
		return L.Num() > 0 ? L[FMath::Clamp(Index, 0, L.Num() - 1)] : FLinearColor::Gray;
	}
	FString Pkg(const FString& Path)
	{
		FString A = Path, B;
		return Path.Split(TEXT("."), &A, &B) ? A : Path;
	}
	bool PkgExists(const FString& Path) { return FPackageName::IsValidLongPackageName(Pkg(Path)) && FPackageName::DoesPackageExist(Pkg(Path)); }
}

static TAutoConsoleVariable<int32> CVarSkeletalAvatars(TEXT("ima.SkeletalAvatars"), 0,
	TEXT("1 = use imported skeletal bodies (UAL / Manny) for the pilot and pedestrians; 0 = customizable shape bodies (default)."));


static TAutoConsoleVariable<int32> CVarAvatarCatalog(TEXT("ima.AvatarCatalog"), 1,
	TEXT("1 = the pilot uses a real character body (MetaHuman / Character Creator: Kevin / CityKit people) when the project has one; 0 = shape body."));

// ============================================================================ real character catalog

namespace AvatarCatalog
{
	static bool IsFemName(const FString& L)
	{
		if (L.Contains(TEXT("female")) || L.Contains(TEXT("woman")) || L.Contains(TEXT("girl")) || L.Contains(TEXT("quinn"))) { return true; }
		if (L.Contains(TEXT("_f_")) || L.StartsWith(TEXT("f_")) || L.EndsWith(TEXT("_f")) || L.Contains(TEXT("/f_"))) { return true; }
		return false;
	}

	static FString MakeLabel(const FString& Name)
	{
		FString L = Name;
		for (const TCHAR* Pre : { TEXT("SKM_"), TEXT("SK_"), TEXT("Quaternius_") }) { L.RemoveFromStart(Pre); }
		L.ReplaceInline(TEXT("_"), TEXT(" "));
		return L.TrimStartAndEnd();
	}

	static TArray<FAvatarBodyEntry> Scan()
	{
		TArray<FAvatarBodyEntry> Out;
		IAssetRegistry* AR = IAssetRegistry::Get();
		if (!AR) { return Out; }
		if (AR->IsLoadingAssets())
		{
			TArray<FString> Paths = { TEXT("/Game/CityKit/People"), TEXT("/Game/MetaHumans"), TEXT("/Game/Kevin"), TEXT("/Game/CharacterCreator"), TEXT("/Game/Reallusion") };
			AR->ScanPathsSynchronous(Paths, false);
		}
		TArray<FAssetData> Meshes;
		AR->GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Meshes, true);
		for (const FAssetData& A : Meshes)
		{
			const FString Path = A.GetObjectPathString();
			const FString Lower = Path.ToLower();
			const FString NameL = A.AssetName.ToString().ToLower();
			if (!Lower.StartsWith(TEXT("/game/"))) { continue; }
			FAvatarBodyEntry E;
			E.MeshPath = Path;
			E.Label = MakeLabel(A.AssetName.ToString());
			const bool bPart = NameL.Contains(TEXT("hair")) || NameL.Contains(TEXT("eye")) || NameL.Contains(TEXT("teeth")) || NameL.Contains(TEXT("tongue"))
				|| NameL.Contains(TEXT("brow")) || NameL.Contains(TEXT("lash")) || NameL.Contains(TEXT("beard")) || NameL.Contains(TEXT("mustache"));
			if (Lower.Contains(TEXT("/metahumans/")) && NameL.Contains(TEXT("body")) && !NameL.Contains(TEXT("face")) && !bPart)
			{
				E.Source = TEXT("MetaHuman"); E.Tier = 2;
				// MetaHuman folder name is the character's name.
				FString Folder = A.PackagePath.ToString(); Folder.RemoveFromStart(TEXT("/Game/MetaHumans/"));
				FString First, Rest; if (Folder.Split(TEXT("/"), &First, &Rest)) { Folder = First; }
				if (!Folder.IsEmpty()) { E.Label = Folder; }
				for (const FAssetData& M2 : Meshes)
				{
					const FString P2 = M2.GetObjectPathString();
					if (P2.StartsWith(FString(TEXT("/Game/MetaHumans/")) + Folder + TEXT("/")) && M2.AssetName.ToString().ToLower().Contains(TEXT("face"))) { E.FacePath = P2; break; }
				}
			}
			else if ((Lower.Contains(TEXT("kevin")) || Lower.Contains(TEXT("charactercreator")) || Lower.Contains(TEXT("reallusion")) || NameL.Contains(TEXT("cc_base")) || Lower.Contains(TEXT("/cc4"))) && !bPart)
			{
				E.Source = TEXT("Character Creator: Kevin"); E.Tier = 2;
			}
			else if (Lower.StartsWith(TEXT("/game/citykit/people/")) && NameL.StartsWith(TEXT("sk_")))
			{
				E.Source = TEXT("CityKit"); E.Tier = 1;
			}
			else { continue; }
			E.bFeminine = IsFemName(NameL) || IsFemName(Lower);
			// Clips that live next to the mesh (Quaternius: <Name>CharacterArmature_Idle / _Walk / _Defeat).
			TArray<FAssetData> Anims;
			AR->GetAssetsByPath(A.PackagePath, Anims, true);
			for (const FAssetData& An : Anims)
			{
				if (An.AssetClassPath != UAnimSequence::StaticClass()->GetClassPathName()) { continue; }
				const FString AL = An.AssetName.ToString().ToLower();
				if (AL.EndsWith(TEXT("_idle")) || AL.Contains(TEXT("idle"))) { E.Idle.Add(An.GetObjectPathString()); }
				else if (AL.EndsWith(TEXT("_walk")) || (AL.Contains(TEXT("walk")) && !AL.Contains(TEXT("carry")))) { E.Walk.Add(An.GetObjectPathString()); }
				else if (AL.Contains(TEXT("defeat")) || AL.Contains(TEXT("death"))) { E.Defeat.Add(An.GetObjectPathString()); }
			}
			Out.Add(E);
		}
		Out.Sort([](const FAvatarBodyEntry& X, const FAvatarBodyEntry& Y) { return X.Tier != Y.Tier ? X.Tier > Y.Tier : X.MeshPath < Y.MeshPath; });
		UE_LOG(LogTemp, Log, TEXT("Career: avatar catalog %d real bodies (%s)"), Out.Num(), Out.Num() ? *Out[0].Source : TEXT("none - shape body"));
		return Out;
	}

	static TArray<FAvatarBodyEntry>& Cache() { static TArray<FAvatarBodyEntry> C; return C; }
	static bool& Scanned() { static bool b = false; return b; }

	const TArray<FAvatarBodyEntry>& All()
	{
		if (!Scanned() || Cache().Num() == 0)
		{
			static double LastTry = -100.0;
			const double Now = FPlatformTime::Seconds();
			if (!Scanned() || Now - LastTry > 5.0) { LastTry = Now; Cache() = Scan(); Scanned() = true; }
		}
		return Cache();
	}

	bool HasPremium() { for (const FAvatarBodyEntry& E : All()) { if (E.Tier >= 2) { return true; } } return false; }

	TArray<int32> ForBody(bool bFeminine)
	{
		const TArray<FAvatarBodyEntry>& L = All();
		const bool bPremium = HasPremium();
		TArray<int32> R;
		for (int32 i = 0; i < L.Num(); ++i) { if (L[i].bFeminine == bFeminine && (!bPremium || L[i].Tier >= 2)) { R.Add(i); } }
		if (R.Num() == 0) { for (int32 i = 0; i < L.Num(); ++i) { if (L[i].bFeminine == bFeminine) { R.Add(i); } } }
		if (R.Num() == 0) { for (int32 i = 0; i < L.Num(); ++i) { R.Add(i); } }
		return R;
	}

	int32 Resolve(const FAvatarConfig& Config)
	{
		const TArray<int32> R = ForBody(Config.Body == EAvatarBody::Feminine);
		if (R.Num() == 0) { return -1; }
		if (Config.BodyModel < 0)
		{
			// Default: a casual outfit when there is one.
			for (int32 i : R) { if (All()[i].Label.Contains(TEXT("Casual"))) { return i; } }
			return R[0];
		}
		return R[Config.BodyModel % R.Num()];
	}

	FString SourceNote()
	{
		const TArray<FAvatarBodyEntry>& L = All();
		if (HasPremium()) { return FString::Printf(TEXT("Character tech: %s"), *L[0].Source); }
		const FString Add = TEXT("No MetaHuman / Character Creator: Kevin in the project yet. Add it: Epic Games Launcher > Fab Library > Character Creator: Kevin > Add to Project > IronManArena.");
		return L.Num() ? FString::Printf(TEXT("Using CityKit people (%d bodies). %s"), L.Num(), *Add) : FString::Printf(TEXT("Using the built-in body. %s"), *Add);
	}
}

static UAnimSequenceBase* FirstClip(const TArray<FString>& Paths, const USkeletalMesh* Mesh)
{
	for (const FString& P : Paths)
	{
		UAnimSequenceBase* Seq = LoadObject<UAnimSequenceBase>(nullptr, *P);
		if (Seq && Mesh && Seq->GetSkeleton() == Mesh->GetSkeleton()) { return Seq; }
	}
	return nullptr;
}

/** A material that can render on a skeletal mesh in this build (cooked / -game runs can't add the usage flag). */
static UMaterialInterface* SkeletalSafe(UMaterialInterface* M)
{
	auto Ok = [](UMaterialInterface* X) { const UMaterial* B = X ? X->GetMaterial() : nullptr; return B && (GIsEditor || B->GetUsageByFlag(MATUSAGE_SkeletalMesh)); };
	if (Ok(M)) { return M; }
	for (const TCHAR* Path : { TEXT("/Game/IronManArena/Materials/M_ArenaEmissive.M_ArenaEmissive"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), TEXT("/Engine/EngineMaterials/DefaultLitMaterial.DefaultLitMaterial") })
	{
		if (UMaterialInterface* F = LoadObject<UMaterialInterface>(nullptr, Path)) { if (Ok(F)) { return F; } }
	}
	return M;
}

bool APilotAvatar::BuildCatalog()
{
	const int32 Index = AvatarCatalog::Resolve(Cfg);
	if (Index < 0) { return false; }
	const FAvatarBodyEntry E = AvatarCatalog::All()[Index];
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *E.MeshPath);
	if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("Career: avatar body %s failed to load"), *E.MeshPath); return false; }
	CatIdle = FirstClip(E.Idle, Mesh);
	CatWalk = FirstClip(E.Walk, Mesh);
	CatDefeat = FirstClip(E.Defeat, Mesh);

	Skel = NewObject<USkeletalMeshComponent>(this);
	Skel->SetupAttachment(BodyRoot);
	Skel->SetSkeletalMesh(Mesh);
	Skel->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Skel->SetCollisionProfileName(TEXT("Ragdoll"));
	Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Skel->bEnableUpdateRateOptimizations = false;
	Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Skel->PrimaryComponentTick.bTickEvenWhenPaused = true; // the creator preview runs while the game is paused
	Skel->SetCastShadow(true);
	Skel->RegisterComponent();
	AddInstanceComponent(Skel);
	bSkeletal = true;
	bCatalog = true;
	if (!E.FacePath.IsEmpty())
	{
		if (USkeletalMesh* FaceMesh = LoadObject<USkeletalMesh>(nullptr, *E.FacePath))
		{
			FaceSkel = NewObject<USkeletalMeshComponent>(this);
			FaceSkel->SetupAttachment(Skel);
			FaceSkel->SetSkeletalMesh(FaceMesh);
			FaceSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			FaceSkel->PrimaryComponentTick.bTickEvenWhenPaused = true;
			FaceSkel->RegisterComponent();
			FaceSkel->SetLeaderPoseComponent(Skel);
			AddInstanceComponent(FaceSkel);
		}
	}

	// Scale to the creator's height (imported bodies come in at any size).
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float MeshH = FMath::Max(1.f, float(B.Origin.Z + B.BoxExtent.Z) - float(B.Origin.Z - B.BoxExtent.Z));
	if (MeshH < 60.f || MeshH > 400.f) { Skel->SetRelativeScale3D(FVector(178.f / MeshH)); }

	// Tint slots by their names (Quaternius: Skin / Hair / Shirt / Pants / Main / Belt / Hat / Detail; MetaHuman / CC: skin, hair, cloth).
	const TArray<FLinearColor>& OC = Career::OutfitColors();
	const FAvatarOutfit& O = Cfg.Outfit;
	const FLinearColor SkinC = ColorAt(Career::SkinTones(), Cfg.SkinTone);
	const FLinearColor HairC = ColorAt(Career::HairColors(), Cfg.HairColor);
	const TArray<FName> Slots = Skel->GetMaterialSlotNames();
	for (int32 m = 0; m < Skel->GetNumMaterials(); ++m)
	{
		UMaterialInterface* Orig = Skel->GetMaterial(m);
		FString N = Slots.IsValidIndex(m) ? Slots[m].ToString() : FString();
		if (Orig) { N += TEXT("|") + Orig->GetName(); }
		N = N.ToLower();
		bool bTint = true;
		FLinearColor C;
		if (N.Contains(TEXT("skin")) || N.Contains(TEXT("body"))) { C = SkinC; }
		else if (N.Contains(TEXT("hair")) || N.Contains(TEXT("beard"))) { C = HairC; }
		else if (N.Contains(TEXT("shirt")) || N.Contains(TEXT("main")) || N.Contains(TEXT("vest")) || N.Contains(TEXT("top")) || N.Contains(TEXT("cloth"))) { C = ColorAt(OC, O.TopColor); }
		else if (N.Contains(TEXT("pants")) || N.Contains(TEXT("legs")) || N.Contains(TEXT("bottom"))) { C = ColorAt(OC, O.PantsColor); }
		else if (N.Contains(TEXT("belt")) || N.Contains(TEXT("shoe")) || N.Contains(TEXT("boot")) || N.Contains(TEXT("black")) || N.Contains(TEXT("brown"))) { C = ColorAt(OC, O.ShoesColor); }
		else if (N.Contains(TEXT("hat")) || N.Contains(TEXT("helmet"))) { C = ColorAt(OC, O.HatColor); }
		else if (N.Contains(TEXT("detail")) || N.Contains(TEXT("red")) || N.Contains(TEXT("green")) || N.Contains(TEXT("grey"))) { C = ColorAt(OC, O.JacketColor); }
		else { bTint = false; }
		UMaterialInterface* Safe = SkeletalSafe(Orig);
		if (Safe != Orig) { Skel->SetMaterial(m, Safe); if (!bTint) { bTint = true; C = (m == 0) ? SkinC : FLinearColor(0.08f, 0.08f, 0.09f); } }
		if (!bTint) { continue; }
		if (UMaterialInstanceDynamic* MID = Skel->CreateAndSetMaterialInstanceDynamic(m))
		{
			for (const TCHAR* Param : { TEXT("DiffuseColor"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("BaseColorFactor"), TEXT("Color"), TEXT("Tint"), TEXT("Paint Tint") }) { MID->SetVectorParameterValue(Param, C); }
		}
	}

	// Head pivot on the head bone (faceplate-open view, hats from the shape kit are not added on real bodies).
	FName HeadBone = NAME_None;
	for (int32 b = 0; b < Skel->GetNumBones(); ++b)
	{
		const FString BN = Skel->GetBoneName(b).ToString().ToLower();
		if (BN == TEXT("head") || (HeadBone.IsNone() && BN.Contains(TEXT("head")) && !BN.Contains(TEXT("end")))) { HeadBone = Skel->GetBoneName(b); if (BN == TEXT("head")) { break; } }
	}
	if (!HeadBone.IsNone())
	{
		USceneComponent* Head = NewObject<USceneComponent>(this);
		Head->SetupAttachment(Skel, HeadBone);
		Head->RegisterComponent();
		Pivots.Add(Head);
		HeadPivot = Head;
	}
	UE_LOG(LogTemp, Log, TEXT("Career: pilot body %s [%s] idle=%s"), *E.Label, *E.Source, CatIdle ? *CatIdle->GetName() : TEXT("none (ref pose)"));
	PlayClip(TEXT("idle"));
	return true;
}

APilotAvatar::APilotAvatar()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SetActorEnableCollision(false);
}

USceneComponent* APilotAvatar::Pivot(USceneComponent* Parent, const FVector& Loc)
{
	USceneComponent* C = NewObject<USceneComponent>(this);
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(Loc);
	C->RegisterComponent();
	Pivots.Add(C);
	return C;
}

UStaticMeshComponent* APilotAvatar::P(USceneComponent* Parent, EArenaShape Shape, const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color,
	EArenaSurface Surface, const FRotator& Rot)
{
	UStaticMeshComponent* C = ArenaVisuals::AddRuntimePart(this, Parent, Shape, Loc, SizeCm / 100.f, Rot, Color, 0.f, Surface, false, Parts);
	if (C) { C->SetCastShadow(true); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	return C;
}

void APilotAvatar::Clear()
{
	for (FArenaPaintedPart& Part : Parts) { if (Part.Component) { Part.Component->DestroyComponent(); } }
	Parts.Reset();
	for (USceneComponent* C : Pivots) { if (C) { C->DestroyComponent(); } }
	Pivots.Reset();
	if (FaceSkel) { FaceSkel->DestroyComponent(); FaceSkel = nullptr; }
	if (Skel) { Skel->DestroyComponent(); Skel = nullptr; }
	HipL = HipR = KneeL = KneeR = ShoulderL = ShoulderR = ElbowL = ElbowR = HeadPivot = Chest = nullptr;
	BodyRoot = nullptr;
	bSkeletal = false;
	bCatalog = false;
	CatIdle = CatWalk = CatDefeat = nullptr;
	CurrentClip = NAME_None;
}

void APilotAvatar::ApplyConfig(const FAvatarConfig& Config, bool bAllowSkeletal)
{
	Cfg = Config;
	Clear();
	bCollapsed = false;
	BodyRoot = Pivot(Root, FVector::ZeroVector);
	BodyRoot->SetRelativeScale3D(FVector(FMath::Clamp(Cfg.Height, 0.8f, 1.15f)));
	// Skeletal bodies are opt-in (ima.SkeletalAvatars 1): the imported UAL / GLB mannequins come in with a broken bind
	// pose (arms folded behind the back), materials without the SkeletalMesh usage flag (engine default grey) and no
	// colour parameters, so the avatar creator's skin / hair / outfit picks did not show. The shape body takes every pick.
	// The player / pilot (no forced pedestrian body) uses a real character from AvatarCatalog: MetaHuman or
	// Character Creator: Kevin when added to the project, else the CityKit people. Each plays its own idle clip, so the
	// bind pose (arms behind the back on the GLB imports) is never shown.
	if (bAllowSkeletal && ForcedBodyIndex < 0 && CVarAvatarCatalog.GetValueOnGameThread() != 0 && BuildCatalog()) { return; }
	if (!(bAllowSkeletal && CVarSkeletalAvatars.GetValueOnGameThread() != 0 && BuildSkeletal())) { BuildShapes(); }
}

float APilotAvatar::GetStandingHeight() const { return 178.f * FMath::Clamp(Cfg.Height, 0.8f, 1.15f); }

bool APilotAvatar::BuildSkeletal()
{
	const bool bFem = Cfg.Body == EAvatarBody::Feminine;
	FString MeshPath, AnimBp;
	// Manifest bodies first (imported pedestrian models), matching the body type.
	const TArray<FNpcBodyDef>& Bodies = NpcManifest::Bodies();
	if (Bodies.Num() > 0)
	{
		int32 Pick = ForcedBodyIndex;
		if (!Bodies.IsValidIndex(Pick))
		{
			for (int32 i = 0; i < Bodies.Num(); ++i) { if (Bodies[i].bFeminine == bFem) { Pick = i; break; } }
		}
		if (Bodies.IsValidIndex(Pick)) { MeshPath = Bodies[Pick].MeshPath; AnimBp = Bodies[Pick].AnimBlueprintPath; }
	}
	if (MeshPath.IsEmpty())
	{
		MeshPath = bFem ? AStoryCharacterNPC::DefaultQuinnPath : AStoryCharacterNPC::DefaultMannyPath;
		if (!PkgExists(MeshPath)) { MeshPath = AStoryCharacterNPC::DefaultMannyPath; }
	}
	USkeletalMesh* Mesh = PkgExists(MeshPath) ? NpcManifest::LoadSkeletalMesh(MeshPath) : nullptr;
	if (!Mesh) { return false; }
	UClass* AnimClass = AnimBp.IsEmpty() ? nullptr : NpcManifest::LoadAnimClass(AnimBp);
	if (!AnimClass)
	{
		int32 Num = 0;
		const TCHAR* const* Paths = AStoryCharacterNPC::GetDefaultAnimClassPaths(Num);
		for (int32 i = 0; i < Num && !AnimClass; ++i) { if (PkgExists(Paths[i])) { AnimClass = LoadClass<UAnimInstance>(nullptr, Paths[i]); } }
	}
	// Without any animation source a T-posing mannequin looks worse than the shape person.
	bool bHasClip = false;
	for (const TCHAR* Cat : { TEXT("idle"), TEXT("walk") })
	{
		for (const FString& A : NpcManifest::Anims(FName(Cat)))
		{
			const UAnimSequenceBase* Seq = NpcManifest::LoadAnim(A);
			if (Seq && Seq->GetSkeleton() == Mesh->GetSkeleton()) { bHasClip = true; break; }
		}
	}
	if (!AnimClass && !bHasClip) { return false; }

	Skel = NewObject<USkeletalMeshComponent>(this);
	Skel->SetupAttachment(BodyRoot);
	Skel->SetSkeletalMesh(Mesh);
	Skel->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skel->SetCollisionProfileName(TEXT("Ragdoll"));
	Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bHasClip) { Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode); }
	else { Skel->SetAnimInstanceClass(AnimClass); }
	Skel->RegisterComponent();
	AddInstanceComponent(Skel);
	bSkeletal = true;
	// Tint every material slot that exposes a colour parameter (skin on Manny / Quinn).
	const FLinearColor Skin = ColorAt(Career::SkinTones(), Cfg.SkinTone);
	const FLinearColor TopC = ColorAt(Career::OutfitColors(), Cfg.Outfit.TopColor);
	for (int32 m = 0; m < Skel->GetNumMaterials(); ++m)
	{
		if (UMaterialInstanceDynamic* MID = Skel->CreateAndSetMaterialInstanceDynamic(m))
		{
			const FLinearColor C = (m == 0) ? Skin : TopC;
			for (const TCHAR* Param : { TEXT("Tint"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("Color"), TEXT("Paint Tint") }) { MID->SetVectorParameterValue(Param, C); }
		}
	}
	// Hair / hat / glasses ride the head bone.
	const FName HeadBone = Skel->GetBoneIndex(TEXT("head")) != INDEX_NONE ? FName(TEXT("head")) : FName(TEXT("Head"));
	if (Skel->GetBoneIndex(HeadBone) != INDEX_NONE)
	{
		USceneComponent* Head = NewObject<USceneComponent>(this);
		Head->SetupAttachment(Skel, HeadBone);
		// Mannequin head bone: +X up the neck, +Y forward-ish; rotate our local frame (X fwd, Z up) onto it.
		Head->SetRelativeRotation(FRotator(0.f, 90.f, -90.f));
		Head->SetRelativeLocation(FVector(4.f, 2.f, 0.f));
		Head->RegisterComponent();
		Pivots.Add(Head);
		HeadPivot = Head;
		BuildHead(Head, FVector(1.5f, 0.f, 6.f), true);
	}
	PlayClip(TEXT("idle"));
	return true;
}

void APilotAvatar::PlayClip(FName Category)
{
	if (bCatalog)
	{
		if (!Skel || CurrentClip == Category) { return; }
		const bool bDeath = Category == FName(TEXT("death"));
		const bool bMove = Category == FName(TEXT("walk")) || Category == FName(TEXT("run"));
		UAnimSequenceBase* Seq = bDeath ? CatDefeat.Get() : bMove ? CatWalk.Get() : CatIdle.Get();
		if (!Seq) { Seq = CatIdle.Get(); }
		if (!Seq) { return; }
		CurrentClip = Category;
		Skel->PlayAnimation(Seq, !bDeath);
		Skel->SetPlayRate(Category == FName(TEXT("run")) ? 1.7f * AnimRate : AnimRate);
		return;
	}
	if (!Skel || CurrentClip == Category || Skel->GetAnimationMode() != EAnimationMode::AnimationSingleNode) { return; }
	const TArray<FString>& L = NpcManifest::Anims(Category);
	TArray<UAnimSequenceBase*> Ok;
	for (const FString& A : L)
	{
		UAnimSequenceBase* Seq = NpcManifest::LoadAnim(A);
		if (Seq && Skel->GetSkeletalMeshAsset() && Seq->GetSkeleton() == Skel->GetSkeletalMeshAsset()->GetSkeleton()) { Ok.Add(Seq); }
	}
	if (Ok.Num() == 0) { return; }
	CurrentClip = Category;
	const bool bLoop = Category != FName(TEXT("death")) && Category != FName(TEXT("hit"));
	Skel->PlayAnimation(Ok[FMath::RandRange(0, Ok.Num() - 1)], bLoop);
	Skel->SetPlayRate(AnimRate);
	if (Category == FName(TEXT("idle"))) { Skel->SetPosition(FMath::FRand() * 2.f, false); }
}

void APilotAvatar::BuildShapes()
{
	const bool bFem = Cfg.Body == EAvatarBody::Feminine;
	const FLinearColor Skin = ColorAt(Career::SkinTones(), Cfg.SkinTone);
	const TArray<FLinearColor>& OC = Career::OutfitColors();
	const FAvatarOutfit& O = Cfg.Outfit;
	const FLinearColor TopC = ColorAt(OC, O.TopColor), JackC = ColorAt(OC, O.JacketColor), PantC = ColorAt(OC, O.PantsColor), ShoeC = ColorAt(OC, O.ShoesColor);

	const float HipHalf = bFem ? 10.5f : 9.5f;
	const float WaistW = bFem ? 25.f : 31.f;
	const float ChestW = bFem ? 31.f : 38.f;
	const float ShoulderHalf = bFem ? 18.f : 21.5f;
	const float Limb = bFem ? 0.88f : 1.f;
	LegLen = 88.f;
	const float HipZ = 92.f, ShoulderZ = 143.f;

	const bool bSkirt = O.Pants == EOutfitPants::Skirt, bShorts = O.Pants == EOutfitPants::Shorts;
	const bool bSleevesLong = O.Top == EOutfitTop::Hoodie || O.Top == EOutfitTop::ShirtAndTie || O.Jacket != EOutfitJacket::None;
	const bool bTank = O.Top == EOutfitTop::TankTop && O.Jacket == EOutfitJacket::None;
	const FLinearColor SleeveC = O.Jacket != EOutfitJacket::None ? JackC : TopC;

	// Legs.
	for (int32 s = 0; s < 2; ++s)
	{
		const float Y = (s == 0 ? -1.f : 1.f) * HipHalf;
		USceneComponent* Hip = Pivot(BodyRoot, FVector(0, Y, HipZ));
		USceneComponent* Knee = Pivot(Hip, FVector(0, 0, -44.f));
		(s == 0 ? HipL : HipR) = Hip;
		(s == 0 ? KneeL : KneeR) = Knee;
		P(Hip, EArenaShape::Cube, FVector(0, 0, -22.f), FVector(17.f * Limb, 16.f * Limb, 46.f), bSkirt ? Skin : PantC);
		P(Knee, EArenaShape::Cube, FVector(0, 0, -21.f), FVector(13.f * Limb, 12.5f * Limb, 42.f), (bShorts || bSkirt) ? Skin : PantC);
		if (bShorts) { P(Hip, EArenaShape::Cube, FVector(0, 0, -30.f), FVector(18.5f, 17.5f, 28.f), PantC); }
		const float ShoeH = O.Shoes == EOutfitShoes::Boots ? 18.f : 8.f;
		const FLinearColor ShoeCol = O.Shoes == EOutfitShoes::DressShoes ? FLinearColor(0.02f, 0.02f, 0.022f) : ShoeC;
		P(Knee, EArenaShape::Cube, FVector(5.f, 0, -44.f + ShoeH * 0.5f - 4.f), FVector(O.Shoes == EOutfitShoes::DressShoes ? 26.f : 27.f, 11.f, ShoeH), ShoeCol,
			O.Shoes == EOutfitShoes::DressShoes ? EArenaSurface::SuitPaint : EArenaSurface::Plain);
		if (O.Shoes == EOutfitShoes::Sneakers) { P(Knee, EArenaShape::Cube, FVector(5.f, 0, -47.5f), FVector(27.5f, 11.5f, 2.5f), FLinearColor(0.85f, 0.85f, 0.83f)); }
	}
	// Torso.
	USceneComponent* Torso = Pivot(BodyRoot, FVector(0, 0, HipZ));
	P(Torso, EArenaShape::Cube, FVector(0, 0, 4.f), FVector(20.f, HipHalf * 2.f + 13.f, 16.f), PantC);
	if (bSkirt) { P(Torso, EArenaShape::Cylinder, FVector(0, 0, -12.f), FVector(30.f, HipHalf * 2.f + 22.f, 30.f), PantC); }
	Chest = Pivot(Torso, FVector(0, 0, 14.f));
	P(Chest, EArenaShape::Cube, FVector(0, 0, 7.f), FVector(19.f, WaistW, 18.f), TopC);
	P(Chest, EArenaShape::Cube, FVector(0, 0, 26.f), FVector(22.f, ChestW, 26.f), TopC);
	if (bFem) { P(Chest, EArenaShape::Sphere, FVector(7.f, 0, 26.f), FVector(12.f, ChestW - 6.f, 13.f), TopC); }
	if (O.Top == EOutfitTop::BandTee) { P(Chest, EArenaShape::Cube, FVector(11.3f, 0, 27.f), FVector(0.6f, ChestW * 0.5f, 12.f), FLinearColor(0.9f, 0.85f, 0.75f)); P(Chest, EArenaShape::Sphere, FVector(11.6f, 0, 27.f), FVector(0.6f, 8.f, 8.f), FLinearColor(0.75f, 0.05f, 0.04f)); }
	if (O.Top == EOutfitTop::ShirtAndTie) { P(Chest, EArenaShape::Cube, FVector(11.3f, 0, 22.f), FVector(0.6f, 4.f, 28.f), FLinearColor(0.35f, 0.03f, 0.05f)); P(Chest, EArenaShape::Cube, FVector(4.f, 0, 40.f), FVector(16.f, 16.f, 4.f), TopC); }
	if (O.Top == EOutfitTop::Hoodie) { P(Chest, EArenaShape::Sphere, FVector(-10.f, 0, 38.f), FVector(10.f, 22.f, 12.f), TopC); P(Chest, EArenaShape::Cube, FVector(10.f, 0, 9.f), FVector(1.5f, WaistW * 0.6f, 9.f), TopC * 0.85f); }
	if (O.Jacket != EOutfitJacket::None)
	{
		const float Puff = O.Jacket == EOutfitJacket::Bomber ? 4.f : 2.f;
		const float Bottom = O.Jacket == EOutfitJacket::SuitJacket ? -6.f : 2.f;
		const EArenaSurface JS = O.Jacket == EOutfitJacket::Leather ? EArenaSurface::SuitPaint : EArenaSurface::Plain;
		P(Chest, EArenaShape::Cube, FVector(-1.f, -ChestW * 0.28f, (Bottom + 39.f) * 0.5f), FVector(22.f + Puff, ChestW * 0.44f + Puff, 39.f - Bottom), JackC, JS);
		P(Chest, EArenaShape::Cube, FVector(-1.f, ChestW * 0.28f, (Bottom + 39.f) * 0.5f), FVector(22.f + Puff, ChestW * 0.44f + Puff, 39.f - Bottom), JackC, JS);
		P(Chest, EArenaShape::Cube, FVector(-3.f, 0, (Bottom + 39.f) * 0.5f), FVector(18.f + Puff, ChestW * 0.4f, 39.f - Bottom), JackC, JS);
		P(Chest, EArenaShape::Cube, FVector(2.f, 0, 39.5f), FVector(14.f, ChestW * 0.6f, 5.f), JackC, JS); // collar
	}
	// Arms.
	for (int32 s = 0; s < 2; ++s)
	{
		const float Y = (s == 0 ? -1.f : 1.f) * ShoulderHalf;
		USceneComponent* Sh = Pivot(BodyRoot, FVector(0, Y, ShoulderZ));
		USceneComponent* El = Pivot(Sh, FVector(0, 0, -29.f));
		(s == 0 ? ShoulderL : ShoulderR) = Sh;
		(s == 0 ? ElbowL : ElbowR) = El;
		Sh->SetRelativeRotation(FRotator(0.f, 0.f, s == 0 ? 5.f : -5.f));
		P(Sh, EArenaShape::Sphere, FVector(0, 0, 0), FVector(14.f, 14.f, 13.f), bTank ? Skin : SleeveC);
		P(Sh, EArenaShape::Cube, FVector(0, 0, -14.f), FVector(11.f * Limb, 11.f * Limb, 29.f), bTank ? Skin : SleeveC);
		P(El, EArenaShape::Cube, FVector(0, 0, -13.f), FVector(9.5f * Limb, 9.5f * Limb, 26.f), bSleevesLong ? SleeveC : Skin);
		P(El, EArenaShape::Sphere, FVector(0, 0, -29.f), FVector(8.5f, 6.f, 10.f), Skin);
	}
	// Neck + head.
	P(BodyRoot, EArenaShape::Cylinder, FVector(0, 0, 151.f), FVector(10.f, 10.f, 10.f), Skin);
	HeadPivot = Pivot(BodyRoot, FVector(0, 0, 155.f));
	BuildHead(HeadPivot, FVector(0, 0, 12.f), false);
}

void APilotAvatar::BuildHead(USceneComponent* H, const FVector& C, bool bOnSkeleton)
{
	const bool bFem = Cfg.Body == EAvatarBody::Feminine;
	const FLinearColor Skin = ColorAt(Career::SkinTones(), Cfg.SkinTone);
	const FLinearColor HairC = ColorAt(Career::HairColors(), Cfg.HairColor);
	const TArray<FLinearColor>& OC = Career::OutfitColors();
	const int32 Face = FMath::Max(0, Cfg.Face) % FMath::Max(1, Career::FaceNames().Num());
	// Face presets: 0 classic, 1 round, 2 angular, 3 long, 4 soft, 5 strong jaw.
	const FVector HeadSize = Face == 1 ? FVector(20.f, 19.f, 22.f) : Face == 2 ? FVector(19.f, 16.5f, 24.f) : Face == 3 ? FVector(19.f, 16.f, 25.5f)
		: Face == 4 ? FVector(19.5f, 17.5f, 22.5f) : FVector(20.f, 17.5f, 23.5f);
	if (!bOnSkeleton)
	{
		P(H, EArenaShape::Sphere, C, HeadSize, Skin);
		if (Face == 2 || Face == 5) { P(H, EArenaShape::Cube, C + FVector(3.f, 0, -7.f), FVector(12.f, Face == 5 ? 15.f : 12.f, 7.f), Skin); }
		const float EyeY = Face == 1 ? 3.6f : 3.3f;
		for (const float s : { -1.f, 1.f })
		{
			P(H, EArenaShape::Sphere, C + FVector(HeadSize.X * 0.44f, s * EyeY, 1.5f), FVector(2.4f, 3.2f, 2.f), FLinearColor(0.9f, 0.9f, 0.88f));
			P(H, EArenaShape::Sphere, C + FVector(HeadSize.X * 0.47f, s * EyeY, 1.5f), FVector(1.4f, 1.7f, 1.7f), FLinearColor(0.08f, 0.05f, 0.03f));
			P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.45f, s * EyeY, bFem ? 4.6f : 4.f), FVector(1.f, 4.2f, bFem ? 0.6f : 1.f), HairC * 0.8f,
				EArenaSurface::Plain, FRotator(0.f, 0.f, s * (Face == 2 ? 10.f : 4.f)));
			P(H, EArenaShape::Sphere, C + FVector(0.f, s * HeadSize.Y * 0.5f, 0.f), FVector(3.f, 2.f, 5.f), Skin);
		}
		P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.5f, 0, -1.5f), FVector(2.4f, Face == 2 ? 1.6f : 2.1f, 4.5f), Skin * 0.95f, EArenaSurface::Plain, FRotator(-12.f, 0.f, 0.f));
		P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.46f, 0, -5.6f), FVector(1.f, bFem ? 4.6f : 4.2f, bFem ? 1.2f : 0.8f), bFem ? FLinearColor(0.55f, 0.15f, 0.18f) : Skin * 0.7f);
	}
	// Hair.
	const FVector Top = C + FVector(-1.f, 0, HeadSize.Z * 0.18f);
	switch (Cfg.Hair)
	{
	case EAvatarHair::Bald: break;
	case EAvatarHair::Buzz: P(H, EArenaShape::Sphere, C + FVector(-0.8f, 0, 1.2f), HeadSize + FVector(0.8f, 0.8f, -1.f), HairC * 0.7f); break;
	case EAvatarHair::Short:
		P(H, EArenaShape::Sphere, Top, FVector(HeadSize.X + 1.2f, HeadSize.Y + 1.f, 15.f), HairC);
		P(H, EArenaShape::Cube, C + FVector(-6.f, 0, 2.f), FVector(9.f, HeadSize.Y + 0.6f, 12.f), HairC);
		break;
	case EAvatarHair::Curly:
		P(H, EArenaShape::Sphere, Top, FVector(HeadSize.X + 1.2f, HeadSize.Y + 1.f, 15.f), HairC);
		for (int32 i = 0; i < 9; ++i)
		{
			const FRotator R(0.f, i * 40.f, 0.f);
			P(H, EArenaShape::Sphere, Top + R.RotateVector(FVector(7.f, 0, 2.f + (i % 2) * 2.f)), FVector(9.f), HairC);
		}
		break;
	case EAvatarHair::Long:
	case EAvatarHair::Ponytail:
	case EAvatarHair::Bun:
		P(H, EArenaShape::Sphere, Top, FVector(HeadSize.X + 1.4f, HeadSize.Y + 1.2f, 15.f), HairC);
		P(H, EArenaShape::Cube, C + FVector(-6.f, 0, 1.f), FVector(9.f, HeadSize.Y + 1.f, 14.f), HairC);
		if (Cfg.Hair == EAvatarHair::Long)
		{
			P(H, EArenaShape::Cube, C + FVector(-7.f, 0, -12.f), FVector(6.f, HeadSize.Y + 1.f, 26.f), HairC);
			for (const float s : { -1.f, 1.f }) { P(H, EArenaShape::Cube, C + FVector(0.5f, s * (HeadSize.Y * 0.5f + 0.6f), -6.f), FVector(10.f, 2.f, 20.f), HairC); }
		}
		else if (Cfg.Hair == EAvatarHair::Ponytail)
		{
			P(H, EArenaShape::Sphere, C + FVector(-10.f, 0, 4.f), FVector(4.f), HairC * 0.6f);
			P(H, EArenaShape::Cylinder, C + FVector(-13.f, 0, -5.f), FVector(5.f, 5.f, 20.f), HairC, EArenaSurface::Plain, FRotator(-20.f, 0.f, 0.f));
		}
		else
		{
			P(H, EArenaShape::Sphere, C + FVector(-7.f, 0, 10.f), FVector(10.f), HairC);
		}
		break;
	default: break;
	}
	// Facial hair (boys only).
	if (!bFem && !bOnSkeleton)
	{
		const FLinearColor FH = HairC * 0.85f;
		switch (Cfg.FacialHair)
		{
		case EAvatarFacialHair::Stubble: P(H, EArenaShape::Sphere, C + FVector(1.6f, 0, -5.f), FVector(HeadSize.X - 1.f, HeadSize.Y + 0.4f, 11.f), FLinearColor::LerpUsingHSV(Skin, FH, 0.45f)); break;
		case EAvatarFacialHair::Moustache: P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.5f, 0, -3.6f), FVector(1.6f, 5.5f, 1.4f), FH); break;
		case EAvatarFacialHair::Goatee:
			P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.5f, 0, -3.6f), FVector(1.6f, 5.f, 1.2f), FH);
			P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.44f, 0, -8.5f), FVector(3.f, 4.f, 4.5f), FH);
			break;
		case EAvatarFacialHair::FullBeard:
			P(H, EArenaShape::Sphere, C + FVector(2.f, 0, -6.f), FVector(HeadSize.X, HeadSize.Y + 1.f, 13.f), FH);
			P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.5f, 0, -3.4f), FVector(1.8f, 6.f, 1.6f), FH);
			break;
		default: break;
		}
	}
	// Hats.
	const FLinearColor HatC = ColorAt(OC, Cfg.Outfit.HatColor);
	switch (Cfg.Outfit.Hat)
	{
	case EOutfitHat::Cap:
		P(H, EArenaShape::Sphere, Top + FVector(0, 0, 1.f), FVector(HeadSize.X + 2.f, HeadSize.Y + 2.f, 14.f), HatC);
		P(H, EArenaShape::Cube, C + FVector(HeadSize.X * 0.55f + 3.f, 0, HeadSize.Z * 0.2f), FVector(10.f, 15.f, 1.f), HatC, EArenaSurface::Plain, FRotator(-8.f, 0.f, 0.f));
		break;
	case EOutfitHat::Beanie: P(H, EArenaShape::Sphere, Top + FVector(0, 0, 2.f), FVector(HeadSize.X + 2.f, HeadSize.Y + 2.f, 17.f), HatC); break;
	case EOutfitHat::Fedora:
		P(H, EArenaShape::Cylinder, C + FVector(-0.5f, 0, HeadSize.Z * 0.38f), FVector(30.f, 30.f, 1.2f), HatC);
		P(H, EArenaShape::Cylinder, C + FVector(-0.5f, 0, HeadSize.Z * 0.38f + 5.f), FVector(HeadSize.X + 0.5f, HeadSize.Y + 1.f, 10.f), HatC);
		P(H, EArenaShape::Cylinder, C + FVector(-0.5f, 0, HeadSize.Z * 0.38f + 1.5f), FVector(HeadSize.X + 1.f, HeadSize.Y + 1.5f, 2.5f), FLinearColor(0.03f, 0.03f, 0.03f));
		break;
	default: break;
	}
	// Glasses.
	const FVector G = C + FVector(HeadSize.X * 0.5f + 0.4f, 0, 1.5f);
	switch (Cfg.Outfit.Glasses)
	{
	case EOutfitGlasses::Aviators:
		for (const float s : { -1.f, 1.f }) { P(H, EArenaShape::Sphere, G + FVector(0, s * 3.5f, -0.4f), FVector(0.8f, 5.f, 4.f), FLinearColor(0.15f, 0.1f, 0.03f), EArenaSurface::Glass); }
		P(H, EArenaShape::Cube, G + FVector(0, 0, 1.2f), FVector(0.4f, 12.f, 0.4f), FLinearColor(0.8f, 0.65f, 0.3f), EArenaSurface::SuitGold);
		break;
	case EOutfitGlasses::Round:
		for (const float s : { -1.f, 1.f }) { P(H, EArenaShape::Cylinder, G + FVector(0, s * 3.4f, 0), FVector(4.f, 4.f, 0.6f), FLinearColor(0.05f, 0.05f, 0.05f), EArenaSurface::Plain, FRotator(90.f, 0.f, 0.f)); }
		break;
	case EOutfitGlasses::Shades:
		P(H, EArenaShape::Cube, G, FVector(1.f, 13.f, 3.4f), FLinearColor(0.01f, 0.01f, 0.012f), EArenaSurface::SuitPaint);
		break;
	default: break;
	}
}

void APilotAvatar::SetMoveSpeed(float Speed) { MoveSpeed = FMath::Max(0.f, Speed); }
void APilotAvatar::SetPanic(bool bPanic) { bPanicking = bPanic; }

void APilotAvatar::Collapse(const FVector& Impulse)
{
	if (bCollapsed) { return; }
	bCollapsed = true;
	CollapseT = 0.f;
	if (Skel && Skel->GetPhysicsAsset())
	{
		Skel->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		Skel->SetSimulatePhysics(true);
		Skel->AddImpulse(Impulse, NAME_None, true);
		return;
	}
	if (Skel) { PlayClip(TEXT("death")); }
	const FVector Dir = Impulse.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Impulse.GetSafeNormal2D();
	const FRotator Local = GetActorTransform().InverseTransformVectorNoScale(Dir).Rotation();
	CollapseRot = FRotator(0.f, Local.Yaw, 0.f);
	CollapseSlide = Dir * FMath::Clamp(Impulse.Size() * 0.3f, 60.f, 600.f);
}

void APilotAvatar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollapsed)
	{
		if (!Skel || !Skel->IsSimulatingPhysics())
		{
			CollapseT = FMath::Min(1.f, CollapseT + DeltaSeconds * 2.6f);
			const float A = FMath::InterpEaseIn(0.f, 88.f, CollapseT, 2.f);
			if (BodyRoot)
			{
				// Tip over around the feet in the impulse direction.
				const FQuat Q = FQuat(FRotator(0.f, CollapseRot.Yaw, 0.f)) * FQuat(FRotator(-A, 0.f, 0.f)) * FQuat(FRotator(0.f, -CollapseRot.Yaw, 0.f));
				BodyRoot->SetRelativeRotation(Q);
				BodyRoot->SetRelativeLocation(GetActorTransform().InverseTransformVectorNoScale(CollapseSlide) * CollapseT + FVector(0, 0, 10.f * CollapseT));
			}
		}
		return;
	}
	if (Skel)
	{
		PlayClip(MoveSpeed > 320.f ? FName(TEXT("run")) : MoveSpeed > 20.f ? FName(TEXT("walk")) : bPanicking ? FName(TEXT("cower")) : FName(TEXT("idle")));
		return;
	}
	if (!HipL || !HipR) { return; }
	const bool bRun = MoveSpeed > 320.f;
	const float Stride = bRun ? 150.f : 110.f;
	Phase += (MoveSpeed / FMath::Max(1.f, Stride)) * PI * DeltaSeconds;
	const float Amp = FMath::Clamp(MoveSpeed / 150.f, 0.f, 1.f) * (bRun ? 38.f : 24.f);
	const float S = FMath::Sin(Phase);
	HipL->SetRelativeRotation(FRotator(S * Amp, 0.f, 0.f));
	HipR->SetRelativeRotation(FRotator(-S * Amp, 0.f, 0.f));
	KneeL->SetRelativeRotation(FRotator(-FMath::Max(0.f, -S) * Amp * 1.3f, 0.f, 0.f));
	KneeR->SetRelativeRotation(FRotator(-FMath::Max(0.f, S) * Amp * 1.3f, 0.f, 0.f));
	if (bPanicking && MoveSpeed < 20.f)
	{
		ShoulderL->SetRelativeRotation(FRotator(150.f, 0.f, 15.f));
		ShoulderR->SetRelativeRotation(FRotator(150.f, 0.f, -15.f));
		ElbowL->SetRelativeRotation(FRotator(40.f, 0.f, 0.f));
		ElbowR->SetRelativeRotation(FRotator(40.f, 0.f, 0.f));
	}
	else
	{
		const float ArmAmp = Amp * (bPanicking ? 1.4f : 0.9f);
		ShoulderL->SetRelativeRotation(FRotator(-S * ArmAmp, 0.f, 5.f));
		ShoulderR->SetRelativeRotation(FRotator(S * ArmAmp, 0.f, -5.f));
		const float Bend = bRun ? 70.f : 12.f;
		ElbowL->SetRelativeRotation(FRotator(Bend, 0.f, 0.f));
		ElbowR->SetRelativeRotation(FRotator(Bend, 0.f, 0.f));
	}
	if (BodyRoot)
	{
		const float Bob = MoveSpeed > 20.f ? FMath::Abs(FMath::Cos(Phase)) * (bRun ? 4.f : 2.f) : FMath::Sin(GetWorld()->GetTimeSeconds() * 1.7f) * 0.4f;
		BodyRoot->SetRelativeLocation(FVector(0, 0, Bob));
		BodyRoot->SetRelativeRotation(FRotator(bRun ? -8.f : 0.f, 0.f, 0.f));
	}
}

FVector APilotAvatar::SetHeadOnly()
{
	SetActorTickEnabled(false);
	if (bCatalog && Skel)
	{
		// Real body: keep the head, hide limbs (they would poke through the armour), no shadow.
		Skel->SetCastShadow(false);
		for (int32 b = 0; b < Skel->GetNumBones(); ++b)
		{
			const FName Bn = Skel->GetBoneName(b);
			const FString L = Bn.ToString().ToLower();
			if (L.Contains(TEXT("arm")) || L.Contains(TEXT("shoulder")) || L.Contains(TEXT("clavicle")) || L.Contains(TEXT("thigh")) || L.Contains(TEXT("upleg")) || L.Contains(TEXT("upperleg")))
			{
				Skel->HideBoneByName(Bn, EPhysBodyOp::PBO_None);
			}
		}
		return HeadPivot ? FVector(HeadPivot->GetComponentLocation() - GetActorLocation()) : FVector(0.f, 0.f, 167.f);
	}
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Pc : Prims)
	{
		bool bHead = false;
		for (const USceneComponent* A = Pc; A; A = A->GetAttachParent()) { if (A == HeadPivot) { bHead = true; break; } }
		Pc->SetVisibility(bHead, false);
		Pc->SetCastShadow(false);
	}
	return HeadPivot ? FVector(HeadPivot->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 12.f)) - GetActorLocation()) : FVector(0.f, 0.f, 167.f);
}
