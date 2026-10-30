#include "CityLayout.h"

#include "ArenaVisuals.h"
#include "ChapterSet.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityLayout, Log, All);

namespace
{
	struct FCitySpawn
	{
		FString Id;
		ECitySpawnType Type = ECitySpawnType::Drone;
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		FString PickupType;
	};

	struct FCityData
	{
		bool bTried = false;
		TSharedPtr<FJsonObject> Root;
		TArray<FCitySpawn> Spawns;
		TMap<FString, FVector2D> Nodes;
	};

	const TArray<TSharedPtr<FJsonValue>>& Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		static const TArray<TSharedPtr<FJsonValue>> Empty;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return (O.IsValid() && O->TryGetArrayField(Key, A) && A) ? *A : Empty;
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		return (O.IsValid() && O->TryGetObjectField(Key, P) && P) ? *P : nullptr;
	}

	TSharedPtr<FJsonObject> AsObj(const TSharedPtr<FJsonValue>& V)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		return (V.IsValid() && V->TryGetObject(P) && P) ? *P : nullptr;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = 0.0)
	{
		double V = Default;
		if (O.IsValid() && O->HasTypedField<EJson::Number>(Key)) { O->TryGetNumberField(Key, V); }
		return V;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString V;
		if (O.IsValid()) { O->TryGetStringField(Key, V); }
		return V;
	}

	TArray<FVector2D> Poly(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		TArray<FVector2D> Out;
		for (const TSharedPtr<FJsonValue>& V : A)
		{
			const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
			if (V.IsValid() && V->TryGetArray(XY) && XY && XY->Num() >= 2) { Out.Add(FVector2D((*XY)[0]->AsNumber(), (*XY)[1]->AsNumber())); }
		}
		return Out;
	}

	FCityData& Data()
	{
		static FCityData D;
		if (D.bTried) { return D; }
		D.bTried = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("NoCityLayout"))) { return D; }
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("City/city_layout.json");
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), D.Root) || !D.Root.IsValid())
		{
			UE_LOG(LogCityLayout, Warning, TEXT("city_layout.json missing or invalid (%s): free play keeps the old grid."), *Path);
			D.Root.Reset();
			return D;
		}
		for (const TSharedPtr<FJsonValue>& V : Arr(D.Root, TEXT("nodes")))
		{
			const TSharedPtr<FJsonObject> N = AsObj(V);
			if (N.IsValid()) { D.Nodes.Add(Str(N, TEXT("id")), FVector2D(Num(N, TEXT("x")), Num(N, TEXT("y")))); }
		}
		for (const TSharedPtr<FJsonValue>& V : Arr(D.Root, TEXT("spawns")))
		{
			const TSharedPtr<FJsonObject> S = AsObj(V);
			if (!S.IsValid()) { continue; }
			FCitySpawn Sp;
			Sp.Id = Str(S, TEXT("id"));
			const FString T = Str(S, TEXT("type"));
			if (T == TEXT("player_start")) { Sp.Type = ECitySpawnType::PlayerStart; }
			else if (T == TEXT("boss")) { Sp.Type = ECitySpawnType::Boss; }
			else if (T == TEXT("tank")) { Sp.Type = ECitySpawnType::Tank; }
			else if (T == TEXT("pickup")) { Sp.Type = ECitySpawnType::Pickup; }
			else if (T == TEXT("drone")) { Sp.Type = ECitySpawnType::Drone; }
			else { continue; }
			Sp.Pos = FVector(Num(S, TEXT("x")), Num(S, TEXT("y")), Num(S, TEXT("z")));
			Sp.Yaw = float(Num(S, TEXT("yawDeg")));
			Sp.PickupType = Str(S, TEXT("pickupType"));
			D.Spawns.Add(Sp);
		}
		UE_LOG(LogCityLayout, Log, TEXT("Loaded city layout: %d buildings, %d roads, %d spawns."),
			Arr(D.Root, TEXT("buildings")).Num(), Arr(D.Root, TEXT("roads")).Num(), D.Spawns.Num());
		return D;
	}

	// ----------------------------------------------------------- triangle buffer for procedural sections
	struct FTriBuf
	{
		TArray<FVector> V;
		TArray<FVector> N;
		TArray<int32> I;
		TArray<FVector2D> UV;

		int32 Add(const FVector& P, const FVector& Nrm)
		{
			V.Add(P);
			N.Add(Nrm);
			UV.Add(FVector2D(P.X / 400.0, P.Y / 400.0));
			return V.Num() - 1;
		}

		/** Front face along Desired (UE front normal = (P2-P0) x (P1-P0)). */
		void Tri(int32 A, int32 B, int32 C, const FVector& Desired)
		{
			const FVector Std = FVector::CrossProduct(V[B] - V[A], V[C] - V[A]);
			if (FVector::DotProduct(Std, Desired) > 0.0) { I.Append({ A, C, B }); }
			else { I.Append({ A, B, C }); }
		}

		void Quad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Nrm)
		{
			const int32 A = Add(P0, Nrm), B = Add(P1, Nrm), C = Add(P2, Nrm), D = Add(P3, Nrm);
			Tri(A, B, C, Nrm);
			Tri(A, C, D, Nrm);
		}

		/** Convex polygon prism between Z0 and Z1 (top cap + walls; bBottom adds the underside). */
		void Prism(const TArray<FVector2D>& P, double Z0, double Z1, bool bBottom = false)
		{
			if (P.Num() < 3) { return; }
			FVector2D C(0, 0);
			for (const FVector2D& Q : P) { C += Q; }
			C /= P.Num();
			const int32 Ci = Add(FVector(C.X, C.Y, Z1), FVector::UpVector);
			TArray<int32> Top;
			for (const FVector2D& Q : P) { Top.Add(Add(FVector(Q.X, Q.Y, Z1), FVector::UpVector)); }
			for (int32 i = 0; i < P.Num(); ++i) { Tri(Ci, Top[i], Top[(i + 1) % P.Num()], FVector::UpVector); }
			if (bBottom)
			{
				const int32 Cb = Add(FVector(C.X, C.Y, Z0), FVector::DownVector);
				TArray<int32> Bot;
				for (const FVector2D& Q : P) { Bot.Add(Add(FVector(Q.X, Q.Y, Z0), FVector::DownVector)); }
				for (int32 i = 0; i < P.Num(); ++i) { Tri(Cb, Bot[i], Bot[(i + 1) % P.Num()], FVector::DownVector); }
			}
			for (int32 i = 0; i < P.Num(); ++i)
			{
				const FVector2D A = P[i], B = P[(i + 1) % P.Num()];
				const FVector2D Mid = (A + B) * 0.5;
				FVector Out(Mid.X - C.X, Mid.Y - C.Y, 0.0);
				const FVector2D E = (B - A).GetSafeNormal();
				FVector Perp(E.Y, -E.X, 0.0);
				if (FVector::DotProduct(Perp, Out) < 0.0) { Perp = -Perp; }
				Quad(FVector(A.X, A.Y, Z0), FVector(B.X, B.Y, Z0), FVector(B.X, B.Y, Z1), FVector(A.X, A.Y, Z1), Perp);
			}
		}
	};

	TArray<FVector2D> Inset(const TArray<FVector2D>& P, double By)
	{
		if (By <= 0.0 || P.Num() < 3) { return P; }
		FVector2D C(0, 0);
		for (const FVector2D& Q : P) { C += Q; }
		C /= P.Num();
		TArray<FVector2D> Out;
		for (const FVector2D& Q : P)
		{
			const FVector2D D = Q - C;
			const double L = D.Size();
			Out.Add(L > By * 1.5 ? C + D * ((L - By) / L) : C + D * 0.33);
		}
		return Out;
	}

	UMaterialInterface* EnvOr(const TCHAR* EnvMI, EArenaSurface Fallback)
	{
		if (UMaterialInterface* M = AChapterSet::EnvMaterial(EnvMI)) { return M; }
		return ArenaVisuals::GetSurfaceMaterial(Fallback);
	}

	constexpr float LowRiseCull = 95000.f;   // instances under 25 m vanish past 950 m (fog has them by then)
	constexpr float PropCull = 30000.f;      // rooftop clutter
}

