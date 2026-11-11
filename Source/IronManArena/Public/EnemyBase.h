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

/**
 * Jocasta combat realism (contrib/jocasta/combat_realism.md): how a hit was dealt. The player's
 * heavy weapons open an FEnemyHitClassScope around their ApplyDamage calls so enemies can tell a
 * Hulkbuster punch / charged shot (Heavy) or the unibeam (Beam) apart from a repulsor bolt
 * (every damage type in the project is the plain UDamageType).
 */
enum class EEnemyHitClass : uint8
{
	Normal,
	Beam,   // continuous beams: stagger, but no hit-stop per tick
	Heavy   // punch, charged shot: stagger + hit-stop
};

struct IRONMANARENA_API FEnemyHitClassScope
{
	explicit FEnemyHitClassScope(EEnemyHitClass InHitClass);
	~FEnemyHitClassScope();
	static EEnemyHitClass GetCurrent();

private:
	EEnemyHitClass PrevHitClass;
};

namespace CombatRealism
{
	/** Tunables from Content/Story/combat_realism.json (Jocasta's file, copied verbatim); defaults if missing. */
	struct FTuning
	{
		float KnockHalfLife = 0.12f;
		float KnockDrone = 220.f;   // cm/s per damage point
		float KnockTank = 25.f;
		float KnockBoss = 8.f;
		float DroneKickMaxDeg = 25.f;
		float StaggerDrone = 0.5f;
		float StaggerTank = 0.3f;
		float StaggerBoss = 0.f;
		float HitStopKillSec = 0.03f;
		float HitStopHeavySec = 0.06f;
		float HitStopBossPhaseSec = 0.09f;
		float HitStopDilation = 0.05f;
		float DroneWobbleBelow = 0.5f;
		float DroneWobbleDeg = 4.f;
		float DroneWobbleHz = 2.f;
		float DroneAimSpreadMul = 1.25f;
		float DroneSmokeBelow = 0.25f;
		float DroneSparkMin = 1.f;
		float DroneSparkMax = 2.f;
		float DroneSagCm = 30.f;
		float DroneSpiralMin = 0.6f;
		float DroneSpiralMax = 1.2f;
		bool bLoadedFromFile = false;
	};

	IRONMANARENA_API const FTuning& Get();
}

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

	/** Health / MaxHealth (0..1). */
	float GetHealthFraction() const { return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f; }

	/** Combat realism: true while a heavy hit has the enemy staggered (no firing). */
	bool IsStaggered() const { return StaggerLeft > 0.f; }

	/** Current decaying knockback velocity (cm/s), added to the swept move every tick. */
	FVector GetKnockbackVelocity() const { return KnockVelocity; }

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

	/** Death visual after the bookkeeping (kill count, score, Destroy are immediate). Default: the blast. */
	virtual void SpawnDeathFX();

	/** Direction the hit pushes us (unit, may be zero): shot direction, projectile velocity or away from the causer. */
	FVector ComputeHitDirection(FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const;

	/** Knockback + angular kick + stagger for one hit (Jocasta combat realism 2). */
	void ApplyHitReaction(float Applied, const FVector& HitDir, bool bStaggerHit);

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

	// ---------------- Combat realism (contrib/jocasta/combat_realism.md) ----------------
	/** Decaying knockback velocity (half-life KnockHalfLife), applied with a swept MoveBy after TickEnemy. */
	FVector KnockVelocity = FVector::ZeroVector;
	/** Unit direction of the last hit (world). */
	FVector LastHitDir = FVector::ZeroVector;
	/** Seconds of stagger left (subclasses skip firing). */
	float StaggerLeft = 0.f;
	/** Drone angular kick (degrees, decays); subclasses add it to their VisualRoot pose. */
	float KickPitch = 0.f;
	float KickRoll = 0.f;

	/** Random helper. */
	static float Rand(float Min, float Max) { return FMath::FRandRange(Min, Max); }
};
