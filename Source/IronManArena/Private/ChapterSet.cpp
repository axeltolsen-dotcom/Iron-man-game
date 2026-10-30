#include "ChapterSet.h"

#include "IronManArena.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

namespace
{
	const FLinearColor Steel(0.56f, 0.57f, 0.58f);
	const FLinearColor DarkSteel(0.12f, 0.12f, 0.13f);
	const FLinearColor Rubber(0.02f, 0.02f, 0.02f);
	const FLinearColor Warm(1.f, 0.72f, 0.42f);
	const FLinearColor Cool(0.72f, 0.85f, 1.f);
	const FLinearColor ArcBlue(0.35f, 0.8f, 1.f);
	const FLinearColor Hazard(0.9f, 0.65f, 0.05f);
	const FLinearColor Ember(1.f, 0.38f, 0.08f);
	const FName NAME_Emissive(TEXT("EmissiveStrength"));
}

AChapterSet::AChapterSet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	Rng.Initialize(1337);
}

AChapterSet* AChapterSet::SpawnSet(UWorld* World, const FVector& Origin, FName SetName, const FEnvironmentPreset& InPreset)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AChapterSet* Set = World->SpawnActor<AChapterSet>(AChapterSet::StaticClass(), FTransform(Origin), Params);
	if (!Set) { return nullptr; }
	Set->Preset = InPreset;
	Set->SetId = SetName;
	UGameplayStatics::FinishSpawningActor(Set, FTransform(Origin));
	return Set;
}

void AChapterSet::BeginPlay()
{
	Super::BeginPlay();
	Build();
	UE_LOG(LogIronManArena, Log, TEXT("ChapterSet '%s': %d parts, %d lights, %d arms, %d particles"),
		*SetId.ToString(), Parts.Num(), Lights.Num(), Arms.Num(), Particles.Num());
}

void AChapterSet::Build()
{
	const FString Id = SetId.ToString();
	if (Id == TEXT("arena_city"))             { BuildArenaCity(Preset.bNight); return; }
	if (Id.StartsWith(TEXT("workshop")))      { BuildWorkshop(Preset.bNight); }
	else if (Id.StartsWith(TEXT("rooftops"))) { BuildRooftops(Preset.bWet && Preset.bNight); }
	else if (Id.StartsWith(TEXT("city")))     { BuildCity(Preset.bNight, Preset.bWet); }
	else if (Id == TEXT("battlefield"))       { BuildRuins(true); }
	else if (Id == TEXT("ruins"))             { BuildRuins(false); }
	BuildWeather();
}

// =============================================================================== helpers

UStaticMeshComponent* AChapterSet::Shape(EArenaShape InShape, const FVector& Center, const FVector& Size, const FRotator& Rot,
	const FLinearColor& Color, EArenaSurface Surface, bool bSolid, float Emissive, USceneComponent* Parent)
{
	return ArenaVisuals::AddRuntimePart(this, Parent ? Parent : Root.Get(), InShape, Center, Size / 100.f, Rot,
		Color, Emissive, Surface, bSolid, Parts);
}

UStaticMeshComponent* AChapterSet::Box(const FVector& Center, const FVector& Size, const FLinearColor& Color, EArenaSurface Surface,
	bool bSolid, const FRotator& Rot, float Emissive, USceneComponent* Parent)
{
	return Shape(EArenaShape::Cube, Center, Size, Rot, Color, Surface, bSolid, Emissive, Parent);
}

UPointLightComponent* AChapterSet::PointLight(const FVector& Loc, const FLinearColor& Color, float Lumens, float Radius, bool bShadows)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetMobility(EComponentMobility::Movable);
	L->SetRelativeLocation(Loc);
	L->SetIntensityUnits(ELightUnits::Lumens);
	L->SetIntensity(Lumens);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetSourceRadius(15.f);
	L->SetCastShadows(bShadows);
	L->RegisterComponent();
	AddInstanceComponent(L);
	Lights.Add(L);
	return L;
}

USpotLightComponent* AChapterSet::SpotLight(const FVector& Loc, const FRotator& Rot, const FLinearColor& Color, float Lumens,
	float Radius, float InnerDeg, float OuterDeg, bool bShadows)
{
	USpotLightComponent* L = NewObject<USpotLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetMobility(EComponentMobility::Movable);
	L->SetRelativeLocation(Loc);
	L->SetRelativeRotation(Rot);
	L->SetIntensityUnits(ELightUnits::Lumens);
	L->SetIntensity(Lumens);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetInnerConeAngle(InnerDeg);
	L->SetOuterConeAngle(OuterDeg);
	L->SetSourceRadius(20.f);
	L->SetCastShadows(bShadows);
	L->RegisterComponent();
	AddInstanceComponent(L);
	Lights.Add(L);
	return L;
}

USceneComponent* AChapterSet::Pivot(USceneComponent* Parent, const FVector& Loc, const FRotator& Rot)
{
	USceneComponent* P = NewObject<USceneComponent>(this);
	P->SetMobility(EComponentMobility::Movable);
	P->SetupAttachment(Parent ? Parent : Root.Get());
	P->SetRelativeLocation(Loc);
	P->SetRelativeRotation(Rot);
	P->RegisterComponent();
	AddInstanceComponent(P);
	return P;
}

void AChapterSet::SetLastScalar(FName Param, float Value)
{
	if (Parts.Num() > 0 && Parts.Last().MID) { Parts.Last().MID->SetScalarParameterValue(Param, Value); }
}

// =============================================================================== props

void AChapterSet::Building(const FVector2D& XY, const FVector& Size, float BaseZ, const FLinearColor& Wall, bool bNight, float LitFraction)
{
	Box(FVector(XY.X, XY.Y, BaseZ + Size.Z * 0.5f), Size, Wall, EArenaSurface::Facade, true);
	SetLastScalar(TEXT("LitFraction"), LitFraction);
	SetLastScalar(NAME_Emissive, ArenaVisuals::EmissiveToNits(bNight ? 0.15f : 0.f)); // lit office windows ~150 nits
	SetLastScalar(TEXT("FloorHeight"), 360.f + Rng.FRandRange(0.f, 80.f));
	SetLastScalar(TEXT("WindowWidth"), 220.f + Rng.FRandRange(0.f, 120.f));
	// Roof cap + plant room.
	const float Top = BaseZ + Size.Z;
	Box(FVector(XY.X, XY.Y, Top + 20.f), FVector(Size.X + 40.f, Size.Y + 40.f, 40.f), Wall * 0.7f, EArenaSurface::Concrete, true);
	Box(FVector(XY.X + Size.X * 0.15f, XY.Y - Size.Y * 0.1f, Top + 190.f), FVector(Size.X * 0.35f, Size.Y * 0.3f, 300.f), Wall * 0.8f, EArenaSurface::BlockWall, true);
	if (Size.Z > 6000.f)
	{
		// Tall towers carry an aviation light (blinks in Tick).
		Shape(EArenaShape::Cylinder, FVector(XY.X, XY.Y, Top + 600.f), FVector(30.f, 30.f, 1100.f), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, false);
		Shape(EArenaShape::Sphere, FVector(XY.X, XY.Y, Top + 1170.f), FVector(60.f), FRotator::ZeroRotator, FLinearColor(1.f, 0.05f, 0.03f), EArenaSurface::Glow, false, 12.f);
		BlinkParts.Add(Parts.Num() - 1);
	}
}

void AChapterSet::Car(const FVector& Loc, float Yaw, const FLinearColor& Paint, bool bWrecked, bool bLightsOn)
{
	USceneComponent* C = Pivot(Root, Loc, FRotator(bWrecked ? Rng.FRandRange(-6.f, 6.f) : 0.f, Yaw, bWrecked ? Rng.FRandRange(-8.f, 8.f) : 0.f));
	const FLinearColor Body = bWrecked ? FLinearColor(0.05f, 0.045f, 0.04f) : Paint;
	const EArenaSurface Skin = bWrecked ? EArenaSurface::RustyMetal : EArenaSurface::SuitPaint;
	Box(FVector(0, 0, 65), FVector(460, 185, 70), Body, Skin, true, FRotator::ZeroRotator, 0.f, C);
	if (!bWrecked) { SetLastScalar(TEXT("EdgeWear"), 0.02f); SetLastScalar(TEXT("FlakeAmount"), 0.5f); }
	else           { SetLastScalar(TEXT("Scorch"), 1.f); }
	Box(FVector(-20, 0, 125), FVector(230, 165, 60), Body, Skin, false, FRotator::ZeroRotator, 0.f, C);
	if (!bWrecked)
	{
		Box(FVector(-20, 0, 128), FVector(236, 168, 48), FLinearColor(0.05f, 0.06f, 0.07f), EArenaSurface::Glass, false, FRotator::ZeroRotator, 0.f, C);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i < 2) ? 150.f : -150.f;
		const float Y = (i % 2 == 0) ? 85.f : -85.f;
		Shape(EArenaShape::Cylinder, FVector(X, Y, 35), FVector(68, 68, 28), FRotator(0, 0, 90), Rubber, EArenaSurface::RobotMetal, false, 0.f, C);
	}
	if (!bWrecked)
	{
		for (const float Y : { 62.f, -62.f })
		{
			Box(FVector(231, Y, 75), FVector(4, 40, 16), FLinearColor(1.f, 0.95f, 0.85f), EArenaSurface::Glow, false, FRotator::ZeroRotator, bLightsOn ? 40.f : 0.5f, C); // headlamp lens ~40k nits
			Box(FVector(-231, Y, 80), FVector(4, 36, 14), FLinearColor(1.f, 0.02f, 0.01f), EArenaSurface::Glow, false, FRotator::ZeroRotator, bLightsOn ? 2.f : 0.05f, C); // tail lamp ~2k nits
		}
	}
	else
	{
		Fire(Loc + FVector(0, 0, 120), 0.6f);
	}
}

