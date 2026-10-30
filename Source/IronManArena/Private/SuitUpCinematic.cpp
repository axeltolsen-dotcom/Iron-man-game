#include "SuitUpCinematic.h"

#include "IronManArena.h"
#include "IronManCharacter.h"
#include "ArenaVisuals.h"
#include "ArenaBlastFX.h"
#include "CinematicOverlayWidget.h"
#include "JarvisComponent.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/UserWidget.h"

#define LOCTEXT_NAMESPACE "SuitUpCinematic"

FOnSuitUpFinished ASuitUpCinematic::OnFinished;

namespace
{
	/** Normalized time at which the real armour replaces the flying copies. */
	constexpr float BodyRevealAt = 0.78f;

	float EaseOutCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); const float U = 1.f - T; return 1.f - U * U * U; }
	float EaseInOut(float T) { return FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f), 2.f); }
}

ASuitUpCinematic::ASuitUpCinematic()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the player moved this frame
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

ASuitUpCinematic* ASuitUpCinematic::Play(AIronManCharacter* InPlayer, int32 InSuitIndex, bool bShortVersion)
{
	UWorld* World = InPlayer ? InPlayer->GetWorld() : nullptr;
	if (!World || !Cast<APlayerController>(InPlayer->GetController())) { return nullptr; }
	if (InPlayer->IsInCinematic()) { return nullptr; }

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASuitUpCinematic* C = World->SpawnActor<ASuitUpCinematic>(ASuitUpCinematic::StaticClass(), InPlayer->GetActorTransform(), Params);
	if (C) { C->Begin(InPlayer, InSuitIndex, bShortVersion); }
	return C;
}

float ASuitUpCinematic::StyleDuration() const
{
	if (bShort) { return 2.f; }
	switch (Style)
	{
	case ECinematicStyle::Assembly: return 5.5f;
	case ECinematicStyle::Gantry:   return 5.f;
	case ECinematicStyle::Drop:     return 4.5f;
	case ECinematicStyle::Nano:     return 5.f;
	case ECinematicStyle::Stealth:  return 5.5f;
	case ECinematicStyle::Suitcase: return 5.f;
	case ECinematicStyle::Ultron:   return 6.5f;
	default:                        return 5.f;
	}
}

void ASuitUpCinematic::Begin(AIronManCharacter* InPlayer, int32 InSuitIndex, bool bInShort)
{
	Player = InPlayer;
	SuitIndex = InSuitIndex;
	bShort = bInShort;
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	Style = Suit.CinematicStyle;
	Duration = StyleDuration();

	UWorld* World = GetWorld();
	APlayerController* PC = Cast<APlayerController>(InPlayer->GetController());

	// New suit on the body first (colours / variants), then copy what is visible.
	InPlayer->ApplySuit(SuitIndex);
	InPlayer->SetCinematicMode(true, this);

	const FRotator Yaw(0.f, InPlayer->GetActorRotation().Yaw, 0.f);
	BaseFwd = Yaw.Vector();
	BaseRight = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	SizeScale = InPlayer->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() / 96.f;
	CasePos = InPlayer->GetActorLocation() + BaseFwd * 110.f * SizeScale - FVector(0.f, 0.f, 70.f * SizeScale);

	SpawnFlyers();
	InPlayer->SetBodyHiddenForCinematic(true);

	// Camera.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CineCam = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), InPlayer->GetActorLocation() + BaseFwd * 400.f, FRotator::ZeroRotator, Params);
	if (CineCam)
	{
		UCineCameraComponent* Cam = CineCam->GetCineCameraComponent();
		Cam->FocusSettings.FocusMethod = ECameraFocusMethod::Disable; // no DOF blur on the placeholder shapes
		Cam->SetConstraintAspectRatio(false);
		Cam->SetCurrentFocalLength(24.f);
		Cam->PostProcessBlendWeight = 1.f;
		FPostProcessSettings& PP = Cam->PostProcessSettings;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.55f;
		if (Style == ECinematicStyle::Stealth)
		{
			PP.bOverride_AutoExposureBias = true;
			PP.AutoExposureBias = -1.5f;
			PP.bOverride_ColorGain = true;
			PP.ColorGain = FVector4(0.75f, 0.85f, 1.f, 1.f);
		}
		else if (Style == ECinematicStyle::Ultron)
		{
			PP.bOverride_ColorGain = true;
			PP.ColorGain = FVector4(1.25f, 0.62f, 0.6f, 1.f);
			PP.bOverride_FilmGrainIntensity = true;
			PP.FilmGrainIntensity = 0.6f;
			PP.bOverride_SceneFringeIntensity = true;
			PP.SceneFringeIntensity = 1.5f;
		}
		UpdateCamera(0.f, 0.f);
		PC->SetViewTargetWithBlend(CineCam, bShort ? 0.25f : 0.f, VTBlend_EaseInOut, 2.f);
	}

	// Overlay.
	Overlay = CreateWidget<UCinematicOverlayWidget>(PC, UCinematicOverlayWidget::StaticClass());
	if (Overlay)
	{
		Overlay->AddToViewport(50);
		const FAIPersonaDef& AI = USuitLibrary::GetPersona(Suit.AI);
		const FText Sub = Suit.Mark.IsEmpty() ? Suit.Description : Suit.Mark;
		Overlay->SetInfo(bShort ? FText::Format(LOCTEXT("SwapTitle", "{0} ONLINE"), Suit.Name) : Suit.Name,
			Sub, FText::Format(LOCTEXT("AiLabel", "ON-BOARD AI: {0}"), AI.Label), AI.SubtitleColor, bShort);
	}

	// Voice: the new suit's AI introduces it (full) or takes over (swap).
	if (UJarvisComponent* J = UJarvisComponent::Get(this))
	{
		if (bShort)
		{
			J->SetPersona(Suit.AI, true);
		}
		else
		{
			J->StopAll();
			J->SetPersona(Suit.AI, false);
			J->Say(EAIEvent::SuitIntro, Suit.Id, true);
		}
	}

	// Opening burst at the reactor.
	AArenaBlastFX::Spawn(World, InPlayer->GetActorLocation() + FVector(0.f, 0.f, 40.f * SizeScale),
		bShort ? 120.f : 200.f, Suit.EmissiveColor, 0.35f);
	NextBurstAt = 0.2f;
	UE_LOG(LogIronManArena, Log, TEXT("Suit-up cinematic: %s (%s, %.1fs)"), *Suit.Id.ToString(), bShort ? TEXT("short") : TEXT("full"), Duration);
}

