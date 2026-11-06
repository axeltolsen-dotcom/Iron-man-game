// APilotAvatar: the person inside the suit (and every city pedestrian). Built at runtime from an FAvatarConfig:
//  * If the project has the engine mannequins (Manny / Quinn, Quinn for a girl) plus their animation blueprint, or a
//    manifest body from Content/NPC/npc_manifest.json, it uses that skeletal mesh (tinted through MIDs) with
//    basic-shape hair / hats / glasses attached to the head bone.
//  * Otherwise it builds a stylised basic-shape person: body proportions (boy / girl), skin tone, seven hair styles,
//    facial hair, face presets (eyes, brows, nose, jaw), and every clothing piece (tops, jackets, pants / shorts /
//    skirt, shoes, hats, glasses) in its own colour, with a procedural walk cycle.
// No binary assets are required.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "CareerTypes.h"
#include "PilotAvatar.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

UCLASS()
class IRONMANARENA_API APilotAvatar : public AActor
{
	GENERATED_BODY()

public:
	APilotAvatar();

	/** (Re)build the person. bAllowSkeletal = use Manny / Quinn / manifest bodies when present. */
	void ApplyConfig(const FAvatarConfig& Config, bool bAllowSkeletal = true);
	const FAvatarConfig& GetConfig() const { return Cfg; }

	/** Locomotion speed in cm/s (0 idle, ~150 walk, ~450 run). Drives the shape walk cycle / skeletal clip choice. */
	void SetMoveSpeed(float Speed);
	/** Hands up / cower pose (pedestrians in panic). */
	void SetPanic(bool bPanic);
	/** Fall over (ragdoll when a physics asset exists, else an animated collapse) pushed along Impulse. */
	void Collapse(const FVector& Impulse);
	bool IsCollapsed() const { return bCollapsed; }
	/** Show only the head (hair, face) - used inside the open faceplate. Returns the head-centre offset from the actor root. */
	FVector SetHeadOnly();
	/** Standing height (cm) including the Height scale. */
	float GetStandingHeight() const;
	/** True when the skeletal (Manny / Quinn / manifest) body is in use. */
	bool UsesSkeletalBody() const { return bSkeletal; }

	/** Manifest-body selection for pedestrians: -1 = auto (match the config's body type). */
	int32 ForcedBodyIndex = -1;
	/** Pedestrians: random anim play rate 0.9 .. 1.1. */
	float AnimRate = 1.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	void Clear();
	void BuildShapes();
	bool BuildSkeletal();
	void BuildHead(USceneComponent* HeadPivot, const FVector& HeadCenter, bool bOnSkeleton);
	void PlayClip(FName Category);

	USceneComponent* Pivot(USceneComponent* Parent, const FVector& Loc);
	UStaticMeshComponent* P(USceneComponent* Parent, EArenaShape Shape, const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color,
		EArenaSurface Surface = EArenaSurface::Plain, const FRotator& Rot = FRotator::ZeroRotator);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Skel;
	UPROPERTY() TArray<FArenaPaintedPart> Parts;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Pivots;

	TObjectPtr<USceneComponent> HipL, HipR, KneeL, KneeR, ShoulderL, ShoulderR, ElbowL, ElbowR, HeadPivot, Chest;

	FAvatarConfig Cfg;
	bool bSkeletal = false;
	bool bCollapsed = false;
	bool bPanicking = false;
	float MoveSpeed = 0.f;
	float Phase = 0.f;
	float CollapseT = 0.f;
	FRotator CollapseRot = FRotator::ZeroRotator;
	FVector CollapseSlide = FVector::ZeroVector;
	FName CurrentClip;
	float LegLen = 88.f;
};