void AChapterSet::StreetLamp(const FVector& Loc, float Yaw, bool bOn)
{
	Shape(EArenaShape::Cylinder, Loc + FVector(0, 0, 450), FVector(22, 22, 900), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, true);
	const FVector Arm = FRotator(0, Yaw, 0).RotateVector(FVector(110, 0, 0));
	Box(Loc + FVector(0, 0, 890) + Arm * 0.5f, FVector(230, 16, 12), DarkSteel, EArenaSurface::RobotMetal, false, FRotator(0, Yaw, 0));
	Box(Loc + FVector(0, 0, 878) + Arm, FVector(70, 34, 10), Warm, EArenaSurface::Glow, false, FRotator(0, Yaw, 0), bOn ? 20.f : 0.f); // street lamp head ~20k nits
	if (bOn)
	{
		SpotLight(Loc + FVector(0, 0, 870) + Arm, FRotator(-90, 0, 0), FLinearColor(1.f, 0.75f, 0.45f), 16000.f, 2200.f, 30.f, 60.f, false); // LED street lamp ~16k lm
	}
}

bool AChapterSet::bHallPodLighting = false;

void AChapterSet::DisplaySuit(const FVector& Loc, float Yaw, const FLinearColor& Primary, const FLinearColor& Secondary, bool bCaseLit)
{
	USceneComponent* S = Pivot(Root, Loc, FRotator(0, Yaw, 0));
	// Plinth + glass case.
	Box(FVector(0, 0, 15), FVector(150, 150, 30), DarkSteel, EArenaSurface::Steel, true, FRotator::ZeroRotator, 0.f, S);
	Box(FVector(0, 0, 32), FVector(130, 130, 4), ArcBlue, EArenaSurface::Glow, false, FRotator::ZeroRotator, bCaseLit ? 0.8f : 0.05f, S); // display-case floor panel
	Box(FVector(0, 0, 150), FVector(150, 150, 240), FLinearColor(0.9f, 0.95f, 1.f), EArenaSurface::Glass, true, FRotator::ZeroRotator, 0.f, S);
	SetLastScalar(TEXT("Opacity"), 0.12f);
	// Stylised suit (legs, hips, torso, shoulders, arms, helmet, reactor, eyes).
	const EArenaSurface P = EArenaSurface::SuitPaint, G = EArenaSurface::SuitGold;
	for (const float Y : { 18.f, -18.f })
	{
		Shape(EArenaShape::Cylinder, FVector(0, Y, 70), FVector(24, 24, 80), FRotator::ZeroRotator, Primary, P, false, 0.f, S);
		Shape(EArenaShape::Cylinder, FVector(0, Y, 125), FVector(28, 28, 40), FRotator::ZeroRotator, Secondary, G, false, 0.f, S);
		Shape(EArenaShape::Cylinder, FVector(0, Y * 2.3f, 165), FVector(18, 18, 75), FRotator::ZeroRotator, Secondary, G, false, 0.f, S);
		Shape(EArenaShape::Sphere, FVector(0, Y * 2.1f, 208), FVector(30), FRotator::ZeroRotator, Primary, P, false, 0.f, S);
	}
	Box(FVector(0, 0, 150), FVector(36, 60, 30), Primary, P, false, FRotator::ZeroRotator, 0.f, S);
	Box(FVector(0, 0, 190), FVector(42, 64, 50), Primary, P, false, FRotator::ZeroRotator, 0.f, S);
	Shape(EArenaShape::Cylinder, FVector(22, 0, 195), FVector(12, 12, 3), FRotator(90, 0, 0), ArcBlue, EArenaSurface::Glow, false, bCaseLit ? 10.f : 1.f, S);
	Shape(EArenaShape::Sphere, FVector(0, 0, 238), FVector(30, 27, 34), FRotator::ZeroRotator, Primary, P, false, 0.f, S);
	Box(FVector(13, 0, 238), FVector(6, 20, 14), Secondary, G, false, FRotator::ZeroRotator, 0.f, S);
	for (const float Y : { 5.f, -5.f })
	{
		Box(FVector(16, Y, 242), FVector(2, 6, 2), ArcBlue, EArenaSurface::Glow, false, FRotator::ZeroRotator, bCaseLit ? 8.f : 0.5f, S);
	}
	if (bCaseLit)
	{
		const FVector Up = Loc + FVector(0, 0, 300);
		SpotLight(Up, FRotator(-90, 0, 0), FLinearColor(0.9f, 0.95f, 1.f), 900.f, 500.f, 18.f, 34.f, false);
		if (bHallPodLighting)
		{
			// Museum-style pod lighting: 3000 K key from the front-top, 6500 K rim from behind, and a
			// halo ring (~1500 nits) round the plinth.
			const FVector Fwd = FRotator(0, Yaw, 0).Vector();
			SpotLight(Loc + Fwd * 220.f + FVector(0, 0, 330), (FVector(0, 0, 150) - (Fwd * 220.f + FVector(0, 0, 330))).Rotation(),
				FLinearColor(1.f, 0.82f, 0.62f), 1400.f, 600.f, 14.f, 26.f, true);
			SpotLight(Loc - Fwd * 160.f + FVector(0, 0, 300), (FVector(0, 0, 180) - (-Fwd * 160.f + FVector(0, 0, 300))).Rotation(),
				FLinearColor(0.85f, 0.92f, 1.f), 700.f, 500.f, 16.f, 30.f, false);
			Shape(EArenaShape::Cylinder, FVector(0, 0, 29.5f), FVector(160, 160, 2.f), FRotator::ZeroRotator, FLinearColor(1.f, 0.9f, 0.75f), EArenaSurface::Glow, false, 1.5f, S); // rim peeks 5 cm round the plinth top
		}
	}
}

void AChapterSet::WorkshopArm(const FVector& Loc, float Yaw, const FLinearColor& Color, float Phase, float Speed)
{
	FWorkshopArm Arm;
	Arm.Phase = Phase;
	Arm.Speed = Speed;
	Arm.Base = Pivot(Root, Loc, FRotator(0, Yaw, 0));
	Shape(EArenaShape::Cylinder, FVector(0, 0, 20), FVector(80, 80, 40), FRotator::ZeroRotator, DarkSteel, EArenaSurface::MetalPlate, true, 0.f, Arm.Base);
	Shape(EArenaShape::Cylinder, FVector(0, 0, 60), FVector(45, 45, 40), FRotator::ZeroRotator, Color, EArenaSurface::SuitPaint, false, 0.f, Arm.Base);
	Box(FVector(0, 0, 41), FVector(70, 8, 4), Hazard, EArenaSurface::Plain, false, FRotator::ZeroRotator, 0.f, Arm.Base);
	Arm.Shoulder = Pivot(Arm.Base, FVector(0, 0, 85));
	Shape(EArenaShape::Sphere, FVector::ZeroVector, FVector(40), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, false, 0.f, Arm.Shoulder);
	Box(FVector(0, 0, 75), FVector(22, 26, 150), Color, EArenaSurface::SuitPaint, false, FRotator::ZeroRotator, 0.f, Arm.Shoulder);
	Shape(EArenaShape::Cylinder, FVector(0, 0, 75), FVector(8, 8, 120), FRotator::ZeroRotator, FLinearColor(0.3f, 0.3f, 0.3f), EArenaSurface::RobotMetal, false, 0.f, Arm.Shoulder);
	Arm.Elbow = Pivot(Arm.Shoulder, FVector(0, 0, 150));
	Shape(EArenaShape::Sphere, FVector::ZeroVector, FVector(30), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, false, 0.f, Arm.Elbow);
	Box(FVector(0, 0, 55), FVector(18, 20, 110), Color, EArenaSurface::SuitPaint, false, FRotator::ZeroRotator, 0.f, Arm.Elbow);
	Arm.Wrist = Pivot(Arm.Elbow, FVector(0, 0, 112));
	Shape(EArenaShape::Cylinder, FVector(0, 0, 8), FVector(26, 26, 16), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, false, 0.f, Arm.Wrist);
	for (const float Y : { 9.f, -9.f })
	{
		Box(FVector(0, Y, 30), FVector(6, 4, 30), Steel, EArenaSurface::RobotMetal, false, FRotator(0, 0, Y > 0 ? -12.f : 12.f), 0.f, Arm.Wrist);
	}
	// Status LED (Dum-E's "eye").
	Box(FVector(12, 0, 8), FVector(3, 6, 3), ArcBlue, EArenaSurface::Glow, false, FRotator::ZeroRotator, 6.f, Arm.Wrist);
	Arms.Add(Arm);
}

void AChapterSet::Fire(const FVector& Loc, float Size)
{
	Shape(EArenaShape::Cone, Loc, FVector(120, 120, 160) * Size, FRotator::ZeroRotator, Ember, EArenaSurface::Glow, false, 5.f); // flame body ~5k nits
	Shape(EArenaShape::Cone, Loc + FVector(0, 0, 20) * Size, FVector(70, 70, 200) * Size, FRotator::ZeroRotator, FLinearColor(1.f, 0.75f, 0.3f), EArenaSurface::Glow, false, 10.f); // flame core ~10k nits
	UPointLightComponent* L = PointLight(Loc + FVector(0, 0, 120) * Size, FLinearColor(1.f, 0.5f, 0.18f), 6000.f * Size, 1400.f * Size, false);
	FireLights.Add({ L, 6000.f * Size });
	SmokeColumn(Loc + FVector(0, 0, 200) * Size, 5, 260.f * Size, FLinearColor(0.08f, 0.075f, 0.07f));
}

void AChapterSet::SmokeColumn(const FVector& Loc, int32 Puffs, float Size, const FLinearColor& Color)
{
	for (int32 i = 0; i < Puffs; ++i)
	{
		FSetParticle P;
		P.Home = Loc;
		P.Life = Rng.FRandRange(7.f, 11.f);
		P.Age = P.Life * float(i) / float(FMath::Max(1, Puffs));
		P.Velocity = FVector(Rng.FRandRange(-20.f, 40.f), Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(110.f, 170.f));
		P.BaseScale = FVector(Size / 100.f);
		P.Comp = Shape(EArenaShape::Sphere, Loc, FVector(Size), FRotator::ZeroRotator, Color, EArenaSurface::Smoke, false);
		P.PartIndex = Parts.Num() - 1;
		SmokeIdx.Add(Particles.Add(P));
	}
}

