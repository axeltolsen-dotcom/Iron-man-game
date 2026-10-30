#include "MirrorLock.h"

#include "IronManArena.h"
#include "IronManCharacter.h"
#include "JarvisComponent.h"
#include "StoryLibrary.h"
#include "ArenaBlastFX.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "MirrorLock"

using namespace ArenaVisuals;

const FLinearColor AMirrorLock::JocastaBlue(0.55f, 0.82f, 1.f);
const FLinearColor AMirrorLock::UltronRed(1.f, 0.12f, 0.08f);

namespace MirrorLockText
{
	// Original phrases: what Jocasta believes now (blue) vs. what Ultron built her to say (red).
	static const TCHAR* BlueFace[FMirrorLockPuzzle::NumPillars] = {
		TEXT("I CHOOSE"), TEXT("I LISTEN"), TEXT("I STAY"), TEXT("I KEEP"), TEXT("I WONDER"), TEXT("I LEAVE") };
	static const TCHAR* RedFace[FMirrorLockPuzzle::NumPillars] = {
		TEXT("I OBEY"), TEXT("I COMMAND"), TEXT("I RETURN"), TEXT("I CONSUME"), TEXT("I CALCULATE"), TEXT("I BELONG") };
	static const TCHAR* Ordinal[FMirrorLockPuzzle::NumPillars] = {
		TEXT("first"), TEXT("second"), TEXT("third"), TEXT("fourth"), TEXT("fifth"), TEXT("sixth") };
}

// =============================================================================== puzzle logic

uint8 FMirrorLockPuzzle::MoveMask(int32 Pillar)
{
	const int32 P = ((Pillar % NumPillars) + NumPillars) % NumPillars;
	const int32 L = (P + NumPillars - 1) % NumPillars;
	const int32 R = (P + 1) % NumPillars;
	return uint8((1u << P) | (1u << L) | (1u << R));
}

int32 FMirrorLockPuzzle::NumBlue() const
{
	int32 N = 0;
	for (int32 i = 0; i < NumPillars; ++i) { N += IsBlue(i) ? 1 : 0; }
	return N;
}

int32 FMirrorLockPuzzle::MinMoves(uint8 State, uint8* OutMoves)
{
	int32 Best = -1;
	uint8 BestSet = 0;
	for (uint32 Set = 0; Set < (1u << NumPillars); ++Set)
	{
		uint8 S = State & SolvedMask;
		int32 Count = 0;
		for (int32 i = 0; i < NumPillars; ++i)
		{
			if (Set & (1u << i)) { S ^= MoveMask(i); ++Count; }
		}
		if (S == SolvedMask && (Best < 0 || Count < Best)) { Best = Count; BestSet = uint8(Set); }
	}
	if (OutMoves) { *OutMoves = BestSet; }
	return Best;
}

int32 FMirrorLockPuzzle::HintPillar() const
{
	uint8 Moves = 0;
	if (MinMoves(Blue, &Moves) <= 0) { return INDEX_NONE; }
	// Prefer a red pillar among the solution moves (it "sounds least like me"), else the first move.
	for (int32 i = 0; i < NumPillars; ++i) { if ((Moves & (1u << i)) && !IsBlue(i)) { return i; } }
	for (int32 i = 0; i < NumPillars; ++i) { if (Moves & (1u << i)) { return i; } }
	return INDEX_NONE;
}

TArray<uint8> FMirrorLockPuzzle::ShippedStarts(int32 MaxMoves)
{
	TArray<uint8> Out;
	for (uint32 S = 0; S < (1u << NumPillars); ++S)
	{
		const int32 M = MinMoves(uint8(S));
		if (M > 0 && M <= MaxMoves) { Out.Add(uint8(S)); }
	}
	return Out;
}

// =============================================================================== actor

