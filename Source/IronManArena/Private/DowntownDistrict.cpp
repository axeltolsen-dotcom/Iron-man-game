#include "DowntownDistrict.h"

#include "CityHouse.h"
#include "CityKit.h"
#include "CareerTypes.h"
#include "CareerSubsystem.h"
#include "CityLife.h"
#include "ChapterSet.h"
#include "IronManArena.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

// Look-dev (contrib/city/gta6_lookdev.md): sunny Florida coastal city - pastel art-deco low-rise with neon trims,
// palms on every street, a glass-tower core, a beach and lagoon on the south edge, wet patches, dense props.

namespace
{
	constexpr float PropCull = 32000.f;   // small street props vanish past 320 m
	constexpr float CarCull = 45000.f;
	constexpr float LowCull = 140000.f;   // low-rise buildings past 1.4 km (fog has them)
	constexpr float WinCull = 90000.f;

	struct FTri
	{
		TArray<FVector> V; TArray<FVector> N; TArray<int32> I; TArray<FVector2D> UV;
		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Nrm)
		{
			const int32 Base = V.Num();
			const bool bFlat = FMath::Abs(Nrm.Z) > 0.5f;
			for (const FVector& P : { A, B, C, D })
			{
				V.Add(P); N.Add(Nrm);
				UV.Add(bFlat ? FVector2D(P.X / 400.0, P.Y / 400.0) : FVector2D((P.X + P.Y) / 400.0, P.Z / 400.0));
			}
			// Winding: UE uses clockwise front faces seen from the normal side.
			const FVector Face = FVector::CrossProduct(B - A, C - A);
			if (FVector::DotProduct(Face, Nrm) < 0.f) { I.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 }); }
			else { I.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 }); }
		}
		/** Box top + 4 sides (no bottom). */
		void Box(const FVector& Min, const FVector& Max)
		{
			Quad(FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), FVector::UpVector);
			Quad(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Max.Z), FVector(Min.X, Min.Y, Max.Z), FVector(0, -1, 0));
			Quad(FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), FVector(0, 1, 0));
			Quad(FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Min.Y, Min.Z), FVector(Min.X, Min.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), FVector(-1, 0, 0));
			Quad(FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), FVector(Max.X, Max.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), FVector(1, 0, 0));
		}
	};

	UMaterialInterface* Pick(const TCHAR* KitKey, const TCHAR* EnvMI, EArenaSurface Fallback)
	{
		if (KitKey) { if (UMaterialInterface* M = CityKit::Material(KitKey)) { return M; } }
		if (EnvMI) { if (UMaterialInterface* M = AChapterSet::EnvMaterial(EnvMI)) { return M; } }
		return ArenaVisuals::GetSurfaceMaterial(Fallback);
	}

	UMaterialInterface* Realism(const TCHAR* Name, EArenaSurface Fallback)
	{
		if (UMaterialInterface* M = CityKit::LoadMaterialPath(FString(TEXT("/Game/Realism/Materials/")) + Name)) { return M; }
		return ArenaVisuals::GetSurfaceMaterial(Fallback);
	}

	// Miami pastels (linear albedo) for the art-deco low-rise.
	const FLinearColor Pastels[] = {
		FLinearColor(0.85f, 0.55f, 0.55f), FLinearColor(0.55f, 0.8f, 0.72f), FLinearColor(0.9f, 0.72f, 0.5f),
		FLinearColor(0.88f, 0.85f, 0.75f), FLinearColor(0.6f, 0.72f, 0.88f), FLinearColor(0.85f, 0.68f, 0.82f) };
	const FLinearColor Neons[] = {
		FLinearColor(1.f, 0.1f, 0.6f), FLinearColor(0.1f, 0.9f, 1.f), FLinearColor(0.6f, 0.2f, 1.f), FLinearColor(1.f, 0.5f, 0.1f) };
}

ADowntownDistrict::ADowntownDistrict()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Static);
}

ADowntownDistrict* ADowntownDistrict::SpawnDistrict(UWorld* World, const FVector& EnvOrigin, bool bHarborPresent)
{
	if (!World || FParse::Param(FCommandLine::Get(), TEXT("NoDowntown"))) { return nullptr; }
	if (ADowntownDistrict* Existing = Find(World)) { return Existing; }
	// West of FRIDAY's Harbor District (its west edge is 1 km out; its water is to the east), or west of the arena.
	const float OffsetX = bHarborPresent ? -195000.f : -(HalfExtent + 30000.f);
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.bDeferConstruction = true;
	ADowntownDistrict* D = World->SpawnActor<ADowntownDistrict>(ADowntownDistrict::StaticClass(), FTransform(EnvOrigin + FVector(OffsetX, 0.f, 0.f)), SP);
	if (!D) { return nullptr; }
	D->bHarbor = bHarborPresent;
	D->FinishSpawning(FTransform(EnvOrigin + FVector(OffsetX, 0.f, 0.f)));
	return D;
}

ADowntownDistrict* ADowntownDistrict::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return nullptr; }
	for (TActorIterator<ADowntownDistrict> It(World); It; ++It) { if (IsValid(*It)) { return *It; } }
	return nullptr;
}

void ADowntownDistrict::BeginPlay()
{
	Super::BeginPlay();
	Build();
	if (const UCareerSubsystem* Career = UCareerSubsystem::Get(this))
	{
		for (ACityHouse* H : Houses) { if (H) { H->SetOwned(Career->OwnsHouse(H->GetHouseId())); } }
	}
	ACityLife::Spawn(this);
}

UMaterialInstanceDynamic* ADowntownDistrict::Mid(const FString& Key, UMaterialInterface* Parent, const FLinearColor& Tint, float EmissiveUnits)
{
	if (const TObjectPtr<UMaterialInstanceDynamic>* Hit = Mids.Find(Key)) { return Hit->Get(); }
	if (!Parent) { return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, this);
	M->SetVectorParameterValue(TEXT("Color"), Tint);
	M->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(EmissiveUnits));
	Mids.Add(Key, M);
	return M;
}

UHierarchicalInstancedStaticMeshComponent* ADowntownDistrict::Hism(const FString& Key, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide, float CullEnd, bool bShadows)
{
	if (const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Hit = Hisms.Find(Key)) { return Hit->Get(); }
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*(TEXT("DT_") + Key.Replace(TEXT("|"), TEXT("_")))));
	H->SetStaticMesh(Mesh);
	H->SetupAttachment(Root);
	H->SetMobility(EComponentMobility::Static);
	if (Mat) { H->SetMaterial(0, Mat); }
	H->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	H->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	H->SetCastShadow(bShadows);
	H->SetCanEverAffectNavigation(false);
	H->SetGenerateOverlapEvents(false);
	if (CullEnd > 0.f) { H->SetCullDistances(int32(CullEnd * 0.8f), int32(CullEnd)); }
	H->RegisterComponent();
	AddInstanceComponent(H);
	Hisms.Add(Key, H);
	Pieces.Add(H);
	return H;
}

int32 ADowntownDistrict::Add(const FString& Key, EArenaShape Shape, UMaterialInterface* Mat, bool bCollide, float CullEnd, bool bShadows,
	const FVector& Center, const FVector& Size, const FRotator& Rot)
{
	FBatch& B = Batches.FindOrAdd(Key);
	if (B.Xf.Num() == 0)
	{
		B.Shape = Shape; B.Mat = Mat; B.bCollide = bCollide; B.Cull = CullEnd; B.bShadows = bShadows;
	}
	// Basic shapes are 100 cm, centred (the cylinder / cone stand on their centre too).
	B.Xf.Add(FTransform(Rot, Center, Size / 100.f));
	LocalBounds += Center;
	return B.Xf.Num() - 1;
}

void ADowntownDistrict::FlushBatches()
{
	for (TPair<FString, FBatch>& P : Batches)
	{
		if (P.Value.Xf.Num() == 0 || !P.Value.Mat) { continue; }
		UHierarchicalInstancedStaticMeshComponent* H = Hism(P.Key, ArenaVisuals::GetShapeMesh(P.Value.Shape), P.Value.Mat, P.Value.bCollide, P.Value.Cull, P.Value.bShadows);
		H->AddInstances(P.Value.Xf, false, false);
	}
	Batches.Reset();
}

UProceduralMeshComponent* ADowntownDistrict::MakeProc(const FString& Key, bool bCollide)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this, FName(*(TEXT("DTP_") + Key)));
	P->SetupAttachment(Root);
	P->bUseComplexAsSimpleCollision = true;
	P->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	P->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	P->SetCanEverAffectNavigation(false);
	P->SetGenerateOverlapEvents(false);
	P->RegisterComponent();
	AddInstanceComponent(P);
	Pieces.Add(P);
	return P;
}

const TArray<FLinearColor>& ADowntownDistrict::CarPaints()
{
	static const TArray<FLinearColor> C = {
		FLinearColor(0.6f, 0.6f, 0.62f), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.85f, 0.85f, 0.86f),
		FLinearColor(0.45f, 0.02f, 0.02f), FLinearColor(0.02f, 0.06f, 0.22f), FLinearColor(0.15f, 0.16f, 0.17f),
		FLinearColor(0.8f, 0.35f, 0.5f), FLinearColor(0.05f, 0.45f, 0.45f), FLinearColor(0.7f, 0.55f, 0.1f) };
	return C;
}

