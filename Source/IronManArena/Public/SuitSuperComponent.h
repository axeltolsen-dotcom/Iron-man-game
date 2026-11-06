// Per-suit SUPER moves + Iron Spider web-swing + faceplate (mask) toggle. Lives on the player's AIronManCharacter
// (UCareerInput adds it in every mode). Table: contrib/city/super_moves.md.
//
// Trigger: when the super is charged (cooldown ring on the HUD), HOLD Right-click (0.3 s) or DOUBLE-click Left-click.
// A right-click tap still fires the suit's normal special. Some moves read context (target distance) - Mark LXXXV:
// near = nano blades combo, far = beam stomp; Iron Spider: near = spider-leg stab & pin, far = nano-blade lunge;
// War Machine: hold to keep the shoulder minigun spinning, release fires the micro-rocket pod.
//
// Every move is a timeline: ANTICIPATION (charge pose / wind-up) -> IMPACT beat (hit-stop 80-150 ms, camera
// punch-in, shake, rumble, VFX + SFX timed to the frame) -> FOLLOW-THROUGH -> RECOVERY (pose eases back out).
// The body pose is keyed through AIronManCharacter::PoseOverride (shoulders / hips / root, slerped, so nothing pops),
// root motion moves the capsule (no sliding feet: ground moves only displace while the pose is airborne / dashing).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuitSuperComponent.generated.h"

class AIronManCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AEnemyBase;

UENUM()
enum class ESuperKind : uint8
{
	BeamStomp, NanoBlades, SpiderArms, MissileBarrage, RemoteStrike, SupportPulse, GroundPound, MinigunPod, CloakSlash, HeavyBlast
};

struct FSuperDef
{
	FName SuitId;
	ESuperKind Kind = ESuperKind::HeavyBlast;
	const TCHAR* Name = TEXT("");
	float Cooldown = 12.f;
	FLinearColor Tint = FLinearColor(0.4f, 0.9f, 1.f);
};

UCLASS(ClassGroup = (IronMan))
class IRONMANARENA_API USuitSuperComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USuitSuperComponent();
	static USuitSuperComponent* Get(const AActor* Owner);

	/** Super for a suit id (falls back by the suit's special weapon). */
	static FSuperDef SuperFor(FName SuitId);
	static const TArray<FSuperDef>& Table();

	FSuperDef CurrentDef() const;
	bool IsReady() const { return CooldownLeft <= 0.f && !bActive; }
	float GetCooldownFraction() const;
	bool IsActive() const { return bActive; }
	ESuperKind GetActiveKind() const { return ActiveKind; }

	/** Iron Spider: web swing instead of flight (B toggles). */
	bool IsSwingMode() const { return bSwingMode; }
	bool IsSwinging() const { return bWebAttached; }
	bool CanSwing() const;

	/** Faceplate (H): lift / close with a hinge animation, the pilot's face shows inside. */
	void ToggleMask();
	bool IsMaskOpen() const { return bMaskOpen; }

	/** Called by the character's right-click handler when the super is ready: true = consumed (decides tap vs hold). */
	bool HandleSpecialPressed();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	AIronManCharacter* Char() const;
	void Begin(ESuperKind Kind);
	void End();
	void TickMove(float Dt);
	void TickInput(float Dt);
	void TickSwing(float Dt);
	void TickMask(float Dt);
	void TickCameraPunch(float Dt);

	// beat helpers
	void Punch(float ArmLength, float FovDelta, float Seconds);
	void Beat(float SlowSeconds, float Dilation, FName Shake, float ShakeScale, float Rumble);
	int32 AreaDamage(const FVector& Center, float Radius, float Damage, float Knock, bool bCity = true);
	int32 ConeDamage(const FVector& Origin, const FVector& Dir, float Range, float HalfAngleDeg, float Damage, float Knock);
	AEnemyBase* FindTarget(float MaxRange, float MaxAngleDeg) const;
	void SetPose(float Weight, const FRotator& Root, const FVector& RootOff, const FRotator& SL, const FRotator& SR, const FRotator& HL, const FRotator& HR);
	UStaticMeshComponent* MakePart(USceneComponent* Parent, int32 Shape, const FVector& Loc, const FVector& Scale, const FRotator& Rot, const FLinearColor& Color, float Glow);
	void ClearParts();
	void FireSalvo(int32 Count, float Speed, float Homing, float Damage, float Blast, const FLinearColor& Color, bool bFromShoulders);

	// state
	bool bActive = false;
	bool bWasFP = false;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> MinigunAudio;
	float GunAcc = 0.f;
	ESuperKind ActiveKind = ESuperKind::HeavyBlast;
	float T = 0.f;
	float CooldownLeft = 0.f;
	float CooldownTotal = 1.f;
	TArray<bool> Fired;
	bool Once(int32 Slot) { if (!Fired.IsValidIndex(Slot)) { Fired.SetNum(Slot + 1); } if (Fired[Slot]) { return false; } Fired[Slot] = true; return true; }
	TWeakObjectPtr<AEnemyBase> Target;
	FVector TargetPoint = FVector::ZeroVector;
	FVector StartLoc = FVector::ZeroVector;
	bool bLanded = false;
	float HoldTime = 0.f;
	bool bRightDown = false;
	float RightDownTime = 0.f;
	double LastLeftClick = -10.0;
	float Spin = 0.f;
	TArray<TWeakObjectPtr<AEnemyBase>> Pinned;
	TArray<FVector> PinPoints;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Temp;      // blades / legs / pieces
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TempMID;
	TArray<FVector> PieceVel;

	// camera punch
	float PunchLeft = 0.f, PunchTotal = 0.f, PunchArm = 0.f, PunchFov = 0.f, BaseArm = -1.f;

	// web swing
	bool bSwingMode = false;
	bool bWebAttached = false;
	FVector Anchor = FVector::ZeroVector;
	float RopeLen = 0.f;
	float ZipLeft = 0.f;
	FVector ZipTarget = FVector::ZeroVector;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WebLine;
	float CamRoll = 0.f;

	// mask
	bool bMaskOpen = false;
	float MaskAlpha = 0.f;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> MaskHinge;
	UPROPERTY(Transient) TObjectPtr<class APilotAvatar> Face;
	struct FMaskPart { TWeakObjectPtr<USceneComponent> C; TWeakObjectPtr<USceneComponent> Parent; };
	TArray<FMaskPart> MaskParts;
};
