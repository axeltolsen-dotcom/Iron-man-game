// Combat feel: hit-stop (short global slow-motion on kills / heavy hits), trauma-based camera shake,
// impact scorch decals and ground telegraph rings. Everything is cosmetic and respects the
// Settings toggles (camera shake, HUD reduced motion); headless (-nullrhi) runs keep working
// because nothing here depends on a renderer.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraModifier.h"
#include "GameFramework/Actor.h"
#include "ArenaFeel.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class IRONMANARENA_API UArenaFeelSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UArenaFeelSubsystem* Get(const UObject* WorldContext);

	/** Slow the whole game to Dilation for RealSeconds of wall-clock time (longest request wins). */
	void HitStop(float RealSeconds, float Dilation = 0.05f);

	/** Add camera trauma (0..1, squared into shake amplitude, decays ~1.6/s). */
	void AddTrauma(float Amount);

	/** Trauma from an explosion of Radius at Location, scaled by distance to the local player. */
	void AddBlastTrauma(const FVector& Location, float Radius);

	float GetTrauma() const { return Trauma; }
	bool IsHitStopped() const { return HitStopLeft > 0.f; }

	/** Current shake offsets (already scaled by trauma^2 and the settings). */
	void GetShake(FRotator& OutRot, FVector& OutLoc) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	bool ShakeAllowed() const;

	float Trauma = 0.f;
	float HitStopLeft = 0.f;
	float HitStopDilation = 1.f;
	bool bDilated = false;
	double ShakeClock = 0.0;
};

/** Camera modifier that applies UArenaFeelSubsystem's shake; added by AArenaPlayerController. */
UCLASS()
class IRONMANARENA_API UArenaShakeModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};

/** Expanding glowing ground ring that marks where an attack will land. Self-destroys. */
UCLASS()
class IRONMANARENA_API AArenaTelegraph : public AActor
{
	GENERATED_BODY()

public:
	AArenaTelegraph();

	/** Ring on the ground under Location (traced down), Radius cm, for Duration s. Optionally follows FollowActor. */
	static AArenaTelegraph* Spawn(UWorld* World, const FVector& Location, float Radius, float Duration, const FLinearColor& Color, AActor* FollowActor = nullptr);

	virtual void Tick(float DeltaSeconds) override;

	float GetAge() const { return Age; }
	float GetDuration() const { return Duration; }

protected:
	virtual void BeginPlay() override;

private:
	void SnapToGround(const FVector& Location);

	UPROPERTY(VisibleAnywhere, Category = "Telegraph")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category = "Telegraph")
	TObjectPtr<UStaticMeshComponent> Fill;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMID;

	TWeakObjectPtr<AActor> Follow;
	FLinearColor TeleColor = FLinearColor(1.f, 0.15f, 0.05f);
	float Radius = 300.f;
	float Duration = 1.f;
	float Age = 0.f;
};

namespace ArenaFeel
{
	/** Scorch mark where a shot hit world geometry (no-op if M_ScorchDecal is not built). */
	IRONMANARENA_API void SpawnImpactDecal(UWorld* World, const FVector& Location, const FVector& Normal, float Size, float LifeSeconds = 12.f);

	/** Number of scorch decals spawned since startup (tests). */
	IRONMANARENA_API int32 GetDecalCount();
}