UMaterialInterface* ADowntownDistrict::CarPaintMaterial(int32 Paint)
{
	const TArray<FLinearColor>& P = CarPaints();
	const int32 Idx = FMath::Clamp(Paint, 0, P.Num() - 1);
	return Mid(FString::Printf(TEXT("carpaint%d"), Idx), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::SuitPaint), P[Idx]);
}

UMaterialInstanceDynamic* ADowntownDistrict::NightMid(const FString& Key, const FLinearColor& Tint, float NightUnits)
{
	const bool bNew = !Mids.Contains(Key);
	UMaterialInstanceDynamic* M = Mid(Key, ArenaVisuals::GetParentMaterial(), Tint, 0.f);
	if (M && bNew) { NightEmissive.Add(TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, float>(M, NightUnits)); }
	return M;
}

UMaterialInterface* ADowntownDistrict::FacadeMaterial(const FString& Style, float& OutNightNits)
{
	OutNightNits = 0.f;
	UMaterialInterface* Parent = nullptr;
	FLinearColor Tint = FLinearColor::White;
	if (Style == TEXT("glass_a"))      { Parent = Pick(TEXT("GlassFacade"), TEXT("MI_Env_GlassTower"), EArenaSurface::Facade); Tint = FLinearColor(0.8f, 0.9f, 0.95f); OutNightNits = 130.f; }
	else if (Style == TEXT("glass_b")) { Parent = Pick(TEXT("GlassFacade"), TEXT("MI_Env_GlassTower"), EArenaSurface::Facade); Tint = FLinearColor(0.55f, 0.72f, 0.75f); OutNightNits = 90.f; }
	else if (Style == TEXT("glass_c")) { Parent = Realism(TEXT("MI_GlassFacade"), EArenaSurface::Facade); Tint = FLinearColor(0.7f, 0.82f, 0.9f); OutNightNits = 110.f; }
	else if (Style == TEXT("office"))  { Parent = Realism(TEXT("MI_OfficeFacade"), EArenaSurface::Facade); Tint = FLinearColor(0.92f, 0.9f, 0.86f); OutNightNits = 160.f; }
	else if (Style == TEXT("panel"))   { Parent = Pick(TEXT("Facade"), nullptr, EArenaSurface::Facade); Tint = FLinearColor(0.9f, 0.9f, 0.9f); OutNightNits = 120.f; }
	else if (Style == TEXT("steel"))   { Parent = Pick(TEXT("Metal"), nullptr, EArenaSurface::Steel); Tint = FLinearColor(0.62f, 0.64f, 0.68f); }
	else if (Style == TEXT("concrete")){ Parent = Pick(nullptr, TEXT("MI_Env_ConcreteTiles"), EArenaSurface::Concrete); Tint = FLinearColor(0.9f, 0.88f, 0.84f); }
	else if (Style == TEXT("brick"))   { Parent = Pick(TEXT("Brick"), nullptr, EArenaSurface::RedBrick); Tint = FLinearColor(0.92f, 0.86f, 0.8f); }
	else if (Style.StartsWith(TEXT("pastel")))
	{
		const int32 i = FMath::Clamp(FCString::Atoi(*Style.RightChop(6)), 0, int32(UE_ARRAY_COUNT(Pastels)) - 1);
		Parent = Pick(nullptr, TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete); Tint = Pastels[i];
	}
	else { Parent = ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Facade); OutNightNits = 120.f; }
	UMaterialInstanceDynamic* M = Mid(TEXT("fac_") + Style, Parent, Tint);
	if (M && OutNightNits > 0.f)
	{
		bool bKnown = false;
		for (const auto& P : NightFacades) { if (P.Key.Get() == M) { bKnown = true; } }
		if (!bKnown) { NightFacades.Add(TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, float>(M, OutNightNits)); }
	}
	return M;
}

// ============================================================================================ build

void ADowntownDistrict::Build()
{
	if (bBuilt) { return; }
	bBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	LocalBounds = FBox(FVector(-HalfExtent, -HalfExtent, 0.f), FVector(HalfExtent, HalfExtent, 30000.f));
	BuildGround();
	BuildStreets();
	BuildBlocks();
	BuildStreetFurniture();
	BuildCars();
	BuildAirstrip();
	FlushBatches();
	BuildHouses();

	// Street-light pool: 24 real lights that follow the camera at night (the rest are emissive heads only).
	for (int32 i = 0; i < 24; ++i)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(14000.f);
		L->SetAttenuationRadius(2600.f);
		L->SetUseTemperature(true);
		L->SetTemperature(3600.f); // warm LED / sodium mix
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		LightPool.Add(L);
	}
	SetNight(false, false);
	UE_LOG(LogIronManArena, Log, TEXT("Midtown built: %d buildings, %d HISMs, %d lamps, %d parked cars, %.2f s."),
		NumBuildings, Hisms.Num(), LampHeads.Num(), ParkedCars.Num(), FPlatformTime::Seconds() - T0);
}