ACityLayout::ACityLayout()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Static);
}

bool ACityLayout::IsAvailable() { return Data().Root.IsValid(); }

void ACityLayout::GetLayoutStats(int32& OutBuildings, int32& OutRoads, int32& OutBlocks, int32& OutSpawns)
{
	const FCityData& D = Data();
	OutBuildings = Arr(D.Root, TEXT("buildings")).Num();
	OutRoads = Arr(D.Root, TEXT("roads")).Num();
	OutBlocks = Arr(D.Root, TEXT("blocks")).Num();
	OutSpawns = D.Spawns.Num();
}

ACityLayout* ACityLayout::SpawnCity(UWorld* World, const FVector& Origin)
{
	if (!World || !IsAvailable()) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ACityLayout>(ACityLayout::StaticClass(), FTransform(Origin), SP);
}

ACityLayout* ACityLayout::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return nullptr; }
	for (TActorIterator<ACityLayout> It(World); It; ++It) { if (IsValid(*It)) { return *It; } }
	return nullptr;
}

void ACityLayout::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ACityLayout::SetCityVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}

UMaterialInstanceDynamic* ACityLayout::Mid(const FString& Key, UMaterialInterface* Parent, const FLinearColor& Tint, float EmissiveNits)
{
	if (const TObjectPtr<UMaterialInstanceDynamic>* Hit = Mids.Find(Key)) { return Hit->Get(); }
	if (!Parent) { return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, this);
	M->SetVectorParameterValue(TEXT("Color"), Tint);
	if (EmissiveNits > 0.f) { M->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveNits); }
	Mids.Add(Key, M);
	return M;
}

