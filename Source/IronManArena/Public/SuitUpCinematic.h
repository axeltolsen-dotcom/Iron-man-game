// ASuitUpCinematic: film-style suit-up, built in code (no Level Sequence asset). Shot lists follow
// contrib/research/suits_and_suitup.md section 3:
//  * Garage gantry (Mark III, Assembly / Gantry / Stealth): robot arms (2-bone IK) carry each plate
//    in bottom-up, weld sparks + hit-stop on the chest, helmet lowered from the ceiling carrier.
//  * Mid-air assembly (XLII, Drop): the pilot falls while pieces streak in on curved paths with
//    contrails, chest clamp hit-stop + shake, thrusters catch the fall.
//  * Nanotech (L / LXXXV, Nano / Ultron): tap the reactor, wet-glow nanite plates flow radially out
//    of the chest over the body with overshoot settle; the helmet forms last.
//  * Suitcase (Mark V): case opens, plates unfold up the body ring by ring.
//  Every version ends with the faceplate swinging shut on its hinge, the eyes igniting (0 -> 25 glow)
//  and a HUD boot overlay. The pilot avatar stands in the middle until the armour closes.
//  Shot-based CineCamera with hard cuts, per-shot focal length and focus on the subject; letterbox,
//  title card. Full 6.5-9 s (game start), short 2.6 s (in-game swap). Space / click skips.
//  Film pass (contrib/jocasta/cinematics): moves land and hold, lens creep, rack focus on cuts, dutch
//  that levels out, speed-ramped hit-stop, cuts placed so the eyes get a held beat and the real-armour
//  swap hides under a cut, fade up from black and a slower letterbox / title card.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuitTypes.h"
#include "ArenaVisuals.h"
#include "SuitUpCinematic.generated.h"

class AIronManCharacter;
class ACineCameraActor;
class UCinematicOverlayWidget;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSuitUpFinished, ASuitUpCinematic* /*Cinematic*/, bool /*bShortVersion*/);

UCLASS(NotBlueprintable)
class IRONMANARENA_API ASuitUpCinematic : public AActor
{
	GENERATED_BODY()

public:
	ASuitUpCinematic();

	/** Starts a suit-up for Player into suit SuitIndex. Returns nullptr if it can't run. */
	static ASuitUpCinematic* Play(AIronManCharacter* Player, int32 SuitIndex, bool bShortVersion);

	/** Jump to the end (armour on, camera back to the player). Ignores the first 0.15 s (click that started the game). */
	void Skip();

	/** Force-end immediately (quit/restart/ReturnToTitle — B08). */
	void Abort();

	bool IsShortVersion() const { return bShort; }
	bool IsFinished() const { return bFinished; }
	float GetDuration() const { return Duration; }

	/** Broadcast once when the sequence ends (played out or skipped). */
	static FOnSuitUpFinished OnFinished;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Film template the suit's ECinematicStyle maps to (contrib/research/suits_and_suitup.md section 3). */
	enum class ETemplate : uint8 { Garage, Air, Nano, Suitcase };
	/** Body group (assembly order). */
	enum EGroup : uint8 { GBoots, GLegs, GTorso, GArms, GHelmet, GFace, GEyes, GNum };

	struct FFlyer
	{
		TWeakObjectPtr<UStaticMeshComponent> Copy;
		TWeakObjectPtr<UStaticMeshComponent> Trail;   // Air: contrail streak
		TWeakObjectPtr<UMaterialInstanceDynamic> GlowMID; // Nano: wet-nanite glow / eyes: ignition ramp
		TWeakObjectPtr<UMaterialInterface> RealMat;
		FTransform RelToRoot;      // final transform in suit-root space
		FVector StartRel = FVector::ZeroVector;  // start in suit-root space
		FVector CtrlRel = FVector::ZeroVector;   // bezier control point in suit-root space
		FQuat StartRot = FQuat::Identity;        // start rotation in suit-root space
		float StartScale = 1.f;
		float Delay = 0.f;         // normalized start time (0..1 of the sequence)
		float FlyTime = 0.1f;      // normalized flight duration
		float BaseNits = 1.f;
		int32 Arm = -1;            // Garage: robot arm carrying it
		uint8 Group = GTorso;
		bool bLanded = false;
		FVector PrevLoc = FVector::ZeroVector;
	};

