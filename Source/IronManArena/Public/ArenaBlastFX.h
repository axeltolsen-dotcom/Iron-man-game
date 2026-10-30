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
	static AArenaBlastFX* Spawn(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Color, float Duration = 0.4f);

	virtual void Tick(float DeltaSeconds) override;

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

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	float Age = 0.f;
};