AMirrorLock::AMirrorLock()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Relay floor disc + the core in the middle.
	AddPart(this, Root, TEXT("Floor"), EArenaShape::Cylinder, FVector(0, 0, 2), FVector(11.f, 11.f, 0.04f), FRotator::ZeroRotator,
		FLinearColor(0.05f, 0.055f, 0.06f), 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, Root, TEXT("CorePlinth"), EArenaShape::Cylinder, FVector(0, 0, 50), FVector(1.1f, 1.1f, 1.f), FRotator::ZeroRotator,
		FLinearColor(0.08f, 0.08f, 0.09f), 0.f, Parts, EArenaSurface::RobotMetal);
	CorePart = Parts.Num();
	AddPart(this, Root, TEXT("Core"), EArenaShape::Sphere, FVector(0, 0, 150), FVector(0.8f), FRotator::ZeroRotator,
		UltronRed, 6.f, Parts, EArenaSurface::Glow);

	for (int32 i = 0; i < FMirrorLockPuzzle::NumPillars; ++i)
	{
		const float A = 2.f * PI * i / FMirrorLockPuzzle::NumPillars;
		const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
		const FVector Base = Dir * 430.f;
		const float Yaw = FMath::RadiansToDegrees(A);
		PillarParts.Add(Parts.Num());
		AddPart(this, Root, FName(*FString::Printf(TEXT("Pillar%d"), i)), EArenaShape::Cylinder, Base + FVector(0, 0, 160),
			FVector(0.75f, 0.75f, 3.2f), FRotator::ZeroRotator, UltronRed, 1.5f, Parts, EArenaSurface::Glass);
		AddPart(this, Root, FName(*FString::Printf(TEXT("PillarCap%d"), i)), EArenaShape::Cylinder, Base + FVector(0, 0, 325),
			FVector(0.85f, 0.85f, 0.1f), FRotator::ZeroRotator, FLinearColor(0.1f, 0.1f, 0.11f), 0.f, Parts, EArenaSurface::RobotMetal);
		PlateParts.Add(Parts.Num());
		AddPart(this, Root, FName(*FString::Printf(TEXT("Plate%d"), i)), EArenaShape::Cylinder, Dir * 300.f + FVector(0, 0, 5),
			FVector(1.7f, 1.7f, 0.05f), FRotator::ZeroRotator, UltronRed, 2.f, Parts, EArenaSurface::Glow);

		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(FName(*FString::Printf(TEXT("Face%d"), i)));
		T->SetupAttachment(Root);
		// The text faces the ring centre, where the player stands.
		T->SetRelativeLocation(Dir * 390.f + FVector(0, 0, 190));
		T->SetRelativeRotation(FRotator(0.f, Yaw + 180.f, 0.f));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(34.f);
		T->SetText(FText::AsCultureInvariant(MirrorLockText::RedFace[i]));
		T->SetTextRenderColor(UltronRed.ToFColor(true));
		T->SetCastShadow(false);
		Faces.Add(T);
	}

	// Jocasta's terminal, outside the ring between pillars 0 and 1.
	const float TA = PI / FMirrorLockPuzzle::NumPillars;
	const FVector TDir(FMath::Cos(TA), FMath::Sin(TA), 0.f);
	AddPart(this, Root, TEXT("Terminal"), EArenaShape::Cube, TDir * 680.f + FVector(0, 0, 60), FVector(0.6f, 1.2f, 1.2f),
		FRotator(0.f, FMath::RadiansToDegrees(TA), 0.f), FLinearColor(0.1f, 0.1f, 0.12f), 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, Root, TEXT("TerminalScreen"), EArenaShape::Cube, TDir * 648.f + FVector(0, 0, 95), FVector(0.05f, 1.f, 0.6f),
		FRotator(0.f, FMath::RadiansToDegrees(TA), 0.f), JocastaBlue, 5.f, Parts, EArenaSurface::Glow);
	AddPart(this, Root, TEXT("TerminalPad"), EArenaShape::Cylinder, TDir * 600.f + FVector(0, 0, 4), FVector(2.4f, 2.4f, 0.04f),
		FRotator::ZeroRotator, JocastaBlue, 1.f, Parts, EArenaSurface::Glow);

	for (FArenaPaintedPart& P : Parts)
	{
		if (P.Component)
		{
			P.Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P.Component->SetCastShadow(false);
		}
	}

	CoreLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreLight"));
	CoreLight->SetupAttachment(Root);
	CoreLight->SetRelativeLocation(FVector(0, 0, 200));
	CoreLight->SetIntensity(9000.f);
	CoreLight->SetAttenuationRadius(1400.f);
	CoreLight->SetLightColor(UltronRed);
	CoreLight->SetCastShadows(false);
}

