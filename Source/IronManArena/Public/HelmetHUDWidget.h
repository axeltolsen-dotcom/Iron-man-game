// UHelmetHUDWidget: the first-person helmet HUD (hud_spec.md, hud_layout.json).
//
// It runs in two build modes, like UArenaHUDWidget:
//  (a) as the parent class of a UMG asset (e.g. WBP_HelmetHUD) whose widgets are named after the
//      hud_layout.json element ids. Every binding is BindWidgetOptional, so leave out anything.
//  (b) purely in C++: when the WidgetTree has no root, BuildTree() creates the canvas tree from
//      the layout (slots, fonts, and materials from MaterialDirectory when those assets exist).
//
// Data comes from FHelmetHUDState. Push it with UpdateFromState() every tick, or leave bAutoPoll
// on and the widget fills it itself through FillStateFromGame() whenever nobody pushed a state
// that frame. Palettes, thresholds and timings come from Content/UI/hud_layout.json
// (FHelmetHUDConfig), falling back to compiled-in defaults that match it.
//
// The reference implementation flattens the spec's sub-widgets (UHUDTapeWidget, ...) into this
// one class: every element and child id is a binding here, and WarningSlot0..2 are UTextBlocks.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "HelmetHUDConfig.h"
#include "HelmetHUDTypes.h"
#include "HelmetHUDWidget.generated.h"

class AIronManCharacter;
class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class URetainerBox;
class USafeZone;
class USizeBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHelmetHUDWarning, FName, WarningId);

