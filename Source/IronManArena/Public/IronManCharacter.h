// AIronManCharacter: the player.
//
//  * FIRST PERSON by default: camera inside the helmet at head height, placeholder gauntlets
//    (forearms + hands) in view that raise and glow at the palm when you fire; big fists in the
//    Hulkbuster. V toggles the third-person chase camera on a SpringArm.
//  * 32 data-driven suits (FSuitDef, see SuitTypes.h): colours, part variants, stats, LMB weapon,
//    RMB special, on-board AI and suit-up cinematic all come from the suit. Hold T for the suit
//    wheel (mouse to pick, mouse wheel / Tab for the next page, release to swap), 1-5 quick slots.
//  * Ground mode (MOVE_Walking) and flight mode (MOVE_Flying), toggled with F.
//      - Flight drains arc-reactor energy (5.5/s, x1.6 in the Hulkbuster); ground recharges (22/s).
//      - At 0 energy you are forced to land and need MinFlyEnergy (20) to take off again.
//      - Space / Ctrl ascend / descend while flying. Space jumps on the ground.
//  * Repulsor (hold LMB): fast projectiles from the palms toward the crosshair (light aim assist).
//  * Chest beam / unibeam (RMB): costs 30 energy, 0.9 s, damages everything along the beam and
//    shoots down boss missiles.
//  * Hulkbuster (Q, needs a charge: every 5000 pts or a gold beacon): 20 s, the suit scales up x1.6,
//    takes only 45% damage, +15 HP on activation, LMB becomes a melee punch shockwave.
//
// Input: Enhanced Input. Input Actions and the Mapping Context are CREATED IN C++ AT RUNTIME
// (CreateDefaultInputIfNeeded) unless you assign your own assets to the UPROPERTYs below.
//
// Visuals: placeholder suit built from engine basic shapes (see ArenaVisuals.h). To use a real
// mesh: make a Blueprint child, set the inherited Mesh (SkeletalMesh) + AnimBP and untick
// bUsePlaceholderVisuals.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaVisuals.h"
#include "SuitRig.h"
#include "SuitTypes.h"
#include "InputActionValue.h"
#include "IronManCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UPointLightComponent;
class UStaticMeshComponent;
class AIronProjectile;
class AArenaGameMode;
class UMaterialInstanceDynamic;
class ASuitUpCinematic;
class UReactorComponent;