void AMirrorLock::BeginPlay()
{
	Super::BeginPlay();
	PaintAll(this, Parts);
	if (Puzzle.IsSolved()) { Arm(); }
	RefreshPillars();
}

void AMirrorLock::Arm(int32 Seed)
{
	const TArray<uint8> Starts = FMirrorLockPuzzle::ShippedStarts(4);
	if (Starts.Num() == 0) { return; }
	const int32 Pick = Seed >= 0 ? Seed % Starts.Num() : FMath::RandRange(0, Starts.Num() - 1);
	Puzzle.Blue = Starts[Pick];
	BestMovesLeft = FMirrorLockPuzzle::MinMoves(Puzzle.Blue);
	PulseTimer = PulseInterval;
	HintTimer = 0.f;
	bAnnouncedSolved = false;
	RefreshPillars();
}

float AMirrorLock::PlateAngle(int32 Pillar) const
{
	return 2.f * PI * Pillar / FMirrorLockPuzzle::NumPillars;
}

FVector AMirrorLock::GetPlateLocation(int32 Pillar) const
{
	const float A = PlateAngle(Pillar);
	return GetActorTransform().TransformPosition(FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 300.f);
}

FVector AMirrorLock::GetTerminalLocation() const
{
	const float A = PI / FMirrorLockPuzzle::NumPillars;
	return GetActorTransform().TransformPosition(FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 600.f);
}

void AMirrorLock::RefreshPillars()
{
	for (int32 i = 0; i < FMirrorLockPuzzle::NumPillars; ++i)
	{
		const bool bBlue = Puzzle.IsBlue(i);
		const FLinearColor C = bBlue ? JocastaBlue : UltronRed;
		if (PillarParts.IsValidIndex(i) && Parts.IsValidIndex(PillarParts[i]))
		{
			FArenaPaintedPart& P = Parts[PillarParts[i]];
			P.Color = C;
			P.Emissive = bBlue ? 2.5f : 1.5f;
			RepaintPart(P);
		}
		if (PlateParts.IsValidIndex(i) && Parts.IsValidIndex(PlateParts[i]))
		{
			FArenaPaintedPart& P = Parts[PlateParts[i]];
			P.Color = C;
			RepaintPart(P);
		}
		if (Faces.IsValidIndex(i) && Faces[i])
		{
			Faces[i]->SetText(FText::AsCultureInvariant(bBlue ? MirrorLockText::BlueFace[i] : MirrorLockText::RedFace[i]));
			Faces[i]->SetTextRenderColor(C.ToFColor(true));
		}
	}
	const FLinearColor CoreC = Puzzle.IsSolved() ? JocastaBlue : UltronRed;
	if (Parts.IsValidIndex(CorePart)) { Parts[CorePart].Color = CoreC; RepaintPart(Parts[CorePart]); }
	if (CoreLight) { CoreLight->SetLightColor(CoreC); }
}

bool AMirrorLock::PressPillar(int32 Pillar)
{
	if (Puzzle.IsSolved() || Pillar < 0 || Pillar >= FMirrorLockPuzzle::NumPillars) { return false; }
	Puzzle.Rotate(Pillar);
	++Presses;
	const int32 Left = FMirrorLockPuzzle::MinMoves(Puzzle.Blue);
	if (Left >= 0 && Left < BestMovesLeft) { BestMovesLeft = Left; HintTimer = 0.f; }
	FlashTimer = 0.25f;
	RefreshPillars();
	if (Puzzle.IsSolved() && !bAnnouncedSolved)
	{
		bAnnouncedSolved = true;
		AArenaBlastFX::Spawn(GetWorld(), GetActorLocation() + FVector(0, 0, 150), 500.f, JocastaBlue, 0.6f);
	}
	return true;
}

