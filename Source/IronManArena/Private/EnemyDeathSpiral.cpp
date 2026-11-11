#include "EnemyDeathSpiral.h"
#include "ArenaAudio.h"
#include "ArenaBlastFX.h"
#include "ArenaCombat.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	constexpr float DeathSpiralGravity = 780.f;     // cm/s^2: a crippled drone still fights a little lift
	constexpr float DeathSpiralSweepRadius = 40.f;
	constexpr float DeathSpiralMaxSpeed = 2200.f;
}

AEnemyDeathSpiral::AEnemyDeathSpiral()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
	USceneComponent* SpiralRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SpiralRoot);
	SpinRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpinRoot"));
	SpinRoot->SetupAttachment(SpiralRoot);
}

int32 AEnemyDeathSpiral::CountLive(const UWorld* World)
{
	int32 N = 0;
	if (!World) { return 0; }
	for (TActorIterator<AEnemyDeathSpiral> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It) && !It->bExploded) { ++N; }
	}
	return N;
}

AEnemyDeathSpiral* AEnemyDeathSpiral::Launch(AActor* SourceActor, const TArray<FArenaPaintedPart>& SourceParts,
	const FVector& InVelocity, int32 SpinSide, float InLifeSeconds, float InBlastRadius)
{
	UWorld* World = SourceActor ? SourceActor->GetWorld() : nullptr;
	if (!World || InLifeSeconds <= 0.f || CountLive(World) >= MaxLive) { return nullptr; }

	int32 Visible = 0;
	for (const FArenaPaintedPart& P : SourceParts) { if (P.Component && P.Component->IsVisible() && P.Component->GetStaticMesh()) { ++Visible; } }
	if (Visible == 0) { return nullptr; } // placeholder visuals hidden (Blueprint mesh): nothing to spin

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform SourceXf = SourceActor->GetActorTransform();
	AEnemyDeathSpiral* Spiral = World->SpawnActor<AEnemyDeathSpiral>(AEnemyDeathSpiral::StaticClass(), SourceXf, Params);
	if (!Spiral) { return nullptr; }

	for (const FArenaPaintedPart& P : SourceParts)
	{
		UStaticMeshComponent* Src = P.Component;
		if (!Src || !Src->IsVisible() || !Src->GetStaticMesh()) { continue; }
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Spiral);
		Copy->SetStaticMesh(Src->GetStaticMesh());
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetGenerateOverlapEvents(false);
		Copy->SetCanEverAffectNavigation(false);
		Copy->SetCastShadow(Src->CastShadow);
		Copy->SetupAttachment(Spiral->SpinRoot);
		Copy->SetRelativeTransform(Src->GetComponentTransform().GetRelativeTransform(SourceXf));
		Copy->RegisterComponent();
		FArenaPaintedPart& Out = Spiral->SpiralParts.AddDefaulted_GetRef();
		Out.Component = Copy;
		Out.Color = P.Color;
		Out.Emissive = P.Emissive;
		Out.Role = P.Role;
		Out.Shade = P.Shade;
		Out.Surface = P.Surface;
	}
	ArenaVisuals::PaintAll(Spiral, Spiral->SpiralParts);

	Spiral->SpiralVelocity = InVelocity.GetClampedToMaxSize(DeathSpiralMaxSpeed);
	Spiral->SpinSign = SpinSide < 0 ? -1.f : (SpinSide > 0 ? 1.f : (FMath::RandBool() ? 1.f : -1.f));
	Spiral->LifeSeconds = InLifeSeconds;
	Spiral->BlastRadius = InBlastRadius;
	Spiral->SetLifeSpan(InLifeSeconds + 2.f); // belt and braces: never lingers
	return Spiral;
}

void AEnemyDeathSpiral::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded) { return; }
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	SpiralAge += Dt;

	// Momentum + gravity + light drag.
	SpiralVelocity.Z -= DeathSpiralGravity * Dt;
	SpiralVelocity *= FMath::Max(0.f, 1.f - 0.35f * Dt);
	const FVector From = GetActorLocation();
	const FVector To = From + SpiralVelocity * Dt;

	UWorld* World = GetWorld();
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DeathSpiral), false, this);
	if (World && World->SweepSingleByObjectType(Hit, From, To, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(DeathSpiralSweepRadius), Q))
	{
		SetActorLocation(Hit.Location);
		Explode();
		return;
	}
	SetActorLocation(To);

	// Spin up toward the damaged side: yaw rate ramps 360 -> 1080 deg/s, roll leans in, nose drops.
	const float Alpha = FMath::Clamp(SpiralAge / FMath::Max(0.1f, LifeSeconds), 0.f, 1.f);
	SpinYaw += SpinSign * FMath::Lerp(360.f, 1080.f, Alpha) * Dt;
	SpinRoot->SetRelativeRotation(FRotator(-35.f * Alpha, SpinYaw, SpinSign * FMath::Lerp(15.f, 55.f, Alpha)));

	if (SpiralAge >= LifeSeconds) { Explode(); }
}

void AEnemyDeathSpiral::Explode()
{
	if (bExploded) { return; }
	bExploded = true;
	UWorld* World = GetWorld();
	const FVector Here = GetActorLocation();
	AArenaBlastFX::Spawn(World, Here, BlastRadius, FLinearColor(1.f, 0.55f, 0.15f), 0.5f);
	AArenaSparks::SpawnDebris(World, Here, SpiralVelocity, FLinearColor(0.35f, 0.36f, 0.4f), 50.f, 3);
	ArenaAudio::PlayAt(this, TEXT("explosion_mid"), Here, 0.7f);
	Destroy();
}