void ADowntownDistrict::BuildGround()
{
	const float H = HalfExtent;
	FTri Asphalt, Sand;
	// One asphalt sheet under the whole grid (blocks and pavements sit on top). On the Harbor ground (z 0) it is
	// lifted 3 cm; without the harbor it is the ground itself and runs out 600 m.
	const float Pad = bHarbor ? 0.f : 60000.f;
	Asphalt.Quad(FVector(-H - Pad, -H - Pad, 3), FVector(H + Pad, -H - Pad, 3), FVector(H + Pad, H, 3), FVector(-H - Pad, H, 3), FVector::UpVector);
	if (UProceduralMeshComponent* P = MakeProc(TEXT("Asphalt"), true))
	{
		P->CreateMeshSection(0, Asphalt.V, Asphalt.I, Asphalt.N, Asphalt.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		UMaterialInstanceDynamic* M = Mid(TEXT("asphalt"), Pick(TEXT("Asphalt"), nullptr, EArenaSurface::Asphalt), FLinearColor(0.62f, 0.62f, 0.64f));
		P->SetMaterial(0, M);
		P->SetCastShadow(false);
		WetMids.Add(M);
	}
	// Beach + lagoon on the south edge (Florida coast look): sand strip sloping to the water line.
	const float B0 = H, B1 = H + 9000.f;
	Sand.Quad(FVector(-H - 8000, B0, 12), FVector(H + 8000, B0, 12), FVector(H + 8000, B1, 2), FVector(-H - 8000, B1, 2), FVector::UpVector);
	if (UProceduralMeshComponent* P = MakeProc(TEXT("Beach"), true))
	{
		P->CreateMeshSection(0, Sand.V, Sand.I, Sand.N, Sand.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		P->SetMaterial(0, Mid(TEXT("sand"), Pick(nullptr, TEXT("MI_Env_Ground"), EArenaSurface::Gravel), FLinearColor(0.95f, 0.85f, 0.65f)));
		P->SetCastShadow(false);
	}
	FTri Water;
	Water.Quad(FVector(-H - 60000, B1 - 500, 6), FVector(H + 60000, B1 - 500, 6), FVector(H + 60000, B1 + 160000, 6), FVector(-H - 60000, B1 + 160000, 6), FVector::UpVector);
	if (UProceduralMeshComponent* P = MakeProc(TEXT("Lagoon"), false))
	{
		P->CreateMeshSection(0, Water.V, Water.I, Water.N, Water.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		// Shallow turquoise water: wet, near-mirror asphalt-rain material tinted teal reads as water under Lumen.
		P->SetMaterial(0, Mid(TEXT("lagoon"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::AsphaltRain), FLinearColor(0.03f, 0.16f, 0.17f)));
		P->SetCastShadow(false);
	}
	LocalBounds += FVector(0.f, B1, 0.f);
}

void ADowntownDistrict::BuildStreets()
{
	UMaterialInterface* Paint = Mid(TEXT("paint_white"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.75f, 0.75f, 0.72f));
	UMaterialInterface* Yellow = Mid(TEXT("paint_yellow"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.75f, 0.55f, 0.08f));
	UMaterialInterface* Puddle = Mid(TEXT("puddle"), Pick(TEXT("AsphaltWet"), nullptr, EArenaSurface::AsphaltRain), FLinearColor(0.35f, 0.35f, 0.37f));
	if (UMaterialInstanceDynamic* PM = Cast<UMaterialInstanceDynamic>(Puddle)) { PM->SetScalarParameterValue(TEXT("Wetness"), 1.f); }
	FRandomStream Rng(20261105);
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		for (int32 i = 0; i <= NumBlocks; ++i)
		{
			const float C = -HalfExtent + i * Pitch;
			for (int32 b = 0; b < NumBlocks; ++b)
			{
				// Segment between intersections b and b+1.
				const float S0 = -HalfExtent + b * Pitch + RoadWidth * 0.5f + WalkWidth;
				const float S1 = S0 + BlockSize;
				for (float t = S0 + 300.f; t < S1 - 300.f; t += 900.f)
				{
					const float Mid2 = t + 225.f;
					const FVector P = Axis == 0 ? FVector(Mid2, C, 4.f) : FVector(C, Mid2, 4.f);
					const FVector Sz = Axis == 0 ? FVector(450.f, 14.f, 1.f) : FVector(14.f, 450.f, 1.f);
					Add(TEXT("LaneYellow"), EArenaShape::Cube, Yellow, false, PropCull * 2.f, false, P, Sz);
				}
				// Edge lines.
				for (const float Off : { -RoadWidth * 0.5f + 40.f, RoadWidth * 0.5f - 40.f })
				{
					const FVector P = Axis == 0 ? FVector((S0 + S1) * 0.5f, C + Off, 4.f) : FVector(C + Off, (S0 + S1) * 0.5f, 4.f);
					const FVector Sz = Axis == 0 ? FVector(BlockSize, 12.f, 1.f) : FVector(12.f, BlockSize, 1.f);
					Add(TEXT("LaneWhite"), EArenaShape::Cube, Paint, false, PropCull * 2.f, false, P, Sz);
				}
				// Wet patches (roughness variation): puddles near the kerbs.
				for (int32 k = 0; k < 3; ++k)
				{
					if (Rng.FRand() > 0.6f) { continue; }
					const float Along = Rng.FRandRange(S0, S1);
					const float Across = Rng.FRandRange(-RoadWidth * 0.45f, RoadWidth * 0.45f);
					const FVector P = Axis == 0 ? FVector(Along, C + Across, 3.6f) : FVector(C + Across, Along, 3.6f);
					Add(TEXT("Puddle"), EArenaShape::Cylinder, Puddle, false, PropCull, false, P,
						FVector(Rng.FRandRange(150.f, 600.f), Rng.FRandRange(120.f, 400.f), 1.f), FRotator(0.f, Rng.FRandRange(0.f, 180.f), 0.f));
				}
			}
		}
	}
	// Crosswalks on every intersection approach.
	for (int32 i = 0; i <= NumBlocks; ++i)
	{
		for (int32 j = 0; j <= NumBlocks; ++j)
		{
			const FVector X(-HalfExtent + i * Pitch, -HalfExtent + j * Pitch, 4.2f);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const FRotator R(0.f, Side * 90.f, 0.f);
				const FVector Out = R.RotateVector(FVector(RoadWidth * 0.5f + 250.f, 0, 0));
				const FVector Along = R.RotateVector(FVector(0, 1, 0));
				for (int32 s = -3; s <= 3; ++s)
				{
					Add(TEXT("Zebra"), EArenaShape::Cube, Paint, false, PropCull, false, X + Out + Along * (s * 190.f), FVector(380.f, 90.f, 1.f), R);
				}
			}
		}
	}
}

void ADowntownDistrict::BuildBlocks()
{
	UMaterialInstanceDynamic* Walk = Mid(TEXT("walk"), Pick(TEXT("Sidewalk"), TEXT("MI_Env_Pavement"), EArenaSurface::Concrete), FLinearColor(0.92f, 0.9f, 0.86f));
	UMaterialInstanceDynamic* Curb = Mid(TEXT("curb"), Pick(TEXT("Curb"), nullptr, EArenaSurface::Concrete), FLinearColor(0.8f, 0.8f, 0.78f));
	UMaterialInstanceDynamic* Grass = Mid(TEXT("grass"), Pick(nullptr, TEXT("MI_Env_Ground"), EArenaSurface::DebrisGround), FLinearColor(0.38f, 0.6f, 0.28f));
	UMaterialInterface* Tactile = Pick(nullptr, TEXT("MI_Env_Tactile"), EArenaSurface::Concrete);
	WetMids.Add(Walk);
	FTri WalkBuf, CurbBuf, GrassBuf;
	FRandomStream Rng(77);

	const int32 Mid0 = NumBlocks / 2;
	// Reserved blocks: penthouse tower (centre), casino (Mid0+1, Mid0), shops, loft (east edge), dealership.
	for (int32 i = 0; i < NumBlocks; ++i)
	{
		for (int32 j = 0; j < NumBlocks; ++j)
		{
			const FVector2D C(-HalfExtent + (i + 0.5f) * Pitch, -HalfExtent + (j + 0.5f) * Pitch);
			const float HalfB = BlockSize * 0.5f + WalkWidth;
			// Pavement slab (top +18) with a curb ring.
			WalkBuf.Box(FVector(C.X - HalfB, C.Y - HalfB, 0.f), FVector(C.X + HalfB, C.Y + HalfB, 18.f));
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const bool bX = (Side % 2) == 0;
				const float Sgn = Side < 2 ? -1.f : 1.f;
				const FVector Mn = bX ? FVector(C.X - HalfB, C.Y + Sgn * HalfB - 15.f, 0.f) : FVector(C.X + Sgn * HalfB - 15.f, C.Y - HalfB, 0.f);
				const FVector Mx = bX ? FVector(C.X + HalfB, C.Y + Sgn * HalfB + 15.f, 20.f) : FVector(C.X + Sgn * HalfB + 15.f, C.Y + HalfB, 20.f);
				CurbBuf.Box(Mn, Mx);
			}
			// Tactile paving on the corners.
			for (const FVector2D& K : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) })
			{
				Add(TEXT("Tactile"), EArenaShape::Cube, Tactile, false, PropCull, false,
					FVector(C.X + K.X * (HalfB - 120.f), C.Y + K.Y * (HalfB - 120.f), 18.5f), FVector(160.f, 160.f, 1.f));
			}

			const float DistBlocks = FVector2D(i - Mid0, j - Mid0).Size();
			if (i == Mid0 && j == Mid0) { ReservedLots.Add(C); continue; }                           // penthouse tower
			if (i == Mid0 + 1 && j == Mid0) { BuildCasino(C); continue; }
			if (i == Mid0 - 1 && j == Mid0 + 1) { BuildShop(C, TEXT("barber"), TEXT("Sharp Cuts Barber"), Neons[1]); continue; }
			if (i == Mid0 + 1 && j == Mid0 + 1) { BuildShop(C, TEXT("clothing"), TEXT("Stark Street Clothing"), Neons[0]); continue; }
			if (i == Mid0 - 1 && j == Mid0) { BuildShop(C, TEXT("dealership"), TEXT("Legacy Motors Dealership"), Neons[3]); continue; }
			if (i == NumBlocks - 1 && j == Mid0) { ReservedLots.Add(C); }                            // Harbor Loft: west half stays free
			// Parks (palms, lawn, fountain) on a few blocks.
			if ((i + 2 * j) % 11 == 3 && DistBlocks > 1.5f)
			{
				GrassBuf.Box(FVector(C.X - BlockSize * 0.5f, C.Y - BlockSize * 0.5f, 18.f), FVector(C.X + BlockSize * 0.5f, C.Y + BlockSize * 0.5f, 26.f));
				for (int32 t = 0; t < 14; ++t)
				{
					const FVector2D P = C + FVector2D(Rng.FRandRange(-6500.f, 6500.f), Rng.FRandRange(-6500.f, 6500.f));
					// palm
					const float Hgt = Rng.FRandRange(700.f, 1300.f);
					const float Lean = Rng.FRandRange(-12.f, 12.f);
					UMaterialInterface* Trunk = Mid(TEXT("palm_trunk"), Pick(nullptr, nullptr, EArenaSurface::RuinedWall), FLinearColor(0.45f, 0.36f, 0.27f));
					UMaterialInterface* Frond = Mid(TEXT("palm_frond"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.08f, 0.24f, 0.06f));
					const FVector Base(P.X, P.Y, 26.f);
					const FRotator TR(Lean, Rng.FRandRange(0.f, 360.f), 0.f);
					const FVector Top = Base + TR.RotateVector(FVector(0, 0, Hgt));
					Add(TEXT("PalmTrunk"), EArenaShape::Cylinder, Trunk, false, PropCull * 1.5f, true, (Base + Top) * 0.5f, FVector(32.f, 32.f, Hgt), TR);
					for (int32 f = 0; f < 7; ++f)
					{
						const FRotator FR(-25.f - Rng.FRandRange(0.f, 20.f), f * (360.f / 7.f) + Rng.FRandRange(-10.f, 10.f), 0.f);
						Add(TEXT("PalmFrond"), EArenaShape::Cube, Frond, false, PropCull * 1.5f, true, Top + FR.RotateVector(FVector(220.f, 0, 0)), FVector(460.f, 90.f, 6.f), FR);
					}
				}
				// Fountain
				Add(TEXT("Fountain"), EArenaShape::Cylinder, Curb, true, PropCull, true, FVector(C.X, C.Y, 60.f), FVector(1200.f, 1200.f, 70.f));
				Add(TEXT("FountainWater"), EArenaShape::Cylinder, Mid(TEXT("fountainwater"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::AsphaltRain), FLinearColor(0.05f, 0.2f, 0.22f)),
					false, PropCull, false, FVector(C.X, C.Y, 92.f), FVector(1120.f, 1120.f, 4.f));
				continue;
			}
			// Lots: split the block into 1-3 x 1-3 lots.
			const int32 Nx = DistBlocks < 1.6f ? 1 + Rng.RandRange(0, 1) : 2 + Rng.RandRange(0, 1);
			const int32 Ny = DistBlocks < 1.6f ? 1 + Rng.RandRange(0, 1) : 2 + Rng.RandRange(0, 1);
			const float LotW = BlockSize / Nx, LotD = BlockSize / Ny;
			for (int32 a = 0; a < Nx; ++a)
			{
				for (int32 b = 0; b < Ny; ++b)
				{
					const FVector2D L(C.X - BlockSize * 0.5f + (a + 0.5f) * LotW, C.Y - BlockSize * 0.5f + (b + 0.5f) * LotD);
					bool bReserved = false;
					for (const FVector2D& R : ReservedLots) { if (FVector2D::Distance(R, C) < 1.f && L.X < C.X) { bReserved = true; } }
					if (bReserved) { continue; }
					const float Setback = 250.f;
					BuildBuilding(Rng, L, FVector2D(LotW - 2.f * Setback, LotD - 2.f * Setback) * Rng.FRandRange(0.8f, 1.f), DistBlocks);
				}
			}
		}
	}
	auto MakeSec = [this](FTri& Buf, const TCHAR* Key, UMaterialInterface* M, bool bShadow)
	{
		if (Buf.I.Num() == 0) { return; }
		UProceduralMeshComponent* P = MakeProc(Key, true);
		P->CreateMeshSection(0, Buf.V, Buf.I, Buf.N, Buf.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		P->SetMaterial(0, M);
		P->SetCastShadow(bShadow);
	};
	MakeSec(WalkBuf, TEXT("Pavements"), Walk, false);
	MakeSec(CurbBuf, TEXT("Curbs"), Curb, false);
	MakeSec(GrassBuf, TEXT("Parks"), Grass, false);
}