void ASuitUpCinematic::SpawnFlyers()
{
	AIronManCharacter* P = Player.Get();
	USceneComponent* Root = P ? P->GetSuitRoot() : nullptr;
	if (!Root) { return; }

	const FTransform RootXf = Root->GetComponentTransform();
	const FVector Center = P->GetActorLocation();
	const TArray<FArenaPaintedPart>& Parts = P->GetSuitParts();

	// Collect visible armour (fixed-colour parts are flames etc.).
	TArray<const FArenaPaintedPart*> Candidates;
	for (const FArenaPaintedPart& Part : Parts)
	{
		if (Part.Component && Part.Role != ESuitPaintRole::Fixed && Part.Component->IsVisible() && !Part.Component->bHiddenInGame && Part.Component->GetStaticMesh())
		{
			Candidates.Add(&Part);
		}
	}
	const int32 MaxCopies = bShort ? 28 : 64;
	const int32 Stride = FMath::Max(1, FMath::DivideAndRoundUp(Candidates.Num(), MaxCopies));

	// Parts land bottom-up (boots first, helmet last), like the films' assembly order.
	TArray<const FArenaPaintedPart*> Picked;
	for (int32 i = 0; i < Candidates.Num(); i += Stride) { Picked.Add(Candidates[i]); }
	Picked.Sort([](const FArenaPaintedPart& A, const FArenaPaintedPart& B)
	{
		return A.Component->GetComponentLocation().Z < B.Component->GetComponentLocation().Z;
	});

	const float PhaseStart = bShort ? 0.05f : 0.12f;
	const float PhaseEnd = bShort ? 0.62f : 0.7f;
	const float FlyTime = bShort ? 0.18f : 0.16f;
	const int32 Num = Picked.Num();

	if (Style == ECinematicStyle::Suitcase && !bShort)
	{
		CaseProp = NewObject<UStaticMeshComponent>(this, TEXT("SuitcaseProp"));
		CaseProp->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cube));
		CaseProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CaseProp->SetWorldLocationAndRotation(CasePos, BaseFwd.Rotation());
		CaseProp->SetWorldScale3D(FVector(0.25f, 0.6f, 0.45f) * SizeScale);
		CaseProp->RegisterComponent();
		ArenaVisuals::PaintComponent(this, CaseProp, FLinearColor(0.12f, 0.12f, 0.13f), 0.f);
	}

	for (int32 i = 0; i < Num; ++i)
	{
		const UStaticMeshComponent* Src = Picked[i]->Component;
		const FTransform Target = Src->GetComponentTransform();

		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(this);
		Copy->SetStaticMesh(Src->GetStaticMesh());
		Copy->SetMaterial(0, Src->GetMaterial(0)); // shares the part's MID (same colour)
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetMobility(EComponentMobility::Movable);
		Copy->SetVisibility(false);
		Copy->RegisterComponent();
		FlyingParts.Add(Copy);

		FFlyer F;
		F.Copy = Copy;
		F.RelToRoot = Target.GetRelativeTransform(RootXf);
		F.Delay = FMath::Lerp(PhaseStart, PhaseEnd - FlyTime, Num > 1 ? float(i) / float(Num - 1) : 0.f) + FMath::FRandRange(-0.02f, 0.02f);
		F.FlyTime = FlyTime * FMath::FRandRange(0.85f, 1.2f);
		F.StartRot = FQuat(FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f)));

		const FVector Rand2D = (BaseRight * FMath::FRandRange(-1.f, 1.f) + BaseFwd * FMath::FRandRange(-1.f, 1.f));
		const float S = SizeScale;
		switch (Style)
		{
		case ECinematicStyle::Gantry:   F.StartLoc = Target.GetLocation() - FVector(0.f, 0.f, 420.f * S) + Rand2D * 60.f; break;
		case ECinematicStyle::Drop:     F.StartLoc = Target.GetLocation() + FVector(0.f, 0.f, 1200.f * S) + Rand2D * 90.f; break;
		case ECinematicStyle::Nano:     F.StartLoc = Center + FVector(0.f, 0.f, 40.f * S); F.StartScale = 0.02f; F.StartRot = Target.GetRotation(); break;
		case ECinematicStyle::Stealth:  F.StartLoc = Target.GetLocation() + Rand2D.GetSafeNormal() * 220.f * S; break;
		case ECinematicStyle::Suitcase: F.StartLoc = CasePos + Rand2D * 20.f; F.StartScale = 0.3f; break;
		case ECinematicStyle::Ultron:   F.StartLoc = Target.GetLocation() + BaseRight * (FMath::RandBool() ? 650.f : -650.f) * S + FVector(0.f, 0.f, FMath::FRandRange(-100.f, 300.f)); break;
		case ECinematicStyle::Assembly:
		default:                        F.StartLoc = Target.GetLocation() + FMath::VRand() * 520.f * S + FVector(0.f, 0.f, 120.f * S); break;
		}
		if (bShort && Style != ECinematicStyle::Nano)
		{
			// Swaps are quick: parts come from close by.
			F.StartLoc = FMath::Lerp(Target.GetLocation(), F.StartLoc, 0.45f);
		}
		Flyers.Add(F);
	}
}

