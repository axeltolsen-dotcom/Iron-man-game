#include "Pickup.h"
#include "IronManCharacter.h"
#include "ArenaGameMode.h"
#include "ArenaBlastFX.h"
#include "JarvisComponent.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CollectRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	SetRootComponent(Collision);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Collision);

	Core = ArenaVisuals::AddPart(this, VisualRoot, TEXT("Core"), EArenaShape::Cube,
		FVector::ZeroVector, FVector(0.45f), FRotator(45.f, 0.f, 45.f), FLinearColor(1.f, 0.2f, 0.25f), 4.f, Parts);
	Ring = ArenaVisuals::AddPart(this, VisualRoot, TEXT("Ring"), EArenaShape::Cylinder,
		FVector::ZeroVector, FVector(0.8f, 0.8f, 0.05f), FRotator::ZeroRotator, FLinearColor(1.f, 1.f, 1.f), 1.f, Parts);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(VisualRoot);
	Light->SetIntensity(2500.f);
	Light->SetAttenuationRadius(400.f);
	Light->SetCastShadows(false);
}

APickup* APickup::SpawnPickup(UWorld* World, EPickupType InType, const FVector& Location)
{
	if (!World) { return nullptr; }
	const FTransform Xf(FRotator::ZeroRotator, Location);
	APickup* P = World->SpawnActorDeferred<APickup>(APickup::StaticClass(), Xf, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (P)
	{
		P->SetPickupType(InType);
		UGameplayStatics::FinishSpawningActor(P, Xf);
	}
	return P;
}

void APickup::SetPickupType(EPickupType InType)
{
	Type = InType;
	FLinearColor C = FLinearColor(1.f, 0.27f, 0.35f);
	float CoreScale = 0.45f;
	switch (Type)
	{
	case EPickupType::Health:     C = FLinearColor(1.f, 0.27f, 0.35f); Lifetime = 14.f; CoreScale = 0.45f; break;
	case EPickupType::Energy:     C = FLinearColor(0.31f, 0.86f, 1.f); Lifetime = 14.f; CoreScale = 0.4f;  break;
	case EPickupType::Hulkbuster: C = FLinearColor(1.f, 0.67f, 0.16f); Lifetime = 20.f; CoreScale = 0.65f; break;
	case EPickupType::Gem:        C = FLinearColor(0.75f, 0.47f, 1.f); Lifetime = 10.f; CoreScale = 0.25f; break;
	}
	if (Parts.Num() > 0) { Parts[0].Color = C; }
	if (Parts.Num() > 1) { Parts[1].Color = C * 0.6f + FLinearColor(0.4f, 0.4f, 0.4f); }
	Core->SetRelativeScale3D(FVector(CoreScale));
	Light->SetLightColor(C);
}

void APickup::BeginPlay()
{
	Super::BeginPlay();
	ArenaVisuals::PaintAll(this, Parts);
	VisualRoot->SetVisibility(bUsePlaceholderVisuals, true);
	Collision->SetSphereRadius(CollectRadius);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnSphereOverlap);

	// Find the floor once, so the pickup can drop onto it.
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(PickupFloor), false, this);
	const FVector Start = GetActorLocation();
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start + FVector(0, 0, 50), Start - FVector(0, 0, 20000),
		FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		FloorZ = Hit.ImpactPoint.Z;
		bHasFloor = true;
	}
}

void APickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	// Expire (blink during the last 3 s).
	if (Lifetime > 0.f)
	{
		const float Left = Lifetime - Age;
		if (Left <= 0.f) { Destroy(); return; }
		if (Left < 3.f && bUsePlaceholderVisuals)
		{
			VisualRoot->SetVisibility(FMath::Fmod(Age * 8.f, 1.f) > 0.35f, true);
		}
	}

	// Spin and bob the visuals.
	VisualRoot->AddLocalRotation(FRotator(0.f, 120.f * DeltaSeconds, 0.f));
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(Age * 3.f) * 12.f));

	FVector Loc = GetActorLocation();
	AIronManCharacter* Player = Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bMagnet = Player && !Player->IsDead() && Age > ArmTime &&
		FVector::Dist(Player->GetActorLocation(), Loc) < MagnetRadius;

	if (bMagnet)
	{
		const FVector To = Player->GetActorLocation() - Loc;
		Loc += To.GetSafeNormal() * FMath::Min(To.Size(), MagnetSpeed * DeltaSeconds);
		SetActorLocation(Loc);
		if (To.Size() < CollectRadius + 40.f) { TryCollect(Player); }
	}
	else if (bHasFloor && Loc.Z > FloorZ + HoverHeight)
	{
		Loc.Z = FMath::Max(FloorZ + HoverHeight, Loc.Z - FallSpeed * DeltaSeconds);
		SetActorLocation(Loc);
	}
}

void APickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryCollect(OtherActor);
}

void APickup::TryCollect(AActor* OtherActor)
{
	if (bCollected || Age < ArmTime) { return; }
	AIronManCharacter* Player = Cast<AIronManCharacter>(OtherActor);
	if (!Player || Player->IsDead()) { return; }
	bCollected = true;

	switch (Type)
	{
	case EPickupType::Health:     Player->Heal(HealthAmount); UJarvisComponent::Notify(this, EAIEvent::PickupHealth); break;
	case EPickupType::Energy:     Player->AddEnergy(EnergyAmount); UJarvisComponent::Notify(this, EAIEvent::PickupEnergy); break;
	case EPickupType::Hulkbuster: Player->GrantHulkbusterCharge(TEXT("BEACON")); UJarvisComponent::Notify(this, EAIEvent::PickupHulkbuster); break;
	case EPickupType::Gem:
		if (AArenaGameMode* GM = GetWorld()->GetAuthGameMode<AArenaGameMode>()) { GM->AddScore(GemScore); }
		break;
	}
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 80.f, Parts.Num() ? Parts[0].Color : FLinearColor::White, 0.25f);
	Destroy();
}