void ADowntownDistrict::BuildBuilding(FRandomStream& Rng, const FVector2D& Center, const FVector2D& Size, float DistBlocks)
{
	++NumBuildings;
	// Height by distance from the core: glass towers in the middle, pastel deco low-rise at the edges.
	const float Core = FMath::Clamp(1.f - DistBlocks / 4.5f, 0.f, 1.f);
	const bool bTower = Rng.FRand() < 0.15f + Core * 0.8f;
	const float Height = bTower ? FMath::Lerp(6000.f, 26000.f, FMath::Pow(Rng.FRand(), 1.6f) * (0.4f + Core * 0.6f))
		: Rng.FRandRange(900.f, 3200.f);
	FString Style;
	if (bTower)
	{
		static const TCHAR* TowerStyles[] = { TEXT("glass_a"), TEXT("glass_b"), TEXT("glass_c"), TEXT("office"), TEXT("panel"), TEXT("steel") };
		Style = TowerStyles[Rng.RandRange(0, 5)];
	}
	else
	{
		const float R = Rng.FRand();
		Style = R < 0.6f ? FString::Printf(TEXT("pastel%d"), Rng.RandRange(0, 5)) : (R < 0.8f ? FString(TEXT("concrete")) : FString(TEXT("brick")));
	}
	float NightNits = 0.f;
	UMaterialInterface* Fac = FacadeMaterial(Style, NightNits);
	const FString BodyKey = TEXT("Bld|") + Style + (Height > 2500.f ? TEXT("|tall") : TEXT("|low"));
	const float Cull = Height > 2500.f ? 0.f : LowCull;

	UMaterialInterface* Trim = Mid(TEXT("trim"), Pick(TEXT("Curb"), TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete), FLinearColor(0.9f, 0.9f, 0.88f));
	UMaterialInterface* Roof = Mid(TEXT("roof"), Pick(TEXT("Roof"), nullptr, EArenaSurface::RoofGravel), FLinearColor(0.7f, 0.7f, 0.68f));
	UMaterialInterface* Dark = Mid(TEXT("windowband"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.03f, 0.04f, 0.05f));
	UMaterialInterface* Store = Mid(TEXT("storefront"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.05f, 0.06f, 0.07f));
	UMaterialInterface* Metal = Mid(TEXT("roofmetal"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.5f, 0.52f, 0.55f));

	const float Base = 18.f;
	// Podium (towers) with storefront glass all round.
	float Z = Base;
	FVector2D Fp = Size;
	if (bTower)
	{
		const float PodH = Rng.FRandRange(900.f, 1600.f);
		Add(TEXT("Bld|concrete|low"), EArenaShape::Cube, FacadeMaterial(TEXT("concrete"), NightNits), true, LowCull, true, FVector(Center.X, Center.Y, Z + PodH * 0.5f), FVector(Size.X, Size.Y, PodH));
		Add(TEXT("Storefront"), EArenaShape::Cube, Store, false, LowCull, false, FVector(Center.X, Center.Y, Z + 230.f), FVector(Size.X + 12.f, Size.Y + 12.f, 380.f));
		Z += PodH;
		Fp = Size * Rng.FRandRange(0.6f, 0.85f);
	}
	// Tiers.
	const int32 NumTiers = bTower ? Rng.RandRange(1, 3) : 1;
	const float Remaining = FMath::Max(400.f, Height - (Z - Base));
	for (int32 t = 0; t < NumTiers; ++t)
	{
		const float TierH = Remaining / NumTiers;
		const FVector C(Center.X, Center.Y, Z + TierH * 0.5f);
		Add(BodyKey, EArenaShape::Cube, Fac, true, Cull, true, C, FVector(Fp.X, Fp.Y, TierH));
		// Ribbon windows on the opaque styles (glass styles carry their own grid).
		if (NightNits <= 0.f)
		{
			const float Floor = 350.f;
			for (float fz = Z + 150.f; fz < Z + TierH - 150.f; fz += Floor)
			{
				Add(TEXT("WindowBand"), EArenaShape::Cube, Dark, false, Cull > 0.f ? Cull : WinCull, false, FVector(Center.X, Center.Y, fz + 120.f), FVector(Fp.X + 8.f, Fp.Y - 120.f, 150.f));
				Add(TEXT("WindowBand"), EArenaShape::Cube, Dark, false, Cull > 0.f ? Cull : WinCull, false, FVector(Center.X, Center.Y, fz + 120.f), FVector(Fp.X - 120.f, Fp.Y + 8.f, 150.f));
			}
		}
		// Lit window strips at night (random floors, random faces, warm / cool / office mixes).
		const int32 NumLit = FMath::Clamp(int32(TierH / 600.f), 1, 10);
		for (int32 w = 0; w < NumLit; ++w)
		{
			const int32 Face = Rng.RandRange(0, 3);
			const float FloorZ = Z + 200.f + FMath::FloorToFloat(Rng.FRandRange(0.f, FMath::Max(1.f, TierH - 400.f)) / 350.f) * 350.f;
			const float Wd = Rng.FRandRange(300.f, FMath::Max(400.f, (Face % 2 ? Fp.Y : Fp.X) * 0.6f));
			const float Off = Rng.FRandRange(-0.3f, 0.3f) * (Face % 2 ? Fp.Y : Fp.X);
			FVector P, S;
			switch (Face)
			{
			case 0: P = FVector(Center.X + Off, Center.Y - Fp.Y * 0.5f - 6.f, FloorZ); S = FVector(Wd, 4.f, 210.f); break;
			case 1: P = FVector(Center.X + Fp.X * 0.5f + 6.f, Center.Y + Off, FloorZ); S = FVector(4.f, Wd, 210.f); break;
			case 2: P = FVector(Center.X + Off, Center.Y + Fp.Y * 0.5f + 6.f, FloorZ); S = FVector(Wd, 4.f, 210.f); break;
			default: P = FVector(Center.X - Fp.X * 0.5f - 6.f, Center.Y + Off, FloorZ); S = FVector(4.f, Wd, 210.f); break;
			}
			static const TCHAR* LitKeys[] = { TEXT("WinLitWarm"), TEXT("WinLitCool"), TEXT("WinLitOffice") };
			static const FLinearColor LitCol[] = { FLinearColor(1.f, 0.72f, 0.42f), FLinearColor(0.75f, 0.85f, 1.f), FLinearColor(0.95f, 0.95f, 0.88f) };
			const int32 K = Rng.RandRange(0, 2);
			Add(LitKeys[K], EArenaShape::Cube, Mid(LitKeys[K], ArenaVisuals::GetParentMaterial(), LitCol[K], 0.2f), false, WinCull * 1.5f, false, P, S);
			NightOnlyKeys.AddUnique(LitKeys[K]);
		}
		Z += TierH;
		if (t + 1 < NumTiers) { Fp *= Rng.FRandRange(0.7f, 0.88f); }
	}
	// Cornice + roof.
	Add(TEXT("Cornice"), EArenaShape::Cube, Trim, false, Cull, true, FVector(Center.X, Center.Y, Z + 25.f), FVector(Fp.X + 60.f, Fp.Y + 60.f, 50.f));
	Add(TEXT("Roof"), EArenaShape::Cube, Roof, true, Cull, false, FVector(Center.X, Center.Y, Z + 52.f), FVector(Fp.X - 40.f, Fp.Y - 40.f, 6.f));
	// Pastel deco low-rise: neon trim line + rounded corner tower.
	if (Style.StartsWith(TEXT("pastel")))
	{
		const int32 N = Rng.RandRange(0, 3);
		const FString NK = FString::Printf(TEXT("Neon%d"), N);
		UMaterialInterface* NeonM = NightMid(NK, Neons[N], 3.f);
		Add(NK, EArenaShape::Cube, NeonM, false, LowCull, false, FVector(Center.X, Center.Y, Z - 60.f), FVector(Fp.X + 20.f, Fp.Y + 20.f, 12.f));
			Add(TEXT("Bld|") + Style + TEXT("|low"), EArenaShape::Cylinder, Fac, true, LowCull, true,
			FVector(Center.X + Fp.X * 0.5f, Center.Y - Fp.Y * 0.5f, Base + (Z - Base + 300.f) * 0.5f), FVector(500.f, 500.f, Z - Base + 300.f));
		Add(TEXT("Storefront"), EArenaShape::Cube, Store, false, LowCull, false, FVector(Center.X, Center.Y, Base + 200.f), FVector(Fp.X + 12.f, Fp.Y + 12.f, 320.f));
		// Awnings
		Add(TEXT("Awning"), EArenaShape::Cube, Mid(FString::Printf(TEXT("awning%d"), N), ArenaVisuals::GetParentMaterial(), Neons[N] * 0.35f), false, PropCull, true,
			FVector(Center.X, Center.Y - Fp.Y * 0.5f - 90.f, Base + 380.f), FVector(Fp.X * 0.8f, 180.f, 10.f), FRotator(0.f, 0.f, -12.f));
	}
	// Rooftop clutter.
	const int32 NumAc = Rng.RandRange(1, 4);
	for (int32 a = 0; a < NumAc; ++a)
	{
		Add(TEXT("RoofAC"), EArenaShape::Cube, Metal, false, LowCull * 0.5f, true,
			FVector(Center.X + Rng.FRandRange(-0.35f, 0.35f) * Fp.X, Center.Y + Rng.FRandRange(-0.35f, 0.35f) * Fp.Y, Z + 110.f), FVector(220.f, 160.f, 120.f));
	}
	if (!bTower && Rng.FRand() < 0.35f)
	{
		const FVector T(Center.X - Fp.X * 0.25f, Center.Y + Fp.Y * 0.2f, Z + 50.f);
		Add(TEXT("TankLegs"), EArenaShape::Cube, Metal, false, LowCull * 0.5f, true, T + FVector(0, 0, 150.f), FVector(260.f, 260.f, 300.f));
		Add(TEXT("WaterTank"), EArenaShape::Cylinder, Mid(TEXT("tank"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::RustyMetal), FLinearColor(0.6f, 0.45f, 0.35f)),
			false, LowCull * 0.5f, true, T + FVector(0, 0, 450.f), FVector(320.f, 320.f, 320.f));
	}
	if (bTower && Height > 15000.f)
	{
		if (Rng.FRand() < 0.5f)
		{
			Add(TEXT("Antenna"), EArenaShape::Cylinder, Metal, false, 0.f, true, FVector(Center.X, Center.Y, Z + 1500.f), FVector(40.f, 40.f, 3000.f));
			Add(TEXT("AviationLight"), EArenaShape::Sphere, Mid(TEXT("avlight"), ArenaVisuals::GetParentMaterial(), FLinearColor(1.f, 0.05f, 0.02f), 6.f), false, 0.f, false,
				FVector(Center.X, Center.Y, Z + 3020.f), FVector(60.f));
		}
		else
		{
			// Helipad (vehicles can take off from here).
			Add(TEXT("Helipad"), EArenaShape::Cylinder, Mid(TEXT("helipad"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Asphalt), FLinearColor(0.2f, 0.2f, 0.22f)), true, 0.f, false,
				FVector(Center.X, Center.Y, Z + 70.f), FVector(FMath::Min(Fp.X, Fp.Y) * 0.8f, FMath::Min(Fp.X, Fp.Y) * 0.8f, 30.f));
			HelipadsLocal.Add(FVector(Center.X, Center.Y, Z + 90.f));
		}
	}
	// Billboards on mid-rise roofs (neon ads at night).
	if (!bTower && Rng.FRand() < 0.25f)
	{
		const int32 N = Rng.RandRange(0, 3);
		const FString BK = FString::Printf(TEXT("Billboard%d"), N);
		Add(TEXT("BillboardFrame"), EArenaShape::Cube, Metal, false, LowCull, true, FVector(Center.X, Center.Y, Z + 300.f), FVector(60.f, 60.f, 500.f));
		Add(BK, EArenaShape::Cube, NightMid(BK, Neons[N] * 0.6f + FLinearColor(0.2f, 0.2f, 0.2f), 0.6f), false, LowCull, true,
			FVector(Center.X, Center.Y, Z + 700.f), FVector(80.f, 1200.f, 450.f), FRotator(0.f, Rng.FRandRange(0.f, 180.f), 0.f));
	}
}

