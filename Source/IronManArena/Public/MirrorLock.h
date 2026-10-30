// AMirrorLock: the "Mirror Lock" puzzle from the Jocasta side mission (sm_jocasta_second_signal,
// contrib/jocasta/vision_quest_side_mission.md). A ring of six glass pillars around Ultron's relay
// core; each shows a phrase in Jocasta's current voice (pale blue) or her Ultron-era voice (red).
// Stepping onto a pillar's floor plate ON FOOT rotates it, which also flips its two neighbours
// (lights-out on a ring of six). The lock opens when all six show blue.
//  - Pressure: every PulseInterval s a red pulse from the core rotates one pillar back, unless the
//    player stands at Jocasta's terminal (she blocks that cycle's pulse). The pulse applies a full
//    rotation (pillar + neighbours) rather than a single flip: on a 6-ring the move set has rank 4,
//    so a single flip could make the lock unsolvable; this keeps every state solvable.
//  - Hint: after HintDelay s without progress Jocasta names the pillar that "sounds least like me",
//    always the first move of a shortest solution.
//  - No fail state. AStoryDirector drives it from a "ground" objective whose target is "mirror lock"
//    and completes the objective when IsSolved().
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVisuals.h"
#include "MirrorLock.generated.h"

class UPointLightComponent;
class UTextRenderComponent;
class USoundBase;

/** Pure puzzle logic (no world), unit-tested by IronManArena.Story.MirrorLock. Bit i set = pillar i blue. */
struct IRONMANARENA_API FMirrorLockPuzzle
{
	static constexpr int32 NumPillars = 6;
	static constexpr uint8 SolvedMask = 0x3F;

	uint8 Blue = SolvedMask;

	static uint8 MoveMask(int32 Pillar);
	void Rotate(int32 Pillar) { Blue ^= MoveMask(Pillar); }
	bool IsSolved() const { return Blue == SolvedMask; }
	bool IsBlue(int32 Pillar) const { return (Blue >> Pillar) & 1; }
	int32 NumBlue() const;

	/** Fewest rotations that solve State (-1 if unsolvable); OutMoves = bitmask of the pillars to rotate. */
	static int32 MinMoves(uint8 State, uint8* OutMoves = nullptr);
	/** First pillar of a shortest solution, INDEX_NONE when solved / unsolvable. */
	int32 HintPillar() const;
	/** Every unsolved state reachable from the solved lock with a shortest solution of <= MaxMoves. */
	static TArray<uint8> ShippedStarts(int32 MaxMoves = 4);
};

UCLASS()
class IRONMANARENA_API AMirrorLock : public AActor
{
	GENERATED_BODY()

public:
	AMirrorLock();

	virtual void Tick(float DeltaSeconds) override;

	/** Picks one of the shipped starting states (Seed < 0: random). */
	void Arm(int32 Seed = -1);

	/** Rotates a pillar (and its neighbours). Returns false once solved. */
	bool PressPillar(int32 Pillar);

	bool IsSolved() const { return Puzzle.IsSolved(); }
	float GetProgress() const { return float(Puzzle.NumBlue()) / FMirrorLockPuzzle::NumPillars; }
	int32 GetHintPillar() const { return Puzzle.HintPillar(); }
	int32 GetMovesLeft() const { return FMirrorLockPuzzle::MinMoves(Puzzle.Blue); }
	uint8 GetState() const { return Puzzle.Blue; }
	FVector GetPlateLocation(int32 Pillar) const;
	FVector GetTerminalLocation() const;

	int32 GetPresses() const { return Presses; }
	int32 GetPulsesApplied() const { return PulsesApplied; }
	int32 GetPulsesBlocked() const { return PulsesBlocked; }
	int32 GetHintsGiven() const { return HintsGiven; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MirrorLock")
	float PulseInterval = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MirrorLock")
	float HintDelay = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MirrorLock")
	float RingRadius = 430.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MirrorLock")
	float PlateRadius = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MirrorLock")
	float TerminalRadius = 260.f;

	/** False: no red pulses (the autopilot / tests can turn the pressure off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MirrorLock")
	bool bPressure = true;

	static const FLinearColor JocastaBlue;
	static const FLinearColor UltronRed;

protected:
	virtual void BeginPlay() override;

	void RefreshPillars();
	void ApplyPulse();
	void Say(const TCHAR* LineId, const FText& Text, bool bInterrupt);
	float PlateAngle(int32 Pillar) const;

	UPROPERTY(VisibleAnywhere, Category = "MirrorLock")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "MirrorLock")
	TObjectPtr<UPointLightComponent> CoreLight;

	UPROPERTY(VisibleAnywhere, Category = "MirrorLock")
	TArray<TObjectPtr<UTextRenderComponent>> Faces;

	UPROPERTY()
	TArray<FArenaPaintedPart> Parts;

	/** Part indices into Parts for the six pillar glass bodies and plates. */
	TArray<int32> PillarParts;
	TArray<int32> PlateParts;
	int32 CorePart = INDEX_NONE;

	FMirrorLockPuzzle Puzzle;
	int32 BestMovesLeft = 99;
	int32 PlateUnderPlayer = INDEX_NONE;
	float PulseTimer = 0.f;
	float HintTimer = 0.f;
	float Time = 0.f;
	float FlashTimer = 0.f;
	int32 Presses = 0;
	int32 PulsesApplied = 0;
	int32 PulsesBlocked = 0;
	int32 HintsGiven = 0;
	bool bAnnouncedSolved = false;
};
