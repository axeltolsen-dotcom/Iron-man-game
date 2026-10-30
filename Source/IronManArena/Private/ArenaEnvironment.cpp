#include "ArenaEnvironment.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

AArenaEnvironment::AArenaEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	using namespace ArenaVisuals;
	const FLinearColor Concrete(0.09f, 0.09f, 0.1f);
	const FLinearColor Steel(0.22f, 0.23f, 0.26f);
	const FLinearColor Glow(0.3f, 0.85f, 1.f);

	auto MakeSolid = [this](UStaticMeshComponent* C)
	{
		// AddPart disables collision; arena geometry must block (WorldStatic, BlockAll).
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCanEverAffectNavigation(true);
	};

	// Floor: top surface at the actor's Z.
	const float FloorScale = FloorHalfSize * 2.f / 100.f;
	Floor = AddPart(this, Root, TEXT("Floor"), EArenaShape::Cube, FVector(0.f, 0.f, -50.f),
		FVector(FloorScale, FloorScale, 1.f), FRotator::ZeroRotator, Concrete, 0.f, Parts);
	MakeSolid(Floor);

	// Glowing grid lines on the floor (purely visual).
	for (int32 i = -4; i <= 4; ++i)
	{
		const float Off = i * 2000.f;
		AddPart(this, Root, *FString::Printf(TEXT("GridX_%d"), i + 4), EArenaShape::Cube, FVector(Off, 0.f, 1.f),
			FVector(0.08f, FloorScale, 0.02f), FRotator::ZeroRotator, Glow * 0.4f, 2.f, Parts);
		AddPart(this, Root, *FString::Printf(TEXT("GridY_%d"), i + 4), EArenaShape::Cube, FVector(0.f, Off, 1.f),
			FVector(FloorScale, 0.08f, 0.02f), FRotator::ZeroRotator, Glow * 0.4f, 2.f, Parts);
	}

	// Ring of pylons marking the arena edge (~7000 uu, matches AIronManCharacter::ArenaBoundaryRadius).
	const int32 NumPylons = 16;
	for (int32 i = 0; i < NumPylons; ++i)
	{
		const float A = (2.f * PI * i) / NumPylons;
		const FVector P(FMath::Cos(A) * 7200.f, FMath::Sin(A) * 7200.f, 0.f);
		UStaticMeshComponent* Pylon = AddPart(this, Root, *FString::Printf(TEXT("Pylon_%d"), i), EArenaShape::Cylinder,
			P + FVector(0.f, 0.f, 600.f), FVector(2.f, 2.f, 12.f), FRotator::ZeroRotator, Steel, 0.f, Parts);
		MakeSolid(Pylon);
		Props.Add(Pylon);
		AddPart(this, Root, *FString::Printf(TEXT("PylonLight_%d"), i), EArenaShape::Cylinder,
			P + FVector(0.f, 0.f, 1220.f), FVector(2.2f, 2.2f, 0.4f), FRotator::ZeroRotator, ArenaVisuals::SuitGold, 6.f, Parts);
	}

	// Cover blocks (fixed layout so the arena is the same every run).
	const FVector CoverSpots[] = {
		FVector(1800, 600, 0), FVector(-1500, 1900, 0), FVector(-2200, -1200, 0), FVector(900, -2500, 0),
		FVector(3600, 2600, 0), FVector(-3800, 200, 0), FVector(400, 4200, 0), FVector(2800, -3900, 0),
		FVector(-2600, -4200, 0), FVector(4700, -700, 0), FVector(-4600, 3900, 0), FVector(5200, 4300, 0)
	};
	int32 Index = 0;
	for (const FVector& Spot : CoverSpots)
	{
		const float H = 1.5f + (Index % 3) * 1.2f;
		const float Yaw = Index * 37.f;
		UStaticMeshComponent* Block = AddPart(this, Root, *FString::Printf(TEXT("Cover_%d"), Index), EArenaShape::Cube,
			Spot + FVector(0.f, 0.f, H * 50.f), FVector(3.f, 1.5f, H), FRotator(0.f, Yaw, 0.f), Steel, 0.f, Parts);
		MakeSolid(Block);
		Props.Add(Block);
		AddPart(this, Root, *FString::Printf(TEXT("CoverTrim_%d"), Index), EArenaShape::Cube,
			Spot + FVector(0.f, 0.f, H * 100.f + 3.f), FVector(3.05f, 1.55f, 0.06f), FRotator(0.f, Yaw, 0.f), Glow, 4.f, Parts);
		++Index;
	}

	// --- Lighting (all Movable: nothing to bake) ---
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-35.f, 40.f, 0.f));
	Sun->SetIntensity(10.f); // lux
	Sun->SetAtmosphereSunLight(true);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true; // captures the sky atmosphere, no cubemap needed
	SkyLight->SetIntensity(1.2f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.01f);
}

void AArenaEnvironment::BeginPlay()
{
	Super::BeginPlay();
	ArenaVisuals::PaintAll(this, Parts);
}

void AArenaEnvironment::RemoveLighting()
{
	for (UActorComponent* C : TArray<UActorComponent*>{ Sun.Get(), SkyLight.Get(), Atmosphere.Get(), Fog.Get() })
	{
		if (C) { C->DestroyComponent(); }
	}
	Sun = nullptr; SkyLight = nullptr; Atmosphere = nullptr; Fog = nullptr;
}

void AArenaEnvironment::RemoveGeometry()
{
	for (FArenaPaintedPart& P : Parts)
	{
		if (P.Component) { P.Component->DestroyComponent(); }
	}
	Parts.Empty();
	Props.Empty();
	Floor = nullptr;
}