UHierarchicalInstancedStaticMeshComponent* ACityLayout::MakeHism(const FString& Key, UMaterialInterface* Material, bool bCollide, float CullEnd, bool bShadows)
{
	if (const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Hit = Hisms.Find(Key)) { return Hit->Get(); }
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*(TEXT("HISM_") + Key)));
	H->SetStaticMesh(ArenaVisuals::GetShapeMesh(Key.Contains(TEXT("Cyl")) ? EArenaShape::Cylinder : EArenaShape::Cube));
	H->SetupAttachment(Root);
	H->SetMobility(EComponentMobility::Static);
	if (Material) { H->SetMaterial(0, Material); }
	if (bCollide)
	{
		H->SetCollisionProfileName(TEXT("BlockAll"));
		H->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
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

UProceduralMeshComponent* ACityLayout::MakeProc(const FString& Key, bool bCollide)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this, FName(*(TEXT("PMC_") + Key)));
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

void ACityLayout::Build()
{
	if (bBuilt || !IsAvailable()) { return; }
	bBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	SpawnLastUsed.Init(-1000.0, Data().Spawns.Num());
	BuildGround();
	BuildRoads();
	BuildBlocks();
	BuildBuildings();
	BuildLandmark();
	UE_LOG(LogCityLayout, Log, TEXT("Harbor District built: %d building boxes, %d components, %.2f s."),
		NumBuildingInstances, Pieces.Num(), FPlatformTime::Seconds() - T0);
}

