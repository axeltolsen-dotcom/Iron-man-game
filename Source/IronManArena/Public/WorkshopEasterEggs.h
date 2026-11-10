// AWorkshopEasterEggs: Dum-E's workshop nook (contrib/dume/easter_eggs.md), a purely cosmetic corner that free play
// drops once per world near the player start. Built from engine basic shapes (ArenaVisuals::AddPart), TextRender
// nameplates and point lights; sounds go through ArenaAudio events; toasts / prompts use the career HUD.
//
//  #1 Eager Extinguisher - stare at the "DUM-E - DO NOT ENGAGE" extinguisher for 1.5 s (or pass within 1.5 m):
//     one foam spray at the player, a brief helmet frost on the career HUD and a toast. Afterwards it only rattles.
//  #2 Bot Bay Cameo     - 8 charging docks (Dum-E, U, Jarvis, Friday, Edith, Vison, Ultron, Jocasta - offline) light
//     up one by one as you approach; Jarvis's tablet reads "Good evening, Axel."; Ultron flickers red then settles
//     amber; Friday blinks teal; Dum-E waves. Looking at Ultron's plate for 3 s blinks every dock once in unison.
//  #3 Sticky Note Pinboard - [E] (career interact key) cycles the notes as a HUD toast; reading all five once
//     gives +50 score "Workshop Historian" (once per nook).
//  #4 Covered Car That Isn't - [E] pulls the tarp: another covered car, another, then a scrap Mark I; a 4th pull
//     puts the tarp back.
//  #5 Wrong Tool, Right Job - below 40% health, idle 2 s at the tool tray: the Calibration Duck slides out, [E] = +5 HP
//     once. At higher health [E] just makes Dum-E nudge the extinguisher.
//
// Never spawned (and self-destroys) in -ArenaSmoke, -StoryAutoplay, automation tests, a running story autopilot or
// story / side-mission play. No collision, no effect on waves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "WorkshopEasterEggs.generated.h"

class AIronManCharacter;
class UPointLightComponent;
class UTextRenderComponent;
class USceneComponent;

namespace WorkshopEggs
{
	/** Interact targets (career [E] prompt). */
	enum ETarget : int32 { TargetNone = -1, TargetPinboard = 0, TargetToolTray = 1, TargetCar = 2 };
	constexpr int32 NumDocks = 8;
	constexpr int32 NumNotes = 5;
	constexpr int32 NumFoam = 24;
	constexpr int32 NumTarps = 3;
}

/** One bot-bay charging dock (runtime state). */
struct FWorkshopDock
{
	int32 BotPart = INDEX_NONE;
	int32 PadPart = INDEX_NONE;
	FLinearColor LitColor = FLinearColor::White;
	float LitGlow = 4.f;
	float LitTime = -1.f; // < 0 = dark
};

UCLASS()
class IRONMANARENA_API AWorkshopEasterEggs : public AActor
{
	GENERATED_BODY()

public:
	AWorkshopEasterEggs();

	virtual void Tick(float DeltaSeconds) override;

	/** True for -ArenaSmoke / -StoryAutoplay / automation / story autopilot runs: the nook never exists there. */
	static bool IsSuppressedRun();

	/** Free play: spawns the nook on a clear floor spot near Player (once per world). Returns the existing one if any. */
	static AWorkshopEasterEggs* SpawnForFreePlay(UWorld* InWorld, AIronManCharacter* Player);

	/** Story / side missions: removes any nook from the world. */
	static void RemoveAll(UWorld* InWorld);

	/** The nook in WorldContext's world (null if none). */
	static AWorkshopEasterEggs* Find(const UObject* WorldContext);

	/** 0..1 helmet foam frost for the career HUD (0 when there is no nook). */
	static float GetFoamFrost(const UObject* WorldContext);

	/** Interact prompt for the career HUD; OutTarget is passed back to Interact. */
	bool GetInteractPrompt(const AIronManCharacter* Char, FText& OutPrompt, int32& OutTarget) const;
	void Interact(AIronManCharacter* Char, int32 Target);

	// Read-only state (debug / tests).
	bool HasSprayed() const { return bFoamSprayed; }
	int32 GetNotesReadMask() const { return NotesReadMask; }
	int32 GetCarStage() const { return CarStage; }
	bool IsDuckGiven() const { return bDuckGiven; }

protected:
	virtual void BeginPlay() override;

	void TickBotBay(float DeltaSeconds, const FVector& PawnLoc, const FVector& ViewLoc, const FVector& ViewDir);
	void TickExtinguisher(float DeltaSeconds, const FVector& PawnLoc, const FVector& ViewLoc, const FVector& ViewDir);
	void TickFoam(float DeltaSeconds);
	void TickToolTray(float DeltaSeconds, const AIronManCharacter* Char);
	void TickCar(float DeltaSeconds);
	void SetDockLit(int32 DockIndex, bool bLit);
	void ApplyDockLook(int32 DockIndex, float Brightness, const FLinearColor& Col);
	void StartSpray(const FVector& TargetLoc);
	void Rattle();
	void ReadNextNote();
	FVector LocalToWorld(const FVector& Local) const { return GetActorTransform().TransformPosition(Local); }
	static bool LookingAt(const FVector& ViewLoc, const FVector& ViewDir, const FVector& Target, float MaxDist, float MinDot);

	UPROPERTY(VisibleAnywhere, Category = "Workshop")
	TObjectPtr<USceneComponent> NookRoot;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	// Bot bay
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> DockLights;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> DockPlates;
	UPROPERTY() TObjectPtr<UTextRenderComponent> JarvisGreeting;
	UPROPERTY() TObjectPtr<USceneComponent> DumEArmPivot;
	TArray<FWorkshopDock> Docks;
	float BayClock = 0.f;
	float BayStepTimer = 0.f;
	int32 BayNextDock = 0;
	bool bBayActive = false;
	float UltronLook = 0.f;
	float ChorusTimer = 0.f;
	bool bChorusArmed = true;

	// Extinguisher
	UPROPERTY() TObjectPtr<USceneComponent> ExtPivot;
	TArray<int32> FoamParts;
	TArray<FVector> FoamVel;
	FVector SprayTarget = FVector::ZeroVector;
	float ExtLook = 0.f;
	float SprayTimer = -1.f;
	float FoamAge = -1.f;
	bool bFoamEmitted = false;
	float FoamFrost = 0.f;
	float RattleTimer = 0.f;
	float RattleCooldown = 0.f;
	bool bFoamSprayed = false;

	// Pinboard
	int32 NotesReadMask = 0;
	int32 NextNote = 0;
	bool bHistorianAwarded = false;

	// Tool tray / duck
	UPROPERTY() TObjectPtr<USceneComponent> DuckPivot;
	float IdleNearTray = 0.f;
	float DuckSlide = -1.f;
	bool bDuckOut = false;
	bool bDuckGiven = false;

	// Covered car
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> TarpPivots;
	UPROPERTY() TObjectPtr<USceneComponent> ScrapPivot;
	UPROPERTY() TObjectPtr<UPointLightComponent> ScrapLight;
	int32 CarStage = 0;
	float TarpAnim = -1.f;
	int32 TarpAnimIndex = INDEX_NONE;
};
