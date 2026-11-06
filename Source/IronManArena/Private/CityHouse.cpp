#include "CityHouse.h"

#include "PilotAvatar.h"
#include "CareerSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ACityHouse::ACityHouse()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UStaticMeshComponent* ACityHouse::Part(EArenaShape Shape, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color,
	EArenaSurface Surface, bool bSolid, float Emissive, const FRotator& Rot)
{
	return ArenaVisuals::AddRuntimePart(this, Root, Shape, Center, SizeCm / 100.f, Rot, Color, Emissive, Surface, bSolid, Parts);
}

void ACityHouse::Setup(FName InHouseId, ECityHouseKind InKind, bool bOnWater)
{
	HouseId = InHouseId;
	Kind = InKind;
	const FCityHouseDef* Def = Career::FindHouse(HouseId);
	bArmory = Def && Def->bArmory;
	switch (Kind)
	{
	case ECityHouseKind::Loft: BuildLoft(); break;
	case ECityHouseKind::Penthouse: BuildPenthouse(); break;
	case ECityHouseKind::Mansion: BuildMansion(bOnWater); break;
	}
	InteriorLight = NewObject<UPointLightComponent>(this);
	InteriorLight->SetupAttachment(Root);
	InteriorLight->SetRelativeLocation(PilotLocal + FVector(0, 0, 260.f));
	InteriorLight->SetIntensityUnits(ELightUnits::Lumens);
	InteriorLight->SetIntensity(9000.f);
	InteriorLight->SetAttenuationRadius(1800.f);
	InteriorLight->SetUseTemperature(true);
	InteriorLight->SetTemperature(3000.f);
	InteriorLight->SetCastShadows(false);
	InteriorLight->SetVisibility(false);
	InteriorLight->RegisterComponent();
	AddInstanceComponent(InteriorLight);
	if (bArmory)
	{
		ArmoryLight = NewObject<UPointLightComponent>(this);
		ArmoryLight->SetupAttachment(Root);
		ArmoryLight->SetRelativeLocation(ArmoryLocal + FVector(0, 0, 300.f));
		ArmoryLight->SetIntensityUnits(ELightUnits::Lumens);
		ArmoryLight->SetIntensity(20000.f);
		ArmoryLight->SetAttenuationRadius(1500.f);
		ArmoryLight->SetLightColor(FLinearColor(0.4f, 0.85f, 1.f));
		ArmoryLight->SetCastShadows(false);
		ArmoryLight->SetVisibility(false);
		ArmoryLight->RegisterComponent();
		AddInstanceComponent(ArmoryLight);
	}
	SetOwned(false);
}

void ACityHouse::BuildBeam(const FVector& Base, float Height)
{
	BeamParts.Add(Parts.Num());
	Part(EArenaShape::Cylinder, Base + FVector(0, 0, Height * 0.5f), FVector(120.f, 120.f, Height), FLinearColor(0.2f, 1.f, 0.4f), EArenaSurface::Glow, false, 3.f);
	BeamParts.Add(Parts.Num());
	Part(EArenaShape::Cylinder, Base + FVector(0, 0, 10.f), FVector(500.f, 500.f, 10.f), FLinearColor(0.2f, 1.f, 0.4f), EArenaSurface::Glow, false, 2.f);
	MarkerLocal = Base + FVector(0, 0, 400.f);
}

void ACityHouse::BuildLoft()
{
	// Five-storey brick walk-up, 20 x 16 m, fire escape on the street face, rooftop deck with string lights.
	const FLinearColor Brick(0.85f, 0.75f, 0.68f);
	const float W = 2000.f, D = 1600.f, H = 1750.f;
	Part(EArenaShape::Cube, FVector(0, 0, H * 0.5f), FVector(W, D, H), Brick, EArenaSurface::RedBrick);
	for (int32 f = 0; f < 5; ++f)
	{
		const float Z = 200.f + f * 340.f;
		for (int32 w = -2; w <= 2; ++w)
		{
			WindowParts.Add(Parts.Num());
			Part(EArenaShape::Cube, FVector(w * 380.f, -D * 0.5f - 4.f, Z + 100.f), FVector(220.f, 6.f, 200.f), FLinearColor(0.05f, 0.06f, 0.07f), EArenaSurface::Plain, false);
		}
		// Fire escape landing + rail.
		if (f > 0)
		{
			Part(EArenaShape::Cube, FVector(500.f, -D * 0.5f - 90.f, Z - 20.f), FVector(700.f, 160.f, 8.f), FLinearColor(0.08f, 0.08f, 0.09f), EArenaSurface::RustyMetal, false);
			Part(EArenaShape::Cube, FVector(500.f, -D * 0.5f - 168.f, Z + 40.f), FVector(700.f, 4.f, 100.f), FLinearColor(0.08f, 0.08f, 0.09f), EArenaSurface::RustyMetal, false);
		}
	}
	Part(EArenaShape::Cube, FVector(0, -D * 0.5f - 3.f, 140.f), FVector(160.f, 6.f, 260.f), FLinearColor(0.15f, 0.06f, 0.03f), EArenaSurface::Plain, false); // door
	Part(EArenaShape::Cube, FVector(0, -D * 0.5f - 120.f, 300.f), FVector(400.f, 240.f, 10.f), FLinearColor(0.05f, 0.25f, 0.15f), EArenaSurface::Plain, false, 0.f, FRotator(0, 0, -10.f)); // awning
	Part(EArenaShape::Cube, FVector(0, 0, H + 15.f), FVector(W - 200.f, D - 200.f, 30.f), FLinearColor(0.35f, 0.22f, 0.12f), EArenaSurface::Plain); // deck
	for (int32 b = 0; b < 8; ++b)
	{
		WindowParts.Add(Parts.Num());
		Part(EArenaShape::Sphere, FVector(-800.f + b * 230.f, 0.f, H + 260.f), FVector(16.f), FLinearColor(1.f, 0.8f, 0.5f), EArenaSurface::Plain, false);
	}
	SpawnLocal = FTransform(FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, H + 140.f));
	PilotLocal = FVector(0.f, -D * 0.5f - 250.f, 0.f);
	PilotYaw = -90.f;
	BuildBeam(FVector(0, 0, H + 30.f), 9000.f);
}

