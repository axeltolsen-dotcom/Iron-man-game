#include "ArenaFeel.h"

#include "ArenaGameUserSettings.h"
#include "ArenaVisuals.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHitStop(TEXT("ima.HitStop"), 1, TEXT("Hit-stop on kills / heavy hits (0 = off)."));
static TAutoConsoleVariable<float> CVarShakeScale(TEXT("ima.ShakeScale"), 1.f, TEXT("Camera shake amplitude multiplier."));

// ------------------------------------------------------------------------------------------------
// Subsystem
// ------------------------------------------------------------------------------------------------

UArenaFeelSubsystem* UArenaFeelSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UArenaFeelSubsystem>() : nullptr;
}

bool UArenaFeelSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UArenaFeelSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArenaFeelSubsystem, STATGROUP_Tickables);
}

bool UArenaFeelSubsystem::ShakeAllowed() const
{
	const UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	return !S || (S->bCameraShake && !S->bHudReducedMotion);
}

void UArenaFeelSubsystem::HitStop(float RealSeconds, float Dilation)
{
	if (CVarHitStop.GetValueOnGameThread() == 0 || RealSeconds <= 0.f) { return; }
	const UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	if (S && S->bHudReducedMotion) { RealSeconds *= 0.5f; } // shorter freeze for motion-sensitive players
	HitStopLeft = FMath::Max(HitStopLeft, FMath::Min(RealSeconds, 0.3f));
	HitStopDilation = FMath::Clamp(FMath::Min(bDilated ? HitStopDilation : 1.f, Dilation), 0.02f, 1.f);
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, HitStopDilation);
		bDilated = true;
	}
}

void UArenaFeelSubsystem::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f);
}

void UArenaFeelSubsystem::AddBlastTrauma(const FVector& Location, float Radius)
{
	const UWorld* World = GetWorld();
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Pawn || Radius <= 0.f) { return; }
	const float Dist = float(FVector::Dist(Pawn->GetActorLocation(), Location));
	const float Reach = Radius * 6.f + 400.f;
	if (Dist >= Reach) { return; }
	const float Size = FMath::Clamp(Radius / 400.f, 0.1f, 1.f);
	AddTrauma(0.45f * Size * (1.f - Dist / Reach));
}

void UArenaFeelSubsystem::GetShake(FRotator& OutRot, FVector& OutLoc) const
{
	OutRot = FRotator::ZeroRotator;
	OutLoc = FVector::ZeroVector;
	if (Trauma <= 0.f || !ShakeAllowed()) { return; }
	const float A = Trauma * Trauma * CVarShakeScale.GetValueOnGameThread();
	const float T = float(ShakeClock) * 22.f;
	OutRot.Pitch = A * 2.4f * FMath::PerlinNoise1D(T + 11.3f);
	OutRot.Yaw = A * 2.4f * FMath::PerlinNoise1D(T + 47.1f);
	OutRot.Roll = A * 3.0f * FMath::PerlinNoise1D(T + 83.7f);
	OutLoc = FVector(0.f, FMath::PerlinNoise1D(T + 5.f), FMath::PerlinNoise1D(T + 9.f)) * (A * 6.f);
}

void UArenaFeelSubsystem::Tick(float DeltaTime)
{
	// Real (undilated) time so the freeze lasts the same however hard it slows the game.
	const float RealDt = FMath::Min(float(FApp::GetDeltaTime()), 0.1f);
	ShakeClock += RealDt;
	Trauma = FMath::Max(0.f, Trauma - 1.6f * RealDt);
	if (bDilated)
	{
		HitStopLeft -= RealDt;
		if (HitStopLeft <= 0.f)
		{
			HitStopLeft = 0.f;
			bDilated = false;
			HitStopDilation = 1.f;
			if (UWorld* World = GetWorld()) { UGameplayStatics::SetGlobalTimeDilation(World, 1.f); }
		}
	}
}

void UArenaFeelSubsystem::Deinitialize()
{
	if (bDilated)
	{
		if (UWorld* World = GetWorld()) { UGameplayStatics::SetGlobalTimeDilation(World, 1.f); }
		bDilated = false;
	}
	Super::Deinitialize();
}

// ------------------------------------------------------------------------------------------------
// Camera modifier
// ------------------------------------------------------------------------------------------------

bool UArenaShakeModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	if (const UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(CameraOwner))
	{
		FRotator R;
		FVector L;
		Feel->GetShake(R, L);
		if (!R.IsNearlyZero(0.001f) || !L.IsNearlyZero(0.01f))
		{
			InOutPOV.Location += InOutPOV.Rotation.RotateVector(L);
			InOutPOV.Rotation += R;
		}
	}
	return false; // let later modifiers run too
}