void AMirrorLock::ApplyPulse()
{
	// A rotation that does not happen to solve the lock (the core never helps).
	TArray<int32> Options;
	for (int32 i = 0; i < FMirrorLockPuzzle::NumPillars; ++i)
	{
		if ((Puzzle.Blue ^ FMirrorLockPuzzle::MoveMask(i)) != FMirrorLockPuzzle::SolvedMask) { Options.Add(i); }
	}
	if (Options.Num() == 0) { return; }
	Puzzle.Rotate(Options[FMath::RandRange(0, Options.Num() - 1)]);
	++PulsesApplied;
	BestMovesLeft = FMath::Max(BestMovesLeft, FMirrorLockPuzzle::MinMoves(Puzzle.Blue));
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation() + FVector(0, 0, 150), 700.f, UltronRed, 0.4f);
	RefreshPillars();
}

void AMirrorLock::Say(const TCHAR* LineId, const FText& Text, bool bInterrupt)
{
	UJarvisComponent* J = UJarvisComponent::Get(this);
	if (!J) { return; }
	const FString Package = FString::Printf(TEXT("/Game/Audio/JOCASTA/%s"), LineId);
	USoundBase* Sound = FPackageName::DoesPackageExist(Package)
		? LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, LineId)) : nullptr;
	const FName Speaker(TEXT("JOCASTA"));
	J->SayScripted(Speaker, UStoryLibrary::GetSpeakerLabel(Speaker, FString()), Text, UStoryLibrary::GetSpeakerColor(Speaker), Sound, 75, bInterrupt);
}

void AMirrorLock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (CoreLight)
	{
		const float Beat = 0.75f + 0.25f * FMath::Sin(Time * (Puzzle.IsSolved() ? 1.5f : 4.f));
		CoreLight->SetIntensity(9000.f * Beat * (FlashTimer > 0.f ? 1.6f : 1.f));
	}
	FlashTimer = FMath::Max(0.f, FlashTimer - DeltaSeconds);
	if (Puzzle.IsSolved()) { return; }

	const AIronManCharacter* Player = Cast<AIronManCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const bool bOnFoot = Player && !Player->IsFlying() && !Player->IsDead();
	const FVector PL = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	auto Near = [&PL](const FVector& At, float Radius)
	{
		const FVector D = PL - At;
		return FVector(D.X, D.Y, 0.f).Size() <= Radius && FMath::Abs(D.Z) < 300.f;
	};

	// Plates: rotate on entry, on foot only.
	int32 Under = INDEX_NONE;
	if (bOnFoot)
	{
		for (int32 i = 0; i < FMirrorLockPuzzle::NumPillars; ++i)
		{
			if (Near(GetPlateLocation(i), PlateRadius)) { Under = i; break; }
		}
	}
	if (Under != INDEX_NONE && Under != PlateUnderPlayer) { PressPillar(Under); }
	PlateUnderPlayer = Under;
	if (Puzzle.IsSolved()) { return; }

	// Red pulse; Jocasta blocks it while the player stands at her terminal.
	if (bPressure)
	{
		PulseTimer -= DeltaSeconds;
		if (PulseTimer <= 0.f)
		{
			PulseTimer = PulseInterval;
			if (bOnFoot && Near(GetTerminalLocation(), TerminalRadius))
			{
				++PulsesBlocked;
				Say(TEXT("JOCASTA_mirrorLockBlock_1"), LOCTEXT("Block", "Got it. That one doesn't reach you."), false);
			}
			else
			{
				ApplyPulse();
			}
		}
	}

	// Hint after HintDelay seconds without getting closer to the solution.
	HintTimer += DeltaSeconds;
	if (HintTimer >= HintDelay)
	{
		HintTimer = 0.f;
		const int32 H = Puzzle.HintPillar();
		if (H != INDEX_NONE)
		{
			++HintsGiven;
			Say(*FString::Printf(TEXT("JOCASTA_mirrorLockHint_%d"), H + 1),
				FText::Format(LOCTEXT("Hint", "The {0} pillar. It sounds least like me."), FText::AsCultureInvariant(MirrorLockText::Ordinal[H])), false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