void ACityLayout::BuildGround()
{
	const TSharedPtr<FJsonObject> Water = Obj(Data().Root, TEXT("waterfront"));
	TArray<FVector2D> Shore = Poly(Arr(Water, TEXT("shoreline")));
	const double WaterZ = Num(Water, TEXT("waterZ"), -300.0);
	if (Shore.Num() < 2) { Shore = { FVector2D(95000, -100000), FVector2D(95000, 100000) }; }
	Shore.Sort([](const FVector2D& A, const FVector2D& B) { return A.Y < B.Y; });
	// Extend the shore past the district so ground and water reach the horizon (fog hides the edge).
	const double Far = 240000.0;
	Shore.Insert(FVector2D(Shore[0].X, -Far), 0);
	Shore.Add(FVector2D(Shore.Last().X, Far));

	FTriBuf Ground, Sea, Wall;
	for (int32 i = 0; i + 1 < Shore.Num(); ++i)
	{
		const FVector2D A = Shore[i], B = Shore[i + 1];
		Ground.Quad(FVector(-Far, A.Y, 0), FVector(A.X, A.Y, 0), FVector(B.X, B.Y, 0), FVector(-Far, B.Y, 0), FVector::UpVector);
		Sea.Quad(FVector(A.X, A.Y, WaterZ), FVector(Far, A.Y, WaterZ), FVector(Far, B.Y, WaterZ), FVector(B.X, B.Y, WaterZ), FVector::UpVector);
		// Seawall face (top +15, footing -450), facing the water (+X).
		const FVector2D E = (B - A).GetSafeNormal();
		FVector Out(E.Y, -E.X, 0.0);
		if (Out.X < 0.0) { Out = -Out; }
		Wall.Quad(FVector(A.X, A.Y, -450), FVector(B.X, B.Y, -450), FVector(B.X, B.Y, 15), FVector(A.X, A.Y, 15), Out);
	}
	if (UProceduralMeshComponent* G = MakeProc(TEXT("Ground"), true))
	{
		G->CreateMeshSection(0, Ground.V, Ground.I, Ground.N, Ground.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		G->SetMaterial(0, Mid(TEXT("ground"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Asphalt), FLinearColor(0.55f, 0.55f, 0.56f)));
		G->SetCastShadow(false);
	}
	if (UProceduralMeshComponent* S = MakeProc(TEXT("Water"), false))
	{
		S->CreateMeshSection(0, Sea.V, Sea.I, Sea.N, Sea.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		// No water shader in the project: a dark, rain-wet surface reads as calm harbor water under Lumen.
		S->SetMaterial(0, Mid(TEXT("water"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::AsphaltRain), FLinearColor(0.06f, 0.11f, 0.14f)));
		S->SetCastShadow(false);
		S->SetCollisionProfileName(TEXT("BlockAll"));
		S->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		S->ContainsPhysicsTriMeshData(true);
	}
	if (UProceduralMeshComponent* W = MakeProc(TEXT("Seawall"), true))
	{
		W->CreateMeshSection(0, Wall.V, Wall.I, Wall.N, Wall.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		W->SetMaterial(0, Mid(TEXT("seawall"), EnvOr(TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete), FLinearColor(0.7f, 0.7f, 0.68f)));
	}
}

void ACityLayout::BuildRoads()
{
	const FCityData& D = Data();
	const TArray<TSharedPtr<FJsonValue>>& Roads = Arr(D.Root, TEXT("roads"));
	TMap<FString, double> NodeHalf;
	for (const TSharedPtr<FJsonValue>& V : Roads)
	{
		const TSharedPtr<FJsonObject> R = AsObj(V);
		const double W = Num(R, TEXT("widthCm"), 1500.0);
		for (const TCHAR* End : { TEXT("from"), TEXT("to") })
		{
			double& H = NodeHalf.FindOrAdd(Str(R, End));
			H = FMath::Max(H, W * 0.5);
		}
	}
	UStaticMesh* Cube = ArenaVisuals::GetShapeMesh(EArenaShape::Cube);
	UMaterialInterface* Road4 = AChapterSet::EnvMaterial(TEXT("MI_Env_Road4Lane"));
	UMaterialInterface* Road2 = AChapterSet::EnvMaterial(TEXT("MI_Env_Road2Lane"));
	UMaterialInstanceDynamic* Plain = Mid(TEXT("asphalt"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Asphalt), FLinearColor(0.5f, 0.5f, 0.51f));
	UHierarchicalInstancedStaticMeshComponent* Walks = MakeHism(TEXT("Sidewalk"), Mid(TEXT("pave"), EnvOr(TEXT("MI_Env_Pavement"), EArenaSurface::Concrete), FLinearColor(0.92f, 0.92f, 0.9f)), true, 0.f, false);
	UHierarchicalInstancedStaticMeshComponent* Patches = MakeHism(TEXT("Junction"), Plain, false, 0.f, false);
	TArray<FTransform> WalkX, PatchX;
	int32 NumRoads = 0;
	for (const TSharedPtr<FJsonValue>& V : Roads)
	{
		const TSharedPtr<FJsonObject> R = AsObj(V);
		const FVector2D* A = D.Nodes.Find(Str(R, TEXT("from")));
		const FVector2D* B = D.Nodes.Find(Str(R, TEXT("to")));
		if (!A || !B) { continue; }
		const FVector2D Dir = (*B - *A).GetSafeNormal();
		const double TrimA = NodeHalf.FindRef(Str(R, TEXT("from"))), TrimB = NodeHalf.FindRef(Str(R, TEXT("to")));
		const double Len = FVector2D::Distance(*A, *B) - TrimA - TrimB;
		if (Len < 50.0) { continue; }
		const FVector2D Mid2 = *A + Dir * (TrimA + Len * 0.5);
		const double Walk = Num(R, TEXT("sidewalkWidthCm"), 0.0);
		const double Carriage = FMath::Max(300.0, Num(R, TEXT("widthCm"), 1500.0) - 2.0 * Walk);
		const float Yaw = float(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
		const int32 Lanes = int32(Num(R, TEXT("lanes"), 2));
		const bool bAlley = Str(R, TEXT("cls")) == TEXT("alley");

		// The road MIs map their texture with the component's object bounds, so each segment is its own component.
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Cube);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Static);
		C->SetRelativeLocation(FVector(Mid2.X, Mid2.Y, 1.0));
		C->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		C->SetRelativeScale3D(FVector(Len / 100.0, Carriage / 100.0, 0.02));
		UMaterialInterface* M = bAlley ? static_cast<UMaterialInterface*>(Plain) : (Lanes >= 4 ? Road4 : Road2);
		C->SetMaterial(0, M ? M : static_cast<UMaterialInterface*>(Plain));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
		AddInstanceComponent(C);
		Pieces.Add(C);
		++NumRoads;

		if (Walk > 1.0)
		{
			const FVector2D Side(-Dir.Y, Dir.X);
			for (const double Sgn : { -1.0, 1.0 })
			{
				const FVector2D P = Mid2 + Side * (Sgn * (Carriage + Walk) * 0.5);
				WalkX.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, 7.5), FVector((Len + Walk * 2.0) / 100.0, Walk / 100.0, 0.15)));
			}
		}
	}
	for (const TPair<FString, double>& N : NodeHalf)
	{
		const FVector2D* P = D.Nodes.Find(N.Key);
		if (P) { PatchX.Add(FTransform(FRotator::ZeroRotator, FVector(P->X, P->Y, 1.0), FVector(N.Value * 2.0 / 100.0, N.Value * 2.0 / 100.0, 0.02))); }
	}
	if (Walks) { Walks->AddInstances(WalkX, false); }
	if (Patches) { Patches->AddInstances(PatchX, false); }
	UE_LOG(LogCityLayout, Log, TEXT("Roads: %d segments, %d sidewalk strips, %d junctions."), NumRoads, WalkX.Num(), PatchX.Num());
}