UCLASS()
class IRONMANARENA_API AIronManCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AIronManCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ------------------------------------------------------------------ gameplay API
	UFUNCTION(BlueprintCallable, Category = "IronMan")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "IronMan")
	void AddEnergy(float Amount);

	/** +1 Hulkbuster charge (from score milestones or beacons). Reason is shown on the banner. */
	UFUNCTION(BlueprintCallable, Category = "IronMan")
	void GrantHulkbusterCharge(const FString& Reason);

	/** Full reset for a new run (called by AArenaGameMode::StartNewGame). */
	UFUNCTION(BlueprintCallable, Category = "IronMan")
	void ResetForNewGame();

	// ------------------------------------------------------------------ suits
	/** Instantly switch to suit Index (colours, parts, stats, weapons). No cinematic, no voice. */
	UFUNCTION(BlueprintCallable, Category = "IronMan|Suits")
	void ApplySuit(int32 Index);

	/** Re-reads the save's side-mission rewards (reactor upgrades, white trim, plaid decal) and
	 *  repaints the current suit. Called at BeginPlay and by the story director after a reward. */
	void ApplyRewards();

	bool HasPlaidDecal() const;

	/** In-game swap: plays the short (2 s) suit-up cinematic, the new suit's AI takes over. */
	UFUNCTION(BlueprintCallable, Category = "IronMan|Suits")
	bool RequestSuitSwap(int32 Index);

	UFUNCTION(BlueprintPure, Category = "IronMan|Suits")
	int32 GetSuitIndex() const { return SuitIndex; }

	const FSuitDef& GetSuit() const { return USuitLibrary::GetSuit(SuitIndex); }

	/** Cinematic mode: input locked, invulnerable, enemies hold fire, third-person body visible. */
	void SetCinematicMode(bool bEnable, ASuitUpCinematic* Cinematic);
	bool IsInCinematic() const { return bInCinematic; }

	/** The suit-up cinematic hides the real armour while flying copies of the parts in. */
	void SetBodyHiddenForCinematic(bool bInHidden) { bCinematicHideBody = bInHidden; RefreshSuitVisibility(); }

	/** Enemies ignore the player while cloaked or during a cinematic. */
	bool IsHiddenFromEnemies() const { return bInCinematic || CloakLeft > 0.f || bDead; }

	UFUNCTION(BlueprintCallable, Category = "IronMan|Camera")
	void SetFirstPerson(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "IronMan|Camera")
	bool IsFirstPerson() const { return bFirstPerson; }

	/** Placeholder suit parts (used by the cinematic to fly copies in). */
	const TArray<FArenaPaintedPart>& GetSuitParts() const { return Parts; }
	USceneComponent* GetSuitRoot() const { return SuitRoot; }

	// --- suit wheel (HUD) ---
	static constexpr int32 WheelSlots = 8;
	bool IsSuitWheelOpen() const { return bWheelOpen; }
	int32 GetWheelPage() const { return WheelPage; }
	int32 GetWheelPageCount() const;
	/** Highlighted slot 0..7 (clockwise from the top) or INDEX_NONE. */
	int32 GetWheelSelection() const { return WheelSelection; }
	/** Suit index shown in a wheel slot on the current page, INDEX_NONE if empty. */
	int32 GetWheelSuit(int32 Slot) const;
	/** Suit index on quick slot 0..4 (keys 1-5). */
	int32 GetQuickSlotSuit(int32 Slot) const;

	// --- special ability (HUD) ---
	FText GetSpecialName() const;
	FText GetPrimaryName() const;
	float GetSpecialCost() const;
	/** Remaining time of a timed special (overdrive / shield / cloak), 0 if none. */
	float GetActiveEffectTime(FText& OutName) const;

	// ------------------------------------------------------------------ HUD getters
	UFUNCTION(BlueprintPure, Category = "IronMan")
	float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	float GetEnergy() const { return Energy; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	float GetMaxEnergy() const { return MaxEnergy; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	bool IsDead() const { return bDead; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	bool IsFlying() const;
	UFUNCTION(BlueprintPure, Category = "IronMan")
	bool IsFlightLocked() const { return bFlightLocked; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	bool IsHulkbusterActive() const { return HulkbusterTimeLeft > 0.f; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	float GetHulkbusterTimeLeft() const { return HulkbusterTimeLeft; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	int32 GetHulkbusterCharges() const { return HulkbusterCharges; }
	UFUNCTION(BlueprintPure, Category = "IronMan")
	bool IsBeamActive() const { return BeamTimeLeft > 0.f; }

	/** Power plant: energy output + heat / venting (see UReactorComponent). */
	UFUNCTION(BlueprintPure, Category = "IronMan|Reactor")
	UReactorComponent* GetReactor() const { return Reactor; }

	UFUNCTION(BlueprintPure, Category = "IronMan|Reactor")
	bool IsBoosting() const { return bBoostActive; }

	/** Harley's mounted reactor is shown (story mode on any suit, or a Mounted suit). */
	bool ShowsMountedReactor() const;

	// ------------------------------------------------------------------ story hooks
	/** Fired when the reactor forces a POWER DEPLETED landing (story 'energy' objectives). */
	FSimpleMulticastDelegate OnForcedLanding;

	/** Fired each time the Hulkbuster is deployed. */
	FSimpleMulticastDelegate OnHulkbusterActivated;

	/** Fired when the ally key (G) is pressed. */
	FSimpleMulticastDelegate OnSummonAllyPressed;

	// ---- Simulated input (AStoryAutopilot / automation tests): the same code paths as the bindings.
	void SimulateFlightKey(bool bFly) { if (!bDead && !bInCinematic && bFly != IsFlying()) { SetFlying(bFly); } }
	void SimulateHulkbusterKey() { Input_Hulkbuster(FInputActionValue()); }
	void SimulateAllyKey() { Input_Ally(FInputActionValue()); }
	void SimulateVentKey() { Input_Vent(FInputActionValue()); }
	/** Space / click during a cinematic (next shot, or skips the suit-up). */
	bool SimulateSkipKey() { return TrySkipCinematic(); }

	/**
	 * Restricts the suit wheel / quick slots / swaps to these suit ids (story mode: unlocked suits).
	 * Empty = every suit (arena).
	 */
	void SetAllowedSuits(const TArray<FName>& SuitIds);
	bool IsSuitAllowed(int32 Index) const;
	const TArray<int32>& GetSelectableSuits() const { return SelectableSuits; }

	/**
	 * Story cutscenes: while in cinematic mode without an ASuitUpCinematic, Space / click call
	 * this with bAll=false (next shot) and Esc with bAll=true (skip the scene).
	 */
	TFunction<void(bool /*bAll*/)> CinematicSkipHandler;

	/** Extra flight drain while the reactor is overheated (it can't sustain the thrusters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Reactor")
	float OverheatFlightDrainMultiplier = 2.f;

	/** Settings menu hooks (FOV in degrees; the third-person camera uses FOV - 5). */
	UFUNCTION(BlueprintCallable, Category = "IronMan|Camera")
	void SetCameraFOV(float NewFOV);

	UFUNCTION(BlueprintCallable, Category = "IronMan|Camera")
	void SetLookSettings(float Sensitivity, bool bInvertY) { LookSensitivity = Sensitivity; bInvertLookY = bInvertY; }

	// ================================================================== TUNABLES
	// (Defaults converted from the 2-D reference CONFIG; 1 px ~= 2.5 uu.)

	/** Health of an Armor x1.0 suit. MaxHealth = BaseMaxHealth * FSuitDef::Armor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Stats")
	float BaseMaxHealth = 100.f;

	/** Current suit's max health (set by ApplySuit). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|Stats")
	float MaxHealth = 100.f;

	/** Current suit's max energy (set by ApplySuit from FSuitDef::MaxEnergy). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|Stats")
	float MaxEnergy = 100.f;

	/** Suit used for the first game (id from suits.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Suits")
	FName DefaultSuitId = TEXT("mark-iii");

	/** Keys 1-5. Missing ids are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Suits")
	TArray<FName> QuickSlotSuitIds;

	/** Minimum seconds between in-game suit swaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Suits")
	float SuitSwapCooldown = 3.f;

	/** Time dilation while the suit wheel is open (1 = no slow motion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Suits")
	float SuitWheelTimeDilation = 0.3f;

	/** Seconds of invulnerability after taking a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Stats")
	float InvulnerabilityAfterHit = 0.45f;

	/** Energy per second while flying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Energy")
	float FlightEnergyDrain = 5.5f;

	/** Energy per second regained while standing on the ground (set by ApplySuit from FSuitDef::Regen). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|Energy")
	float GroundEnergyRegen = 22.f;

	/** Flight boost (hold Shift): speed multiplier, extra energy drain multiplier (heat: see Reactor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Energy")
	float BoostSpeedMultiplier = 1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Energy")
	float BoostEnergyDrainMultiplier = 2.5f;

	/** After a forced landing you need this much energy to take off again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Energy")
	float MinFlyEnergy = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float WalkSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float FlySpeed = 1350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float JumpVelocity = 750.f;

	/** How hard Space / Ctrl push up / down in flight (fraction of FlySpeed via input scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float VerticalFlightInputScale = 1.f;

	/** Flight braking (higher = snappier stops when you release the keys). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float FlightBraking = 2500.f;

	/** Can't fly higher than this above the arena floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float MaxFlightAltitude = 3500.f;

	/** Soft boundary: horizontal distance from the spawn point you can't go beyond. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Movement")
	float ArenaBoundaryRadius = 7000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	float LookSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	bool bInvertLookY = false;

	/** Start in first person (V toggles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	bool bStartInFirstPerson = true;

	/** First-person eye height above the capsule centre (capsule half height is 96). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	float FirstPersonEyeHeight = 74.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	float FirstPersonFOV = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	float CameraDistance = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	float HulkbusterCameraDistance = 650.f;

	/** Over-the-shoulder offset (SpringArm SocketOffset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Camera")
	FVector CameraShoulderOffset = FVector(0.f, 70.f, 70.f);

	// --- Repulsor ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Repulsor")
	float RepulsorDamage = 1.f;

	/** Seconds between shots while LMB is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Repulsor")
	float RepulsorFireInterval = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Repulsor")
	float RepulsorSpeed = 3200.f;

	/** Seconds the shot lives (range = speed * lifetime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Repulsor")
	float RepulsorLifetime = 1.0f;

	/** Aim assist cone (degrees) around the crosshair. 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Repulsor")
	float AimAssistDegrees = 5.f;

	// --- Chest beam ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamEnergyCost = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamDuration = 0.9f;

	/** Damage per second to every enemy inside the beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamDamagePerSecond = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamRange = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamRadius = 60.f;

	/** Beam wind-up before it starts doing damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamWindup = 0.15f;

	/** Movement input multiplier while the beam fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|ChestBeam")
	float BeamMoveMultiplier = 0.35f;

	// --- Hulkbuster ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterDuration = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterScale = 1.6f;

	/** Fraction of incoming damage taken in the Hulkbuster (0.45 = ~2.2x effective HP). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterDamageTaken = 0.45f;

	/** Instant repair when suiting up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterActivationHeal = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterFlightDrainMultiplier = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterWalkSpeed = 430.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterFlySpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float HulkbusterJumpVelocity = 560.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float PunchDamage = 4.f;

	/** Seconds per punch (also the cooldown). Impact lands at ~47% of the swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float PunchDuration = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float PunchReach = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float PunchRadius = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Hulkbuster")
	float PunchLunge = 500.f;

	/** Untick (in a Blueprint child) when you supply a real skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IronMan|Visuals")
	bool bUsePlaceholderVisuals = true;

	// ================================================================== INPUT
	// Leave empty to auto-create at runtime, or assign your own Input Action / IMC assets.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> MoveAction;        // Axis2D  WASD / left stick

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> LookAction;        // Axis2D  mouse / right stick

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> AscendAction;      // Axis1D  Space (jump on ground)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> DescendAction;     // Axis1D  Left Ctrl / C

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> ToggleFlightAction; // Bool   F

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> FireAction;        // Bool    LMB

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> BeamAction;        // Bool    RMB

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> HulkbusterAction;  // Bool    Q

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> PauseAction;       // Bool    P / Esc

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> ToggleViewAction;  // Bool    V

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> SuitWheelAction;   // Bool    T (hold)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> WheelPageAction;   // Axis1D  mouse wheel / Tab

	/** Keys 1-5. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TArray<TObjectPtr<UInputAction>> QuickSlotActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> BoostAction;       // Bool    Left Shift (hold, flight boost)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> VentAction;        // Bool    X (reactor vent)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronMan|Input")
	TObjectPtr<UInputAction> AllyAction;        // Bool    G (story: summon ally)

protected:
	/** Cosmetic reward garage-plaid-decal: a small flannel patch on the left shoulder (third person). */
	UPROPERTY(Transient)
	TObjectPtr<class UDecalComponent> PlaidDecal;

	bool bPlaidWanted = false;

	virtual void BeginPlay() override;

	// ------------------------------------------------------------------ input handlers
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Ascend(const FInputActionValue& Value);
	void Input_AscendStarted(const FInputActionValue& Value);
	void Input_Descend(const FInputActionValue& Value);
	void Input_ToggleFlight(const FInputActionValue& Value);
	void Input_FireStarted(const FInputActionValue& Value);
	void Input_FireCompleted(const FInputActionValue& Value);
	void Input_Beam(const FInputActionValue& Value);
	void Input_Hulkbuster(const FInputActionValue& Value);
	void Input_Pause(const FInputActionValue& Value);
	void Input_ToggleView(const FInputActionValue& Value);
	void Input_WheelOpen(const FInputActionValue& Value);
	void Input_WheelClose(const FInputActionValue& Value);
	void Input_WheelPage(const FInputActionValue& Value);
	void Input_Slot1(const FInputActionValue& Value) { QuickSlot(0); }
	void Input_Slot2(const FInputActionValue& Value) { QuickSlot(1); }
	void Input_Slot3(const FInputActionValue& Value) { QuickSlot(2); }
	void Input_Slot4(const FInputActionValue& Value) { QuickSlot(3); }
	void Input_Slot5(const FInputActionValue& Value) { QuickSlot(4); }
	void QuickSlot(int32 Slot);
	void Input_BoostStarted(const FInputActionValue& Value);
	void Input_BoostCompleted(const FInputActionValue& Value);
	void Input_Vent(const FInputActionValue& Value);
	void Input_Ally(const FInputActionValue& Value);

	UFUNCTION()
	void HandleReactorOverheat(bool bOverheated);
	void CloseSuitWheel(bool bApply);
	/** True if a cinematic consumed the input (skip). */
	bool TrySkipCinematic();

	/** Builds Input Actions + Mapping Context in code if no assets were assigned. */
	void CreateDefaultInputIfNeeded();

	// ------------------------------------------------------------------ mechanics
	void SetFlying(bool bFly, bool bForced = false);
	void ApplySuitMovementSettings();
	void TickEnergy(float DeltaSeconds);
	void TickWeapons(float DeltaSeconds);
	void TickBeam(float DeltaSeconds);
	void TickHulkbuster(float DeltaSeconds);
	void TickVisuals(float DeltaSeconds);
	void TickBoundary();

	/** Drives M_CarPaint "Damage" (battle wear from lost health) and "HeatGlow" (reactor heat). */
	void TickSuitWear(float DeltaSeconds);
	float SuitWearTimer = 0.f;
	float ShownSuitDamage = -1.f;
	float ShownSuitHeat = -1.f;

	void FireRepulsor();
	void UseSpecial();
	void FireHoming(int32 Count, float Speed, float Homing, float Damage, float Blast, float Radius, float Life, const FLinearColor& Color);
	void NanoBladeSweep();
	void TickEffects(float DeltaSeconds);
	void TickFirstPerson(float DeltaSeconds);
	void RefreshSuitVisibility();
	void UpdateSuitRootTransform();
	void UpdateCameraForSuit();
	void NotifyAI(uint8 Event) const;
	UCameraComponent* GetActiveCamera() const;
	void StartPunch();
	void PunchImpact();
	void ActivateHulkbuster();
	void RevertHulkbuster();
	void Die();

	/** World point under the crosshair (with aim assist). */
	FVector ComputeAimPoint() const;

	AArenaGameMode* GetArenaGameMode() const;
	void Banner(const FText& Title, const FText& Sub, float Seconds = 2.f) const;

	// ------------------------------------------------------------------ components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Inside the helmet. Active by default. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** All suit components + variants (see SuitRig.h). */
	UPROPERTY()
	FSuitRig Rig;

	/** First-person gauntlet parts (only the owner sees them). */
	UPROPERTY()
	TArray<FArenaPaintedPart> FPParts;

	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TObjectPtr<UPointLightComponent> FPPalmLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UReactorComponent> Reactor;

	bool bBoostHeld = false;
	TArray<int32> SelectableSuits; // rebuilt by SetAllowedSuits; all suits when unrestricted
	bool bBoostActive = false;
	float VentPuffTimer = 0.f;

	/** Shield special: orbiting glow orbs. */
	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TObjectPtr<USceneComponent> ShieldRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ShieldOrbs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ShieldMIDs;

	/** Root of the placeholder suit. Feet at Z=0 of this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<USceneComponent> SuitRoot;

	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> ShoulderR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> HipL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> HipR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> PalmL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> PalmR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> ArcReactor;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> FlameBootL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> FlameBootR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> FlameHandL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> FlameHandR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> HulkbusterPlates;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UPointLightComponent> ArcLight;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UPointLightComponent> RepulsorLight;

	/** Chest beam: pivot oriented along the aim, cylinder child stretched to the beam length. */
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> BeamPivot;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> BeamMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMID;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	// ------------------------------------------------------------------ state
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|State") float Health = 100.f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|State") float Energy = 100.f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|State") int32 HulkbusterCharges = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "IronMan|State") float HulkbusterTimeLeft = 0.f;

	bool bDead = false;
	bool bFlightLocked = false;
	bool bFireHeld = false;
	bool bUseLeftHand = false;
	float InvulnerableLeft = 0.f;
	float FireCooldown = 0.f;
	float AimPoseTime = 0.f;      // arm stays raised for a moment after shooting
	float BeamTimeLeft = 0.f;
	float PunchTimeLeft = 0.f;
	bool bPunchLanded = false;
	bool bPunchLeft = false;
	float HitFlashLeft = 0.f;
	float RepulsorFlash = 0.f;
	float WalkCycle = 0.f;
	float FloorZAtSpawn = 0.f;
	float BaseCapsuleRadius = 42.f;
	float BaseCapsuleHalfHeight = 96.f;
	FTransform SpawnTransform;
	TMap<TWeakObjectPtr<AActor>, float> BeamDamageAccum;

	// suits / view
	int32 SuitIndex = 0;
	bool bFirstPerson = true;
	bool bInCinematic = false;
	bool bCinematicHideBody = false;
	TWeakObjectPtr<ASuitUpCinematic> ActiveCinematic;
	float SwapCooldownLeft = 0.f;
	float SuitSpeedMul = 1.f;
	float SuitDamageMul = 1.f;

	// wheel
	bool bWheelOpen = false;
	int32 WheelPage = 0;
	int32 WheelSelection = INDEX_NONE;
	FVector2D WheelCursor = FVector2D::ZeroVector;

	// specials
	bool bBeamWhip = false;
	float OverdriveLeft = 0.f;
	float ShieldLeft = 0.f;
	float CloakLeft = 0.f;
	float FlashL = 0.f;
	float FlashR = 0.f;
	float FPKick = 0.f;

	// AI chatter state
	bool bWarnedLowEnergy = false;
	bool bWarnedLowHealth = false;
};