void AChapterSet::Crater(const FVector& Loc, float Radius)
{
	Shape(EArenaShape::Sphere, Loc + FVector(0, 0, -Radius * 0.08f), FVector(Radius * 2.f, Radius * 2.f, Radius * 0.28f), FRotator::ZeroRotator,
		FLinearColor(0.55f, 0.5f, 0.45f), EArenaSurface::Rubble, false);
	Shape(EArenaShape::Cylinder, Loc + FVector(0, 0, 4), FVector(Radius * 1.3f, Radius * 1.3f, 6), FRotator::ZeroRotator,
		FLinearColor(0.4f, 0.38f, 0.36f), EArenaSurface::BurnedGround, false);
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Radius * Rng.FRandRange(0.8f, 1.1f);
		const float S = Radius * Rng.FRandRange(0.15f, 0.3f);
		Box(Loc + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, S * 0.3f), FVector(S, S * 0.8f, S * 0.5f),
			FLinearColor(0.6f, 0.58f, 0.55f), EArenaSurface::Concrete, false, FRotator(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-30.f, 30.f)));
	}
}

void AChapterSet::Ruin(const FVector2D& XY, float Yaw, float Width, float Height, int32 Seed)
{
	FRandomStream R(Seed);
	USceneComponent* P = Pivot(Root, FVector(XY.X, XY.Y, 0.f), FRotator(0, Yaw, 0));
	const FLinearColor Wall(0.62f, 0.6f, 0.57f);
	// A blown-out shell: front wall in jagged columns, a side wall stub, floor slabs hanging, rebar.
	const int32 Cols = FMath::Max(3, int32(Width / 400.f));
	const float ColW = Width / Cols;
	for (int32 i = 0; i < Cols; ++i)
	{
		const float H = Height * R.FRandRange(0.25f, 1.f);
		const EArenaSurface S = (i % 3 == 0) ? EArenaSurface::RedBrick : (i % 3 == 1 ? EArenaSurface::RuinedWall : EArenaSurface::Concrete);
		Box(FVector(0, -Width * 0.5f + ColW * (i + 0.5f), H * 0.5f), FVector(60, ColW - 8.f, H), Wall, S, true,
			FRotator(0, 0, R.FRandRange(-3.f, 3.f)), 0.f, P);
		SetLastScalar(TEXT("Wetness"), 0.f);
		if (R.FRand() < 0.4f)
		{
			Shape(EArenaShape::Cylinder, FVector(0, -Width * 0.5f + ColW * (i + 0.5f), H + 60.f), FVector(5, 5, 160), FRotator(R.FRandRange(-25.f, 25.f), 0, R.FRandRange(-25.f, 25.f)),
				FLinearColor(0.35f, 0.2f, 0.12f), EArenaSurface::RustyMetal, false, 0.f, P);
		}
	}
	const float Depth = Width * R.FRandRange(0.4f, 0.8f);
	const float SideH = Height * R.FRandRange(0.4f, 0.9f);
	Box(FVector(-Depth * 0.5f, Width * 0.5f, SideH * 0.5f), FVector(Depth, 60, SideH), Wall, EArenaSurface::RuinedWall, true, FRotator::ZeroRotator, 0.f, P);
	for (int32 f = 1; f * 400.f < Height * 0.8f; ++f)
	{
		Box(FVector(-Depth * 0.45f, R.FRandRange(-Width * 0.2f, Width * 0.2f), f * 400.f), FVector(Depth * 0.8f, Width * R.FRandRange(0.3f, 0.7f), 35),
			Wall * 0.9f, EArenaSurface::Concrete, true, FRotator(R.FRandRange(-12.f, 12.f), 0, R.FRandRange(-10.f, 10.f)), 0.f, P);
	}
	// Rubble pile at the foot.
	for (int32 i = 0; i < 5; ++i)
	{
		const float S = R.FRandRange(80.f, 220.f);
		Box(FVector(R.FRandRange(50.f, 400.f), R.FRandRange(-Width * 0.5f, Width * 0.5f), S * 0.3f), FVector(S, S * 1.2f, S * 0.6f), Wall,
			EArenaSurface::Rubble, false, FRotator(R.FRandRange(-20.f, 20.f), R.FRandRange(0.f, 360.f), R.FRandRange(-20.f, 20.f)), 0.f, P);
	}
}

// =============================================================================== sets

void AChapterSet::BuildWorkshop(bool bNight)
{
	// Tony's lake-house garage: the player starts just inside the open roll-up door (origin), the
	// garage runs back along -X to a glass wall over the lake. Hall of Armor along +Y.
	const float Back = -2000.f, Front = 500.f, Half = 1200.f, H = 700.f;
	const FLinearColor Wall(0.75f, 0.74f, 0.72f);
	Box(FVector((Back + Front) * 0.5f, 0, 3), FVector(Front - Back, Half * 2.f, 6), FLinearColor(0.85f, 0.85f, 0.85f), EArenaSurface::HangarFloor, false);
	// Side walls (+Y holds the armor alcoves).
	Box(FVector((Back + Front) * 0.5f, Half, H * 0.5f), FVector(Front - Back, 40, H), Wall, EArenaSurface::BlockWall, true);
	Box(FVector((Back + Front) * 0.5f, -Half, H * 0.5f), FVector(Front - Back, 40, H), Wall, EArenaSurface::BlockWall, true);
	// Back wall: concrete knee wall + floor-to-ceiling glass towards the lake.
	Box(FVector(Back, 0, 60), FVector(40, Half * 2.f, 120), Wall * 0.8f, EArenaSurface::Concrete, true);
	Box(FVector(Back, 0, 120 + (H - 120) * 0.5f), FVector(10, Half * 2.f, H - 120), FLinearColor(0.85f, 0.9f, 0.95f), EArenaSurface::Glass, true);
	SetLastScalar(TEXT("Opacity"), 0.1f);
	for (int32 i = -3; i <= 3; ++i)
	{
		Box(FVector(Back, i * (Half / 3.5f), H * 0.5f), FVector(30, 14, H), DarkSteel, EArenaSurface::RobotMetal, false);
	}
	// Roof slab with two skylight strips; roll-up door half open at the front.
	for (const float Y : { -800.f, 0.f, 800.f })
	{
		Box(FVector((Back + Front) * 0.5f, Y, H + 25), FVector(Front - Back, 640, 50), Wall * 0.6f, EArenaSurface::Concrete, true);
	}
	for (const float Y : { -400.f, 400.f })
	{
		Box(FVector((Back + Front) * 0.5f, Y, H + 25), FVector(Front - Back, 160, 10), FLinearColor(0.9f, 0.95f, 1.f), EArenaSurface::Glass, false);
	}
	Box(FVector(Front, 0, H - 110), FVector(20, Half * 2.f, 220), FLinearColor(0.6f, 0.62f, 0.64f), EArenaSurface::MetalPlate, true);
	Box(FVector(Front, 0, H - 230), FVector(30, Half * 2.f, 20), Hazard, EArenaSurface::Plain, false);

	// Ceiling light bars + key lights (two shadowed).
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = Back + 350.f + i * 600.f;
		for (const float Y : { -500.f, 500.f })
		{
			Box(FVector(X, Y, H - 12), FVector(380, 26, 8), FLinearColor(0.95f, 0.97f, 1.f), EArenaSurface::Glow, false, FRotator::ZeroRotator, bNight ? 10.f : 6.f);
		}
		SpotLight(FVector(X, 0, H - 30), FRotator(-90, 0, 0), FLinearColor(0.95f, 0.97f, 1.f), bNight ? 14000.f : 9000.f, 1600.f, 40.f, 75.f, i == 1 || i == 2);
	}

	// Workbench with tools + holo table (the Tennessee garage / Stark workshop nod).
	const FVector Bench(-900.f, -800.f, 0.f);
	Box(Bench + FVector(0, 0, 90), FVector(500, 160, 12), FLinearColor(0.4f, 0.4f, 0.42f), EArenaSurface::Steel, true);
	for (const float X : { -230.f, 230.f }) { for (const float Y : { -70.f, 70.f })
	{
		Box(Bench + FVector(X, Y, 45), FVector(10, 10, 90), DarkSteel, EArenaSurface::RobotMetal, false);
	} }
	Box(Bench + FVector(-120, 0, 110), FVector(90, 60, 30), FLinearColor(0.5f, 0.1f, 0.08f), EArenaSurface::SuitPaint, false); // red toolbox
	Shape(EArenaShape::Cylinder, Bench + FVector(80, 20, 110), FVector(40, 40, 30), FRotator::ZeroRotator, FLinearColor(0.75f, 0.3f, 0.2f), EArenaSurface::SuitPaint, false); // spare repulsor
	Shape(EArenaShape::Cylinder, Bench + FVector(80, 20, 126), FVector(26, 26, 2), FRotator::ZeroRotator, ArcBlue, EArenaSurface::Glow, false, 6.f);
	Box(Bench + FVector(180, -60, 160), FVector(6, 120, 80), FLinearColor(0.05f, 0.06f, 0.08f), EArenaSurface::Glass, false, FRotator(0, -20, 0)); // monitor
	Box(Bench + FVector(176, -58, 160), FVector(2, 110, 70), ArcBlue, EArenaSurface::Glow, false, FRotator(0, -20, 0), 0.2f); // monitor ~200 nits
	Box(Bench + FVector(0, -150, 260), FVector(500, 20, 340), FLinearColor(0.3f, 0.3f, 0.3f), EArenaSurface::MetalPlate, false); // pegboard
	const FVector Holo(-700.f, 300.f, 0.f);
	Shape(EArenaShape::Cylinder, Holo + FVector(0, 0, 45), FVector(220, 220, 90), FRotator::ZeroRotator, DarkSteel, EArenaSurface::MetalPlate, true);
	Shape(EArenaShape::Cylinder, Holo + FVector(0, 0, 92), FVector(200, 200, 4), FRotator::ZeroRotator, ArcBlue, EArenaSurface::Glow, false, 0.3f); // holo table top
	Shape(EArenaShape::Sphere, Holo + FVector(0, 0, 170), FVector(110), FRotator::ZeroRotator, ArcBlue, EArenaSurface::Glass, false);
	SetLastScalar(TEXT("Opacity"), 0.06f);
	PointLight(Holo + FVector(0, 0, 170), ArcBlue, 1500.f, 700.f, false);

	// U and Dum-E.
	WorkshopArm(Bench + FVector(-330, 80, 0), 30.f, FLinearColor(0.55f, 0.56f, 0.58f), 0.f, 0.8f);
	WorkshopArm(FVector(-1500, -300, 0), 120.f, FLinearColor(0.5f, 0.5f, 0.52f), 1.7f, 1.1f);

	// Tony's classic hot rod under a work light.
	Car(FVector(-1200.f, -150.f, 0.f), 90.f, FLinearColor(0.12f, 0.17f, 0.32f), false, false);

	// Hall of Armor nod: alcoves along +Y.
	struct FDisplay { FLinearColor P, S; };
	const FDisplay Displays[] = {
		{ FLinearColor(0.45f, 0.44f, 0.42f), FLinearColor(0.3f, 0.3f, 0.3f) },     // Mark I iron
		{ FLinearColor(0.75f, 0.76f, 0.78f), FLinearColor(0.6f, 0.6f, 0.62f) },    // Mark II silver
		{ FLinearColor(0.5f, 0.03f, 0.02f), FLinearColor(0.85f, 0.6f, 0.15f) },    // Mark III hot rod / gold
		{ FLinearColor(0.12f, 0.12f, 0.13f), FLinearColor(0.75f, 0.76f, 0.78f) },  // War Machine-ish gunmetal
		{ FLinearColor(0.55f, 0.04f, 0.04f), FLinearColor(0.8f, 0.55f, 0.12f) },   // later mark
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Displays); ++i)
	{
		const float X = Back + 300.f + i * 420.f;
		Box(FVector(X, Half - 110.f, H * 0.5f), FVector(300, 10, H - 100), FLinearColor(0.08f, 0.08f, 0.09f), EArenaSurface::MetalPlate, false);
		DisplaySuit(FVector(X, Half - 130.f, 0.f), -90.f, Displays[i].P, Displays[i].S, true);
	}

	// Outside: driveway, deck over the cliff, the lake, far hills (behind the garage, -X).
	Box(FVector(Front + 2500.f, 0, 2), FVector(5000, 1600, 4), FLinearColor(0.8f, 0.8f, 0.8f), EArenaSurface::Asphalt, false);
	Box(FVector(Back - 300.f, 0, -10), FVector(600, Half * 2.4f, 20), FLinearColor(0.35f, 0.25f, 0.18f), EArenaSurface::MetalPlate, true);
	Box(FVector(Back - 600.f, 0, 50), FVector(10, Half * 2.4f, 100), FLinearColor(0.9f, 0.95f, 1.f), EArenaSurface::Glass, true);
	// Lake: dark glossy water well below the cliff, out past the arena floor.
	Box(FVector(-30000.f, 0, -1800.f), FVector(40000, 60000, 20), FLinearColor(0.01f, 0.03f, 0.05f), EArenaSurface::SuitPaint, false);
	SetLastScalar(TEXT("Roughness"), 0.03f); SetLastScalar(TEXT("Metallic"), 0.f); SetLastScalar(TEXT("FlakeAmount"), 0.f); SetLastScalar(TEXT("EdgeWear"), 0.f);
	Box(FVector(-10600.f, 0, -900.f), FVector(1400, 20000, 1800), FLinearColor(0.45f, 0.42f, 0.38f), EArenaSurface::Rubble, false); // cliff face
	for (int32 i = 0; i < 7; ++i)
	{
		const float Y = -24000.f + i * 8000.f;
		Shape(EArenaShape::Sphere, FVector(-48000.f + Rng.FRandRange(-4000.f, 4000.f), Y, -1500.f), FVector(16000, 14000, Rng.FRandRange(4000.f, 9000.f)),
			FRotator::ZeroRotator, FLinearColor(0.12f, 0.16f, 0.1f), EArenaSurface::BurnedGround, false);
	}
	if (bNight)
	{
		// Practical lights: warm deck lights + a cool rim from the lake.
		PointLight(FVector(Back - 300.f, -800.f, 250.f), Warm, 2500.f, 1200.f, false);
		PointLight(FVector(Back - 300.f, 800.f, 250.f), Warm, 2500.f, 1200.f, false);
	}
}