void ADowntownDistrict::BuildShop(const FVector2D& Center, FName Type, const FString& Name, const FLinearColor& Neon)
{
	// Two-storey pastel deco store with a big neon sign and glass front, door on the south side (+Y toward the street).
	FRandomStream Rng(GetTypeHash(Type));
	float Nits = 0.f;
	UMaterialInterface* Fac = FacadeMaterial(FString::Printf(TEXT("pastel%d"), Rng.RandRange(0, 5)), Nits);
	const FVector2D Sz(6000.f, 4200.f);
	const FVector C(Center.X, Center.Y - 2500.f, 18.f);
	Add(TEXT("Shop|body"), EArenaShape::Cube, Fac, true, LowCull, true, C + FVector(0, 0, 450.f), FVector(Sz.X, Sz.Y, 900.f));
	UMaterialInterface* Glass = Mid(TEXT("shopglass"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.08f, 0.1f, 0.12f), 0.05f);
	Add(TEXT("Storefront"), EArenaShape::Cube, Glass, false, LowCull, false, C + FVector(0, -Sz.Y * 0.5f - 6.f, 220.f), FVector(Sz.X * 0.8f, 10.f, 360.f));
	const FString NK = TEXT("ShopNeon_") + Type.ToString();
	Add(NK, EArenaShape::Cube, NightMid(NK, Neon, 4.f), false, LowCull, false, C + FVector(0, -Sz.Y * 0.5f - 30.f, 700.f), FVector(Sz.X * 0.6f, 20.f, 160.f));
	Add(TEXT("Awning"), EArenaShape::Cube, Mid(NK + TEXT("_aw"), ArenaVisuals::GetParentMaterial(), Neon * 0.3f), false, PropCull, true,
		C + FVector(0, -Sz.Y * 0.5f - 150.f, 420.f), FVector(Sz.X * 0.85f, 300.f, 10.f), FRotator(0.f, 0.f, -10.f));
	FCityPoi P;
	P.Type = Type;
	P.Name = Name;
	P.DoorLocal = C + FVector(0, -Sz.Y * 0.5f - 250.f, 100.f);
	P.Radius = 700.f;
	Pois.Add(P);
	++NumBuildings;
}

