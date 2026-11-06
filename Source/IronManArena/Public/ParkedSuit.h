// AParkedSuit: the empty armour left behind when the pilot exits the suit (career key O).
// A static copy of every visible suit part (same meshes + painted materials) posed as it was left.
// Stands on the ground, or hover-idles (gentle bob + boot flames) if it was left in the air.
// "Call suit" (O while far away): it flies to the pilot (thruster loop), lands beside them and the
// short suit-up cinematic plays.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParkedSuit.generated.h"

class AIronManCharacter;
class UStaticMeshComponent;
class UAudioComponent;

UCLASS(NotBlueprintable)
class IRONMANARENA_API AParkedSuit : public AActor
{
	GENERATED_BODY()
public:
	AParkedSuit();
	static AParkedSuit* SpawnFrom(AIronManCharacter* Char, bool bHover);
	/** Fly to Target (the pilot). */
	void Call(AActor* Target);
	bool IsCalled() const { return CalledBy.IsValid(); }
	bool HasArrived() const { return bArrived; }
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Body;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Thrust;
	TWeakObjectPtr<AActor> CalledBy;
	bool bHover = false;
	bool bArrived = false;
	float Age = 0.f;
	FVector Vel = FVector::ZeroVector;
};
