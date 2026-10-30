// AStoryAlly: Vison (and, lent by the side mission The Second Signal, Jocasta), the synthezoid ally unlocked at the end of chapter 5 ("unlockAlly": "vison").
// Summoned with G during objectives flagged "allyAvailable" (ch7, ch9). Flies beside the player for
// AllyDuration seconds and burns the nearest hostile with a gold forehead-gem beam.
// Placeholder shapes (red/green synthezoid, gold gem, cape) - a nod to the Vision Quest.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "StoryAlly.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AEnemyBase;

UCLASS()
class IRONMANARENA_API AStoryAlly : public AActor
{
	GENERATED_BODY()

public:
	AStoryAlly();

	virtual void Tick(float DeltaSeconds) override;

	/** Spawns the ally next to the player. AllyId "jocasta" gives Jocasta's look and short-range bursts. */
	static AStoryAlly* Summon(UWorld* World, AActor* Player, float Duration, FName AllyId = FName(TEXT("vison")));

	FName GetAllyId() const { return AllyKind; }

	/** Ends early with a phase-out flash. */
	void Dismiss();

	float GetTimeLeft() const { return TimeLeft; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ally")
	float BeamDamagePerSecond = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ally")
	float BeamRange = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ally")
	float FollowDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ally")
	float FollowHeight = 140.f;

protected:
	virtual void BeginPlay() override;

	AEnemyBase* FindTarget() const;

	/** Repaints the placeholder body for AllyId (Vison is the default build). */
	void ApplyLook(FName AllyId);

	UPROPERTY(VisibleAnywhere, Category = "Ally")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Ally")
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Ally")
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	UPROPERTY(VisibleAnywhere, Category = "Ally")
	TObjectPtr<UPointLightComponent> GemLight;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMID;

	TWeakObjectPtr<AActor> FollowTarget;
	TWeakObjectPtr<AEnemyBase> Target;
	float TimeLeft = 15.f;
	float Retarget = 0.f;
	float Time = 0.f;
	float OrbitSide = 1.f;
	float DamageTick = 0.f;
	FName AllyKind = FName(TEXT("vison"));
	FLinearColor GlowColor = FLinearColor(1.f, 0.85f, 0.25f);
};