	struct FRobotArm
	{
		FVector BaseRel = FVector::ZeroVector;   // column foot in suit-root space
		FVector RestRel = FVector::ZeroVector;   // claw rest point
		FVector ClawRel = FVector::ZeroVector;   // current claw point (smoothed)
		TWeakObjectPtr<UStaticMeshComponent> Column, Upper, Fore, ClawA, ClawB, Light;
	};

	struct FCamShot
	{
		float Start = 0.f; float Focal = 35.f; FVector PosA, PosB, LookA, LookB; bool bWorld = false; float OrbitA = 0.f; float OrbitB = 0.f; float Whip = 0.f;
		// Film feel (Jocasta c16): lens creep, move-then-hold, settle overshoot, dutch that levels out, rack focus on the cut.
		float FocalB = 0.f;    // focal at the end of the shot (0 = no creep)
		float MoveEnd = 1.f;   // fraction of the shot the move takes; the rest is a held frame
		float Settle = 0.f;    // overshoot of the move's landing (0 = ease-out, ~1 = visible settle)
		float Roll = 0.f;      // dutch angle at the cut, levels to 0 as the move lands
		float RackFrom = 0.f;  // focus starts this many cm behind the subject and pulls on in ~0.35 s
	};

	void Begin(AIronManCharacter* InPlayer, int32 InSuitIndex, bool bInShort);
	void SpawnFlyers();
	void SpawnProps();
	void BuildShots();
	void TickFlyers(float Alpha, float Dt);
	void TickProps(float Alpha, float Dt);
	void TickBeats(float Alpha);
	void UpdateCamera(float Alpha, float DeltaSeconds);
	void Finish(bool bSkipped);
	void DestroyFlyers();
	float StyleDuration() const;
	FTransform FrameXf(float Alpha, bool bWorldAnchor = false) const;
	UStaticMeshComponent* Prop(EArenaShape Shape, const FLinearColor& Color, float Emissive, const FVector& Scale);
	void Shake(float Amount) { ShakeAmp = FMath::Max(ShakeAmp, Amount); }
	void PlaceBeam(UStaticMeshComponent* C, const FVector& A, const FVector& B, float Thick) const;

	UPROPERTY(VisibleAnywhere, Category = "Cinematic")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<ACineCameraActor> CineCam;

	UPROPERTY(Transient)
	TObjectPtr<UCinematicOverlayWidget> Overlay;

	/** Runtime copies of the armour parts, trails and set props (gantry, suitcase, platform). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FlyingParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CaseProp;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CaseLid;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CaseHinge;

	/** The pilot (avatar) standing in the middle while the armour closes around them. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PilotBody;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Thrusters;

	TArray<FFlyer> Flyers;
	TArray<FRobotArm> Arms;
	TArray<FCamShot> Shots;
	TWeakObjectPtr<AIronManCharacter> Player;
	int32 SuitIndex = 0;
	bool bShort = false;
	bool bFinished = false;
	bool bBodyShown = false;
	bool bEyesLit = false;
	bool bFaceClosed = false;
	bool bChestHit = false;
	bool bThrust = false;
	bool bTapped = false;
	bool bCaseOpened = false;
	ECinematicStyle Style = ECinematicStyle::Assembly;
	ETemplate Tpl = ETemplate::Garage;
	float Elapsed = 0.f;
	float Duration = 5.f;
	float Clock = 0.f;          // sequence time (slows down during the chest hit-stop)
	float HitStopLeft = 0.f;
	float ShakeAmp = 0.f;
	float LastLockSfx = -1.f;
	float SizeScale = 1.f;
	float SuitH = 185.f;
	float EyesAt = 0.83f;
	float RevealAt = 0.86f;
	float FaceStart = 0.76f, FaceEnd = 0.81f;
	FVector HingeRel = FVector(-6.f, 0.f, 186.f);
	FVector FaceRel = FVector(10.f, 0.f, 172.f);
	FVector ReactorRel = FVector(14.f, 0.f, 135.f);
	FVector CaseRel = FVector(60.f, -45.f, 0.f);
	FVector BaseFwd = FVector::ForwardVector;
	FVector BaseRight = FVector::RightVector;
	FTransform Root0;
	int32 LastShot = -1;
	FVector LastCamLoc = FVector::ZeroVector;
	float ShotCutAt = 0.f;       // Elapsed (real time) of the last cut, for the rack focus
	FRotator CamRot = FRotator::ZeroRotator; // operator-smoothed aim (snaps on cuts)
};