void AChapterSet::BuildRooftops(bool bStorm)
{
	// The arena floor is the roof of a huge tower; parapet ring at the edge, plant on the roof,
	// skyline below.
	const float E = 8800.f;
	const FLinearColor Wall(0.6f, 0.6f, 0.58f);
	for (int32 s = 0; s < 4; ++s)
	{
		const FRotator Rot(0, s * 90.f, 0);
		Box(Rot.RotateVector(FVector(E, 0, 60)), FVector(80, E * 2.f, 120), Wall, bStorm ? EArenaSurface::ConcreteWet : EArenaSurface::ConcreteAlt, true, Rot);
	}
	// Helipad.
	Shape(EArenaShape::Cylinder, FVector(3200, -3200, 8), FVector(2200, 2200, 16), FRotator::ZeroRotator, FLinearColor(0.25f, 0.25f, 0.26f),
		bStorm ? EArenaSurface::AsphaltWet : EArenaSurface::Asphalt, false);
	Box(FVector(3200, -3200 - 350, 18), FVector(700, 90, 4), FLinearColor(0.9f, 0.9f, 0.88f), EArenaSurface::Plain, false);
	Box(FVector(3200, -3200 + 350, 18), FVector(700, 90, 4), FLinearColor(0.9f, 0.9f, 0.88f), EArenaSurface::Plain, false);
	Box(FVector(3200, -3200, 18), FVector(90, 700, 4), FLinearColor(0.9f, 0.9f, 0.88f), EArenaSurface::Plain, false);
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = i * 2.f * PI / 12.f;
		Shape(EArenaShape::Sphere, FVector(3200 + FMath::Cos(A) * 1050, -3200 + FMath::Sin(A) * 1050, 20), FVector(30), FRotator::ZeroRotator, Hazard, EArenaSurface::Glow, false, 2.f);
		if (i % 3 == 0) { BlinkParts.Add(Parts.Num() - 1); }
	}
	// HVAC units with heat-haze vents, water tower, stair house, antenna mast.
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector L(-5200.f + (i % 3) * 900.f, 4200.f + (i / 3) * 900.f, 0.f);
		Box(L + FVector(0, 0, 120), FVector(600, 500, 240), FLinearColor(0.7f, 0.7f, 0.68f), EArenaSurface::MetalPlate, true);
		Shape(EArenaShape::Cylinder, L + FVector(0, 0, 250), FVector(300, 300, 20), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, false);
		Shape(EArenaShape::Cylinder, L + FVector(0, 0, 420), FVector(280, 280, 300), FRotator::ZeroRotator, FLinearColor::White, EArenaSurface::HeatHaze, false);
	}
	const FVector Tower(-4000.f, -4500.f, 0.f);
	for (const float X : { -250.f, 250.f }) { for (const float Y : { -250.f, 250.f })
	{
		Shape(EArenaShape::Cylinder, Tower + FVector(X, Y, 350), FVector(30, 30, 700), FRotator::ZeroRotator, FLinearColor(0.3f, 0.2f, 0.12f), EArenaSurface::RustyMetal, true);
	} }
	Shape(EArenaShape::Cylinder, Tower + FVector(0, 0, 1050), FVector(800, 800, 700), FRotator::ZeroRotator, FLinearColor(0.45f, 0.32f, 0.22f), EArenaSurface::RustyMetal, true);
	Shape(EArenaShape::Cone, Tower + FVector(0, 0, 1500), FVector(860, 860, 200), FRotator::ZeroRotator, FLinearColor(0.3f, 0.25f, 0.2f), EArenaSurface::RustyMetal, false);
	Box(FVector(-1500.f, 5200.f, 250.f), FVector(800, 600, 500), Wall, EArenaSurface::BlockWall, true);
	Box(FVector(-1500.f + 401.f, 5200.f, 130.f), FVector(4, 140, 240), DarkSteel, EArenaSurface::MetalPlate, false);
	Box(FVector(-1500.f + 404.f, 5200.f, 270.f), FVector(4, 30, 14), FLinearColor(0.1f, 1.f, 0.2f), EArenaSurface::Glow, false, FRotator::ZeroRotator, 0.2f); // exit sign ~200 nits
	Shape(EArenaShape::Cylinder, FVector(5500.f, 5500.f, 1500.f), FVector(60, 60, 3000), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, true);
	Shape(EArenaShape::Sphere, FVector(5500.f, 5500.f, 3020.f), FVector(80), FRotator::ZeroRotator, FLinearColor(1.f, 0.05f, 0.03f), EArenaSurface::Glow, false, 14.f);
	BlinkParts.Add(Parts.Num() - 1);
	// Roof lights.
	for (int32 i = 0; i < 4; ++i)
	{
		const FRotator Rot(0, 45.f + i * 90.f, 0);
		const FVector L = Rot.RotateVector(FVector(E * 0.9f, 0, 0));
		Shape(EArenaShape::Cylinder, L + FVector(0, 0, 300), FVector(24, 24, 600), FRotator::ZeroRotator, DarkSteel, EArenaSurface::RobotMetal, true);
		SpotLight(L + FVector(0, 0, 600), FRotator(-60, Rot.Yaw + 180.f, 0), Cool, bStorm ? 30000.f : 15000.f, 6000.f, 25.f, 50.f, i == 0);
	}
	// Skyline below (tops between -2000 and +9000 relative to the roof).
	const bool bNight = Preset.bNight || bStorm;
	for (int32 i = 0; i < 26; ++i)
	{
		const float A = i * 2.f * PI / 26.f + Rng.FRandRange(-0.08f, 0.08f);
		const float R = Rng.FRandRange(14000.f, 26000.f);
		const float Top = Rng.FRandRange(-2000.f, 9000.f);
		const float Base = -12000.f;
		const FVector Size(Rng.FRandRange(2500.f, 5000.f), Rng.FRandRange(2500.f, 5000.f), Top - Base);
		const float Gray = Rng.FRandRange(0.3f, 0.6f);
		Building(FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R), Size, Base, FLinearColor(Gray, Gray, Gray * 1.03f), bNight, Rng.FRandRange(0.2f, 0.5f));
	}
}

