// City life for Midtown (ADowntownDistrict): destructible cars (parked + simple traffic on the road grid),
// pooled pedestrians (max 12 on screen; data-driven looks / anims / Piper voices from npc_manifest.json) and a
// GTA-style 1..5 star wanted level with escalating responders (police drones -> armoured drones + tanks ->
// gunships) that decays once you stay out of the search area.
//
// Damage entry point for everything in the city: CityLife::Damage(...) (projectile blasts / direct hits, the
// unibeam, punches, car explosions).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "CareerTypes.h"
#include "CityLife.generated.h"

class UBoxComponent;
class UCapsuleComponent;
class UPointLightComponent;
class APilotAvatar;
class ADowntownDistrict;
class AEnemyBase;
class USoundAttenuation;
class USoundBase;

namespace CityLife
{
	/** Damage cars / pedestrians in Radius (DirectHit gets the full amount). bFromPlayer counts as a crime. */
	IRONMANARENA_API void Damage(UWorld* World, const FVector& Location, float Radius, float Amount, AActor* DirectHit, bool bFromPlayer);
	/** True for wanted-level responders (excluded from the arena's hostile count). */
	IRONMANARENA_API bool IsResponder(const AActor* Actor);
}

UENUM()
enum class ECityCarMode : uint8 { Parked, Traffic, Wreck };

UCLASS()
class IRONMANARENA_API ACityCar : public AActor
{
	GENERATED_BODY()

public:
	ACityCar();
	void Setup(int32 Paint, ECityCarMode Mode);
	void ApplyCityDamage(float Amount, const FVector& From, bool bFromPlayer);
	bool IsWrecked() const { return Mode == ECityCarMode::Wreck; }
	ECityCarMode GetMode() const { return Mode; }
	/** Traffic: drive along a road (Axis 0 = along X, 1 = along Y; Dir +-1). */
	void StartDriving(int32 Axis, float Dir, float Speed);

	virtual void Tick(float DeltaSeconds) override;

	/** Traffic: lane centre line coordinate (world) on the other axis. */
	float LaneLine = 0.f;
	int32 DriveAxis = 0;
	float DriveDir = 1.f;
	float DriveSpeed = 1300.f;
	float CurrentSpeed = 0.f;
	float LastTurnAt = 0.f;
	float StopHold = 0.f;
	/** 0 coupe, 1 sedan, 2 van/taxi-ish — procedural body proportions. */
	int32 BodyVariant = 0;
	bool bRecycle = false;

protected:
	void Explode(bool bFromPlayer);
	void TickDamageFX(float DeltaSeconds);
	void TickTraffic(float DeltaSeconds);

	UPROPERTY() TObjectPtr<UBoxComponent> Box;
	UPROPERTY() TObjectPtr<USceneComponent> Body;
	UPROPERTY() TArray<FArenaPaintedPart> Parts;
	UPROPERTY() TArray<FArenaPaintedPart> SmokeParts;
	UPROPERTY() TObjectPtr<UPointLightComponent> FireLight;
	TArray<int32> LightParts;
	TArray<FVector> SmokeBase;
	ECityCarMode Mode = ECityCarMode::Parked;
	float Health = 8.f;
	float MaxHealth = 8.f;
	float BurnTimer = 0.f;
	float PhysicsTimer = 0.f;
	float FxTime = 0.f;
	bool bLastHitByPlayer = false;
	int32 PaintIndex = 0;
};

UENUM()
enum class EPedState : uint8 { Wander, Idle, Watch, Flee, Dead, Pooled };

UCLASS()
class IRONMANARENA_API ACityPedestrian : public AActor
{
	GENERATED_BODY()

public:
	ACityPedestrian();
	/** Random look, gait and voice. */
	void Randomize(FRandomStream& Rng);
	void Activate(const FVector& Location);
	void Pool();
	bool IsPooled() const { return State == EPedState::Pooled; }
	bool IsDeadPed() const { return State == EPedState::Dead; }
	void ApplyCityDamage(float Amount, const FVector& From, bool bFromPlayer);
	/** Something scary at Location (explosion, gunfire). */
	void Scare(const FVector& Location, bool bExplosion);
	FName GetVoice() const { return VoiceId; }

