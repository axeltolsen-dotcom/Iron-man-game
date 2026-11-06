#include "ArenaBlastFX.h"
#include "ArenaFXPool.h"
#include "ArenaFeel.h"
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

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Ball);
	Ring->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->CastShadow = false;
	Ring->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // plate faces camera-ish along XY
	Ring->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.01f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Ball);
	Light->SetCastShadows(false);
	Light->SetIntensity(0.f);
	Light->SetIndirectLightingIntensity(0.f); // out of Lumen scene lighting (perf budget §5.1)

	InitialLifeSpan = 3.f; // safety net
}

AArenaBlastFX* AArenaBlastFX::Spawn(UWorld* World, const FVector& Location, float Radius, const FLinearColor& InColor, float InDuration, bool bShake)
{
	if (!World) { return nullptr; }
	// Epicenter + blast radius -> Vison explosion_nearby shake (falloff inner R .. 6R + 400 cm).
	if (bShake) { if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(World)) { Feel->PlayShake(TEXT("explosion_nearby"), 1.f, &Location, Radius); } }
	AArenaBlastFX* FX = nullptr;
	if (UArenaFXPool* Pool = UArenaFXPool::Get(World))
	{
		FX = Pool->AcquireBlast(Location);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FX = World->SpawnActor<AArenaBlastFX>(AArenaBlastFX::StaticClass(), Location, FRotator::ZeroRotator, Params);
	}
	if (FX)
	{
		FX->SetLightClaimed(true);
		FX->MaxRadius = Radius;
		FX->Color = InColor;
		FX->Duration = FMath::Max(0.05f, InDuration);
		FX->Light->SetLightColor(InColor);
		FX->Light->SetAttenuationRadius(Radius * 6.f);
		if (FX->MID) { FX->MID->SetVectorParameterValue(TEXT("Color"), InColor); }
		if (FX->RingMID) { FX->RingMID->SetVectorParameterValue(TEXT("Color"), InColor); }
	}
	return FX;
}

void AArenaBlastFX::BeginPlay()
{
	Super::BeginPlay();
	MID = ArenaVisuals::PaintComponent(this, Ball, Color, 8.f);
	RingMID = ArenaVisuals::PaintComponent(this, Ring, Color, 10.f, EArenaSurface::Glow);
}

void AArenaBlastFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Duration, 0.f, 1.f);

	// Film blast: bright core pops then dies; thin plate expands faster and fades sooner.
	const float CoreGrow = 1.f - FMath::Square(1.f - FMath::Clamp(T / 0.55f, 0.f, 1.f));
	const float CoreFade = 1.f - FMath::Pow(T, 0.65f);
	const float PlateT = FMath::Clamp(T / 0.85f, 0.f, 1.f);
	const float PlateGrow = 1.f - FMath::Square(1.f - PlateT);
	const float PlateFade = 1.f - PlateT * PlateT;
	const float CoreDiam = FMath::Max(0.01f, (MaxRadius * 1.15f / 100.f) * CoreGrow);
	Ball->SetWorldScale3D(FVector(CoreDiam));
	const float PlateXY = FMath::Max(0.02f, (MaxRadius * 2.4f / 100.f) * PlateGrow);
	const float PlateZ = FMath::Max(0.004f, (MaxRadius * 0.08f / 100.f) * (0.55f + 0.45f * PlateFade));
	Ring->SetRelativeScale3D(FVector(PlateXY, PlateXY, PlateZ));
	Light->SetIntensity(LightIntensity * CoreFade);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(14.f * CoreFade));
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::LerpUsingHSV(Color, FLinearColor(1.f, 0.95f, 0.85f), 0.35f * (1.f - T)) * CoreFade);
	}
	if (RingMID)
	{
		RingMID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(8.f * PlateFade));
		RingMID->SetVectorParameterValue(TEXT("Color"), Color * PlateFade);
	}
	if (T >= 1.f) { Park(); }
}

void AArenaBlastFX::SetLightClaimed(bool bWant)
{
	UArenaFXPool* Pool = UArenaFXPool::Get(this);
	if (bWant && !bHasLight) { bHasLight = !Pool || Pool->TryClaimLight(); }
	else if (!bWant && bHasLight)
	{
		bHasLight = false;
		if (Pool) { Pool->ReturnLight(); }
	}
	Light->SetVisibility(bHasLight);
}

void AArenaBlastFX::Park()
{
	if (!bActiveBlast) { return; }
	bActiveBlast = false;
	SetLightClaimed(false);
	Light->SetIntensity(0.f);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetLifeSpan(0.f);
	if (UArenaFXPool* Pool = UArenaFXPool::Get(this)) { Pool->ReleaseBlast(this); }
	else { Destroy(); }
}

void AArenaBlastFX::ReactivateFromPool(const FVector& Location)
{
	bActiveBlast = true;
	Age = 0.f;
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	Ball->SetWorldScale3D(FVector(0.05f));
	Ring->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.01f));
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetLifeSpan(3.f); // safety net, same as InitialLifeSpan
}

void AArenaBlastFX::LifeSpanExpired()
{
	Park();
}

void AArenaBlastFX::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetLightClaimed(false);
	if (UArenaFXPool* Pool = UArenaFXPool::Get(this)) { Pool->Forget(this); }
	Super::EndPlay(EndPlayReason);
}
