// AStoryCharacterNPC: a cutscene actor for story mode (Tony, Pepper, Morgan, Harley out of the suit,
// Vison, U, DUM-E ...). It is an ACharacter so the stock Manny/Quinn animation blueprints work.
//
// Look, in priority order (see Docs/ADDING_METAHUMANS.md):
//   1. FStoryCastMember::MetaHumanClass  - the director spawns that Blueprint instead of this actor
//   2. FStoryCastMember::Mesh (+AnimClass) on this actor's skeletal mesh
//      (defaults: /Game/Characters/Mannequins/Meshes/SKM_Manny / SKM_Quinn from the Third Person template)
//   3. a placeholder built from basic shapes (human, robot arm, synthezoid, or a SuitRig suit)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaVisuals.h"
#include "SuitRig.h"
#include "StoryCharacterNPC.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EStoryBody : uint8
{
	None,        // voice only (suit AIs, comms)
	Human,
	Robot,       // U / DUM-E workshop arms
	Synthezoid,  // Vison
	Suit,        // an armour from suits.json (Morgan in Rescue for the finale)
	Hologram     // Tony's message (human placeholder, translucent blue glow)
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FStoryCastMember
{
	GENERATED_BODY()

	/** Speaker key from story.json (TONY, PEPPER, MORGAN, PLAYER, Vison, U, DUM-E ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	FName Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	EStoryBody Body = EStoryBody::Human;

	/** Skeletal mesh (MetaHuman body, Manny/Quinn ...). Unset or missing -> placeholder shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Animation blueprint for Mesh (e.g. the template's ABP_Unarmed / ABP_Manny). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	TSoftClassPtr<UAnimInstance> AnimClass;

	/** A full MetaHuman Blueprint (BP_<Name> from Fab/Bridge). Wins over Mesh when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	TSoftClassPtr<AActor> MetaHumanClass;

	/** Uniform scale (teens ~0.85-0.92). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	float Scale = 1.f;

	/** Placeholder colour (clothes / plating). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	FLinearColor Tint = FLinearColor(0.3f, 0.32f, 0.36f);

	/** For Body == Suit: suit id from suits.json. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	FName SuitId;
};

UCLASS()
class IRONMANARENA_API AStoryCharacterNPC : public ACharacter
{
	GENERATED_BODY()

public:
	AStoryCharacterNPC();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetActorHiddenInGame(bool bNewHidden) override;

	/** Apply a cast entry (loads the mesh synchronously; cutscenes start after a fade). */
	void SetupCast(const FStoryCastMember& Member);

	/** Speaking: head bob / glow pulse on the placeholder. */
	void SetSpeaking(bool bInSpeaking) { bSpeaking = bInSpeaking; }

	/** Prologue flashback: Damage 0..1 scorches the suit plating, ReactorLevel 0..1 scales every suit glow (1 = normal). */
	void SetWounded(float Damage, float ReactorLevel);

	/** Eye-height point for camera framing. */
	FVector GetFocusPoint() const;

	FName GetSpeaker() const { return CastInfo.Speaker; }

	/** True if the skeletal mesh is used (not the shapes). */
	bool UsesSkeletalMesh() const { return bUsingMesh; }

	/** The spawned MetaHuman Blueprint actor (FStoryCastMember::MetaHumanClass), if any. */
	AActor* GetMetaHumanActor() const { return MetaHumanActor.Get(); }

	/** Default Manny / Quinn paths (Third Person template content). */
	static const TCHAR* DefaultMannyPath;
	static const TCHAR* DefaultQuinnPath;
	static const TCHAR* const* GetDefaultAnimClassPaths(int32& OutNum);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Spawns Member.MetaHumanClass attached to this actor; false if unset / missing. */
	bool SpawnMetaHuman(const FStoryCastMember& Member);

	UPROPERTY(Transient)
	TObjectPtr<AActor> MetaHumanActor;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<USceneComponent> ShapeRoot;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<USceneComponent> HumanRoot;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<USceneComponent> RobotRoot;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<USceneComponent> SynthRoot;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<USceneComponent> SuitRoot;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<USceneComponent> HeadPivot;

	UPROPERTY(VisibleAnywhere, Category = "Story")
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	UPROPERTY()
	TArray<FArenaPaintedPart> SuitParts;

	UPROPERTY()
	FSuitRig Rig;

	/** Indices into Parts that take the cast tint (clothes) vs. skin. */
	TArray<int32> TintParts;
	int32 GemPart = INDEX_NONE;

	/** Suit colours / glows as painted by SetupCast (SetWounded scales from these). */
	TArray<FLinearColor> SuitBaseColors;
	TArray<float> SuitBaseGlow;
	float AppliedDamage = 0.f;

	FStoryCastMember CastInfo;
	bool bSpeaking = false;
	bool bUsingMesh = false;
	float Time = 0.f;
};
