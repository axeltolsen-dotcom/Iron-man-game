#include "MenuCameraActor.h"
#include "ArenaPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Misc/App.h"

AMenuCameraActor::AMenuCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // the title menu pauses the game
	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->SetFieldOfView(52.f);
		Cam->bConstrainAspectRatio = false;
	}
}

void AMenuCameraActor::UpdatePose()
{
	// Slow orbit (about 3 deg/s) with a gentle crane move: rooftops in the distance, plazas below.
	const float Yaw = 35.f + PathTime * (bCityScale ? 1.5f : 3.f);
	const float Scale = bCityScale ? 5.7f : 1.f;
	const float Radius = (5600.f + 900.f * FMath::Sin(PathTime * 0.07f)) * Scale;
	const float Height = bCityScale ? 9000.f + 2000.f * FMath::Sin(PathTime * 0.11f + 1.f)
		: 1500.f + 600.f * FMath::Sin(PathTime * 0.11f + 1.f);
	const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Eye = Focus + Dir * Radius + FVector(0.f, 0.f, Height);
	// Look a little ahead of the orbit so the city slides through frame.
	const FVector Target = Focus + FRotator(0.f, Yaw + 100.f, 0.f).Vector() * (2200.f * Scale) + FVector(0.f, 0.f, 700.f);
	SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
}

void AMenuCameraActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PathTime += FMath::Clamp(FMath::Max(DeltaSeconds, float(FApp::GetDeltaTime())), 0.f, 0.1f);
	UpdatePose();
	if (AArenaPlayerController* PC = HoldFor.Get())
	{
		if (PC->GetViewTarget() != this) { PC->SetViewTarget(this); }
	}
}