	virtual void Tick(float DeltaSeconds) override;

	float DeadTime = 0.f;

protected:
	void Bark(FName Category, float Cooldown = 8.f);
	void PickWanderTarget();

	UPROPERTY() TObjectPtr<UCapsuleComponent> Capsule;
	UPROPERTY() TObjectPtr<APilotAvatar> Avatar;
	EPedState State = EPedState::Pooled;
	FVector Target = FVector::ZeroVector;
	FVector Threat = FVector::ZeroVector;
	FName VoiceId;
	float WalkSpeed = 140.f;
	float PedHealth = 1.f;
	bool bFemaleSfx = false;
	float StateTime = 0.f;
	float NextBarkAt = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;
	FRandomStream Rng;
};

UCLASS()
class IRONMANARENA_API ACityLife : public AActor
{
	GENERATED_BODY()

public:
	ACityLife();
	static ACityLife* Find(const UObject* WorldContext);
	static ACityLife* Spawn(ADowntownDistrict* District);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- wanted level
	int32 GetStars() const { return Stars; }
	float GetHeat() const { return Heat; }
	/** 0..1 while hiding (search lost countdown). */
	float GetEscapeFraction() const;
	bool IsPlayerInSearchArea() const { return bInSearch; }
	FVector GetSearchCenter() const { return SearchCenter; }
	float GetSearchRadius() const;
	void AddCrime(float HeatAmount, const FVector& Where);
	void SetWanted(int32 InStars);
	void ClearWanted();
	void OnResponderKilled(AEnemyBase* Enemy);

	// ---- people / cars
	void OnPedKilled(ACityPedestrian* Ped, bool bByPlayer);
	void ScareAround(const FVector& Where, float Radius, bool bExplosion);
	ACityCar* WakeParkedCar(const FTransform& Xf, int32 Paint);
	ADowntownDistrict* GetDistrict() const { return District.Get(); }
	const TArray<TObjectPtr<ACityPedestrian>>& GetPeds() const { return Peds; }
	const TArray<TObjectPtr<ACityCar>>& GetCars() const { return Cars; }

	/** Plays a spatialised bark at Location (global rate limit). */
	bool PlayBark(FName VoiceId, FName Category, const FVector& Location);

	/** Caps from CareerEconomy / city_economy.json (defaults 12 / 8). */
	int32 MaxPeds = 12;
	int32 MaxTraffic = 8;
	bool bLifeEnabled = true;

protected:
	void TickPeds(float DeltaSeconds, const FVector& PlayerLoc, bool bNear);
	void TickTraffic(float DeltaSeconds, const FVector& PlayerLoc, bool bNear);
	void TickWanted(float DeltaSeconds, const FVector& PlayerLoc);
	void SpawnResponders();
	bool PlaceTrafficCar(ACityCar* Car, const FVector& PlayerLoc);

	TWeakObjectPtr<ADowntownDistrict> District;
	UPROPERTY() TArray<TObjectPtr<ACityPedestrian>> Peds;
	UPROPERTY() TArray<TObjectPtr<ACityCar>> Cars;       // traffic
	UPROPERTY() TArray<TObjectPtr<ACityCar>> Wrecks;     // woken parked cars + wrecks
	UPROPERTY() TArray<TWeakObjectPtr<AEnemyBase>> Responders;
	UPROPERTY() TObjectPtr<USoundAttenuation> BarkAttenuation;

	float Heat = 0.f;
	int32 Stars = 0;
	FVector SearchCenter = FVector::ZeroVector;
	bool bInSearch = false;
	float HiddenTime = 0.f;
	float ResponderTimer = 0.f;
	float PaTimer = 0.f;
	/** Siren loop on the closest responder while wanted (ArenaAudio police_siren). */
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> SirenAudio;
	TWeakObjectPtr<AActor> SirenOwner;
	float LightPhase = 0.f;
	double LastBarkTime = 0.0;
	FRandomStream Rng;
};