void ACityHouse::BuildPenthouse()
{
	// 220 m glass tower with a two-level glass penthouse, terrace, pool and helipad on top.
	const float W = 3200.f, H = 22000.f;
	Part(EArenaShape::Cube, FVector(0, 0, 600.f), FVector(W + 800.f, W + 800.f, 1200.f), FLinearColor(0.85f, 0.83f, 0.8f), EArenaSurface::Concrete);
	Part(EArenaShape::Cube, FVector(0, 0, 1200.f + (H - 1200.f) * 0.5f), FVector(W, W, H - 1200.f), FLinearColor(0.7f, 0.82f, 0.9f), EArenaSurface::Facade);
	for (int32 f = 0; f < 12; ++f)
	{
		WindowParts.Add(Parts.Num());
		Part(EArenaShape::Cube, FVector(0, -W * 0.5f - 6.f, 1800.f + f * 1700.f), FVector(W * 0.7f, 4.f, 210.f), FLinearColor(1.f, 0.78f, 0.5f), EArenaSurface::Plain, false);
	}
	const float T = H;
	Part(EArenaShape::Cube, FVector(0, 0, T + 20.f), FVector(W + 600.f, W + 600.f, 40.f), FLinearColor(0.8f, 0.78f, 0.74f), EArenaSurface::Concrete); // terrace slab
	Part(EArenaShape::Cube, FVector(-500.f, 300.f, T + 270.f), FVector(1800.f, 1600.f, 460.f), FLinearColor(0.6f, 0.75f, 0.82f), EArenaSurface::Glass); // glass box
	WindowParts.Add(Parts.Num());
	Part(EArenaShape::Cube, FVector(-500.f, 300.f, T + 470.f), FVector(1700.f, 1500.f, 20.f), FLinearColor(1.f, 0.85f, 0.6f), EArenaSurface::Plain, false); // ceiling glow
	Part(EArenaShape::Cube, FVector(-500.f, 300.f, T + 520.f), FVector(1900.f, 1700.f, 40.f), FLinearColor(0.15f, 0.15f, 0.16f), EArenaSurface::Steel);
	Part(EArenaShape::Cube, FVector(900.f, -900.f, T + 45.f), FVector(1000.f, 500.f, 30.f), FLinearColor(0.05f, 0.45f, 0.55f), EArenaSurface::Glass, false); // pool
	for (const float s : { -1.f, 1.f })
	{
		Part(EArenaShape::Cube, FVector(0, s * (W * 0.5f + 290.f), T + 95.f), FVector(W + 600.f, 6.f, 110.f), FLinearColor(0.7f, 0.8f, 0.85f), EArenaSurface::Glass);
		Part(EArenaShape::Cube, FVector(s * (W * 0.5f + 290.f), 0, T + 95.f), FVector(6.f, W + 600.f, 110.f), FLinearColor(0.7f, 0.8f, 0.85f), EArenaSurface::Glass);
	}
	Part(EArenaShape::Cylinder, FVector(900.f, 900.f, T + 50.f), FVector(1100.f, 1100.f, 20.f), FLinearColor(0.12f, 0.12f, 0.13f), EArenaSurface::Asphalt); // helipad
	Part(EArenaShape::Cube, FVector(900.f, 900.f, T + 62.f), FVector(500.f, 80.f, 2.f), FLinearColor(0.9f, 0.9f, 0.9f), EArenaSurface::Plain, false);
	SpawnLocal = FTransform(FRotator(0.f, -90.f, 0.f), FVector(900.f, 900.f, T + 200.f));
	PilotLocal = FVector(300.f, -700.f, T + 40.f);
	PilotYaw = -90.f;
	BuildBeam(FVector(-500.f, 300.f, T + 540.f), 9000.f);
}

