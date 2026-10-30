#include "ArenaBlastFX.h"
#include "ArenaVisuals.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AArenaBlastFX::AArenaBlastFX()
{
	PrimaryActorTick.bCanEverTick = true;

	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	SetRootComponent(Ball);
	Ball->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Sphere));
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->CastShadow = false;
	Ball->SetRelativeScale3D(FVector(0.05f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Ball);
	Light->SetCastShadows(false);
	Light->SetIntensity(0.f);

	InitialLifeSpan = 3.f; // safety net
}

AArenaBlastFX* AArenaBlastFX::Spawn(UWorld* World, const FVector& Location, float Radius, const FLinearColor& InColor, float InDuration)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaBlastFX* FX = World->SpawnActor<AArenaBlastFX>(AArenaBlastFX::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (FX)
	{
		FX->MaxRadius = Radius;
		FX->Color = InColor;
		FX->Duration = FMath::Max(0.05f, InDuration);
		FX->Light->SetLightColor(InColor);
		FX->Light->SetAttenuationRadius(Radius * 6.f);
		if (FX->MID) { FX->MID->SetVectorParameterValue(TEXT("Color"), InColor); }
	}
	return FX;
}

void AArenaBlastFX::BeginPlay()
{
	Super::BeginPlay();
	MID = ArenaVisuals::PaintComponent(this, Ball, Color, 8.f);
}

void AArenaBlastFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Duration, 0.f, 1.f);

	// Ease-out growth: basic sphere is 100 uu across, so scale = diameter / 100.
	const float Grow = 1.f - FMath::Square(1.f - T);
	Ball->SetWorldScale3D(FVector(FMath::Max(0.01f, (MaxRadius * 2.f / 100.f) * Grow)));
	Light->SetIntensity(LightIntensity * (1.f - T));
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(10.f * (1.f - T)));
		MID->SetVectorParameterValue(TEXT("Color"), Color * (1.f - 0.7f * T));
	}
	if (T >= 1.f) { Destroy(); }
}