void ACityLayout::BuildBlocks()
{
	FTriBuf Pave, Park, Ruin, Pier;
	for (const TSharedPtr<FJsonValue>& V : Arr(Data().Root, TEXT("blocks")))
	{
		const TSharedPtr<FJsonObject> B = AsObj(V);
		const TArray<FVector2D> P = Poly(Arr(B, TEXT("poly")));
		const FString Kind = Str(B, TEXT("kind"));
		if (Kind == TEXT("pier")) { Pier.Prism(P, -300.0, 100.0); }
		else if (Kind == TEXT("park")) { Park.Prism(P, 0.0, 15.0); }
		else if (Kind == TEXT("ruins")) { Ruin.Prism(P, 0.0, 15.0); }
		else { Pave.Prism(P, 0.0, 15.0); }
	}
	// Promenade along the seawall.
	if (const TSharedPtr<FJsonObject> Prom = Obj(Obj(Data().Root, TEXT("waterfront")), TEXT("promenade")))
	{
		const TArray<FVector2D> P = Poly(Arr(Prom, TEXT("poly")));
		// Not guaranteed convex: split into a strip of quads along its length when it has an even point count.
		if (P.Num() >= 4 && P.Num() % 2 == 0)
		{
			const int32 Half = P.Num() / 2;
			for (int32 i = 0; i + 1 < Half; ++i)
			{
				Pave.Prism({ P[i], P[i + 1], P[P.Num() - 2 - i], P[P.Num() - 1 - i] }, 0.0, 15.0);
			}
		}
	}
	struct FSec { FTriBuf* Buf; const TCHAR* Key; UMaterialInterface* Mat; FLinearColor Tint; };
	const FSec Secs[] = {
		{ &Pave, TEXT("blocks"), EnvOr(TEXT("MI_Env_Pavement"), EArenaSurface::Concrete), FLinearColor(0.9f, 0.9f, 0.88f) },
		{ &Park, TEXT("park"), EnvOr(TEXT("MI_Env_Ground"), EArenaSurface::DebrisGround), FLinearColor(0.75f, 0.85f, 0.6f) },
		{ &Ruin, TEXT("ruins"), EnvOr(TEXT("MI_Env_Debris"), EArenaSurface::Rubble), FLinearColor(0.8f, 0.78f, 0.75f) },
		{ &Pier, TEXT("piers"), EnvOr(TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete), FLinearColor(0.75f, 0.74f, 0.72f) } };
	for (const FSec& S : Secs)
	{
		if (S.Buf->I.Num() == 0) { continue; }
		UProceduralMeshComponent* P = MakeProc(S.Key, true);
		P->CreateMeshSection(0, S.Buf->V, S.Buf->I, S.Buf->N, S.Buf->UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		P->SetMaterial(0, Mid(FString(S.Key) + TEXT("_slab"), S.Mat, S.Tint));
		P->SetCastShadow(false);
	}
}

void ACityLayout::BuildBuildings()
{
	struct FFacade { const TCHAR* EnvMI; EArenaSurface Surface; FLinearColor Tint; };
	auto FacadeFor = [](const FString& Id) -> FFacade
	{
		if (Id == TEXT("gl1")) { return { TEXT("MI_Env_GlassTower"), EArenaSurface::Facade, FLinearColor(0.85f, 0.9f, 0.95f) }; }
		if (Id == TEXT("gl2")) { return { TEXT("MI_Env_GlassTower"), EArenaSurface::Facade, FLinearColor(0.62f, 0.7f, 0.74f) }; }
		if (Id == TEXT("cn1")) { return { TEXT("MI_Env_ConcreteTiles"), EArenaSurface::Concrete, FLinearColor(0.9f, 0.88f, 0.84f) }; }
		if (Id == TEXT("cn2")) { return { TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete, FLinearColor(0.8f, 0.8f, 0.78f) }; }
		if (Id == TEXT("br1")) { return { nullptr, EArenaSurface::RedBrick, FLinearColor(0.95f, 0.9f, 0.85f) }; }
		if (Id == TEXT("br2")) { return { nullptr, EArenaSurface::RedBrick, FLinearColor(0.7f, 0.6f, 0.52f) }; }
		return { nullptr, EArenaSurface::Facade, FLinearColor(0.85f, 0.85f, 0.85f) }; // mx: procedural window grid
	};
	TMap<FString, TArray<FTransform>> Batches;     // key = facade|tall
	TArray<FTransform> Roofs, Ac, Tanks, Pads, Antennas;
	TMap<FString, FTriBuf> PolyBufs;
	const FString LandmarkId = Str(Obj(Data().Root, TEXT("landmark")), TEXT("buildingId"));

	for (const TSharedPtr<FJsonValue>& V : Arr(Data().Root, TEXT("buildings")))
	{
		const TSharedPtr<FJsonObject> B = AsObj(V);
		if (!B.IsValid()) { continue; }
		const FString Facade = Str(B, TEXT("facade"));
		const double FloorCm = Num(B, TEXT("floorCm"), 350.0);
		double Height = Num(B, TEXT("heightCm"), 1000.0);
		const TSharedPtr<FJsonObject> Ruin = Obj(B, TEXT("ruin"));
		float Tilt = 0.f;
		if (Ruin.IsValid())
		{
			Height = FMath::Max(300.0, Num(Ruin, TEXT("standingCm"), Height));
			Tilt = float(Num(Ruin, TEXT("tiltDeg"), 0.0));
		}
		// Tier stack: [floor 0 .. tier0), [tier0 .. tier1), ... with absolute insets.
		TArray<TPair<double, double>> Tiers; // start Z, inset
		Tiers.Add(TPair<double, double>(0.0, 0.0));
		for (const TSharedPtr<FJsonValue>& TV : Arr(B, TEXT("tiers")))
		{
			const TSharedPtr<FJsonObject> T = AsObj(TV);
			const double Z = Num(T, TEXT("fromFloor")) * FloorCm;
			if (Z > 0.0 && Z < Height) { Tiers.Add(TPair<double, double>(Z, Num(T, TEXT("insetCm")))); }
		}
		const bool bTall = Height >= 2500.0;
		const TSharedPtr<FJsonObject> Fp = Obj(B, TEXT("fp"));
		const TArray<FVector2D> PolyFp = Poly(Arr(Fp, TEXT("poly")));
		if (PolyFp.Num() >= 3)
		{
			const FString Key = Str(B, TEXT("id")) == LandmarkId ? FString(TEXT("landmark")) : Facade;
			FTriBuf& Buf = PolyBufs.FindOrAdd(Key);
			for (int32 t = 0; t < Tiers.Num(); ++t)
			{
				const double Z0 = Tiers[t].Key, Z1 = t + 1 < Tiers.Num() ? Tiers[t + 1].Key : Height;
				Buf.Prism(Inset(PolyFp, Tiers[t].Value), Z0, Z1);
			}
			NumBuildingInstances += Tiers.Num();
			continue;
		}
		const FVector2D C(Num(Fp, TEXT("x")), Num(Fp, TEXT("y")));
		const double W = Num(Fp, TEXT("w"), 1000.0), Dp = Num(Fp, TEXT("d"), 1000.0);
		const float Yaw = float(Num(Fp, TEXT("rotDeg")));
		TArray<FTransform>& Batch = Batches.FindOrAdd(Facade + (bTall ? TEXT("|tall") : TEXT("|low")));
		for (int32 t = 0; t < Tiers.Num(); ++t)
		{
			const double Z0 = Tiers[t].Key, Z1 = t + 1 < Tiers.Num() ? Tiers[t + 1].Key : Height;
			const double In = Tiers[t].Value * 2.0;
			const double Hh = Z1 - Z0;
			// Ruins lean: rotate about the footprint centre; the slab sinks a little so the low edge stays buried.
			FVector Loc(C.X, C.Y, Z0 + Hh * 0.5);
			if (Tilt != 0.f) { Loc.Z -= FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Tilt))) * W * 0.25; }
			Batch.Add(FTransform(FRotator(0.f, Yaw, Tilt), Loc, FVector(FMath::Max(100.0, W - In) / 100.0, FMath::Max(100.0, Dp - In) / 100.0, Hh / 100.0)));
			++NumBuildingInstances;
		}
		// Roof plate (parapet lip, 110 cm) on intact buildings.
		if (!Ruin.IsValid())
		{
			const double In = Tiers.Last().Value * 2.0;
			Roofs.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(C.X, C.Y, Height + 55.0),
				FVector(FMath::Max(100.0, W - In - 60.0) / 100.0, FMath::Max(100.0, Dp - In - 60.0) / 100.0, 1.1)));
		}
		// Rooftop props (world XY in the JSON).
		if (const TSharedPtr<FJsonObject> Roof = Obj(B, TEXT("roof")))
		{
			for (const TSharedPtr<FJsonValue>& AV : Arr(Roof, TEXT("ac")))
			{
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				if (AV->TryGetArray(A) && A->Num() >= 3)
				{
					Ac.Add(FTransform(FRotator(0.f, float((*A)[2]->AsNumber()), 0.f), FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), Height + 70.0), FVector(2.2, 1.4, 1.4)));
				}
			}
			for (const TSharedPtr<FJsonValue>& AV : Arr(Roof, TEXT("waterTower")))
			{
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				if (AV->TryGetArray(A) && A->Num() >= 3)
				{
					const double H = (*A)[2]->AsNumber();
					Tanks.Add(FTransform(FRotator::ZeroRotator, FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), Height + H * 0.6), FVector(4.0, 4.0, H * 0.8 / 100.0)));
				}
			}
			for (const TSharedPtr<FJsonValue>& AV : Arr(Roof, TEXT("antenna")))
			{
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				if (AV->TryGetArray(A) && A->Num() >= 3)
				{
					const double H = (*A)[2]->AsNumber();
					Antennas.Add(FTransform(FRotator::ZeroRotator, FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), Height + H * 0.5), FVector(0.25, 0.25, H / 100.0)));
				}
			}
			if (const TSharedPtr<FJsonObject> Pad = Obj(Roof, TEXT("helipad")))
			{
				const double R = Num(Pad, TEXT("rCm"), 1100.0);
				Pads.Add(FTransform(FRotator::ZeroRotator, FVector(Num(Pad, TEXT("x")), Num(Pad, TEXT("y")), Num(Pad, TEXT("z"), Height) + 30.0), FVector(R * 2.0 / 100.0, R * 2.0 / 100.0, 0.6)));
			}
		}
	}

	for (TPair<FString, TArray<FTransform>>& Pair : Batches)
	{
		FString FacadeId, Band;
		Pair.Key.Split(TEXT("|"), &FacadeId, &Band);
		const FFacade F = FacadeFor(FacadeId);
		UMaterialInterface* Parent = F.EnvMI ? EnvOr(F.EnvMI, F.Surface) : ArenaVisuals::GetSurfaceMaterial(F.Surface);
		UHierarchicalInstancedStaticMeshComponent* H = MakeHism(TEXT("Bldg_") + FacadeId + TEXT("_") + Band, Mid(TEXT("facade_") + FacadeId, Parent, F.Tint),
			true, Band == TEXT("low") ? LowRiseCull : 0.f);
		if (H) { H->AddInstances(Pair.Value, false); }
	}
	UMaterialInstanceDynamic* RoofMat = Mid(TEXT("roof"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::RoofGravel), FLinearColor(0.7f, 0.7f, 0.68f));
	UMaterialInstanceDynamic* MetalMat = Mid(TEXT("metal"), EnvOr(TEXT("MI_Env_Metal"), EArenaSurface::MetalPlate), FLinearColor(0.6f, 0.6f, 0.62f));
	if (UHierarchicalInstancedStaticMeshComponent* H = MakeHism(TEXT("Roofs"), RoofMat, true, 0.f, false)) { H->AddInstances(Roofs, false); }
	if (UHierarchicalInstancedStaticMeshComponent* H = MakeHism(TEXT("RoofAC"), MetalMat, false, PropCull)) { H->AddInstances(Ac, false); }
	if (UHierarchicalInstancedStaticMeshComponent* H = MakeHism(TEXT("WaterTankCyl"), Mid(TEXT("wood"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::RustyMetal), FLinearColor(0.55f, 0.4f, 0.3f)), true, LowRiseCull)) { H->AddInstances(Tanks, false); }
	if (UHierarchicalInstancedStaticMeshComponent* H = MakeHism(TEXT("AntennaCyl"), MetalMat, false, LowRiseCull)) { H->AddInstances(Antennas, false); }
	if (UHierarchicalInstancedStaticMeshComponent* H = MakeHism(TEXT("HelipadCyl"), Mid(TEXT("pad"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.35f, 0.36f, 0.38f)), true, 0.f)) { H->AddInstances(Pads, false); }

	for (TPair<FString, FTriBuf>& Pair : PolyBufs)
	{
		UProceduralMeshComponent* P = MakeProc(TEXT("Poly_") + Pair.Key, true);
		P->CreateMeshSection(0, Pair.Value.V, Pair.Value.I, Pair.Value.N, Pair.Value.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		const FFacade F = Pair.Key == TEXT("landmark") ? FFacade{ TEXT("MI_Env_GlassTower"), EArenaSurface::Facade, FLinearColor(0.8f, 0.86f, 0.92f) } : FacadeFor(Pair.Key);
		P->SetMaterial(0, Mid(TEXT("facade_") + Pair.Key, F.EnvMI ? EnvOr(F.EnvMI, F.Surface) : ArenaVisuals::GetSurfaceMaterial(F.Surface), F.Tint));
	}
}