void ADowntownDistrict::BuildCasino(const FVector2D& Center)
{
	// "The Arc Royale": wide low hall with a gold crown, neon marquee, open glass front you can walk through.
	UMaterialInterface* Gold = Mid(TEXT("casino_gold"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::SuitGold), FLinearColor(0.85f, 0.6f, 0.2f));
	UMaterialInterface* Wall = Mid(TEXT("casino_wall"), Pick(nullptr, TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete), FLinearColor(0.95f, 0.92f, 0.88f));
	UMaterialInterface* Carpet = Mid(TEXT("casino_carpet"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.25f, 0.02f, 0.05f));
	UMaterialInterface* Felt = Mid(TEXT("casino_felt"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.02f, 0.18f, 0.06f));
	UMaterialInterface* Marquee = NightMid(TEXT("casino_marquee"), FLinearColor(1.f, 0.75f, 0.3f), 5.f);
	UMaterialInterface* Inner = Mid(TEXT("casino_inner"), ArenaVisuals::GetParentMaterial(), FLinearColor(1.f, 0.6f, 0.3f), 0.4f);
	const FVector C(Center.X, Center.Y, 18.f);
	const float W = 9000.f, D = 7000.f, H = 1400.f;
	// Walls: back + sides + front with a 1600 cm opening.
	Add(TEXT("Casino|wall"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(0, D * 0.5f, H * 0.5f), FVector(W, 60.f, H));
	Add(TEXT("Casino|wall"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(-W * 0.5f, 0, H * 0.5f), FVector(60.f, D, H));
	Add(TEXT("Casino|wall"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(W * 0.5f, 0, H * 0.5f), FVector(60.f, D, H));
	Add(TEXT("Casino|wall"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(-W * 0.3f - 400.f, -D * 0.5f, H * 0.5f), FVector(W * 0.4f - 800.f, 60.f, H));
	Add(TEXT("Casino|wall"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(W * 0.3f + 400.f, -D * 0.5f, H * 0.5f), FVector(W * 0.4f - 800.f, 60.f, H));
	Add(TEXT("Casino|wall"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(0, -D * 0.5f, H - 200.f), FVector(1700.f, 60.f, 400.f));
	Add(TEXT("Casino|roof"), EArenaShape::Cube, Wall, true, LowCull, true, C + FVector(0, 0, H + 30.f), FVector(W + 100.f, D + 100.f, 60.f));
	Add(TEXT("Casino|crown"), EArenaShape::Cylinder, Gold, true, 0.f, true, C + FVector(0, 0, H + 600.f), FVector(3000.f, 3000.f, 1100.f));
	Add(TEXT("Casino|marquee"), EArenaShape::Cube, Marquee, false, 0.f, false, C + FVector(0, -D * 0.5f - 60.f, H + 120.f), FVector(W * 0.7f, 30.f, 220.f));
	Add(TEXT("Casino|carpet"), EArenaShape::Cube, Carpet, false, LowCull, false, C + FVector(0, 0, 3.f), FVector(W - 100.f, D - 100.f, 4.f));
	Add(TEXT("Casino|ceiling"), EArenaShape::Cube, Inner, false, LowCull, false, C + FVector(0, 0, H - 40.f), FVector(W - 300.f, D - 300.f, 10.f));
	// Stations: slots row, blackjack, roulette, wheel.
	UMaterialInterface* SlotM = NightMid(TEXT("casino_slot"), FLinearColor(0.9f, 0.2f, 0.5f), 1.f);
	for (int32 s = 0; s < 6; ++s)
	{
		Add(TEXT("Casino|slot"), EArenaShape::Cube, SlotM, true, PropCull, true, C + FVector(-3200.f + s * 260.f, 2800.f, 100.f), FVector(160.f, 120.f, 200.f));
	}
	Add(TEXT("Casino|table"), EArenaShape::Cylinder, Felt, true, PropCull, true, C + FVector(-1200.f, 0.f, 50.f), FVector(500.f, 300.f, 100.f));   // blackjack
	Add(TEXT("Casino|table"), EArenaShape::Cylinder, Felt, true, PropCull, true, C + FVector(1500.f, 0.f, 50.f), FVector(400.f, 400.f, 100.f));   // roulette
	Add(TEXT("Casino|wheel"), EArenaShape::Cylinder, Mid(TEXT("casino_wheelface"), ArenaVisuals::GetParentMaterial(), FLinearColor(1.f, 0.8f, 0.2f), 1.5f), false, PropCull, false,
		C + FVector(3200.f, 3000.f, 500.f), FVector(700.f, 700.f, 40.f), FRotator(90.f, 0.f, 0.f));
	const struct { ECasinoGame G; FVector L; const TCHAR* N; } St[] = {
		{ ECasinoGame::Slots, FVector(-2550.f, 2500.f, 100.f), TEXT("Slot machines") },
		{ ECasinoGame::Blackjack, FVector(-1200.f, -500.f, 100.f), TEXT("Blackjack table") },
		{ ECasinoGame::Roulette, FVector(1500.f, -600.f, 100.f), TEXT("Roulette table") },
		{ ECasinoGame::Wheel, FVector(3200.f, 2500.f, 100.f), TEXT("Wheel of fortune") } };
	for (const auto& S : St)
	{
		FCityPoi P;
		P.Type = TEXT("casino_game");
		P.Name = S.N;
		P.DoorLocal = C + S.L;
		P.Radius = 450.f;
		P.Game = int32(S.G);
		Pois.Add(P);
	}
	FCityPoi Door;
	Door.Type = TEXT("casino");
	Door.Name = TEXT("The Arc Royale Casino");
	Door.DoorLocal = C + FVector(0.f, -D * 0.5f - 300.f, 100.f);
	Door.Radius = 0.f; // marker only: walk in to the stations
	Pois.Add(Door);
	++NumBuildings;
}

void ADowntownDistrict::BuildAirstrip()
{
	// Airstrip on the north edge (-Y), 1.4 km runway with edge lights and a hangar; planes spawn at its start.
	const float Y = -HalfExtent - 9000.f;
	UMaterialInterface* Run = Mid(TEXT("runway"), Pick(TEXT("Asphalt"), nullptr, EArenaSurface::Asphalt), FLinearColor(0.35f, 0.35f, 0.37f));
	UMaterialInterface* Paint = Mid(TEXT("paint_white"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.75f, 0.75f, 0.72f));
	UMaterialInterface* EdgeL = NightMid(TEXT("runwaylight"), FLinearColor(1.f, 0.85f, 0.5f), 3.f);
	const float L = 140000.f;
	Add(TEXT("Runway"), EArenaShape::Cube, Run, true, 0.f, false, FVector(0.f, Y, 6.f), FVector(L, 4500.f, 8.f));
	for (float x = -L * 0.5f + 2000.f; x < L * 0.5f; x += 6000.f)
	{
		Add(TEXT("RunwayStripe"), EArenaShape::Cube, Paint, false, PropCull * 3.f, false, FVector(x, Y, 10.5f), FVector(3000.f, 90.f, 1.f));
	}
	for (float x = -L * 0.5f; x <= L * 0.5f; x += 3000.f)
	{
		for (const float s : { -1.f, 1.f })
		{
			Add(TEXT("RunwayLights"), EArenaShape::Sphere, EdgeL, false, PropCull * 3.f, false, FVector(x, Y + s * 2300.f, 25.f), FVector(35.f));
		}
	}
	Add(TEXT("Hangar"), EArenaShape::Cube, Mid(TEXT("hangar"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.7f, 0.72f, 0.75f)), true, LowCull, true,
		FVector(-L * 0.5f + 5000.f, Y - 6000.f, 800.f), FVector(6000.f, 4000.f, 1600.f));
	AirstripLocal = FVector(-L * 0.5f + 3000.f, Y, 120.f);
	AirstripYaw = 0.f;
	FCityPoi P;
	P.Type = TEXT("airstrip");
	P.Name = TEXT("Midtown Airstrip");
	P.DoorLocal = AirstripLocal;
	P.Radius = 0.f;
	Pois.Add(P);
	LocalBounds += FVector(0.f, Y - 8000.f, 0.f);
}

void ADowntownDistrict::BuildStreetFurniture()
{
	UMaterialInterface* Pole = Mid(TEXT("pole"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.25f, 0.26f, 0.28f));
	UMaterialInterface* LampHead = NightMid(TEXT("lamphead"), FLinearColor(1.f, 0.82f, 0.55f), 6.f);
	UMaterialInterface* SigRed = Mid(TEXT("sig_red"), ArenaVisuals::GetParentMaterial(), FLinearColor(1.f, 0.05f, 0.02f), 4.f);
	UMaterialInterface* SigGreen = Mid(TEXT("sig_green"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.1f, 1.f, 0.4f), 4.f);
	UMaterialInterface* SigBox = Mid(TEXT("sig_box"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.02f, 0.02f, 0.02f));
	UMaterialInterface* SignGreen = Mid(TEXT("streetsign"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.02f, 0.2f, 0.07f));
	UMaterialInterface* Trunk = Mid(TEXT("palm_trunk"), Pick(nullptr, nullptr, EArenaSurface::RuinedWall), FLinearColor(0.45f, 0.36f, 0.27f));
	UMaterialInterface* Frond = Mid(TEXT("palm_frond"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.08f, 0.24f, 0.06f));
	UStaticMesh* KitTree = CityKit::Prop(TEXT("tree_small_02"));
	UStaticMesh* KitBench = CityKit::Prop(TEXT("painted_wooden_bench"));
	UStaticMesh* KitTrash = CityKit::Prop(TEXT("metal_trash_can"));
	UStaticMesh* Hydrant = CityKit::LoadMeshPath(TEXT("/Game/Env/Props/fire_hydrant/SM_fire_hydrant"));
	UStaticMesh* Utility = CityKit::LoadMeshPath(TEXT("/Game/Env/Props/utility_box_01/SM_utility_box_01"));
	if (!KitTrash) { KitTrash = CityKit::LoadMeshPath(TEXT("/Game/Env/Props/metal_trash_can/SM_metal_trash_can")); }
	if (!KitBench) { KitBench = CityKit::LoadMeshPath(TEXT("/Game/Env/Props/modular_street_seating/SM_modular_street_seating")); }
	TMap<UStaticMesh*, TArray<FTransform>> MeshProps;
	FRandomStream Rng(4242);

	const float KerbOff = RoadWidth * 0.5f + 80.f;     // from road centre to lamp line
	const float WalkMid = RoadWidth * 0.5f + WalkWidth * 0.5f;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		for (int32 i = 0; i <= NumBlocks; ++i)
		{
			const float C = -HalfExtent + i * Pitch;
			for (int32 b = 0; b < NumBlocks; ++b)
			{
				const float S0 = -HalfExtent + b * Pitch + RoadWidth * 0.5f + WalkWidth + 500.f;
				const float S1 = S0 + BlockSize - 1000.f;
				for (const float Side : { -1.f, 1.f })
				{
					int32 n = 0;
					for (float t = S0; t <= S1; t += 1500.f, ++n)
					{
						auto At = [Axis, C](float Along, float Across, float Z) { return Axis == 0 ? FVector(Along, C + Across, Z) : FVector(C + Across, Along, Z); };
						const float Yaw = Axis == 0 ? (Side > 0.f ? -90.f : 90.f) : (Side > 0.f ? 180.f : 0.f);
						if (n % 2 == 0)
						{
							// Street light: pole + arm over the road + emissive head.
							const FVector Base = At(t, Side * KerbOff, 18.f);
							Add(TEXT("LampPole"), EArenaShape::Cylinder, Pole, false, PropCull * 1.5f, true, Base + FVector(0, 0, 450.f), FVector(22.f, 22.f, 900.f));
							const FVector ArmC = At(t, Side * (KerbOff - 120.f), 905.f);
							Add(TEXT("LampArm"), EArenaShape::Cube, Pole, false, PropCull * 1.5f, true, ArmC, Axis == 0 ? FVector(14.f, 260.f, 14.f) : FVector(260.f, 14.f, 14.f));
							const FVector Head = At(t, Side * (KerbOff - 240.f), 895.f);
							Add(TEXT("LampHead"), EArenaShape::Cube, LampHead, false, PropCull * 3.f, false, Head, FVector(60.f, 60.f, 12.f));
							LampHeads.Add(GetActorTransform().TransformPosition(Head - FVector(0, 0, 30.f)));
						}
						else
						{
							// Palm (Florida streets) or a CityKit tree.
							const FVector Base = At(t, Side * (WalkMid + 40.f), 18.f);
							if (KitTree && Rng.FRand() < 0.3f)
							{
								MeshProps.FindOrAdd(KitTree).Add(FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Base, FVector(Rng.FRandRange(0.9f, 1.2f))));
							}
							else
							{
								const float Hgt = Rng.FRandRange(650.f, 1100.f);
								const FRotator TR(Rng.FRandRange(-8.f, 8.f), Rng.FRandRange(0.f, 360.f), 0.f);
								const FVector Top = Base + TR.RotateVector(FVector(0, 0, Hgt));
								Add(TEXT("PalmTrunk"), EArenaShape::Cylinder, Trunk, false, PropCull * 1.5f, true, (Base + Top) * 0.5f, FVector(30.f, 30.f, Hgt), TR);
								for (int32 f = 0; f < 7; ++f)
								{
									const FRotator FR(-25.f - Rng.FRandRange(0.f, 20.f), f * (360.f / 7.f) + Rng.FRandRange(-10.f, 10.f), 0.f);
									Add(TEXT("PalmFrond"), EArenaShape::Cube, Frond, false, PropCull * 1.5f, true, Top + FR.RotateVector(FVector(220.f, 0, 0)), FVector(460.f, 90.f, 6.f), FR);
								}
							}
						}
						// Dense small props.
						const float R = Rng.FRand();
						const FVector PropPos = At(t + 500.f, Side * (WalkMid + 120.f), 18.f);
						if (R < 0.12f && Hydrant) { MeshProps.FindOrAdd(Hydrant).Add(FTransform(FRotator(0.f, Yaw, 0.f), PropPos)); }
						else if (R < 0.24f && KitTrash) { MeshProps.FindOrAdd(KitTrash).Add(FTransform(FRotator(0.f, Yaw, 0.f), PropPos)); }
						else if (R < 0.34f && KitBench) { MeshProps.FindOrAdd(KitBench).Add(FTransform(FRotator(0.f, Yaw, 0.f), PropPos)); }
						else if (R < 0.4f && Utility) { MeshProps.FindOrAdd(Utility).Add(FTransform(FRotator(0.f, Yaw, 0.f), PropPos)); }
					}
				}
			}
		}
	}
	// Traffic signals + street-name signs on the intersections.
	for (int32 i = 0; i <= NumBlocks; ++i)
	{
		for (int32 j = 0; j <= NumBlocks; ++j)
		{
			const FVector X(-HalfExtent + i * Pitch, -HalfExtent + j * Pitch, 18.f);
			const FVector Corner = X + FVector(RoadWidth * 0.5f + 150.f, RoadWidth * 0.5f + 150.f, 0.f);
			Add(TEXT("SignalPole"), EArenaShape::Cylinder, Pole, false, PropCull * 1.5f, true, Corner + FVector(0, 0, 350.f), FVector(20.f, 20.f, 700.f));
			Add(TEXT("SignalArm"), EArenaShape::Cube, Pole, false, PropCull * 1.5f, true, Corner + FVector(-450.f, 0, 690.f), FVector(900.f, 12.f, 12.f));
			Add(TEXT("SignalBox"), EArenaShape::Cube, SigBox, false, PropCull * 1.5f, true, Corner + FVector(-800.f, 0, 620.f), FVector(40.f, 40.f, 120.f));
			const bool bRed = ((i + j) % 2) == 0;
			Add(bRed ? TEXT("SigRed") : TEXT("SigGreen"), EArenaShape::Sphere, bRed ? SigRed : SigGreen, false, PropCull * 2.f, false,
				Corner + FVector(-800.f, -24.f, bRed ? 655.f : 585.f), FVector(26.f));
			Add(TEXT("StreetSign"), EArenaShape::Cube, SignGreen, false, PropCull, true, Corner + FVector(0, 0, 560.f), FVector(260.f, 6.f, 50.f));
		}
	}
	// Real prop meshes (one HISM per mesh, no collision, no shadows from the tiny ones).
	int32 n = 0;
	for (TPair<UStaticMesh*, TArray<FTransform>>& P : MeshProps)
	{
		UHierarchicalInstancedStaticMeshComponent* H = Hism(FString::Printf(TEXT("Prop%d"), n++), P.Key, nullptr, false, PropCull, P.Key == KitTree);
		H->AddInstances(P.Value, false, false);
	}
}

void ADowntownDistrict::AddCarShapes(const FTransform& Xf, int32 Paint, TArray<TPair<FString, int32>>* OutInst)
{
	UMaterialInterface* Body = CarPaintMaterial(Paint);
	UMaterialInterface* Glass = Mid(TEXT("carglass"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.02f, 0.025f, 0.03f));
	UMaterialInterface* Tyre = Mid(TEXT("tyre"), ArenaVisuals::GetParentMaterial(), FLinearColor(0.015f, 0.015f, 0.015f));
	UMaterialInterface* Head = NightMid(TEXT("headlight"), FLinearColor(1.f, 0.95f, 0.85f), 0.5f);
	UMaterialInterface* Tail = Mid(TEXT("taillight"), ArenaVisuals::GetParentMaterial(), FLinearColor(1.f, 0.02f, 0.02f), 0.5f);
	auto Put = [&](const FString& Key, EArenaShape Shape, UMaterialInterface* M, bool bCol, const FVector& LocalC, const FVector& Size, const FRotator& R)
	{
		const FVector W = Xf.TransformPosition(LocalC);
		const FRotator Rot = (Xf.GetRotation() * R.Quaternion()).Rotator();
		const int32 Idx = Add(Key, Shape, M, bCol, CarCull, true, W, Size, Rot);
		if (OutInst) { OutInst->Add(TPair<FString, int32>(Key, Idx)); }
	};
	const FString BodyKey = FString::Printf(TEXT("CarBody%d"), Paint);
	Put(BodyKey, EArenaShape::Cube, Body, true, FVector(0, 0, 60.f), FVector(450.f, 185.f, 70.f), FRotator::ZeroRotator);
	Put(BodyKey, EArenaShape::Cube, Body, false, FVector(-30.f, 0, 112.f), FVector(230.f, 168.f, 40.f), FRotator::ZeroRotator);
	Put(TEXT("CarGlass"), EArenaShape::Cube, Glass, false, FVector(-30.f, 0, 135.f), FVector(220.f, 170.f, 30.f), FRotator::ZeroRotator);
	for (const FVector2D& W : { FVector2D(140, 82), FVector2D(140, -82), FVector2D(-140, 82), FVector2D(-140, -82) })
	{
		Put(TEXT("CarTyre"), EArenaShape::Cylinder, Tyre, false, FVector(W.X, W.Y, 33.f), FVector(66.f, 66.f, 24.f), FRotator(0.f, 0.f, 90.f));
	}
	Put(TEXT("CarHead"), EArenaShape::Cube, Head, false, FVector(226.f, 0, 70.f), FVector(4.f, 150.f, 14.f), FRotator::ZeroRotator);
	Put(TEXT("CarTail"), EArenaShape::Cube, Tail, false, FVector(-226.f, 0, 78.f), FVector(4.f, 150.f, 12.f), FRotator::ZeroRotator);
}

void ADowntownDistrict::BuildCars()
{
	FRandomStream Rng(555);
	const int32 NumPaints = CarPaints().Num();
	const float Lane = RoadWidth * 0.5f - 150.f; // parking lane centre from the road centre
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		for (int32 i = 0; i <= NumBlocks; ++i)
		{
			const float C = -HalfExtent + i * Pitch;
			for (int32 b = 0; b < NumBlocks; ++b)
			{
				const float S0 = -HalfExtent + b * Pitch + RoadWidth * 0.5f + WalkWidth + 600.f;
				const float S1 = S0 + BlockSize - 1200.f;
				for (const float Side : { -1.f, 1.f })
				{
					for (float t = S0; t < S1; t += Rng.FRandRange(620.f, 900.f))
					{
						if (Rng.FRand() > 0.45f) { continue; }
						const FVector P = Axis == 0 ? FVector(t, C + Side * Lane, 3.f) : FVector(C + Side * Lane, t, 3.f);
						const float Yaw = (Axis == 0 ? 0.f : 90.f) + (Side > 0.f ? 0.f : 180.f) + Rng.FRandRange(-2.f, 2.f);
						FParkedCar Car;
						Car.Xf = FTransform(FRotator(0.f, Yaw, 0.f), P);
						Car.Paint = Rng.RandRange(0, NumPaints - 1);
						AddCarShapes(Car.Xf, Car.Paint, &Car.Inst);
						ParkedCars.Add(MoveTemp(Car));
					}
				}
			}
		}
	}
}

void ADowntownDistrict::BuildHouses()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Me = GetActorTransform();
	auto Spawn = [&](FName Id, ECityHouseKind Kind, const FVector& Local, bool bWater)
	{
		ACityHouse* H = World->SpawnActor<ACityHouse>(ACityHouse::StaticClass(), FTransform(Me.TransformPosition(Local)), SP);
		if (H) { H->Setup(Id, Kind, bWater); Houses.Add(H); }
	};
	const FVector2D PentLot = ReservedLots.IsValidIndex(0) ? ReservedLots[0] : FVector2D::ZeroVector;
	const FVector2D LoftLot = ReservedLots.IsValidIndex(1) ? ReservedLots[1] : FVector2D(HalfExtent - Pitch * 0.5f, 0.f);
	Spawn(TEXT("skyline-penthouse"), ECityHouseKind::Penthouse, FVector(PentLot.X, PentLot.Y, 18.f), false);
	Spawn(TEXT("harbor-loft"), ECityHouseKind::Loft, FVector(LoftLot.X - 3600.f, LoftLot.Y, 18.f), false);
	// Cliff mansion: out in the harbor's sea, or on a bluff past the beach when there is no harbor.
	if (bHarbor)
	{
		// District origin is 1950 m west of the arena; the sea starts ~880 m east of the arena.
		Spawn(TEXT("malibu-mansion"), ECityHouseKind::Mansion, FVector(195000.f + 150000.f, 60000.f, -300.f), true);
		LocalBounds += FVector(195000.f + 160000.f, 70000.f, 0.f);
	}
	else
	{
		Spawn(TEXT("malibu-mansion"), ECityHouseKind::Mansion, FVector(HalfExtent + 20000.f, HalfExtent + 4000.f, 0.f), false);
		LocalBounds += FVector(HalfExtent + 30000.f, HalfExtent + 14000.f, 0.f);
	}
}

// ============================================================================================ runtime

FBox ADowntownDistrict::GetPlayBounds() const
{
	return LocalBounds.TransformBy(GetActorTransform());
}

ACityHouse* ADowntownDistrict::FindHouse(FName HouseId) const
{
	for (const TObjectPtr<ACityHouse>& H : Houses) { if (H && H->GetHouseId() == HouseId) { return H; } }
	return nullptr;
}

void ADowntownDistrict::SetDistrictVisible(bool bVisible)
{
	bShown = bVisible;
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
	for (const TObjectPtr<ACityHouse>& H : Houses) { if (H) { H->SetHouseVisible(bVisible); } }
	if (!bVisible) { for (UPointLightComponent* L : LightPool) { if (L) { L->SetVisibility(false); } } }
}

void ADowntownDistrict::SetNight(bool bNight, bool bWet)
{
	bNightOn = bNight;
	for (const FString& K : NightOnlyKeys)
	{
		if (const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* H = Hisms.Find(K)) { if (*H) { (*H)->SetVisibility(bNight); } }
	}
	for (const auto& P : NightEmissive)
	{
		if (UMaterialInstanceDynamic* M = P.Key.Get()) { M->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(bNight ? P.Value : 0.f)); }
	}
	for (const auto& P : NightFacades)
	{
		if (UMaterialInstanceDynamic* M = P.Key.Get()) { M->SetScalarParameterValue(TEXT("EmissiveStrength"), bNight ? P.Value : 0.f); }
	}
	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& W : WetMids)
	{
		if (UMaterialInstanceDynamic* M = W.Get()) { M->SetScalarParameterValue(TEXT("Wetness"), bWet ? 0.85f : 0.15f); }
	}
	for (const TObjectPtr<ACityHouse>& H : Houses) { if (H) { H->SetNight(bNight); } }
	if (!bNight) { for (UPointLightComponent* L : LightPool) { if (L) { L->SetVisibility(false); } } }
	PoolTimer = 0.f;
}

