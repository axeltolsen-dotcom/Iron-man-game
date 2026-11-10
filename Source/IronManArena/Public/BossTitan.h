// ABossTitan "OBSIDIAN TITAN": multi-phase flying boss that appears every 4th wave.
//
// Phases (by remaining health):
//   1 (100%-66%) : twin arm-cannon volleys, 2 bolts per arm every 2.0 s
//   2 ( 66%-33%) : 3 bolts / 1.6 s + homing missile pods (shootable)
//   3 ( 33%- 0%) : 4 bolts / 1.3 s, double missiles, telegraphed ramming dash, 2 escort drones
// Each phase change: 1.3 s invulnerability, drops an armour pack, banner on the HUD.
// Health = BaseHealth * (1 + HealthPerAppearance * (appearance - 1)).
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "BossTitan.generated.h"

class UPointLightComponent;
class UBossVFXComponent;

UENUM(BlueprintType)
enum class EBossState : uint8
{
	Entering,
	Fighting,
	Charging,
	Dashing,
	Venting    // core exposed after a dash or missile salvo: takes extra damage, holds fire
};

UCLASS()
class IRONMANARENA_API ABossTitan : public AEnemyBase
{
	GENERATED_BODY()

public:
	ABossTitan();

	virtual void InitForWave(int32 InWave) override;

	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetPhase() const { return Phase; }
	EBossState GetBossState() const { return State; }
	bool IsCoreExposed() const { return State == EBossState::Venting; }
	int32 GetVentWindowsOpened() const { return VentWindowsOpened; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsInvulnerable() const { return InvulnerableLeft > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	FText GetBossName() const { return BossName; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	FText BossName = NSLOCTEXT("IronManArena", "BossName", "OBSIDIAN TITAN");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float BaseHealth = 90.f;

	/** +35% health for every repeat appearance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float HealthPerAppearance = 0.35f;

	/** Health fractions where phase 2 / phase 3 begin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phases")
	float Phase2Threshold = 0.66f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phases")
	float Phase3Threshold = 0.33f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phases")
	float PhaseChangeInvulnerability = 1.3f;

	/** Bolts per arm per volley, index = phase (index 0 unused). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Volley")
	TArray<int32> VolleyBolts;

	/** Seconds between volleys, index = phase (index 0 unused). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Volley")
	TArray<float> VolleyCooldown;

	/** Fan spread between bolts (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Volley")
	float VolleySpreadDegrees = 11.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Volley")
	float BoltSpeed = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Volley")
	float BoltDamage = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missiles")
	float MissileDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missiles")
	float MissileSpeed = 825.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missiles")
	float MissileHoming = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missiles")
	float MissileCooldownPhase2 = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missiles")
	float MissileCooldownPhase3 = 3.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Dash")
	float DashSpeed = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Dash")
	float DashDuration = 0.6f;

	/** Wind-up before the dash (telegraph: core glows white). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Dash")
	float DashChargeTime = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Dash")
	FVector2D DashIntervalRange = FVector2D(4.5f, 6.f);

	/** Seconds the core stays exposed after a ramming dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Vent")
	float VentAfterDashSeconds = 2.2f;

	/** Seconds the core stays exposed after a missile salvo (the pods dump heat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Vent")
	float VentAfterMissilesSeconds = 1.4f;

	/** Damage multiplier while the core is exposed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Vent")
	float VentDamageMultiplier = 2.f;

	/** Telegraph ring radius under the player for missile salvos and dashes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Telegraph")
	float TelegraphRadius = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Contact")
	float ContactDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Contact")
	float ContactRange = 330.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Contact")
	float ContactCooldown = 0.8f;

	/** Hover distance from the player (normal / phase 3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Movement")
	float HoverDistance = 950.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Movement")
	float HoverDistanceRage = 750.f;

	/** Height above the player's floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Movement")
	float HoverHeight = 600.f;

	/** Drones summoned when phase 3 starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Phases")
	int32 EscortDrones = 2;

protected:
	virtual void BeginPlay() override;
	virtual void TickEnemy(float DeltaSeconds, AIronManCharacter* Player) override;
	virtual float ModifyIncomingDamage(float Amount) override;
	virtual void OnKilled() override;

	void FireVolley(AIronManCharacter* Player);
	void FireMissiles(AIronManCharacter* Player);
	void EnterPhase(int32 NewPhase);
	void OpenVent(float Seconds);
	void SetCoreColor(const FLinearColor& C);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> CannonL;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> CannonR;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Shield;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> CoreLight;

	/** Niagara layer (no-op until /Game/FX/Boss content exists; BlastFX stays the fallback). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBossVFXComponent> BossVFX;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldMID;

	EBossState State = EBossState::Entering;
	int32 Phase = 1;
	float InvulnerableLeft = 99.f;
	float VolleyTimer = 2.2f;
	float MissileTimer = 4.f;
	float DashTimer = 5.f;
	float StateTimer = 0.f;
	float ContactTimer = 0.f;
	float SideTimer = 7.f;
	float OrbitAngle = 0.f;
	float OrbitDir = 1.f;
	FVector EnterTarget = FVector::ZeroVector;
	FVector EnterPrevLoc = FVector::ZeroVector;
	float EnterElapsed = 0.f;
	float EnterStuckTime = 0.f;
	float VentLeft = 0.f;
	int32 VentWindowsOpened = 0;
	bool bVentHintShown = false;
	/** Last state pushed to BossVFX (state changes happen in several places; TickEnemy forwards them). */
	EBossState LastVFXState = EBossState::Entering;
};
