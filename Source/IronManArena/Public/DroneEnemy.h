// ADroneEnemy: small flying attack drone.
// Reference numbers (2-D game): hp 3, 100 pts, fires every 1.8-3.2 s, bolt 7 dmg, range 700 px.
// 3-D conversion used throughout this project: 1 reference pixel ~= 2.5 Unreal units (cm).
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "DroneEnemy.generated.h"

class UPointLightComponent;

UCLASS()
class IRONMANARENA_API ADroneEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ADroneEnemy();

	virtual void InitForWave(int32 InWave) override;

	/** Min/max seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float FireIntervalMin = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float FireIntervalMax = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float BoltSpeed = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float BoltDamage = 7.f;

	/** Only shoots when the player is closer than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float FireRange = 2200.f;

	/** Preferred distance from the player (random in range per drone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	FVector2D OrbitDistanceRange = FVector2D(600.f, 1000.f);

	/** Height offset relative to the player (random in range per drone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	FVector2D HeightOffsetRange = FVector2D(-100.f, 400.f);

	/** Never fly lower than this above the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float MinAltitude = 200.f;

	/** Spring-damper steering (same feel as the 2-D game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float Spring = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float Damping = 1.3f;

	/** Degrees per second the orbit position drifts around the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	float OrbitDriftSpeed = 18.f;

	/** Drop chances on death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Drops")
	float GemDropChance = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Drops")
	float EnergyDropChance = 0.06f;

protected:
	virtual void BeginPlay() override;
	virtual void TickEnemy(float DeltaSeconds, AIronManCharacter* Player) override;
	virtual void OnKilled() override;
	virtual void SpawnDeathFX() override;

	/** Combat realism damage state: wobble/list, rotor flicker, sag and sparks. */
	void TickDroneDamageState(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RotorL;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RotorR;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> EyeLight;

	float FireCooldown = 2.f;
	float OrbitAngle = 0.f;
	float OrbitDistance = 800.f;
	float HeightOffset = 150.f;
	float OrbitDirection = 1.f;

	/** Aim cone (degrees) once the drone is at or below the wobble threshold (it has no spread when healthy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Damage")
	float DamagedAimSpreadDeg = 2.f;

	/** -1 = left rotor damaged, +1 = right, 0 = not damaged yet (picked from the hit side at the 50% line). */
	int32 DamagedSide = 0;
	float SparkTimer = 1.f;
	float DamagePitch = 0.f;
	float DamageRoll = 0.f;
	float SagOffset = 0.f;
};
