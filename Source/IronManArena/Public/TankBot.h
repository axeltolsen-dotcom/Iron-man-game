// ATankBot: slow tracked artillery bot that keeps its distance and lobs explosive shells.
// Reference: hp 16, 350 pts, speed 55 px/s, keepDist 420 px, fires every 2.6-3.8 s,
// shell flight 1.35 s, 12 dmg, blast radius 80 px, range 950 px. Drops: 30% Hulkbuster beacon,
// 30% health, else energy, plus a score gem.
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "TankBot.generated.h"

UCLASS()
class IRONMANARENA_API ATankBot : public AEnemyBase
{
	GENERATED_BODY()

public:
	ATankBot();

	virtual void InitForWave(int32 InWave) override;
	virtual FVector GetAimPoint() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float MoveSpeed = 150.f;

	/** Tries to stay roughly this far from the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float KeepDistance = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float FireIntervalMin = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float FireIntervalMax = 3.8f;

	/** Seconds a shell is in the air (the launch velocity is solved for this). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float ShellFlightTime = 1.35f;

	/** Multiplier on world gravity for shells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float ShellGravityScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float ShellDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float ShellBlastRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float FireRange = 2600.f;

	/** Height of the hull centre above the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank")
	float RideHeight = 125.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drops")
	float HulkbusterDropChance = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drops")
	float HealthDropChance = 0.3f;

protected:
	virtual void BeginPlay() override;
	virtual void TickEnemy(float DeltaSeconds, AIronManCharacter* Player) override;
	virtual void OnKilled() override;

	/** Yaws independently of the hull to track the player. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Turret;

	/** Pitches the barrel. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> BarrelPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Muzzle;

	float FireCooldown = 2.f;
	float Recoil = 0.f;
};