void ASuitUpCinematic::TickFlyers(float Alpha)
{
	AIronManCharacter* P = Player.Get();
	USceneComponent* Root = P ? P->GetSuitRoot() : nullptr;
	if (!Root) { return; }
	const FTransform RootXf = Root->GetComponentTransform();
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	int32 Landed = 0;

	for (FFlyer& F : Flyers)
	{
		UStaticMeshComponent* Copy = F.Copy.Get();
		if (!Copy) { continue; }
		const float T = (Alpha - F.Delay) / FMath::Max(0.01f, F.FlyTime);
		if (T <= 0.f) { Copy->SetVisibility(false); continue; }

		const FTransform Target = F.RelToRoot * RootXf;
		const float E = EaseOutCubic(T);
		FVector Loc = FMath::Lerp(F.StartLoc, Target.GetLocation(), E);
		if (Style == ECinematicStyle::Nano)
		{
			// Nano particles spiral outward from the reactor.
			const float Ang = (1.f - E) * 540.f;
			const FVector Off = Loc - Target.GetLocation();
			Loc = Target.GetLocation() + Off.RotateAngleAxis(Ang, FVector::UpVector);
		}
		if (Style == ECinematicStyle::Ultron && T < 1.f && FMath::FRand() < 0.15f)
		{
			Loc += FMath::VRand() * 25.f; // glitch teleport jitter
		}
		const FQuat Rot = FQuat::Slerp(F.StartRot, Target.GetRotation(), E);
		const float ScaleMul = FMath::Lerp(F.StartScale, 1.f, E);
		Copy->SetWorldTransform(FTransform(Rot, Loc, Target.GetScale3D() * ScaleMul));

		bool bShow = true;
		if (Style == ECinematicStyle::Stealth && T < 1.f) { bShow = FMath::FRand() > 0.35f; } // flicker in
		Copy->SetVisibility(bShow);

		if (T >= 1.f)
		{
			++Landed;
			if (!F.bLanded)
			{
				F.bLanded = true;
				// Small spark on every few landings.
				if ((Landed % 5) == 0 && Style != ECinematicStyle::Stealth)
				{
					AArenaBlastFX::Spawn(GetWorld(), Target.GetLocation(), 35.f * SizeScale, Suit.EmissiveColor, 0.12f);
				}
			}
		}
	}
}