void ACityHouse::BuildMansion(bool bOnWater)
{
	// Headland cliff (stacked, tilted rock blocks) with a white cantilevered cliff house, glass walls, infinity pool,
	// workshop garage and an armory bay on the top plateau.
	FRandomStream Rng(1963);
	const float Top = bOnWater ? 3800.f : 2800.f;
	const FLinearColor Rock(0.42f, 0.36f, 0.3f);
	for (int32 r = 0; r < 14; ++r)
	{
		const float Ang = r * (2.f * PI / 14.f);
		const FVector P(FMath::Cos(Ang) * 2600.f, FMath::Sin(Ang) * 2200.f, Top * 0.5f - 200.f);
		Part(EArenaShape::Cube, P, FVector(Rng.FRandRange(1800.f, 2600.f), Rng.FRandRange(1600.f, 2400.f), Top + 400.f), Rock * Rng.FRandRange(0.85f, 1.1f),
			EArenaSurface::RuinedWall, true, 0.f, FRotator(Rng.FRandRange(-8.f, 8.f), Rng.FRandRange(0.f, 90.f), Rng.FRandRange(-8.f, 8.f)));
	}
	Part(EArenaShape::Cube, FVector(0, 0, Top * 0.5f), FVector(5200.f, 4400.f, Top), Rock, EArenaSurface::RuinedWall);
	Part(EArenaShape::Cube, FVector(0, 0, Top + 10.f), FVector(5000.f, 4200.f, 20.f), FLinearColor(0.35f, 0.5f, 0.25f), EArenaSurface::DebrisGround); // lawn
	const FLinearColor White(0.92f, 0.92f, 0.9f);
	const float Z = Top + 20.f;
	Part(EArenaShape::Cube, FVector(-400.f, 0.f, Z + 200.f), FVector(2600.f, 1800.f, 400.f), White, EArenaSurface::Concrete);
	Part(EArenaShape::Cube, FVector(300.f, -300.f, Z + 600.f), FVector(2800.f, 1500.f, 400.f), White, EArenaSurface::Concrete); // cantilever over the edge
	Part(EArenaShape::Cube, FVector(300.f, -1060.f, Z + 600.f), FVector(2700.f, 10.f, 360.f), FLinearColor(0.55f, 0.7f, 0.78f), EArenaSurface::Glass);
	Part(EArenaShape::Cube, FVector(-400.f, -910.f, Z + 200.f), FVector(2500.f, 10.f, 360.f), FLinearColor(0.55f, 0.7f, 0.78f), EArenaSurface::Glass);
	WindowParts.Add(Parts.Num());
	Part(EArenaShape::Cube, FVector(300.f, -300.f, Z + 790.f), FVector(2700.f, 1400.f, 10.f), FLinearColor(1.f, 0.82f, 0.6f), EArenaSurface::Plain, false);
	WindowParts.Add(Parts.Num());
	Part(EArenaShape::Cube, FVector(-400.f, 0.f, Z + 395.f), FVector(2500.f, 1700.f, 10.f), FLinearColor(1.f, 0.82f, 0.6f), EArenaSurface::Plain, false);
	Part(EArenaShape::Cube, FVector(-400.f, -1500.f, Z + 30.f), FVector(2600.f, 900.f, 40.f), FLinearColor(0.4f, 0.27f, 0.16f), EArenaSurface::Plain); // deck
	Part(EArenaShape::Cube, FVector(-400.f, -1550.f, Z + 52.f), FVector(1800.f, 500.f, 10.f), FLinearColor(0.05f, 0.5f, 0.6f), EArenaSurface::Glass, false); // infinity pool
	// Palms on the lawn.
	for (int32 p = 0; p < 5; ++p)
	{
		const FVector B(Rng.FRandRange(-2200.f, 2200.f), Rng.FRandRange(800.f, 1900.f), Z);
		const float Hgt = Rng.FRandRange(700.f, 1100.f);
		Part(EArenaShape::Cylinder, B + FVector(0, 0, Hgt * 0.5f), FVector(30.f, 30.f, Hgt), FLinearColor(0.45f, 0.36f, 0.27f), EArenaSurface::RuinedWall, false);
		for (int32 f = 0; f < 6; ++f)
		{
			const FRotator FR(-30.f, f * 60.f, 0.f);
			Part(EArenaShape::Cube, B + FVector(0, 0, Hgt) + FR.RotateVector(FVector(200.f, 0, 0)), FVector(420.f, 80.f, 6.f), FLinearColor(0.08f, 0.24f, 0.06f), EArenaSurface::Plain, false, 0.f, FR);
		}
	}
	// Workshop garage + armory bay.
	Part(EArenaShape::Cube, FVector(1800.f, 1300.f, Z + 300.f), FVector(1600.f, 1200.f, 600.f), FLinearColor(0.2f, 0.21f, 0.23f), EArenaSurface::Steel);
	Part(EArenaShape::Cube, FVector(1800.f, 690.f, Z + 230.f), FVector(1100.f, 10.f, 420.f), FLinearColor(0.5f, 0.52f, 0.55f), EArenaSurface::MetalPlate, false); // roll door
	ArmoryLocal = FVector(1800.f, 150.f, Z + 20.f);
	ArmoryParts.Add(Parts.Num());
	Part(EArenaShape::Cylinder, ArmoryLocal + FVector(0, 0, 8.f), FVector(400.f, 400.f, 8.f), FLinearColor(0.3f, 0.85f, 1.f), EArenaSurface::Glow, false, 2.f);
	for (int32 g = 0; g < 3; ++g)
	{
		const FRotator R(0.f, g * 120.f, 0.f);
		Part(EArenaShape::Cube, ArmoryLocal + R.RotateVector(FVector(260.f, 0, 250.f)), FVector(30.f, 30.f, 500.f), FLinearColor(0.3f, 0.3f, 0.32f), EArenaSurface::Steel, false);
		Part(EArenaShape::Cube, ArmoryLocal + R.RotateVector(FVector(150.f, 0, 480.f)), FVector(220.f, 24.f, 24.f), FLinearColor(0.3f, 0.3f, 0.32f), EArenaSurface::Steel, false, 0.f, R);
	}
	SpawnLocal = FTransform(FRotator(0.f, -90.f, 0.f), FVector(-400.f, -1400.f, Z + 150.f));
	PilotLocal = FVector(-1200.f, -1300.f, Z + 50.f);
	PilotYaw = -60.f;
	BuildBeam(FVector(-400.f, 0.f, Z + 820.f), 9000.f);
}