// ------------------------------------------------------------------------------------------------
// Telegraph ring
// ------------------------------------------------------------------------------------------------

AArenaTelegraph::AArenaTelegraph()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(SceneRoot);
	Fill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fill"));
	Fill->SetupAttachment(SceneRoot);
	for (UStaticMeshComponent* C : { Ring.Get(), Fill.Get() })
	{
		C->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetGenerateOverlapEvents(false);
	}
	SetActorEnableCollision(false);
}

AArenaTelegraph* AArenaTelegraph::Spawn(UWorld* World, const FVector& Location, float InRadius, float InDuration, const FLinearColor& InColor, AActor* FollowActor)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.bDeferConstruction = true;
	AArenaTelegraph* T = World->SpawnActor<AArenaTelegraph>(AArenaTelegraph::StaticClass(), FTransform(Location), P);
	if (!T) { return nullptr; }
	T->Radius = FMath::Max(20.f, InRadius);
	T->Duration = FMath::Max(0.1f, InDuration);
	T->TeleColor = InColor;
	T->Follow = FollowActor;
	T->FinishSpawning(FTransform(Location));
	return T;
}

void AArenaTelegraph::BeginPlay()
{
	Super::BeginPlay();
	RingMID = ArenaVisuals::PaintComponent(this, Ring, TeleColor * 0.35f, 2.f, EArenaSurface::Glow);
	FillMID = ArenaVisuals::PaintComponent(this, Fill, TeleColor, 6.f, EArenaSurface::Glow);
	// Basic cylinder is 100 uu across and 100 tall: a 1.5 cm thick disc.
	Ring->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.015f));
	Ring->SetRelativeLocation(FVector(0, 0, 1.f));
	Fill->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.02f));
	Fill->SetRelativeLocation(FVector(0, 0, 2.f));
	SnapToGround(GetActorLocation());
	SetLifeSpan(Duration + 0.25f);
}

void AArenaTelegraph::SnapToGround(const FVector& Location)
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArenaTelegraph), false, this);
	if (const AActor* F = Follow.Get()) { Q.AddIgnoredActor(F); }
	const FVector Start = Location + FVector(0, 0, 50.f);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Start, Location - FVector(0, 0, 20000.f), ECC_WorldStatic, Q))
	{
		SetActorLocation(Hit.ImpactPoint);
	}
	else
	{
		SetActorLocation(Location);
	}
}

void AArenaTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (const AActor* F = Follow.Get()) { SnapToGround(F->GetActorLocation()); }
	const float K = FMath::Clamp(Age / Duration, 0.f, 1.f);
	const float S = FMath::Max(0.01f, K * Radius / 50.f);
	Fill->SetRelativeScale3D(FVector(S, S, 0.02f));
	// Blink faster as the hit approaches.
	const float Blink = 0.6f + 0.4f * FMath::Abs(FMath::Sin(Age * (6.f + 18.f * K)));
	if (FillMID) { FillMID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(6.f * Blink)); }
	if (RingMID) { RingMID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(2.f + 2.f * K)); }
	if (Age >= Duration) { Destroy(); }
}

// ------------------------------------------------------------------------------------------------
// Decals
// ------------------------------------------------------------------------------------------------

namespace
{
	int32 GDecalCount = 0;
}

void ArenaFeel::SpawnImpactDecal(UWorld* World, const FVector& Location, const FVector& Normal, float Size, float LifeSeconds)
{
	if (!World || Size <= 0.f) { return; }
	static const TCHAR* Path = TEXT("/Game/Realism/Materials/M_ScorchDecal.M_ScorchDecal");
	static TWeakObjectPtr<UMaterialInterface> Cached;
	static bool bLooked = false;
	if (!bLooked || !Cached.IsValid())
	{
		bLooked = true;
		if (FPackageName::DoesPackageExist(TEXT("/Game/Realism/Materials/M_ScorchDecal")))
		{
			Cached = LoadObject<UMaterialInterface>(nullptr, Path);
		}
	}
	UMaterialInterface* Mat = Cached.Get();
	if (!Mat) { return; }
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	// Decals project along their local X: point X into the surface, random spin about it.
	FRotator R = (-N).Rotation();
	R.Roll = FMath::FRandRange(0.f, 360.f);
	if (UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(World, Mat, FVector(Size * 0.5f, Size, Size), Location, R, LifeSeconds))
	{
		D->SetFadeOut(LifeSeconds * 0.6f, LifeSeconds * 0.4f, false);
		D->SetFadeScreenSize(0.002f);
		++GDecalCount;
	}
}

int32 ArenaFeel::GetDecalCount()
{
	return GDecalCount;
}