void ASuitUpCinematic::UpdateCamera(float Alpha, float DeltaSeconds)
{
	AIronManCharacter* P = Player.Get();
	if (!CineCam || !P) { return; }
	const FVector Focus = P->GetActorLocation();
	const float S = SizeScale;
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	const float E = EaseInOut(A);

	float AngleDeg = 0.f; // 0 = in front of the suit, + = to its right
	float Dist = 320.f;
	float Height = 20.f;
	float LookHeight = 25.f;
	float Focal = 30.f;
	float Roll = 0.f;
	float Track = 0.f;    // along the suit's forward axis (suitcase tracking shot)

	switch (Style)
	{
	case ECinematicStyle::Assembly:
		AngleDeg = FMath::Lerp(-120.f, 40.f, E); Dist = FMath::Lerp(380.f, 300.f, E); Height = FMath::Lerp(60.f, 25.f, E); Focal = FMath::Lerp(24.f, 32.f, E);
		break;
	case ECinematicStyle::Gantry:
		AngleDeg = 25.f; Dist = FMath::Lerp(420.f, 300.f, E); Height = FMath::Lerp(-70.f, 50.f, E); LookHeight = FMath::Lerp(-10.f, 45.f, E); Focal = 28.f;
		break;
	case ECinematicStyle::Drop:
		AngleDeg = FMath::Lerp(160.f, 0.f, E); Dist = FMath::Lerp(520.f, 320.f, E); Height = FMath::Lerp(650.f, 30.f, EaseOutCubic(A)); Focal = FMath::Lerp(20.f, 30.f, E);
		break;
	case ECinematicStyle::Nano:
		AngleDeg = FMath::Lerp(-30.f, 400.f, E); Dist = FMath::Lerp(240.f, 330.f, E); Height = FMath::Lerp(70.f, 25.f, E); Roll = FMath::Sin(A * PI) * 8.f; Focal = 30.f;
		break;
	case ECinematicStyle::Stealth:
		AngleDeg = 12.f; Dist = FMath::Lerp(720.f, 260.f, E); Height = 10.f; Focal = FMath::Lerp(45.f, 35.f, E);
		break;
	case ECinematicStyle::Suitcase:
		if (A < 0.72f)
		{
			const float T = A / 0.72f;
			AngleDeg = 90.f; Dist = 360.f; Height = 10.f; Track = FMath::Lerp(-160.f, 90.f, T); Focal = 30.f;
		}
		else
		{
			const float T = EaseOutCubic((A - 0.72f) / 0.12f); // whip pan to the front
			AngleDeg = FMath::Lerp(90.f, 0.f, T); Dist = 360.f - 50.f * T; Height = 10.f + 15.f * T; Track = FMath::Lerp(90.f, 0.f, T); Focal = 30.f;
		}
		break;
	case ECinematicStyle::Ultron:
		AngleDeg = FMath::Lerp(-8.f, 8.f, E); Dist = FMath::Lerp(460.f, 270.f, E); Height = FMath::Lerp(-20.f, -45.f, E); LookHeight = 45.f; Focal = FMath::Lerp(35.f, 28.f, E);
		break;
	default:
		break;
	}
	if (bShort)
	{
		// Compressed version: closer and calmer.
		Dist *= 0.8f;
		Height = FMath::Clamp(Height, -40.f, 120.f);
	}

	const float Rad = FMath::DegreesToRadians(AngleDeg);
	const FVector Dir = BaseFwd * FMath::Cos(Rad) + BaseRight * FMath::Sin(Rad);
	FVector CamLoc = Focus + (Dir * Dist + BaseFwd * Track + FVector(0.f, 0.f, Height)) * S;
	const FVector LookAt = Focus + FVector(0.f, 0.f, LookHeight * S);

	UCineCameraComponent* Cam = CineCam->GetCineCameraComponent();
	if (Style == ECinematicStyle::Ultron)
	{
		const bool bGlitch = FMath::FRand() < 0.18f;
		if (bGlitch) { CamLoc += FMath::VRand() * 12.f; }
		Cam->PostProcessSettings.SceneFringeIntensity = bGlitch ? FMath::FRandRange(3.f, 5.f) : 1.5f;
		Cam->PostProcessSettings.FilmGrainIntensity = bGlitch ? 1.f : 0.6f;
	}
	else if (Style == ECinematicStyle::Stealth)
	{
		Cam->PostProcessSettings.AutoExposureBias = FMath::Lerp(-2.f, -0.5f, E) + (FMath::FRand() < 0.08f ? -1.f : 0.f);
	}

	FRotator Rot = (LookAt - CamLoc).Rotation();
	Rot.Roll = Roll;
	CineCam->SetActorLocationAndRotation(CamLoc, Rot);
	Cam->SetCurrentFocalLength(Focal);
}

