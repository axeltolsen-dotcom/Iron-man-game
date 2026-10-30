// ASuitUpCinematic: scripted suit-up sequence without any Level Sequence asset.
//
//  * Spawns an ACineCameraActor and drives it along a per-style camera path (orbit, low hero
//    rise, drop-in, nano spiral, stealth push-in, suitcase tracking + whip pan, Ultron glitch).
//  * Hides the real armour and flies runtime copies of the visible painted parts onto the body
//    with a stagger, plus Niagara-free bursts (AArenaBlastFX); the real armour reappears at ~78%.
//  * Letterbox bars + title card (suit name / mark / on-board AI) via UCinematicOverlayWidget.
//  * The suit's AI speaks the intro (full version) or the handover line (in-game swap).
//  * Full version 4.5-6.5 s (game start), short version 2 s (in-game swap). Space / click skips.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuitTypes.h"
#include "SuitUpCinematic.generated.h"

class AIronManCharacter;
class ACineCameraActor;
class UCinematicOverlayWidget;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSuitUpFinished, ASuitUpCinematic* /*Cinematic*/, bool /*bShortVersion*/);

UCLASS(NotBlueprintable)
class IRONMANARENA_API ASuitUpCinematic : public AActor
{
	GENERATED_BODY()

public:
	ASuitUpCinematic();

	/** Starts a suit-up for Player into suit SuitIndex. Returns nullptr if it can't run. */
	static ASuitUpCinematic* Play(AIronManCharacter* Player, int32 SuitIndex, bool bShortVersion);

	/** Jump to the end (armour on, camera back to the player). */
	void Skip();

	bool IsShortVersion() const { return bShort; }
	bool IsFinished() const { return bFinished; }
	float GetDuration() const { return Duration; }

	/** Broadcast once when the sequence ends (played out or skipped). */
	static FOnSuitUpFinished OnFinished;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FFlyer
	{
		TWeakObjectPtr<UStaticMeshComponent> Copy;
		FTransform RelToRoot;      // final transform relative to the suit root
		FVector StartLoc = FVector::ZeroVector;
		FQuat StartRot = FQuat::Identity;
		float StartScale = 1.f;
		float Delay = 0.f;         // normalized start time (0..1 of the sequence)
		float FlyTime = 0.2f;      // normalized flight duration
		bool bLanded = false;
	};

	void Begin(AIronManCharacter* InPlayer, int32 InSuitIndex, bool bInShort);
	void SpawnFlyers();
	void TickFlyers(float Alpha);
	void UpdateCamera(float Alpha, float DeltaSeconds);
	void Finish(bool bSkipped);
	void DestroyFlyers();
	float StyleDuration() const;

	UPROPERTY(VisibleAnywhere, Category = "Cinematic")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<ACineCameraActor> CineCam;

	UPROPERTY(Transient)
	TObjectPtr<UCinematicOverlayWidget> Overlay;

	/** Runtime copies of the armour parts (and the suitcase prop). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FlyingParts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CaseProp;

	TArray<FFlyer> Flyers;
	TWeakObjectPtr<AIronManCharacter> Player;
	int32 SuitIndex = 0;
	bool bShort = false;
	bool bFinished = false;
	bool bBodyShown = false;
	ECinematicStyle Style = ECinematicStyle::Assembly;
	float Elapsed = 0.f;
	float Duration = 5.f;
	float SizeScale = 1.f;
	FVector BaseFwd = FVector::ForwardVector;
	FVector BaseRight = FVector::RightVector;
	FVector CasePos = FVector::ZeroVector;
	float NextBurstAt = 0.f;
};
