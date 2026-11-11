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

	/** Slow the whole game to Dilation for RealSeconds of wall-clock time (longest request wins).
	 *  Restores to the dilation stack (suit wheel, etc.), not a hard 1.0. */
	void HitStop(float RealSeconds, float Dilation = 0.05f);

	/**
	 * B01: shared global time-dilation stack. Push/Pop by named Source; applied value is min(active).
	 * Suit wheel, cutscenes and similar should go through this so HitStop can restore correctly.
	 */
	void PushDilation(FName Source, float Value);
	void PopDilation(FName Source);
	float GetStackDilation() const;

	/** Add camera trauma (0..1, squared into shake amplitude, decays ~1.6/s). */
	void AddTrauma(float Amount);

	/** Trauma from an explosion of Radius at Location, scaled by distance to the local player. */
	void AddBlastTrauma(const FVector& Location, float Radius);

	float GetTrauma() const { return Trauma; }
	bool IsHitStopped() const { return HitStopLeft > 0.f; }

	/** Current shake offsets (already scaled by trauma^2 and the settings). */
	void GetShake(FRotator& OutRot, FVector& OutLoc) const;

	/**
	 * The ONE gameplay shake source (Vison camera_notes #4): play shake Id from Content/Story/camera_shake.json.
	 * It becomes the active profile (axes/amplitudes/frequencies in real units) and adds its
	 * arenafeel_trauma_equivalent x Scale as trauma; trauma^2 then drives the profile's amplitude, so the
	 * old AddTrauma callers and the Vison shakes never stack. Cutscene-only shakes (supersonic_flight,
	 * handheld_cutscene) are ignored here; StoryDirector owns the cutscene handheld.
	 * If Epicenter is given: falloff inner R, outer 6R + 400 cm, exponent 1.6 (camera_shake.json falloff).
	 * Returns false if the shake is unknown/cutscene-only/out of range.
	 */
	bool PlayShake(FName Id, float Scale = 1.f, const FVector* Epicenter = nullptr, float InnerRadius = 0.f);

	/** Settings intensity (0..1.5) x accessibility (off / reduced) x ima.ShakeScale. */
	static float GetShakeIntensity();

	/** Set by AStoryDirector while a cutscene owns the camera: gameplay shake is muted (no stacking). */
	void SetCinematicCamera(bool bOn) { bCinematicCamera = bOn; }
	bool IsCinematicCamera() const { return bCinematicCamera; }

	/** Shake profiles loaded (0 if camera_shake.json is missing: falls back to the built-in trauma noise). */
	int32 NumShakeProfiles() const;
	FName GetActiveShake() const { return ActiveShake; }
	/** True if Id is in the json and tagged cutscene-only. */
	static bool IsCutsceneOnlyShake(FName Id);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	bool ShakeAllowed() const;
	void ApplyDilation();

	float Trauma = 0.f;
	float HitStopLeft = 0.f;
	float HitStopDilation = 1.f;
	/** FPlatformTime deadline of the current freeze: a hitch or a pause can never leave the dilation stuck. */
	double HitStopRealEnd = 0.0;
	/** Last value we wrote via ApplyDilation (for detecting external overrides). */
	float AppliedDilation = 1.f;
	/** Named sources (SuitWheel, ...). Applied = min(values), or 1 if empty; HitStop mins on top. */
	TMap<FName, float> DilationStack;
	double ShakeClock = 0.0;
	bool bCinematicCamera = false;
	FName ActiveShake;
	float ActiveTraumaRef = 0.f;
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