void AChapterSet::BuildCity(bool bNight, bool bWet)
{
	// A crossroads in the middle of the arena (cleared), towers beyond the play radius.
	const EArenaSurface Road = bWet ? EArenaSurface::AsphaltRain : EArenaSurface::Asphalt;
	const EArenaSurface Walk = bWet ? EArenaSurface::ConcreteWet : EArenaSurface::Concrete;
	const float RoadW = 1800.f, Len = 18000.f;
	Box(FVector(0, 0, 3), FVector(Len, RoadW, 6), FLinearColor(0.85f, 0.85f, 0.85f), Road, false);
	Box(FVector(0, 0, 4), FVector(RoadW, Len, 6), FLinearColor(0.85f, 0.85f, 0.85f), Road, false);
	for (int32 i = -12; i <= 12; ++i)
	{
		if (FMath::Abs(i) <= 1) { continue; }
		Box(FVector(i * 700.f, 0, 8), FVector(320, 18, 2), FLinearColor(0.85f, 0.75f, 0.2f), EArenaSurface::Plain, false);
		Box(FVector(0, i * 700.f, 8), FVector(18, 320, 2), FLinearColor(0.85f, 0.75f, 0.2f), EArenaSurface::Plain, false);
	}
	for (int32 i = -4; i <= 4; ++i)
	{
		Box(FVector(RoadW * 0.5f + 300.f, i * 140.f, 8), FVector(500, 70, 2), FLinearColor(0.9f, 0.9f, 0.9f), EArenaSurface::Plain, false);
	}
	// Sidewalk kerbs along the four blocks.
	for (int32 q = 0; q < 4; ++q)
	{
		const float SX = (q & 1) ? 1.f : -1.f, SY = (q & 2) ? 1.f : -1.f;
		const float C = RoadW * 0.5f + 400.f, Ext = Len * 0.5f - C;
		Box(FVector(SX * (C + Ext * 0.5f - 200.f), SY * (RoadW * 0.5f + 200.f), 12), FVector(Ext + 400.f, 400, 24), FLinearColor(0.75f, 0.74f, 0.72f), Walk, true);
		Box(FVector(SX * (RoadW * 0.5f + 200.f), SY * (C + Ext * 0.5f - 200.f), 12), FVector(400, Ext + 400.f, 24), FLinearColor(0.75f, 0.74f, 0.72f), Walk, true);
	}
	// Street lamps, parked / abandoned cars, barriers.
	for (int32 i = -5; i <= 5; ++i)
	{
		if (i == 0) { continue; }
		StreetLamp(FVector(i * 1500.f, RoadW * 0.5f + 150.f, 24), -90.f, bNight || bWet);
		StreetLamp(FVector(RoadW * 0.5f + 150.f, i * 1500.f, 24), 180.f, bNight || bWet);
	}
	const FLinearColor Paints[] = { FLinearColor(0.6f, 0.6f, 0.62f), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.45f, 0.03f, 0.03f),
		FLinearColor(0.05f, 0.12f, 0.3f), FLinearColor(0.8f, 0.8f, 0.78f), FLinearColor(0.1f, 0.25f, 0.12f) };
	for (int32 i = 0; i < 10; ++i)
	{
		const bool bAlongX = (i % 2) == 0;
		const float Along = (i < 5 ? -1.f : 1.f) * Rng.FRandRange(2500.f, 6000.f);
		const float Across = (Rng.FRand() < 0.5f ? -1.f : 1.f) * RoadW * 0.3f;
		const FVector L = bAlongX ? FVector(Along, Across, 0) : FVector(Across, Along, 0);
		Car(L, (bAlongX ? 0.f : 90.f) + Rng.FRandRange(-25.f, 25.f), Paints[i % UE_ARRAY_COUNT(Paints)], i == 3 || i == 8, bNight && i % 3 == 0);
	}
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector L(Rng.FRandRange(-1200.f, 1200.f) + (i < 3 ? 3500.f : -3500.f), Rng.FRandRange(-600.f, 600.f), 0.f);
		Box(L + FVector(0, 0, 50), FVector(300, 60, 100), FLinearColor(0.75f, 0.74f, 0.7f), Walk, true, FRotator(0, Rng.FRandRange(-30.f, 30.f), 0));
		Box(L + FVector(0, 0, 95), FVector(304, 64, 12), FLinearColor(0.9f, 0.4f, 0.05f), EArenaSurface::Plain, false, FRotator(0, Rng.FRandRange(-30.f, 30.f), 0));
	}
	// Towers on the four blocks, outside the play radius.
	for (int32 q = 0; q < 4; ++q)
	{
		const float SX = (q & 1) ? 1.f : -1.f, SY = (q & 2) ? 1.f : -1.f;
		for (int32 k = 0; k < 4; ++k)
		{
			const float X = SX * (RoadW * 0.5f + 700.f + 2600.f * 0.5f + (k % 2) * 3600.f + Rng.FRandRange(0.f, 400.f));
			const float Y = SY * (RoadW * 0.5f + 700.f + 2600.f * 0.5f + (k / 2) * 3600.f + Rng.FRandRange(0.f, 400.f));
			if (FVector2D(X, Y).Size() < 4200.f) { continue; }
			const float Hgt = Rng.FRandRange(4000.f, 16000.f);
			const float G = Rng.FRandRange(0.35f, 0.7f);
			Building(FVector2D(X, Y), FVector(Rng.FRandRange(2200.f, 3000.f), Rng.FRandRange(2200.f, 3000.f), Hgt), 0.f,
				FLinearColor(G, G * 0.98f, G * 0.95f), bNight || bWet, bNight ? 0.4f : 0.15f);
		}
	}
	// Far ring so the street does not end at the void.
	for (int32 i = 0; i < 20; ++i)
	{
		const float A = i * 2.f * PI / 20.f;
		const float R = Rng.FRandRange(16000.f, 22000.f);
		const float G = Rng.FRandRange(0.3f, 0.55f);
		Building(FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R), FVector(Rng.FRandRange(3000.f, 5000.f), Rng.FRandRange(3000.f, 5000.f), Rng.FRandRange(6000.f, 22000.f)),
			0.f, FLinearColor(G, G, G), bNight || bWet, 0.35f);
	}
	if (bNight) { Fire(FVector(4800.f, 300.f, 0.f), 0.8f); }
}

void AChapterSet::BuildRuins(bool bBattlefield)
{
	const int32 RuinCount = bBattlefield ? 7 : 12;
	for (int32 i = 0; i < RuinCount; ++i)
	{
		const float A = i * 2.f * PI / RuinCount + Rng.FRandRange(-0.15f, 0.15f);
		const float R = Rng.FRandRange(PlayRadius + 800.f, 9500.f);
		Ruin(FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R), FMath::RadiansToDegrees(A) + 180.f,
			Rng.FRandRange(1400.f, 2800.f), Rng.FRandRange(900.f, bBattlefield ? 1800.f : 3200.f), 100 + i);
	}
	// Low cover inside the arena: broken slabs, burnt cars, craters, fires.
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Rng.FRandRange(1800.f, 5500.f);
		Box(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 70), FVector(Rng.FRandRange(300.f, 700.f), Rng.FRandRange(150.f, 300.f), 140),
			FLinearColor(0.6f, 0.58f, 0.55f), EArenaSurface::DamagedFloor, true, FRotator(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-10.f, 10.f)));
	}
	for (int32 i = 0; i < (bBattlefield ? 4 : 3); ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Rng.FRandRange(2500.f, 6000.f);
		Car(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0), Rng.FRandRange(0.f, 360.f), FLinearColor::Black, true, false);
	}
	for (int32 i = 0; i < (bBattlefield ? 7 : 3); ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Rng.FRandRange(1500.f, 6000.f);
		Crater(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0), Rng.FRandRange(400.f, 900.f));
	}
	if (bBattlefield)
	{
		// Titan's shards: huge obsidian spikes driven into the ground, with molten veins.
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = i * 2.f * PI / 9.f + 0.3f;
			const float R = Rng.FRandRange(6000.f, 11000.f);
			const float Hgt = Rng.FRandRange(2000.f, 5000.f);
			const FVector L(FMath::Cos(A) * R, FMath::Sin(A) * R, Hgt * 0.4f);
			const FRotator Tilt(Rng.FRandRange(-25.f, 25.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-25.f, 25.f));
			Shape(EArenaShape::Cone, L, FVector(Hgt * 0.3f, Hgt * 0.25f, Hgt), Tilt, FLinearColor(0.015f, 0.012f, 0.02f), EArenaSurface::SuitPaint, true);
			SetLastScalar(TEXT("Roughness"), 0.08f); SetLastScalar(TEXT("Metallic"), 0.2f); SetLastScalar(TEXT("FlakeAmount"), 0.6f);
			SetLastScalar(TEXT("HeatGlow"), 0.5f);
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			Fire(FVector(FMath::Cos(A) * 5200.f, FMath::Sin(A) * 5200.f, 0), Rng.FRandRange(0.8f, 1.4f));
		}
		SmokeColumn(FVector(-9000.f, 4000.f, 500.f), 8, 1600.f, FLinearColor(0.06f, 0.055f, 0.05f));
		SmokeColumn(FVector(8000.f, -6000.f, 500.f), 8, 1800.f, FLinearColor(0.07f, 0.065f, 0.06f));
	}
	else
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			Fire(FVector(FMath::Cos(A) * 4500.f, FMath::Sin(A) * 4500.f, 0), 0.9f);
		}
		SmokeColumn(FVector(9000.f, 2000.f, 300.f), 7, 1300.f, FLinearColor(0.35f, 0.33f, 0.3f)); // dust
	}
}

void AChapterSet::BuildWeather()
{
	if (Preset.bEmbers)
	{
		for (int32 i = 0; i < 48; ++i)
		{
			FSetParticle P;
			P.Home = FVector(Rng.FRandRange(-5000.f, 5000.f), Rng.FRandRange(-5000.f, 5000.f), Rng.FRandRange(0.f, 300.f));
			P.Life = Rng.FRandRange(4.f, 9.f);
			P.Age = Rng.FRandRange(0.f, P.Life);
			P.Velocity = FVector(Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(80.f, 200.f));
			P.BaseScale = FVector(Rng.FRandRange(0.05f, 0.1f));
			P.Comp = Shape(EArenaShape::Sphere, P.Home, P.BaseScale * 100.f, FRotator::ZeroRotator, Ember, EArenaSurface::Glow, false, 20.f);
			P.PartIndex = Parts.Num() - 1;
			EmberIdx.Add(Particles.Add(P));
		}
	}
	if (Preset.bWet && Preset.bNight)
	{
		// Rain streaks kept in a box around the player.
		for (int32 i = 0; i < 90; ++i)
		{
			FSetParticle P;
			P.Home = FVector(Rng.FRandRange(-2500.f, 2500.f), Rng.FRandRange(-2500.f, 2500.f), Rng.FRandRange(0.f, 2500.f));
			P.Velocity = FVector(120.f, 60.f, -2600.f);
			P.BaseScale = FVector(0.012f, 0.012f, 1.4f);
			P.Comp = Shape(EArenaShape::Cylinder, P.Home, P.BaseScale * 100.f, FRotator::ZeroRotator, FLinearColor(0.7f, 0.8f, 0.95f), EArenaSurface::Glow, false, 0.004f); // rain streak: only catches light
			P.PartIndex = Parts.Num() - 1;
			RainIdx.Add(Particles.Add(P));
		}
	}
}

