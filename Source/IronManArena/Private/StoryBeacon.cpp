#include "StoryBeacon.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

using namespace ArenaVisuals;

AStoryBeacon::AStoryBeacon()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Root);

	const FLinearColor C(1.f, 0.7f, 0.2f);
	// Ring of 12 segments (radius ~150 uu) in the XY plane of Spinner.
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = 2.f * PI * i / 12.f;
		const FVector L(FMath::Cos(A) * 150.f, FMath::Sin(A) * 150.f, 0.f);
		AddPart(this, Spinner, FName(*FString::Printf(TEXT("Seg%d"), i)), EArenaShape::Cube, L,
			FVector(0.12f, 0.78f, 0.06f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f), C, 8.f, Parts);
	}
	// Column (ground beacons only; hidden for air).
	AddPart(this, Root, TEXT("Column"), EArenaShape::Cylinder, FVector(0, 0, 1500), FVector(0.25f, 0.25f, 30.f), FRotator::ZeroRotator, C, 3.f, Parts);
	AddPart(this, Root, TEXT("Pad"), EArenaShape::Cylinder, FVector(0, 0, 2), FVector(3.4f, 3.4f, 0.04f), FRotator::ZeroRotator, C, 1.5f, Parts);

	for (FArenaPaintedPart& P : Parts)
	{
		if (P.Component)
		{
			P.Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P.Component->SetCastShadow(false);
		}
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0, 0, 120));
	Light->SetIntensity(6000.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetLightColor(C);
	Light->SetCastShadows(false);
}

void AStoryBeacon::BeginPlay()
{
	Super::BeginPlay();
	PaintAll(this, Parts);
}

void AStoryBeacon::Setup(bool bAir, float InRadius, const FLinearColor& Color)
{
	bAirBeacon = bAir;
	Radius = FMath::Max(50.f, InRadius);
	Light->SetLightColor(Color);
	for (FArenaPaintedPart& P : Parts)
	{
		if (!P.Component) { continue; }
		const FName N = P.Component->GetFName();
		if (bAir && (N == TEXT("Column") || N == TEXT("Pad"))) { P.Component->SetVisibility(false); }
		P.Color = Color;
		RepaintPart(P);
	}
	// Air rings stand upright so the player flies through them.
	Spinner->SetRelativeRotation(bAir ? FRotator(0.f, 0.f, 90.f) : FRotator::ZeroRotator);
	Spinner->SetRelativeLocation(bAir ? FVector::ZeroVector : FVector(0, 0, 60));
	const float S = Radius / 150.f;
	Spinner->SetRelativeScale3D(FVector(S, S, 1.f));
}

bool AStoryBeacon::IsReached(const FVector& Location) const
{
	const FVector D = Location - GetActorLocation();
	if (bAirBeacon) { return D.Size() <= Radius * 1.2f; }
	return FVector(D.X, D.Y, 0.f).Size() <= Radius && D.Z < 400.f;
}

void AStoryBeacon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (bAirBeacon)
	{
		Spinner->SetRelativeRotation(FRotator(0.f, Time * 25.f, 90.f));
	}
	else
	{
		Spinner->SetRelativeRotation(FRotator(0.f, Time * 40.f, 0.f));
		Spinner->SetRelativeLocation(FVector(0, 0, 60.f + 25.f * FMath::Sin(Time * 2.f)));
	}
	Light->SetIntensity(5000.f + 2000.f * FMath::Sin(Time * 4.f));
}