void ADowntownDistrict::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bNightOn || !bShown || LampHeads.Num() == 0) { return; }
	PoolTimer -= DeltaSeconds;
	if (PoolTimer > 0.f) { return; }
	PoolTimer = 0.3f;
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager) { return; }
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	// Nearest lamps within 220 m get a real light.
	TArray<TPair<float, int32>> Near;
	for (int32 i = 0; i < LampHeads.Num(); ++i)
	{
		const float D = FVector::DistSquared(Cam, LampHeads[i]);
		if (D < 22000.f * 22000.f) { Near.Add(TPair<float, int32>(D, i)); }
	}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 l = 0; l < LightPool.Num(); ++l)
	{
		UPointLightComponent* L = LightPool[l];
		if (!L) { continue; }
		if (Near.IsValidIndex(l))
		{
			L->SetWorldLocation(LampHeads[Near[l].Value]);
			L->SetVisibility(true);
		}
		else { L->SetVisibility(false); }
	}
}

bool ADowntownDistrict::TakeParkedCarNear(const FVector& WorldLoc, float Radius, FTransform& OutXf, int32& OutPaint)
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLoc);
	int32 Best = INDEX_NONE;
	float BestD = Radius * Radius;
	for (int32 i = 0; i < ParkedCars.Num(); ++i)
	{
		if (ParkedCars[i].bGone) { continue; }
		const float D = FVector::DistSquared(ParkedCars[i].Xf.GetLocation() + FVector(0, 0, 60.f), Local);
		if (D < BestD) { BestD = D; Best = i; }
	}
	if (Best == INDEX_NONE) { return false; }
	FParkedCar& Car = ParkedCars[Best];
	Car.bGone = true;
	for (const TPair<FString, int32>& I : Car.Inst)
	{
		if (const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* H = Hisms.Find(I.Key))
		{
			if (*H) { (*H)->UpdateInstanceTransform(I.Value, FTransform(FQuat::Identity, FVector(0, 0, -100000.f), FVector(0.001f)), false, true, true); }
		}
	}
	OutXf = Car.Xf * GetActorTransform();
	OutPaint = Car.Paint;
	return true;
}

