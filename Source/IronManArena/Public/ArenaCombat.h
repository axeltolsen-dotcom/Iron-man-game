// Shooting overhaul (Axel: "the shooting sucks"). Research pack #1/#5: blasts and beams, not balls.
//  - Hitscan repulsor / laser / minigun / frost / red-blast: an instant sphere sweep from the palm, a white-hot
//    tracer beam that fades in ~90 ms, palm muzzle flash, spark burst + scorch at the impact.
//  - Hit markers + kill confirms (drawn by UCareerHUD around the crosshair), hit-stop on kills (B01-safe),
//    controller rumble, camera kick, punchy SFX through ArenaAudio.
//  - Enemy knockback on every hit, debris "ragdoll" chunks on kills; city cars / pedestrians take the damage too.
//  - Focus (hold Left Alt / gamepad L3-less: automatic while hovering): tighter FOV, stronger aim assist, and holding
//    fire charges a heavy repulsor blast that releases when you let go.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaCombat.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class APlayerController;

/** Short-lived beam between two points (pooled). */
UCLASS(NotPlaceable)
class IRONMANARENA_API AArenaTracer : public AActor
{
	GENERATED_BODY()
public:
	AArenaTracer();
	static void Spawn(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& Color, float Width, float Life = 0.09f);
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
	void Begin(const FVector& From, const FVector& To, const FLinearColor& Color, float Width, float Life);
	UPROPERTY() TObjectPtr<USceneComponent> Pivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f, Life = 0.09f, Width = 6.f, Length = 100.f;
	bool bActive = false;
};

/** Spark burst / debris chunks: a few small parts flying ballistic (sparks glow, debris is dark metal). */
UCLASS(NotPlaceable)
class IRONMANARENA_API AArenaSparks : public AActor
{
	GENERATED_BODY()
public:
	AArenaSparks();
	static void Spawn(UWorld* World, const FVector& Location, const FVector& Normal, const FLinearColor& Color, int32 Count = 8, float Speed = 900.f);
	/** Kill debris: chunks in Color thrown away from Dir, tumbling, bounce off the ground, fade after ~3 s. */
	static void SpawnDebris(UWorld* World, const FVector& Location, const FVector& Dir, const FLinearColor& Color, float Size = 60.f, int32 Count = 7);
	virtual void Tick(float DeltaSeconds) override;
protected:
	struct FBit { UStaticMeshComponent* C = nullptr; FVector V = FVector::ZeroVector; FRotator Spin = FRotator::ZeroRotator; };
	TArray<FBit> Bits;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Comps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
	float Age = 0.f, Life = 0.4f, Gravity = 1800.f;
	bool bDebris = false;
	FLinearColor Color = FLinearColor::White;
};

namespace ArenaCombat
{
	struct FShot
	{
		bool bHit = false;
		bool bEnemy = false;   // hit an enemy / city pawn / car
		bool bKill = false;
		FVector End = FVector::ZeroVector;
	};

	/** Instant player shot. Damages enemies (+knockback), city cars / pedestrians, intercepts hostile missiles. */
	IRONMANARENA_API FShot FireHitscan(AActor* Shooter, AController* Instigator, const FVector& Muzzle, const FVector& Dir,
		float Range, float Damage, float BlastRadius, float Radius, const FLinearColor& Color, float Knockback = 450.f);

	/** Push an enemy / actor along Dir (characters are launched, others nudged with a sweep). */
	IRONMANARENA_API void Knockback(AActor* Target, const FVector& Dir, float Strength);

	/** Hit-marker bookkeeping (real time) for the HUD. */
	IRONMANARENA_API void RegisterHit(bool bKill);
	IRONMANARENA_API float HitMarkerAge();   // seconds since the last hit (large if none)
	IRONMANARENA_API float KillMarkerAge();
	IRONMANARENA_API int32 KillStreak();     // kills within 3 s of each other

	/** Controller rumble (no-op without a gamepad). */
	IRONMANARENA_API void Rumble(const UObject* WorldContext, float Intensity, float Duration, bool bHeavy = false);

	/** Focus / charge state for the crosshair. */
	IRONMANARENA_API void SetAimState(bool bFocus, float Charge01, float Bloom01);
	IRONMANARENA_API bool IsFocus();
	IRONMANARENA_API float GetCharge();
	IRONMANARENA_API float GetBloom();
}