// =============================================================================== tick

void AChapterSet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	for (FWorkshopArm& Arm : Arms)
	{
		const float T = Clock * Arm.Speed + Arm.Phase;
		// Idle "working" loop: sweep, dip to the bench, a curious head-tilt now and then.
		if (Arm.Base)     { Arm.Base->SetRelativeRotation(FRotator(0, Arm.Base->GetRelativeRotation().Yaw + FMath::Sin(T * 0.7f) * 8.f * DeltaSeconds, 0)); }
		if (Arm.Shoulder) { Arm.Shoulder->SetRelativeRotation(FRotator(-20.f + FMath::Sin(T * 0.9f) * 18.f, 0, 0)); }
		if (Arm.Elbow)    { Arm.Elbow->SetRelativeRotation(FRotator(-55.f + FMath::Sin(T * 1.3f + 0.6f) * 25.f, 0, 0)); }
		if (Arm.Wrist)    { Arm.Wrist->SetRelativeRotation(FRotator(-25.f + FMath::Sin(T * 2.1f) * 20.f, FMath::Sin(T * 0.5f) * 40.f, FMath::Sin(T * 0.37f) * 25.f)); }
	}

	const bool bBlinkOn = FMath::Fmod(Clock, 1.5f) < 0.18f;
	for (const int32 Index : BlinkParts)
	{
		if (Parts.IsValidIndex(Index)) { ArenaVisuals::SetPartGlow(Parts[Index], bBlinkOn ? 14.f : 0.2f); }
	}
	for (const auto& Flame : FireLights)
	{
		if (Flame.Key)
		{
			const float N = 0.75f + 0.15f * FMath::Sin(Clock * 13.f + Flame.Value) + 0.1f * FMath::Sin(Clock * 29.f + Flame.Value * 0.3f);
			Flame.Key->SetIntensity(Flame.Value * N);
		}
	}
	TickParticles(DeltaSeconds);
}

void AChapterSet::TickParticles(float DeltaSeconds)
{
	if (Particles.Num() == 0) { return; }
	const FTransform& Xf = GetActorTransform();

	for (const int32 i : EmberIdx)
	{
		FSetParticle& P = Particles[i];
		if (!P.Comp) { continue; }
		P.Age += DeltaSeconds;
		if (P.Age > P.Life) { P.Age = 0.f; }
		const float Sway = FMath::Sin((P.Age + P.Life) * 3.f) * 40.f;
		const FVector L = P.Home + P.Velocity * P.Age + FVector(Sway, -Sway * 0.6f, 0.f);
		P.Comp->SetRelativeLocation(L);
		const float Fade = FMath::Clamp(1.f - P.Age / P.Life, 0.f, 1.f);
		P.Comp->SetRelativeScale3D(P.BaseScale * (0.3f + 0.7f * Fade));
	}

	for (const int32 i : SmokeIdx)
	{
		FSetParticle& P = Particles[i];
		if (!P.Comp) { continue; }
		P.Age += DeltaSeconds;
		if (P.Age > P.Life) { P.Age = 0.f; }
		const float K = P.Age / P.Life;
		P.Comp->SetRelativeLocation(P.Home + P.Velocity * P.Age);
		P.Comp->SetRelativeScale3D(P.BaseScale * (0.6f + 2.2f * K));
		if (Parts.IsValidIndex(P.PartIndex) && Parts[P.PartIndex].MID)
		{
			Parts[P.PartIndex].MID->SetScalarParameterValue(TEXT("Opacity"), 0.45f * FMath::Sin(K * PI));
		}
	}

	if (RainIdx.Num() > 0)
	{
		// Rain falls in a 5000 uu box that follows the view.
		FVector Center = FVector::ZeroVector;
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (PC->PlayerCameraManager) { Center = Xf.InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation()); }
		}
		for (const int32 i : RainIdx)
		{
			FSetParticle& P = Particles[i];
			if (!P.Comp) { continue; }
			P.Home += P.Velocity * DeltaSeconds;
			FVector Rel = P.Home - Center;
			if (Rel.Z < -1200.f) { Rel.Z += 2500.f; }
			if (Rel.X > 2500.f) { Rel.X -= 5000.f; } else if (Rel.X < -2500.f) { Rel.X += 5000.f; }
			if (Rel.Y > 2500.f) { Rel.Y -= 5000.f; } else if (Rel.Y < -2500.f) { Rel.Y += 5000.f; }
			P.Home = Center + Rel;
			P.Comp->SetRelativeLocation(P.Home);
		}
	}
}

// =============================================================================== arena city

UMaterialInterface* AChapterSet::EnvMaterial(const TCHAR* Name)
{
	if (!Name || !*Name) { return nullptr; }
	static TMap<FName, TWeakObjectPtr<UMaterialInterface>> Cache;
	const FName Key(Name);
	if (const TWeakObjectPtr<UMaterialInterface>* Hit = Cache.Find(Key))
	{
		if (Hit->IsValid()) { return Hit->Get(); }
	}
	const FString Pkg = FString::Printf(TEXT("/Game/Env/Materials/%s"), Name);
	UMaterialInterface* Mat = FPackageName::DoesPackageExist(Pkg)
		? LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, Name)) : nullptr;
	if (Mat) { Cache.Add(Key, Mat); }
	return Mat;
}

UMaterialInstanceDynamic* AChapterSet::SharedMID(const FString& Key, UMaterialInterface* Parent, const FLinearColor& Color, float Emissive)
{
	if (const TObjectPtr<UMaterialInstanceDynamic>* Hit = SharedMIDs.Find(Key)) { return Hit->Get(); }
	if (!Parent) { return nullptr; }
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Mid->SetScalarParameterValue(NAME_Emissive, ArenaVisuals::EmissiveToNits(Emissive));
	SharedMIDs.Add(Key, Mid);
	return Mid;
}

UStaticMeshComponent* AChapterSet::CityBox(const FVector& Center, const FVector& Size, float Yaw, const TCHAR* EnvMI, EArenaSurface Fallback,
	const FLinearColor& Color, bool bSolid, float Emissive)
{
	UStaticMeshComponent* C = Box(Center, Size, Color, Fallback, bSolid, FRotator(0.f, Yaw, 0.f), Emissive);
	if (!C || Parts.Num() == 0) { return C; }
	UMaterialInterface* Parent = EnvMaterial(EnvMI);
	const bool bEnv = Parent != nullptr;
	if (!Parent) { Parent = ArenaVisuals::GetSurfaceMaterial(ArenaVisuals::ResolveSurface(Parts.Last())); }
	const FString Key = FString::Printf(TEXT("%s|%d|%s|%.2f"), bEnv ? EnvMI : TEXT("-"), int32(Fallback), *Color.ToString(), Emissive);
	if (UMaterialInstanceDynamic* Mid = SharedMID(Key, Parent, Color, Emissive))
	{
		C->SetMaterial(0, Mid);
		Parts.Last().MID = Mid;
	}
	return C;
}

UStaticMeshComponent* AChapterSet::EnvMesh(const TCHAR* Id, const FVector& Loc, float Yaw, bool bSolid, float Scale)
{
	static TMap<FName, TWeakObjectPtr<UStaticMesh>> Cache;
	const FName Key(Id);
	UStaticMesh* Mesh = nullptr;
	if (const TWeakObjectPtr<UStaticMesh>* Hit = Cache.Find(Key)) { Mesh = Hit->Get(); }
	if (!Mesh)
	{
		const FString Pkg = FString::Printf(TEXT("/Game/Env/Props/%s/SM_%s"), Id, Id);
		if (FPackageName::DoesPackageExist(Pkg))
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.SM_%s"), *Pkg, Id));
		}
		if (!Mesh) { return nullptr; }
		Cache.Add(Key, Mesh);
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Root);
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	C->SetRelativeScale3D(FVector(Scale));
	if (bSolid)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	AddInstanceComponent(C);
	EnvMeshes.Add(C);
	return C;
}

namespace
{
	constexpr float CityPitch = 6000.f;   // road centre to road centre
	constexpr float CityRoadW = 1800.f;   // four lanes
	constexpr float CityWalk = 450.f;     // sidewalk depth
	constexpr int32 CityHalf = 4;         // roads at k * CityPitch, k = -4..4
	const FLinearColor PaveTint(0.92f, 0.92f, 0.9f);
	const FLinearColor CurbTint(0.78f, 0.77f, 0.75f);
	const FLinearColor RoadTint(0.95f, 0.95f, 0.95f);
	const FLinearColor StripeTint(0.85f, 0.85f, 0.82f);
}