bool ADowntownDistrict::RandomSidewalkPoint(FRandomStream& Rng, const FVector& Near, float MinDist, float MaxDist, FVector& Out) const
{
	const FVector LocalNear = GetActorTransform().InverseTransformPosition(Near);
	for (int32 Try = 0; Try < 24; ++Try)
	{
		const float Ang = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Rng.FRandRange(MinDist, MaxDist);
		FVector2D P(LocalNear.X + FMath::Cos(Ang) * R, LocalNear.Y + FMath::Sin(Ang) * R);
		if (FMath::Abs(P.X) > HalfExtent - 200.f || FMath::Abs(P.Y) > HalfExtent - 200.f) { continue; }
		// Snap to the nearest pavement strip: road line + (RoadWidth/2 + WalkWidth/2) on either side.
		const bool bAlongX = Rng.FRand() < 0.5f;
		double& Across = bAlongX ? P.Y : P.X;
		const float Line = float(FMath::RoundToDouble((Across + HalfExtent) / Pitch) * Pitch - HalfExtent);
		Across = Line + (Rng.FRand() < 0.5f ? -1.f : 1.f) * (RoadWidth * 0.5f + WalkWidth * 0.5f);
		if (FMath::Abs(Across) > HalfExtent) { continue; }
		Out = GetActorTransform().TransformPosition(FVector(P.X, P.Y, 18.f));
		return true;
	}
	return false;
}

FTransform ADowntownDistrict::GetRoadsideSpotNear(const FVector& WorldLoc) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(WorldLoc);
	const float Lx = FMath::Clamp(FMath::RoundToFloat((L.X + HalfExtent) / Pitch), 0.f, float(NumBlocks)) * Pitch - HalfExtent;
	const float Ly = FMath::Clamp(FMath::RoundToFloat((L.Y + HalfExtent) / Pitch), 0.f, float(NumBlocks)) * Pitch - HalfExtent;
	FVector Out;
	float Yaw;
	if (FMath::Abs(L.X - Lx) < FMath::Abs(L.Y - Ly))
	{
		// Nearest road runs along Y at X = Lx.
		Out = FVector(Lx + RoadWidth * 0.25f, FMath::Clamp(L.Y, -HalfExtent, HalfExtent), 80.f);
		Yaw = 90.f;
	}
	else
	{
		Out = FVector(FMath::Clamp(L.X, -HalfExtent, HalfExtent), Ly + RoadWidth * 0.25f, 80.f);
		Yaw = 0.f;
	}
	return FTransform(FRotator(0.f, Yaw, 0.f), GetActorTransform().TransformPosition(Out));
}
