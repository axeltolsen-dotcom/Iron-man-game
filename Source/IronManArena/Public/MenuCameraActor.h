// AMenuCameraActor: the title-menu background. A slow cinematic orbit over the free-play city
// (ticks while the menu has the game paused). AArenaPlayerController views through it while the
// Title menu (and suit select opened from it) is up, and hands the view back on HideMenu.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "MenuCameraActor.generated.h"

class AArenaPlayerController;

UCLASS(NotBlueprintable)
class IRONMANARENA_API AMenuCameraActor : public ACameraActor
{
	GENERATED_BODY()

public:
	AMenuCameraActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Orbit centre (the arena / city origin). */
	FVector Focus = FVector::ZeroVector;

	/** While true the camera re-claims the view if possession switched it to the pawn. */
	TWeakObjectPtr<AArenaPlayerController> HoldFor;

	/** Seconds since the menu opened (drives the path). */
	float PathTime = 0.f;

	void UpdatePose();
};