void ASuitUpCinematic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) { return; }
	if (!Player.IsValid()) { Finish(true); return; }

	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(0.1f, Duration), 0.f, 1.f);

	TickFlyers(Alpha);
	UpdateCamera(Alpha, DeltaSeconds);

	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	if (CaseProp && Alpha > 0.65f) { CaseProp->SetVisibility(false); }

	// Mid-sequence sparks around the body.
	if (!bShort && Alpha >= NextBurstAt && Alpha < BodyRevealAt)
	{
		NextBurstAt += 0.17f;
		const FVector At = Player->GetActorLocation() + FMath::VRand() * 70.f * SizeScale;
		AArenaBlastFX::Spawn(GetWorld(), At, 60.f * SizeScale, Style == ECinematicStyle::Ultron ? FLinearColor(1.f, 0.1f, 0.05f) : Suit.EmissiveColor, 0.2f);
	}

	// Real armour replaces the copies; reactor flares.
	if (!bBodyShown && Alpha >= BodyRevealAt)
	{
		bBodyShown = true;
		DestroyFlyers();
		Player->SetBodyHiddenForCinematic(false);
		AArenaBlastFX::Spawn(GetWorld(), Player->GetActorLocation() + FVector(0.f, 0.f, 40.f * SizeScale),
			(bShort ? 160.f : 260.f) * SizeScale, Suit.EmissiveColor, 0.4f);
	}

	if (Overlay)
	{
		const float BarIn = FMath::Clamp(Elapsed / 0.4f, 0.f, 1.f);
		const float BarOut = FMath::Clamp((Duration - Elapsed) / 0.35f, 0.f, 1.f);
		const float TitleIn = FMath::Clamp((Alpha - (bShort ? 0.35f : 0.55f)) / 0.12f, 0.f, 1.f);
		const float TitleOut = FMath::Clamp((Duration - Elapsed) / 0.3f, 0.f, 1.f);
		Overlay->SetState(FMath::Min(BarIn, BarOut), FMath::Min(TitleIn, TitleOut), Style == ECinematicStyle::Ultron ? 1.f : 0.f);
	}

	if (Elapsed >= Duration) { Finish(false); }
}

void ASuitUpCinematic::Skip()
{
	if (!bFinished && Elapsed > 0.15f) { Finish(true); } // ignore the click that started the game
}

void ASuitUpCinematic::DestroyFlyers()
{
	for (UStaticMeshComponent* C : FlyingParts)
	{
		if (C) { C->DestroyComponent(); }
	}
	FlyingParts.Empty();
	Flyers.Empty();
	if (CaseProp) { CaseProp->DestroyComponent(); CaseProp = nullptr; }
}

void ASuitUpCinematic::Finish(bool bSkipped)
{
	if (bFinished) { return; }
	bFinished = true;
	DestroyFlyers();

	const float Blend = bSkipped ? 0.15f : 0.45f;
	if (AIronManCharacter* P = Player.Get())
	{
		P->SetBodyHiddenForCinematic(false);
		P->SetCinematicMode(false, nullptr);
		if (APlayerController* PC = Cast<APlayerController>(P->GetController()))
		{
			PC->SetViewTargetWithBlend(P, Blend, VTBlend_EaseInOut, 2.f);
		}
	}
	if (Overlay)
	{
		Overlay->RemoveFromParent();
		Overlay = nullptr;
	}
	if (!bShort)
	{
		if (UJarvisComponent* J = UJarvisComponent::Get(this))
		{
			if (bSkipped) { J->StopAll(); }
			J->Say(EAIEvent::Start);
		}
	}

	// Keep the camera alive until the blend back has finished.
	if (CineCam) { CineCam->SetLifeSpan(Blend + 0.5f); }
	SetLifeSpan(Blend + 0.6f);
	SetActorTickEnabled(false);

	OnFinished.Broadcast(this, bShort);
}

void ASuitUpCinematic::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!bFinished)
	{
		// Level change mid-sequence: just clean up.
		bFinished = true;
		DestroyFlyers();
		if (Overlay) { Overlay->RemoveFromParent(); Overlay = nullptr; }
		if (AIronManCharacter* P = Player.Get()) { P->SetCinematicMode(false, nullptr); }
	}
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