void ACityLayout::BuildLandmark()
{
	const TSharedPtr<FJsonObject> L = Obj(Data().Root, TEXT("landmark"));
	if (!L.IsValid()) { return; }
	const FVector Base(Num(L, TEXT("x")), Num(L, TEXT("y")), 0.0);
	const double Roof = Num(L, TEXT("heightCm"), 31155.0);
	UMaterialInstanceDynamic* Steel = Mid(TEXT("pad"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Steel), FLinearColor(0.35f, 0.36f, 0.38f));
	UHierarchicalInstancedStaticMeshComponent* Solid = MakeHism(TEXT("Landmark"), Steel, true, 0.f);
	TArray<FTransform> X;
	if (const TSharedPtr<FJsonObject> Pad = Obj(L, TEXT("landingPad")))
	{
		const FVector P(Num(Pad, TEXT("x")), Num(Pad, TEXT("y")), Num(Pad, TEXT("z")));
		X.Add(FTransform(FRotator::ZeroRotator, P - FVector(0, 0, 40), FVector(Num(Pad, TEXT("wCm"), 2200) / 100.0, Num(Pad, TEXT("dCm"), 2400) / 100.0, 0.8)));
		// Armor gantries along the pad's far edge.
		for (int32 g = 0; g < int32(Num(Pad, TEXT("armorGantries"), 3)); ++g)
		{
			X.Add(FTransform(FRotator::ZeroRotator, P + FVector(Num(Pad, TEXT("wCm"), 2200) * 0.35, (g - 1) * 700.0, 250.0), FVector(0.4, 1.6, 5.0)));
		}
	}
	// Spire above the crown.
	const double Top = Num(L, TEXT("topCm"), 34355.0);
	X.Add(FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, (Roof + Top) * 0.5), FVector(0.6, 0.6, (Top - Roof) / 100.0)));
	if (Solid) { Solid->AddInstances(X, false); }

	// Arc-reactor crown: ring of emissive segments (r 17 m) plus a cool point light.
	UHierarchicalInstancedStaticMeshComponent* Ring = MakeHism(TEXT("Crown"), Mid(TEXT("crown"), ArenaVisuals::GetSurfaceMaterial(EArenaSurface::Glow),
		FLinearColor(0.55f, 0.85f, 1.f), ArenaVisuals::EmissiveToNits(12.f)), false, 0.f, false);
	TArray<FTransform> RingX;
	const double CrownZ = Roof + 700.0, CrownR = 1700.0;
	for (int32 i = 0; i < 32; ++i)
	{
		const double A = 2.0 * PI * i / 32.0;
		RingX.Add(FTransform(FRotator(0.f, float(FMath::RadiansToDegrees(A)) + 90.f, 0.f), Base + FVector(FMath::Cos(A) * CrownR, FMath::Sin(A) * CrownR, CrownZ),
			FVector(3.4, 0.5, 0.6)));
	}
	if (Ring) { Ring->AddInstances(RingX, false); }
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(Base + FVector(0, 0, CrownZ));
	Light->SetLightColor(FLinearColor(0.55f, 0.85f, 1.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(60000.f);
	Light->SetAttenuationRadius(9000.f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	AddInstanceComponent(Light);
}

bool ACityLayout::GetStoryLocation(FName Location, FVector& OutCenter, float& OutRadius) const
{
	const TSharedPtr<FJsonObject> L = Obj(Obj(Data().Root, TEXT("storyLocations")), *Location.ToString());
	if (!L.IsValid()) { return false; }
	const TArray<FVector2D> C = Poly(TArray<TSharedPtr<FJsonValue>>{ L->TryGetField(TEXT("center")) });
	OutCenter = GetActorLocation() + (C.Num() == 1 ? FVector(C[0].X, C[0].Y, 0.0) : FVector::ZeroVector);
	OutRadius = float(Num(L, TEXT("playRadiusCm"), 6500.0));
	return true;
}

bool ACityLayout::GetPlayerStart(FName Id, FTransform& OutTransform) const
{
	for (const FCitySpawn& S : Data().Spawns)
	{
		if (S.Type == ECitySpawnType::PlayerStart && S.Id == Id.ToString())
		{
			OutTransform = FTransform(FRotator(0.f, S.Yaw, 0.f), GetActorLocation() + S.Pos);
			return true;
		}
	}
	return false;
}

bool ACityLayout::PickSpawn(ECitySpawnType Type, const FVector& Near, float MinDist, float MaxDist, const FVector& PlayCenter,
	float PlayRadius, FVector& OutLocation)
{
	const TArray<FCitySpawn>& All = Data().Spawns;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FVector Origin = GetActorLocation();
	TArray<int32> InBand;
	int32 Closest = INDEX_NONE;
	double ClosestD = TNumericLimits<double>::Max();
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i].Type != Type || (SpawnLastUsed.IsValidIndex(i) && Now - SpawnLastUsed[i] < 8.0)) { continue; }
		const FVector W = Origin + All[i].Pos;
		if (FVector::Dist2D(W, PlayCenter) > PlayRadius) { continue; }
		const double D = FVector::Dist2D(W, Near);
		if (D < MinDist) { continue; }
		if (D <= MaxDist) { InBand.Add(i); }
		if (D < ClosestD) { ClosestD = D; Closest = i; }
	}
	const int32 Pick = InBand.Num() > 0 ? InBand[FMath::RandRange(0, InBand.Num() - 1)] : Closest;
	if (Pick == INDEX_NONE) { return false; }
	if (SpawnLastUsed.IsValidIndex(Pick)) { SpawnLastUsed[Pick] = Now; }
	OutLocation = Origin + All[Pick].Pos;
	return true;
}

void ACityLayout::GetPickups(const FVector& Center, float Radius, TArray<TPair<FVector, FString>>& Out) const
{
	for (const FCitySpawn& S : Data().Spawns)
	{
		const FVector W = GetActorLocation() + S.Pos;
		if (S.Type == ECitySpawnType::Pickup && FVector::Dist2D(W, Center) <= Radius) { Out.Add(TPair<FVector, FString>(W, S.PickupType)); }
	}
}

FVector ACityLayout::GetLandmarkLocation() const
{
	const TSharedPtr<FJsonObject> L = Obj(Data().Root, TEXT("landmark"));
	return GetActorLocation() + FVector(Num(L, TEXT("x"), 76500.0), Num(L, TEXT("y"), -12000.0), 0.0);
}