FVector ACityHouse::GetMarkerLocation() const { return GetActorTransform().TransformPosition(MarkerLocal); }
FTransform ACityHouse::GetSpawnTransform() const { return SpawnLocal * GetActorTransform(); }
FVector ACityHouse::GetArmoryLocation() const { return GetActorTransform().TransformPosition(ArmoryLocal); }

void ACityHouse::SetOwned(bool bOwned)
{
	bIsOwned = bOwned;
	const FLinearColor C = bOwned ? FLinearColor(0.25f, 0.6f, 1.f) : FLinearColor(0.2f, 1.f, 0.4f);
	for (const int32 i : BeamParts)
	{
		if (Parts.IsValidIndex(i)) { Parts[i].Color = C; ArenaVisuals::RepaintPart(Parts[i]); }
	}
	if (ArmoryLight) { ArmoryLight->SetVisibility(bOwned); }
	for (const int32 i : ArmoryParts) { if (Parts.IsValidIndex(i) && Parts[i].Component) { Parts[i].Component->SetVisibility(bOwned); } }
	SetNight(bNightOn);
	RefreshPilot();
}

void ACityHouse::SetNight(bool bNight)
{
	bNightOn = bNight;
	// Windows glow at night (brighter when you own the place).
	for (const int32 i : WindowParts)
	{
		if (!Parts.IsValidIndex(i)) { continue; }
		FArenaPaintedPart& P = Parts[i];
		const bool bLit = bNight && (bIsOwned || Kind != ECityHouseKind::Mansion);
		ArenaVisuals::SetPartGlow(P, bLit ? (bIsOwned ? 0.35f : 0.15f) : 0.f);
	}
	if (InteriorLight) { InteriorLight->SetVisibility(bIsOwned && bNight && !IsHidden()); }
}

void ACityHouse::RefreshPilot()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (!bIsOwned)
	{
		if (Pilot) { Pilot->Destroy(); Pilot = nullptr; }
		return;
	}
	const UCareerSubsystem* Career = UCareerSubsystem::Get(this);
	if (!Pilot)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pilot = World->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), FTransform(FRotator(0.f, PilotYaw, 0.f), GetActorTransform().TransformPosition(PilotLocal)), SP);
	}
	if (Pilot && Career) { Pilot->ApplyConfig(Career->GetAvatar()); }
	if (Pilot) { Pilot->SetActorHiddenInGame(IsHidden()); }
}

void ACityHouse::SetHouseVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
	if (Pilot) { Pilot->SetActorHiddenInGame(!bVisible); }
	SetNight(bNightOn);
}

void ACityHouse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Pilot) { Pilot->Destroy(); Pilot = nullptr; }
	Super::EndPlay(EndPlayReason);
}
