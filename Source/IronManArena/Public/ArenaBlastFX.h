// AArenaBlastFX: zero-asset explosion / impact flash.
// A glowing sphere that grows and fades plus a point light, then self-destructs.
// Spawn with AArenaBlastFX::Spawn(World, Location, Radius, Color).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaBlastFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class IRONMANARENA_API AArenaBlastFX : public AActor
{
	GENERATED_BODY()

public:
	AArenaBlastFX();

	/** Convenience spawner. Radius in Unreal units (cm). */
	static AArenaBlastFX* Spawn(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Color, float Duration = 0.4f, bool bShake = true);

	virtual void Tick(float DeltaSeconds) override;

	/** Pool hooks (UArenaFXPool): hide + stop ticking, or come back at a new spot. */
	void Park();
	void ReactivateFromPool(const FVector& Location);
	bool IsActiveBlast() const { return bActiveBlast; }

	UPROPERTY(EditAnywhere, Category = "FX")
	float MaxRadius = 200.f;

	UPROPERTY(EditAnywhere, Category = "FX")
	float Duration = 0.4f;

	UPROPERTY(EditAnywhere, Category = "FX")
	FLinearColor Color = FLinearColor(1.f, 0.6f, 0.2f);

	UPROPERTY(EditAnywhere, Category = "FX")
	float LightIntensity = 5000.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void LifeSpanExpired() override;
	void SetLightClaimed(bool bWant);

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UStaticMeshComponent> Ball;

	/** Flat expanding blast plate (film shell) — sits on Ball so pool relocate still works. */
	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UStaticMeshComponent> Ring;

	/** Thin outer shock ring — expands ahead of the plate for readable impact. */
	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UStaticMeshComponent> Shock;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShockMID;

	float Age = 0.f;
	bool bActiveBlast = true;
	bool bHasLight = false;
};