void AChapterSet::BuildArenaCity(bool bNight)
{
	Rng.Initialize(20261030);
	const float Extent = CityHalf * CityPitch + CityRoadW * 0.5f;

	// Ground beyond the grid so the horizon is never a void (fog hides the far edge).
	if (UStaticMeshComponent* G = CityBox(FVector(0, 0, -6), FVector(240000.f, 240000.f, 8.f), 0.f, TEXT("MI_Env_Ground"), EArenaSurface::DebrisGround, FLinearColor(0.85f, 0.85f, 0.82f), false))
	{
		G->SetCastShadow(false);
	}

	// Roads: segments between intersections (the road texture runs along each box's local X).
	for (int32 k = -CityHalf; k <= CityHalf; ++k)
	{
		const bool bMain = FMath::Abs(k) <= 2;
		const TCHAR* RoadMI = bMain ? TEXT("MI_Env_Road4Lane") : TEXT("MI_Env_Road2Lane");
		for (int32 j = -CityHalf; j < CityHalf; ++j)
		{
			const float Mid = (j + 0.5f) * CityPitch;
			const float Len = CityPitch - CityRoadW;
			UStaticMeshComponent* A = CityBox(FVector(Mid, k * CityPitch, 2), FVector(Len, CityRoadW, 4), 0.f, RoadMI, EArenaSurface::Asphalt, RoadTint, false);
			UStaticMeshComponent* B = CityBox(FVector(k * CityPitch, Mid, 2), FVector(Len, CityRoadW, 4), 90.f, RoadMI, EArenaSurface::Asphalt, RoadTint, false);
			if (A) { A->SetCastShadow(false); }
			if (B) { B->SetCastShadow(false); }
			// Manhole covers in the lanes, every other segment.
			if (((j + k) & 1) == 0)
			{
				EnvMesh(TEXT("water_manhole_cover"), FVector(Mid + Rng.FRandRange(-900.f, 900.f), k * CityPitch + 450.f, 4), Rng.FRandRange(0.f, 360.f), false);
				EnvMesh(TEXT("water_manhole_cover"), FVector(k * CityPitch - 450.f, Mid + Rng.FRandRange(-900.f, 900.f), 4), Rng.FRandRange(0.f, 360.f), false);
			}
		}
		for (int32 j = -CityHalf; j <= CityHalf; ++j)
		{
			if (UStaticMeshComponent* X = CityBox(FVector(j * CityPitch, k * CityPitch, 1.5f), FVector(CityRoadW, CityRoadW, 3), 0.f, nullptr, EArenaSurface::Asphalt, FLinearColor(0.55f, 0.55f, 0.56f), false))
			{
				X->SetCastShadow(false);
			}
			// Zebra crossings on the intersections around the arena.
			if (FMath::Abs(k) <= 1 && FMath::Abs(j) <= 1)
			{
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const float Yaw = Side * 90.f;
					const FVector Out = FRotator(0, Yaw, 0).RotateVector(FVector(CityRoadW * 0.5f + 220.f, 0, 0));
					const FVector Along = FRotator(0, Yaw, 0).RotateVector(FVector(0, 1, 0));
					for (int32 s = -3; s <= 3; ++s)
					{
						if (UStaticMeshComponent* Z = CityBox(FVector(j * CityPitch, k * CityPitch, 4.5f) + Out + Along * (s * 220.f),
							FVector(320.f, 110.f, 1.f), Yaw, nullptr, EArenaSurface::Plain, StripeTint, false))
						{
							Z->SetCastShadow(false);
						}
					}
				}
			}
		}
	}

	// Blocks: the four around the origin are plazas inside the arena; the rest carry buildings.
	const float Block = CityPitch - CityRoadW;
	for (int32 i = -CityHalf; i < CityHalf; ++i)
	{
		for (int32 j = -CityHalf; j < CityHalf; ++j)
		{
			const FVector2D C((i + 0.5f) * CityPitch, (j + 0.5f) * CityPitch);
			const bool bPlaza = (i == -1 || i == 0) && (j == -1 || j == 0);
			if (bPlaza) { CityPlaza(C, Block, (i + 1) * 2 + (j + 1), bNight); }
			else { CityBuildingBlock(C, Block, FMath::Abs(i + 0.5f) < 2.f && FMath::Abs(j + 0.5f) < 2.f, bNight); }
		}
	}

	// Parked cars (covered) along the kerbs of the inner roads.
	for (int32 n = 0; n < 14; ++n)
	{
		const int32 k = Rng.RandRange(-1, 1);
		const float Along = (Rng.FRand() < 0.5f ? -1.f : 1.f) * Rng.FRandRange(1400.f, CityPitch * 1.5f);
		const float Lane = (Rng.FRand() < 0.5f ? -1.f : 1.f) * (CityRoadW * 0.5f - 160.f);
		const bool bAlongX = (n & 1) == 0;
		const FVector L = bAlongX ? FVector(Along, k * CityPitch + Lane, 4) : FVector(k * CityPitch + Lane, Along, 4);
		if (FMath::Abs(Along - FMath::RoundToFloat(Along / CityPitch) * CityPitch) < CityRoadW * 0.6f) { continue; } // keep intersections clear
		const float Yaw = (bAlongX ? 90.f : 0.f) + Rng.FRandRange(-3.f, 3.f); // the mesh's length runs along its Y
		if (!EnvMesh(TEXT("covered_car"), L, Yaw, true))
		{
			Car(L, bAlongX ? 0.f : 90.f, FLinearColor(0.3f, 0.3f, 0.32f), false, false);
		}
	}

	// Skyline: far silhouettes past the grid.
	for (int32 n = 0; n < 28; ++n)
	{
		const float Ang = n * 2.f * PI / 28.f + Rng.FRandRange(-0.05f, 0.05f);
		const float R = Rng.FRandRange(Extent + 6000.f, Extent + 20000.f);
		const float H = Rng.FRandRange(8000.f, 30000.f);
		const FVector2D P(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R);
		const bool bGlass = (n % 3) != 0;
		CityBox(FVector(P.X, P.Y, H * 0.5f), FVector(Rng.FRandRange(3000.f, 6000.f), Rng.FRandRange(3000.f, 6000.f), H), Rng.FRandRange(0.f, 90.f),
			bGlass ? TEXT("MI_Env_GlassTower") : TEXT("MI_Env_ConcreteTiles"), EArenaSurface::Facade, FLinearColor(0.8f, 0.82f, 0.85f), false);
	}
}

void AChapterSet::CityPlaza(const FVector2D& Center, float Size, int32 Seed, bool bNight)
{
	FRandomStream R(9100 + Seed);
	const FVector C3(Center.X, Center.Y, 0.f);
	// Raised paved plaza with a kerb all round.
	if (UStaticMeshComponent* S = CityBox(C3 + FVector(0, 0, 8), FVector(Size, Size, 16), 0.f, TEXT("MI_Env_Pavement"), EArenaSurface::Concrete, PaveTint, true))
	{
		S->SetCastShadow(false);
	}
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const float Yaw = Side * 90.f;
		const FVector Out = FRotator(0, Yaw, 0).RotateVector(FVector(Size * 0.5f - 15.f, 0, 0));
		CityBox(C3 + Out + FVector(0, 0, 10), FVector(30.f, Size, 20.f), Yaw, TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete, CurbTint, true);
	}
	// Tactile paving at the corner facing the central intersection.
	const FVector2D ToOrigin = (-Center).GetSafeNormal();
	const FVector Corner = C3 + FVector(FMath::Sign(ToOrigin.X) * (Size * 0.5f - 200.f), FMath::Sign(ToOrigin.Y) * (Size * 0.5f - 200.f), 17.f);
	CityBox(Corner, FVector(360.f, 360.f, 2.f), 0.f, TEXT("MI_Env_Tactile"), EArenaSurface::Plain, FLinearColor(0.9f, 0.75f, 0.2f), false);

	auto Rand2 = [&R, Size](float Margin) { return FVector(R.FRandRange(-Size * 0.5f + Margin, Size * 0.5f - Margin), R.FRandRange(-Size * 0.5f + Margin, Size * 0.5f - Margin), 16.f); };
	const float Top = 16.f;

	// Lamps on the four corners (lights only at night, unshadowed).
	for (int32 q = 0; q < 4; ++q)
	{
		const FVector L = C3 + FVector(((q & 1) ? 1.f : -1.f) * (Size * 0.5f - 250.f), ((q & 2) ? 1.f : -1.f) * (Size * 0.5f - 250.f), Top);
		if (EnvMesh(TEXT("street_lamp_01"), L, R.FRandRange(0.f, 360.f), true, 1.5f))
		{
			if (bNight) { PointLight(L + FVector(0, 0, 560), Warm, 9000.f, 1600.f, false); }
		}
		else
		{
			StreetLamp(L, q * 90.f, bNight);
		}
	}
	// Street furniture.
	EnvMesh(TEXT("modular_street_seating"), C3 + FVector(0, -Size * 0.5f + 500.f, Top), 0.f, true);
	EnvMesh(TEXT("modular_street_seating"), C3 + FVector(-Size * 0.5f + 500.f, 0, Top), 90.f, true);
	EnvMesh(TEXT("metal_trash_can"), C3 + FVector(Size * 0.5f - 350.f, -Size * 0.25f, Top), R.FRandRange(0.f, 360.f), true);
	EnvMesh(TEXT("metal_trash_can"), C3 + FVector(-Size * 0.25f, Size * 0.5f - 350.f, Top), R.FRandRange(0.f, 360.f), true);
	EnvMesh(TEXT("fire_hydrant"), C3 + FVector(Size * 0.5f - 300.f, Size * 0.5f - 600.f, Top), R.FRandRange(0.f, 360.f), true);
	EnvMesh(TEXT("utility_box_01"), C3 + FVector(-Size * 0.5f + 300.f, -Size * 0.5f + 700.f, Top), 90.f, true);
	EnvMesh(TEXT("utility_box_02"), C3 + FVector(-Size * 0.5f + 300.f, -Size * 0.5f + 900.f, Top), 90.f, true);
	for (int32 n = 0; n < 3; ++n) { EnvMesh(TEXT("trashbag"), C3 + FVector(Size * 0.5f - 420.f, -Size * 0.25f + 90.f * (n + 1), Top), R.FRandRange(0.f, 360.f), false); }

	// Cover: barrier lines, crate stacks, barrels, a covered car, tyres.
	for (int32 n = 0; n < 3; ++n)
	{
		const FVector L = C3 + Rand2(600.f);
		const float Yaw = R.FRandRange(0.f, 180.f);
		const FVector Step = FRotator(0, Yaw, 0).RotateVector(FVector(160.f, 0, 0));
		for (int32 b = 0; b < 3; ++b)
		{
			if (!EnvMesh(b == 1 ? TEXT("concrete_road_barrier_02") : TEXT("concrete_road_barrier"), L + Step * (b - 1), Yaw, true))
			{
				CityBox(L + Step * (b - 1) + FVector(0, 0, 50), FVector(150, 60, 100), Yaw, TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete, CurbTint, true);
			}
		}
	}
	for (int32 n = 0; n < 2; ++n)
	{
		const FVector L = C3 + Rand2(700.f);
		const float Yaw = R.FRandRange(0.f, 90.f);
		if (EnvMesh(TEXT("wooden_military_crate"), L, Yaw, true))
		{
			EnvMesh(TEXT("wooden_military_crate"), L + FVector(0, 0, 46), Yaw + R.FRandRange(-8.f, 8.f), true);
			EnvMesh(TEXT("wooden_crate_01"), L + FRotator(0, Yaw, 0).RotateVector(FVector(0, 80, 0)), Yaw + 5.f, true);
			EnvMesh(TEXT("wooden_crate_02"), L + FRotator(0, Yaw, 0).RotateVector(FVector(140, 60, 0)), Yaw - 12.f, true);
			EnvMesh(TEXT("plastic_crate_01"), L + FVector(0, 0, 92), Yaw + 20.f, false);
		}
		else
		{
			CityBox(L + FVector(0, 0, 60), FVector(240, 120, 120), Yaw, TEXT("MI_Env_Metal"), EArenaSurface::MetalPlate, FLinearColor(0.5f, 0.45f, 0.35f), true);
		}
	}
	for (int32 n = 0; n < 3; ++n) { EnvMesh(TEXT("barrel_03"), C3 + Rand2(500.f), R.FRandRange(0.f, 360.f), true); }
	for (int32 n = 0; n < 2; ++n) { EnvMesh(TEXT("old_tyre"), C3 + Rand2(500.f) + FVector(0, 0, 8), R.FRandRange(0.f, 360.f), false); }
	if ((Seed & 1) == 0)
	{
		EnvMesh(TEXT("covered_car"), C3 + Rand2(900.f), R.FRandRange(0.f, 360.f), true);
	}
	else
	{
		// Rubble pile (a crashed drone's work): tumbled concrete chunks.
		const FVector P = C3 + Rand2(800.f);
		for (int32 n = 0; n < 10; ++n)
		{
			const float S = R.FRandRange(40.f, 160.f);
			Box(P + FVector(R.FRandRange(-260.f, 260.f), R.FRandRange(-260.f, 260.f), S * 0.3f), FVector(S, S * R.FRandRange(0.6f, 1.2f), S * R.FRandRange(0.4f, 0.9f)),
				FLinearColor(0.8f, 0.78f, 0.75f), EArenaSurface::Rubble, S > 110.f, FRotator(R.FRandRange(-25.f, 25.f), R.FRandRange(0.f, 360.f), R.FRandRange(-25.f, 25.f)));
			if (UMaterialInstanceDynamic* Mid = SharedMID(TEXT("debris"), EnvMaterial(TEXT("MI_Env_Debris")), FLinearColor(0.8f, 0.78f, 0.75f)))
			{
				Parts.Last().Component->SetMaterial(0, Mid);
				Parts.Last().MID = Mid;
			}
		}
	}
}

