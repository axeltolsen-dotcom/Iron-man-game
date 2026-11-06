// ACityVehicle: dealership vehicles (Career::Vehicles) with arcade handling. The player character stays possessed
// (hidden, attached, input off) and AArenaPlayerController forwards the keys; the view switches to the vehicle's
// chase camera.
//   Car / bike : W/S throttle + brake/reverse, A/D steer, Space handbrake.
//   Plane      : Shift/Ctrl throttle, W/S pitch, A/D roll, Q/E rudder. Rotate > ~130 km/h on the airstrip.
//   Helicopter : Space/Ctrl up/down, W/S forward/back, A/D strafe, Q/E (and the mouse) yaw.
//   E          : get out.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "CareerTypes.h"
#include "CityVehicle.generated.h"

class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class AIronManCharacter;
class APilotAvatar;
class APlayerController;

UCLASS()
class IRONMANARENA_API ACityVehicle : public AActor
{
	GENERATED_BODY()

public:
	ACityVehicle();

	static ACityVehicle* SpawnVehicle(UWorld* World, FName VehicleId, const FTransform& Xf);
	void Setup(FName VehicleId);
	FName GetVehicleId() const { return VehicleId; }
	ECityVehicleKind GetKind() const { return Kind; }

	bool Enter(AIronManCharacter* Driver);
	void Exit();
	bool IsOccupied() const { return Driver.IsValid(); }
	float GetSpeedKmh() const { return FMath::Abs(Speed) * 0.036f; }
	float GetAltitudeM() const;
	float GetThrottle() const { return Throttle; }

	/** Raw input each frame from the controller (axes -1..1). */
	void SetInput(float InForward, float InRight, float InUp, float InYaw, bool bInBrake, float InThrottleDelta, float InMouseYaw);

	virtual void Tick(float DeltaSeconds) override;

protected:
	void BuildCar(const FVehicleDef& D);
	void BuildBike(const FVehicleDef& D);
	void BuildPlane(const FVehicleDef& D, bool bJet);
	void BuildHeli(const FVehicleDef& D);
	UStaticMeshComponent* Part(USceneComponent* Parent, EArenaShape Shape, const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color,
		EArenaSurface Surface = EArenaSurface::Plain, float Emissive = 0.f, const FRotator& Rot = FRotator::ZeroRotator);
	void TickGround(float DeltaSeconds);
	void TickPlane(float DeltaSeconds);
	void TickHeli(float DeltaSeconds);
	bool TraceGround(const FVector& At, float& OutZ, FVector& OutNormal) const;
	void HitThings(const FVector& From, const FVector& To);

	UPROPERTY() TObjectPtr<UBoxComponent> Box;
	UPROPERTY() TObjectPtr<USceneComponent> Body;
	UPROPERTY() TObjectPtr<USceneComponent> Spinner;
	UPROPERTY() TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<APilotAvatar> Rider;
	UPROPERTY() TArray<FArenaPaintedPart> Parts;
	TArray<TObjectPtr<USceneComponent>> Wheels;

	TWeakObjectPtr<AIronManCharacter> Driver;
	FName VehicleId;
	ECityVehicleKind Kind = ECityVehicleKind::Car;
	FVector HalfSize = FVector(220.f, 95.f, 70.f);
	float TopSpeed = 5000.f;
	float Speed = 0.f;
	float VerticalSpeed = 0.f;
	float Throttle = 0.f;
	float Steer = 0.f;
	float WheelSpin = 0.f;
	float RotorSpin = 0.f;
	FRotator Attitude = FRotator::ZeroRotator;
	FVector HeliVel = FVector::ZeroVector;
	bool bGrounded = true;

	// input
	float InForward = 0.f, InRight = 0.f, InUp = 0.f, InYaw = 0.f, InThrottleDelta = 0.f, InMouseYaw = 0.f;
	bool bInBrake = false;
};