UCLASS()
class IRONMANARENA_API UHelmetHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Pushes this frame's HUD state. Call it every tick (from the player controller or a BP). */
	UFUNCTION(BlueprintCallable, Category = "HelmetHUD")
	void UpdateFromState(const FHelmetHUDState& State);

	/** Raises a warning from hud_layout.json warnings[] (or a free-text Info). Re-pushing refreshes it. */
	UFUNCTION(BlueprintCallable, Category = "HelmetHUD")
	void PushWarning(FName WarningId, const FText& OverrideText, float HoldSeconds = 0.f);

	UFUNCTION(BlueprintCallable, Category = "HelmetHUD")
	void ClearWarning(FName WarningId);

	/** HudOpacity is clamped to 0.5..1 and SubtitleScale to 0.8..1.6 (spec section 13). */
	UFUNCTION(BlueprintCallable, Category = "HelmetHUD")
	void SetAccessibility(bool bInColorBlindSafe, bool bInReducedMotion, float InHudOpacity, float InSubtitleScale);

	/** Re-reads hud_layout.json and re-applies slots, fonts and palettes. */
	UFUNCTION(BlueprintCallable, Category = "HelmetHUD")
	void ReloadLayout();

	/** Replays the boot sequence (it also plays on construct and whenever bVisible turns on). */
	UFUNCTION(BlueprintCallable, Category = "HelmetHUD")
	void PlayBootSequence();

	/** Id of the slot-0 warning, NAME_None if none. */
	UFUNCTION(BlueprintPure, Category = "HelmetHUD")
	FName GetTopWarning() const { return TopWarningId; }

	/** Fires when the slot-0 warning changes (play its audio here; only slot 0 plays audio). */
	UPROPERTY(BlueprintAssignable, Category = "HelmetHUD")
	FOnHelmetHUDWarning OnTopWarningChanged;

	/**
	 * Fills a state from the game using the real getters of AIronManCharacter, UReactorComponent,
	 * AArenaGameMode, ABossTitan, AStoryDirector and UJarvisComponent. InOutState is read for
	 * frame-to-frame values (previous energy for the draw rate) and then overwritten.
	 */
	static void FillStateFromGame(const UObject* WorldContext, const AIronManCharacter* Player, FHelmetHUDState& InOutState, float DeltaSeconds);

	/** If no UpdateFromState() arrived this frame, fill the state from the owning pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	bool bAutoPoll = true;

	/** Where MI_HUD_* materials are looked up for the C++-built tree. */
	UPROPERTY(EditDefaultsOnly, Category = "HelmetHUD")
	FString MaterialDirectory = TEXT("/Game/UI/Materials/");

	/** Empty = FPaths::ProjectContentDir() / UI/hud_layout.json. */
	UPROPERTY(EditDefaultsOnly, Category = "HelmetHUD")
	FString LayoutPathOverride;

	/** Key label for the vent prompt ("VENT [X]"); set it from the bound Enhanced Input key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmetHUD")
	FText VentKeyLabel;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ---- Containers (spec section 15) -----------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> HUDRetainer;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<USafeZone> HUDSafeZone;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> HUDContentBox;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RootCanvas;

	// ---- Layout elements: names = hud_layout.json ids (checked by validate_layout.py) -------------
	// BINDINGS_BEGIN
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> VisorFrame;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> WarningVignette;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PitchLadder;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeadingTape;

	// child of HeadingTape
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeadingReadout;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SpeedTape;

	// child of SpeedTape
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedReadout;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MachReadout;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AltitudeTape;

	// child of AltitudeTape
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AltitudeReadout;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AglReadout;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> VerticalSpeedCaret;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TargetLayer;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Reticle;

	// child of Reticle
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ReticleImage;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockProgressRing;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PersonaGlyph;

	// child of PersonaGlyph
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> GlyphImage;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PersonaNameText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SuitNameText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ObjectivePanel;

	// child of ObjectivePanel
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveChapterText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveProgressText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BossBar;

	// child of BossBar
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BossFill;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CommsPortrait;

	// child of CommsPortrait
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CommsFeed;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommsNameText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CommsSignalBars;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CommsRing;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ScoreCombo;

	// child of ScoreCombo
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ComboText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> IntegrityDiagram;

	// child of IntegrityDiagram
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_Helmet;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_Torso;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_SpineMount;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_ArmL;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_ArmR;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_LegL;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Region_LegR;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> IntegrityPercentText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PowerGauge;

	// child of PowerGauge
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PowerFill;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PowerValueText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PowerDrawText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeatGauge;

	// child of HeatGauge
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeatFill;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeatRecoverMarker;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeatValueText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VentPrompt;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> VentCooldownRing;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HulkbusterStatus;

	// child of HulkbusterStatus
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HulkbusterText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> WeaponReadout;

	// child of WeaponReadout
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponNameText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpecialCostText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BeamTimerText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ThreatRadar;

	// child of ThreatRadar
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> RadarSweep;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RadarRangeText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> WarningStack;

	// child of WarningStack
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WarningSlot0;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WarningSlot1;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WarningSlot2;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SubtitleBar;

	// child of SubtitleBar
	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> VisorNoise;

	UPROPERTY(BlueprintReadOnly, Category = "HelmetHUD|Bind", meta = (BindWidgetOptional))
	TObjectPtr<UImage> GlitchOverlay;
	// BINDINGS_END

private:
	struct FActiveWarning
	{
		FName Id;
		FText Label;
		int32 Priority = 7;
		FName ColorKey;
		float FlashHz = 0.f;
		float StartTime = 0.f;
		float ExpireTime = 0.f;
		float LastPushTime = 0.f;
		bool bManual = false;
	};

	struct FWarningSlotAnim
	{
		FName ShownId;
		FText ShownLabel;
		FLinearColor ShownColor = FLinearColor::White;
		float ShownFlashHz = 0.f;
		float AnimStart = -100.f;
		/** 0 = enter (scale in), 1 = slide down (demoted), 2 = slide up (promoted), 3 = fade out. */
		int32 AnimKind = 0;
	};

	struct FLockTrack
	{
		float Progress = 0.f;
		float LastSeen = 0.f;
		float FirstSeen = 0.f;
		float LockedAt = -1.f;
		float AcquireStart = -100.f;
		float LastInCone = -100.f;
		bool bInCone = false;
	};

	struct FMIDParamCache
	{
		TMap<FName, float> Scalars;
		TMap<FName, FLinearColor> Vectors;
	};

	// Setup
	void LoadLayoutConfig();
	void BuildTree();
	UWidget* CreateElementWidget(const FHUDElementDef& Def);
	void ApplyElementSlot(UWidget* Widget, const FHUDElementDef& Def) const;
	void ApplyElementFont(UTextBlock* Text, const FHUDElementDef& Def, int32 ExtraSpacing) const;
	void CacheBindings();
	UWidget* FindBoundWidget(FName Id) const;
	UMaterialInterface* LoadHUDMaterial(FName MaterialName) const;
	UObject* LoadHUDFont(FName FontKey) const;
	void SetupRetainer();

	// Per-frame update
	void TickHUD(float DeltaSeconds);
	void UpdatePalette(float DeltaSeconds);
	void UpdateFlight();
	void UpdatePersona();
	void UpdateGauges();
	void UpdateIntegrity(bool bWasHit, float DeltaSeconds);
	void UpdateTargets(float DeltaSeconds);
	void UpdateWarnings();
	void UpdateWarningSlots();
	void UpdateComms();
	void UpdateSubtitle();
	void UpdateGame();
	void UpdateRadarAndWeapon();
	void UpdateGlitch(bool bWasHit);
	void UpdateFX();
	void UpdateTransforms(float DeltaSeconds);

	// Change-only setters (null-safe)
	void SetShown(UWidget* Widget, bool bShow);
	void SetOpacityIfChanged(UWidget* Widget, float Opacity);
	void SetTextIfChanged(UTextBlock* Text, const FString& Value);
	void SetTextIfChanged(UTextBlock* Text, const FText& Value);
	void SetTextColorIfChanged(UTextBlock* Text, const FLinearColor& Value);
	void SetImageColorIfChanged(UImage* Image, const FLinearColor& Value);
	void SetTransformIfChanged(UWidget* Widget, const FVector2D& Translation, const FVector2D& Scale, float AngleDeg = 0.f);
	UMaterialInstanceDynamic* GetMID(UImage* Image);
	void SetScalar(UMaterialInstanceDynamic* MID, FName Param, float Value);
	void SetVector(UMaterialInstanceDynamic* MID, FName Param, const FLinearColor& Value);
	/** Fill 0..1: MI "Fill" param if the image has a material, otherwise X scale from the left edge. */
	void SetFill(UImage* Image, float Fill, const FLinearColor& FillColor);

	// Helpers
	FLinearColor Col(FName Key) const;
	float Now() const { return HudTime; }
	float FlashOpacity(float Hz) const;
	float AnimAlpha(FName AnimKey, float Elapsed) const;
	void RaiseWarning(FName WarningId, bool bCondition, const FText* OverrideText = nullptr);
	bool IsEssential(FName Id) const;
	int32 TopIndexOf(FName Id) const;
	void SetElementScale(FName Id, const FVector2D& Scale);
	void MulElementOpacity(FName Id, float Opacity);
	UImage* PoolImage(TArray<TObjectPtr<UImage>>& Pool, int32 Index, UCanvasPanel* Layer, FName MaterialName, bool bOutline);

	// Config / state
	FHelmetHUDConfig Config;
	FHelmetHUDState HudState;
	FHelmetHUDState PrevHudState;
	bool bStatePushedThisFrame = false;
	bool bHasState = false;
	bool bTreeBuiltInCode = false;
	float HudTime = 0.f;

	// Accessibility
	bool bColorBlindSafe = false;
	bool bReducedMotion = false;
	float HudOpacityPref = 1.f;
	float SubtitleScalePref = 1.f;

	// Boot
	float BootStartTime = -100.f;
	bool bWasVisible = false;

	// Persona palette (linear crossfade)
	int32 ShownPersona = -1;
	int32 FromPersona = 0;
	int32 FontPersona = -1;
	float PersonaSwapStart = -100.f;
	FLinearColor PalPrimary = FLinearColor::White;
	FLinearColor PalSecondary = FLinearColor::White;
	FLinearColor PalWarning = FLinearColor::White;
	FLinearColor PalCritical = FLinearColor::White;

	// Parallax / motion (one entry per top-level element)
	TArray<int32> TopElementIdx;
	TArray<FVector2D> ParallaxOffset;
	TArray<FVector2D> ExtraScale;
	TArray<float> ExtraOpacity;
	TMap<FName, int32> TopIndexById;
	FVector2D KickDir = FVector2D::ZeroVector;
	float KickStart = -100.f;

	// Integrity
	float RegionFlashStart[(int32)EHUDBodyRegion::Count];
	float RegionDip[(int32)EHUDBodyRegion::Count];

	// Locks
	TMap<int32, FLockTrack> LockTracks;
	float ReticleLock = 0.f;

	// Warnings
	TArray<FActiveWarning> ActiveWarnings;
	FWarningSlotAnim SlotAnims[3];
	FName TopWarningId;
	TSet<FName> AnnouncedWarnings;

	// Comms / subtitle
	int32 CommsPhase = 0; // 0 closed, 1 ring, 2 opening, 3 open, 4 closing
	float CommsPhaseStart = -100.f;
	float CommsLastActive = -100.f;
	FName CommsShownSpeaker;
	float SubtitleFadeStart = -100.f;
	bool bSubtitleShowing = false;

	// Glitch
	float GlitchValue = 0.f;
	int32 GlitchTier = 0;
	float TearStart = -100.f;
	float NextTearTime = 0.f;
	float BlackoutStart = -100.f;
	float NextBlackoutTime = 0.f;
	float DropoutStart = -100.f;
	float NextDropoutTime = 0.f;
	int32 TearSeedValue = 0;
	int32 TearCountValue = 0;
	bool bBlackoutActive = false;
	bool bDropoutActive = false;
	float JitterPx = 0.f;
	FRandomStream GlitchRng;

	// Bracket pool (only when TargetLayer is a UCanvasPanel)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BracketPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> OffscreenPool;

	/** Bound widget per Config.Elements index (null where unbound). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> ElementWidgets;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UImage>, TObjectPtr<UMaterialInstanceDynamic>> ImageMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RetainerMID;

	// Change caches (keys are never dereferenced)
	TMap<const UWidget*, FString> TextCache;
	TMap<const UWidget*, FLinearColor> ColorCache;
	TMap<const UMaterialInstanceDynamic*, FMIDParamCache> MIDCache;
	TSet<const UImage*> ImagesWithoutMID;
	FSlateFontInfo PaintFont;
};
