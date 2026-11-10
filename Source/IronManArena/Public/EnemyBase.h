// AEnemyBase: shared base for ADroneEnemy, ATankBot and ABossTitan.
//
// Enemies are lightweight kinematic pawns (no AIController / CharacterMovement): a Pawn-channel
// sphere root that is moved with swept SetActorLocation, plus a VisualRoot of basic-shape parts.
// Subclasses implement TickEnemy() for their behaviour. Damage arrives through the standard
// AActor::TakeDamage pipeline (UGameplayStatics::ApplyDamage) so Blueprint damage also works.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ArenaVisuals.h"
#include "Pickup.h"
#include "EnemyBase.generated.h"

class USphereComponent;
class AIronManCharacter;
class AIronProjectile;
class AArenaGameMode;

UENUM(BlueprintType)
enum class EEnemyKind : uint8
{
	Drone,
	Tank,
	Boss
};

UCLASS(Abstract)
class IRONMANARENA_API AEnemyBase : public APawn
{
	GENERATED_BODY()

public:
	AEnemyBase();

	virtual void Tick(float DeltaSeconds) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** Called by the game mode right after spawning: wave number for difficulty scaling. */
	virtual void InitForWave(int32 InWave) { Wave = InWave; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	int32 GetScoreValue() const { return ScoreValue; }

	EEnemyKind GetKind() const { return Kind; }

	/** Point the player should aim at (centre of mass). */
	virtual FVector GetAimPoint() const { return GetActorLocation(); }

	/** Rough radius used by the chest beam / melee hit tests and aim assist. */
	float GetHitRadius() const;

	// ---------------- Tunables (shared) ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float MaxHealth = 3.f;

	/** Base points for a kill (multiplied by the combo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	int32 ScoreValue = 100;

	/** Seconds the parts flash white after being hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Visuals")
	float HitFlashTime = 0.12f;

	/** Size of the death explosion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Visuals")
	float DeathBlastRadius = 250.f;

	/** Untick in a Blueprint subclass to hide the placeholder shapes and add your own mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Visuals")
	bool bUsePlaceholderVisuals = true;

protected:
	virtual void BeginPlay() override;

	/** Per-frame behaviour. Player may be null / dead. */
	virtual void TickEnemy(float DeltaSeconds, AIronManCharacter* Player) {}

	/** Damage filter (boss invulnerability, phase changes). Return the damage actually taken. */
	virtual float ModifyIncomingDamage(float Amount) { return Amount; }

	/** Called once after Health reaches 0, before the actor is destroyed. Spawn drops here. */
	virtual void OnKilled();

	/** Spawn a pickup slightly offset from this enemy. */
	void DropPickup(EPickupType Type, const FVector& Offset = FVector::ZeroVector);

	/** Fire an enemy shot. Returns the projectile (may be null). */
	AIronProjectile* FireShot(const FVector& Origin, const FVector& Velocity, float Damage, float Radius,
		const FLinearColor& Color, float BlastRadius = 0.f, float GravityScale = 0.f,
		USceneComponent* HomingTarget = nullptr, float HomingAccel = 0.f, float LifeSeconds = 4.f);

	/** Swept move. Returns false if blocked. */
	bool MoveBy(const FVector& Delta);

	/** Smoothly yaw toward a world location. */
	void FaceToward(const FVector& Target, float DeltaSeconds, float DegreesPerSecond);

	/** Floor height under a location (trace against WorldStatic, starting StartAbove uu above it). */
	bool FindFloorZ(const FVector& Location, float& OutZ, float StartAbove = 500.f) const;

	/**
	 * B17: spring-damper steering toward Target, semi-implicit Euler sub-stepped at <= 1/60 s so the
	 * overshoot doesn't depend on frame rate. Updates InOutVelocity, returns the displacement to MoveBy.
	 */
	static FVector IntegrateSpringSteer(FVector& InOutVelocity, const FVector& From, const FVector& Target,
		float Stiffness, float DampingRate, float DeltaSeconds);

	AArenaGameMode* GetArenaGameMode() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|State")
	float Health = 3.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|State")
	int32 Wave = 1;

	EEnemyKind Kind = EEnemyKind::Drone;

	/** Current velocity for the simple kinematic movement. */
	FVector Velocity = FVector::ZeroVector;

	float Age = 0.f;
	float FlashLeft = 0.f;
	bool bDead = false;

	/** Random helper. */
	static float Rand(float Min, float Max) { return FMath::FRandRange(Min, Max); }
};
