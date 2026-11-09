// UHUDDataSubsystem (FRIDAY; merged into the live project from contrib/friday/realism): one place where the realistic helmet HUD
// reads its data. It owns the directional damage arcs and per-zone suit integrity, and it snapshots
// heat/power/flight data once per frame so every HUD widget sees the same values.
//
// Data sources (no new gameplay state):
//   AIronManCharacter::GetHealth/GetMaxHealth/GetEnergy/GetMaxEnergy/IsFlying, GetReactor()->
//   GetHeatFraction/IsVenting/IsOverheated, UJarvisComponent::GetSubtitle, and the player hit
//   reported from AIronManCharacter::TakeDamage (ReportPlayerHit, one line, see the patch).
//
// Reading: UHelmetHUDWidget::FillStateFromGame can copy GetImpactDir2D() / GetLastHitRegion() /
// GetRegionIntegrity() into FHelmetHUDState (it lists those as "not wired" today), and
// UDamageDirectionWidget paints GetArcs().
//
// Time: arcs age with FApp::GetDeltaTime() (undilated), so ArenaFeel's hit-stop (dilation 0.05)
// does not freeze them on screen.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HelmetHUDTypes.h"     // EHUDBodyRegion
#include "DamageArcMath.h"
#include "HUDDataSubsystem.generated.h"

class AIronManCharacter;
class AController;
struct FDamageEvent;

/** One resolved player hit (what the HUD needs, nothing more). */
USTRUCT(BlueprintType)
struct FHUDHitReport
{
	GENERATED_BODY()

	/** World-space unit vector from the player TOWARD the hit source (horizontal part drives the arc). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") FVector FromDir = FVector::ForwardVector;
	/** Where the hit landed (actor location when unknown). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") FVector HitLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") float Damage = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") float MaxHealth = 100.f;
	/** Screen angle, degrees: 0 = ahead/top, +90 = right, -90 = left, +-180 = behind. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") float ScreenAngleDeg = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") EHUDBodyRegion Region = EHUDBodyRegion::Torso;
	/** false when nothing told us where the hit came from (the arc is then drawn at the top). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hit") bool bDirectionKnown = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHUDPlayerHit, const FHUDHitReport&, Report);

UCLASS()
class IRONMANARENA_API UHUDDataSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHUDDataSubsystem* Get(const UObject* WorldContext);

	/**
	 * Call from AIronManCharacter::TakeDamage once Applied damage is known (after the shield /
	 * invulnerability early-outs). Resolves the hit direction, adds or merges an arc, and wears the
	 * zone it struck.
	 */
	void ReportPlayerHit(const AIronManCharacter* Victim, float AppliedDamage, float MaxHealth,
		const FDamageEvent& DamageEvent, const AController* EventInstigator, const AActor* DamageCauser);

	/** Blueprint / test entry: a hit from world direction FromDir (player -> source). */
	UFUNCTION(BlueprintCallable, Category = "HUD|Hit")
	void ReportPlayerHitFromDirection(FVector FromDir, float Damage, float MaxHealth, float ControlYawDeg);

	/**
	 * Hit-direction resolution, in order (Jocasta, combat_realism.md section 5: record the hit location and direction):
	 *  1. FPointDamageEvent      -> -ShotDirection, HitInfo.ImpactPoint
	 *  2. FRadialDamageEvent     -> Origin - victim
	 *  3. DamageCauser with velocity within 2 m (direct projectile hit) -> -Velocity
	 *  4. DamageCauser location  (splash centre, boss contact)
	 *  5. EventInstigator's pawn location
	 * Returns false if none applies (OutFromDir = victim forward).
	 */
	static bool ResolveHitSource(const AActor* Victim, const FDamageEvent& DamageEvent, const AController* EventInstigator,
		const AActor* DamageCauser, FVector& OutFromDir, FVector& OutHitLocation);

	/** Which suit zone a hit most likely struck: height on the capsule plus the screen angle. */
	static EHUDBodyRegion RegionForHit(float ScreenAngleDeg, float HeightAlpha01);