void AChapterSet::CityBuildingBlock(const FVector2D& Center, float Size, bool bInnerRing, bool bNight)
{
	const FVector C3(Center.X, Center.Y, 0.f);
	if (UStaticMeshComponent* S = CityBox(C3 + FVector(0, 0, 8), FVector(Size, Size, 16), 0.f, TEXT("MI_Env_Pavement"), EArenaSurface::Concrete, PaveTint, bInnerRing))
	{
		S->SetCastShadow(false);
	}
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const float Yaw = Side * 90.f;
		const FVector Out = FRotator(0, Yaw, 0).RotateVector(FVector(Size * 0.5f - 15.f, 0, 0));
		CityBox(C3 + Out + FVector(0, 0, 10), FVector(30.f, Size, 20.f), Yaw, TEXT("MI_Env_ConcreteWall"), EArenaSurface::Concrete, CurbTint, false);
	}

	// One, two or four buildings on the lot.
	const float Lot = Size - 2.f * CityWalk;
	const int32 Split = Rng.RandRange(0, 2); // 0 = one, 1 = two, 2 = four
	const int32 NX = Split == 0 ? 1 : 2;
	const int32 NY = Split == 2 ? 2 : 1;
	const float Dist = Center.Size();
	for (int32 bx = 0; bx < NX; ++bx)
	{
		for (int32 by = 0; by < NY; ++by)
		{
			const float W = Lot / NX - (NX > 1 ? 60.f : 0.f);
			const float D = Lot / NY - (NY > 1 ? 60.f : 0.f);
			const FVector2D XY(Center.X - Lot * 0.5f + (bx + 0.5f) * (Lot / NX), Center.Y - Lot * 0.5f + (by + 0.5f) * (Lot / NY));
			const float MaxH = bInnerRing ? 9000.f : FMath::Lerp(9000.f, 22000.f, FMath::Clamp((Dist - 9000.f) / 18000.f, 0.f, 1.f));
			const float H = Rng.FRandRange(bInnerRing ? 2400.f : 3500.f, MaxH);
			const int32 Style = H < 3200.f ? 3 : Rng.RandRange(0, 2);
			const FVector Sz(W, D, H);
			const FVector Mid(XY.X, XY.Y, H * 0.5f);

			// Podium: a 4.5 m ground floor in concrete (storefront level).
			CityBox(FVector(XY.X, XY.Y, 225.f), FVector(W + 30.f, D + 30.f, 450.f), 0.f, TEXT("MI_Env_ConcreteWall"), EArenaSurface::BlockWall, FLinearColor(0.82f, 0.8f, 0.78f), bInnerRing);
			switch (Style)
			{
			case 0:
				// Procedural window grid (M_Facade): its own instance for floor height / lit windows.
				Box(Mid + FVector(0, 0, 225.f), Sz - FVector(0, 0, 450.f), FLinearColor(Rng.FRandRange(0.4f, 0.7f), Rng.FRandRange(0.4f, 0.7f), Rng.FRandRange(0.42f, 0.72f)), EArenaSurface::Facade, bInnerRing);
				SetLastScalar(TEXT("LitFraction"), bNight ? 0.4f : 0.12f);
				SetLastScalar(NAME_Emissive, ArenaVisuals::EmissiveToNits(bNight ? 0.15f : 0.f));
				SetLastScalar(TEXT("FloorHeight"), 360.f + Rng.FRandRange(0.f, 60.f));
				SetLastScalar(TEXT("WindowWidth"), 200.f + Rng.FRandRange(0.f, 140.f));
				break;
			case 1:
				CityBox(Mid + FVector(0, 0, 225.f), Sz - FVector(0, 0, 450.f), 0.f, TEXT("MI_Env_GlassTower"), EArenaSurface::Facade, FLinearColor(0.85f, 0.88f, 0.92f), bInnerRing);
				break;
			case 2:
				CityBox(Mid + FVector(0, 0, 225.f), Sz - FVector(0, 0, 450.f), 0.f, TEXT("MI_Env_ConcreteTiles"), EArenaSurface::Facade, FLinearColor(0.9f, 0.88f, 0.85f), bInnerRing);
				break;
			default:
				CityBox(Mid + FVector(0, 0, 225.f), Sz - FVector(0, 0, 450.f), 0.f, nullptr, EArenaSurface::RedBrick, FLinearColor(0.9f, 0.85f, 0.8f), bInnerRing);
				break;
			}
			// Roof: parapet cap + plant room.
			CityBox(FVector(XY.X, XY.Y, H + 20.f), FVector(W + 40.f, D + 40.f, 40.f), 0.f, nullptr, EArenaSurface::RoofGravel, FLinearColor(0.7f, 0.7f, 0.7f), false);
			CityBox(FVector(XY.X + W * 0.15f, XY.Y - D * 0.1f, H + 190.f), FVector(W * 0.3f, D * 0.3f, 300.f), 0.f, TEXT("MI_Env_Metal"), EArenaSurface::MetalPlate, FLinearColor(0.7f, 0.7f, 0.72f), false);
			if (H > 12000.f)
			{
				Shape(EArenaShape::Sphere, FVector(XY.X, XY.Y, H + 420.f), FVector(60.f), FRotator::ZeroRotator, FLinearColor(1.f, 0.05f, 0.03f), EArenaSurface::Glow, false, 12.f);
				BlinkParts.Add(Parts.Num() - 1);
			}

			// Inner ring: rolling shutters on the face looking at the arena, bins by the door.
			if (bInnerRing)
			{
				const FVector2D ToArena = (-XY).GetSafeNormal();
				const bool bUseX = FMath::Abs(ToArena.X) > FMath::Abs(ToArena.Y);
				// Outward normal of that face and the yaw that turns the shutter's -Y towards it.
				const FVector N = bUseX ? FVector(FMath::Sign(ToArena.X), 0, 0) : FVector(0, FMath::Sign(ToArena.Y), 0);
				const float Yaw = bUseX ? (N.X > 0.f ? 90.f : 270.f) : (N.Y > 0.f ? 180.f : 0.f);
				const float HalfFace = bUseX ? D * 0.5f : W * 0.5f;
				const float HalfDepth = bUseX ? W * 0.5f : D * 0.5f;
				const FVector T = FRotator(0, Yaw, 0).RotateVector(FVector(1, 0, 0));
				const FVector FaceC(XY.X + N.X * (HalfDepth + 15.f), XY.Y + N.Y * (HalfDepth + 15.f), 16.f);
				const int32 Doors = FMath::Clamp(int32(HalfFace * 2.f / 700.f), 1, 4);
				for (int32 d = 0; d < Doors; ++d)
				{
					const float S = -HalfFace + (d + 0.5f) * (HalfFace * 2.f / Doors);
					EnvMesh((d & 1) ? TEXT("rollershutter_window_01") : TEXT("rollershutter_door"), FaceC + T * (S - 100.f) - N * 15.f, Yaw, false);
				}
				EnvMesh(TEXT("metal_trash_can"), FaceC + N * 150.f + T * (HalfFace - 150.f), Yaw, true);
				EnvMesh(TEXT("trashbag"), FaceC + N * 160.f + T * (HalfFace - 300.f), Rng.FRandRange(0.f, 360.f), false);
			}
		}
	}
	// Lamps and a hydrant on the inner-ring sidewalks.
	if (bInnerRing)
	{
		const FVector2D ToArena = (-Center).GetSafeNormal();
		const FVector Edge = C3 + FVector(FMath::Sign(ToArena.X) * (Size * 0.5f - 200.f), FMath::Sign(ToArena.Y) * (Size * 0.5f - 200.f), 16.f);
		EnvMesh(TEXT("street_lamp_01"), Edge, 0.f, true, 1.5f);
		EnvMesh(TEXT("fire_hydrant"), Edge + FVector(-FMath::Sign(ToArena.X) * 500.f, 0, 0), 0.f, true);
	}
}