	// ---- read side -------------------------------------------------------------------------
	const DamageArcMath::FArcSet& GetArcs() const { return Arcs; }
	DamageArcMath::FArcConfig& MutableArcConfig() { return Arcs.Config; }

	UFUNCTION(BlueprintPure, Category = "HUD|Hit") FHUDHitReport GetLastHit() const { return LastHit; }
	UFUNCTION(BlueprintPure, Category = "HUD|Hit") float GetSecondsSinceHit() const { return SecondsSinceHit; }
	/** Increments once per reported hit: consumers detect a NEW hit by comparing it with the last value they saw. */
	int32 GetHitSerial() const { return HitSerial; }
	/** True once AIronManCharacter::TakeDamage has reported a hit (the TakeDamage hook is installed). */
	bool HasTakeDamageHook() const { return bHookSeen; }
	/** Screen-space 2D direction of the last hit for FHelmetHUDState::ImpactDir2D (x right, y down); zero after 0.1 s. */
	UFUNCTION(BlueprintPure, Category = "HUD|Hit") FVector2D GetImpactDir2D() const;
	UFUNCTION(BlueprintPure, Category = "HUD|Hit") int32 GetLastHitRegion() const { return SecondsSinceHit < 0.1f ? int32(LastHit.Region) : -1; }
	/** 7 entries, indexed by EHUDBodyRegion, 0..1. */
	UFUNCTION(BlueprintPure, Category = "HUD|Suit") TArray<float> GetRegionIntegrity() const { return RegionIntegrity; }

	UFUNCTION(BlueprintPure, Category = "HUD|Suit") float GetHealthFraction() const { return HealthFraction; }
	UFUNCTION(BlueprintPure, Category = "HUD|Suit") float GetPowerFraction() const { return PowerFraction; }
	/** Fraction of max per second (negative = draining). */
	UFUNCTION(BlueprintPure, Category = "HUD|Suit") float GetPowerRate() const { return PowerRate; }
	UFUNCTION(BlueprintPure, Category = "HUD|Suit") float GetHeatFraction() const { return HeatFraction; }
	UFUNCTION(BlueprintPure, Category = "HUD|Suit") bool IsVenting() const { return bVenting; }
	UFUNCTION(BlueprintPure, Category = "HUD|Flight") bool IsFlying() const { return bFlying; }
	UFUNCTION(BlueprintPure, Category = "HUD|Flight") float GetAirspeedMs() const { return AirspeedMs; }
	/** Always -1 in the live merge: UHelmetHUDWidget::FillStateFromGame already traces AGL once per frame. */
	UFUNCTION(BlueprintPure, Category = "HUD|Flight") float GetAltitudeM() const { return AltitudeM; }
	UFUNCTION(BlueprintPure, Category = "HUD|Flight") float GetVerticalSpeedMs() const { return VerticalSpeedMs; }
	/** Settings: UArenaGameUserSettings::bHudReducedMotion. */
	UFUNCTION(BlueprintPure, Category = "HUD") bool IsReducedMotion() const;

	UPROPERTY(BlueprintAssignable, Category = "HUD|Hit")
	FOnHUDPlayerHit OnPlayerHit;

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void AddArc(const FHUDHitReport& Report);
	void SnapshotPlayer(float RealDt);

	/** Fallback when the TakeDamage hook is missing: a health drop becomes an arc toward the nearest enemy. */
	void ReportUnhookedHealthDrop(const AIronManCharacter* Player, float Damage);

	DamageArcMath::FArcSet Arcs;
	FHUDHitReport LastHit;
	int32 HitSerial = 0;
	bool bHookSeen = false;
	bool bReportedSinceSnapshot = false;
	float SecondsSinceHit = 1e6f;
	TArray<float> RegionIntegrity;
	float LastHealth = -1.f;

	float HealthFraction = 1.f, PowerFraction = 1.f, PowerRate = 0.f, HeatFraction = 0.f;
	float LastPower = -1.f;
	bool bVenting = false, bFlying = false;
	float AirspeedMs = 0.f, AltitudeM = -1.f, VerticalSpeedMs = 0.f;
};
