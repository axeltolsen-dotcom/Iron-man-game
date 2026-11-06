#include "SuitUpCinematic.h"

#include "IronManArena.h"
#include "IronManCharacter.h"
#include "ArenaVisuals.h"
#include "ArenaBlastFX.h"
#include "ArenaAudio.h"
#include "CinematicOverlayWidget.h"
#include "JarvisComponent.h"
#include "PilotAvatar.h"
#include "CareerSubsystem.h"

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
	float EaseOutCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); const float U = 1.f - T; return 1.f - U * U * U; }
	float EaseInOut(float T) { return FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f), 2.f); }
	float EaseInCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * T; }
	/** Ease out with a small overshoot (back), settles on 1. */
	float EaseOutBack(float T) { T = FMath::Clamp(T, 0.f, 1.f); const float C1 = 1.70158f, C3 = C1 + 1.f; return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f); }
	FVector Bezier(const FVector& A, const FVector& C, const FVector& B, float T) { const float U = 1.f - T; return A * (U * U) + C * (2.f * U * T) + B * (T * T); }

	bool IsFaceName(const FString& N)
	{
		for (const TCHAR* T : { TEXT("Faceplate"), TEXT("Face"), TEXT("Jaw"), TEXT("Chin"), TEXT("Cheek"), TEXT("Mouth") })
		{
			if (N.Contains(T, ESearchCase::CaseSensitive)) { return true; }
		}
		return false;
	}
	bool IsEyeName(const FString& N) { return N.Contains(TEXT("Eye"), ESearchCase::CaseSensitive); }

	const FName NitsParam(TEXT("EmissiveStrength"));
	const FLinearColor GantrySteel(0.045f, 0.047f, 0.052f);
	const FLinearColor GantryYellow(0.75f, 0.45f, 0.02f);
	const FLinearColor WeldColor(1.f, 0.62f, 0.25f);
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
	if (bShort) { return 2.6f; }
	switch (Tpl)
	{
	case ETemplate::Garage:   return 9.f;
	case ETemplate::Air:      return 8.f;
	case ETemplate::Nano:     return 6.5f;
	case ETemplate::Suitcase: return 7.f;
	default:                  return 7.f;
	}
}

FTransform ASuitUpCinematic::FrameXf(float Alpha, bool bWorldAnchor) const
{
	FTransform X = Root0;
	if (Tpl == ETemplate::Air && !bShort && !bWorldAnchor)
	{
		// Freefall: the pilot drops while the armour assembles; the boot thrusters catch the fall at ~0.8.
		const float T = FMath::Clamp(Alpha / 0.82f, 0.f, 1.f);
		const float S = T * T * (3.f - 2.f * T);
		X.AddToTranslation(FVector(0.f, 0.f, 900.f * SizeScale * (1.f - S)));
	}
	return X;
}

UStaticMeshComponent* ASuitUpCinematic::Prop(EArenaShape Shape, const FLinearColor& Color, float Emissive, const FVector& Scale)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ArenaVisuals::GetShapeMesh(Shape));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(SceneRoot);
	C->SetWorldScale3D(Scale);
	C->RegisterComponent();
	ArenaVisuals::PaintComponent(this, C, Color, Emissive, Emissive > 0.5f ? EArenaSurface::Glow : EArenaSurface::Steel);
	Props.Add(C);
	return C;
}

void ASuitUpCinematic::PlaceBeam(UStaticMeshComponent* C, const FVector& A, const FVector& B, float Thick) const
{
	if (!C) { return; }
	const FVector D = B - A;
	const float L = FMath::Max(1.f, float(D.Size()));
	C->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(D / L).Rotator());
	C->SetWorldScale3D(FVector(Thick / 100.f, Thick / 100.f, L / 100.f));
}

void ASuitUpCinematic::Begin(AIronManCharacter* InPlayer, int32 InSuitIndex, bool bInShort)
{
	Player = InPlayer;
	SuitIndex = InSuitIndex;
	bShort = bInShort;
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	Style = Suit.CinematicStyle;
	switch (Style)
	{
	case ECinematicStyle::Drop:     Tpl = ETemplate::Air; break;
	case ECinematicStyle::Nano:
	case ECinematicStyle::Ultron:   Tpl = ETemplate::Nano; break;
	case ECinematicStyle::Suitcase: Tpl = ETemplate::Suitcase; break;
	default:                        Tpl = ETemplate::Garage; break;
	}
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
	if (USceneComponent* R = InPlayer->GetSuitRoot()) { Root0 = R->GetComponentTransform(); }
	else { Root0 = FTransform(Yaw, InPlayer->GetActorLocation() - FVector(0.f, 0.f, 96.f * SizeScale)); }

	SpawnFlyers();
	SpawnProps();
	BuildShots();
	InPlayer->SetBodyHiddenForCinematic(true);

	// The pilot stands in the middle while the armour closes around them.
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APilotAvatar* Pilot = World->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), FTransform(Root0.GetRotation(), Root0.GetLocation()), SP);
		if (Pilot)
		{
			const UCareerSubsystem* CS = UCareerSubsystem::Get(this);
			Pilot->ApplyConfig(CS ? CS->GetAvatar() : FAvatarConfig(), true);
			Pilot->SetActorScale3D(FVector(FMath::Clamp(SuitH / 190.f, 0.85f, 1.1f)));
			PilotBody = Pilot;
		}
	}

	// Camera.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CineCam = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), InPlayer->GetActorLocation() + BaseFwd * 400.f, FRotator::ZeroRotator, Params);
	if (CineCam)
	{
		UCineCameraComponent* Cam = CineCam->GetCineCameraComponent();
		Cam->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		Cam->SetConstraintAspectRatio(false);
		Cam->SetCurrentFocalLength(35.f);
		Cam->CurrentAperture = 4.f;
		Cam->PostProcessBlendWeight = 1.f;
		FPostProcessSettings& PP = Cam->PostProcessSettings;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.55f;
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.15f;
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
			PP.FilmGrainIntensity = 0.6f;
			PP.bOverride_SceneFringeIntensity = true;
			PP.SceneFringeIntensity = 1.5f;
		}
		else if (Tpl == ETemplate::Garage)
		{
			// Workshop grade: warm key, teal shadows.
			PP.bOverride_ColorGain = true;
			PP.ColorGain = FVector4(1.05f, 1.f, 0.95f, 1.f);
			PP.bOverride_ColorOffsetShadows = true;
			PP.ColorOffsetShadows = FVector4(0.f, 0.01f, 0.02f, 0.f);
		}
		UpdateCamera(0.f, 0.f);
		PC->SetViewTargetWithBlend(CineCam, bShort ? 0.2f : 0.f, VTBlend_EaseInOut, 2.f);
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
		Overlay->SetHudBoot(0.f, AI.Label, Suit.EmissiveColor);
	}

	// Voice: the new suit's AI introduces it (full) or takes over (swap). Voices default OFF in settings.
	if (UJarvisComponent* J = UJarvisComponent::Get(this))
	{
		if (bShort) { J->SetPersona(Suit.AI, true); }
		else
		{
			J->StopAll();
			J->SetPersona(Suit.AI, false);
			J->Say(EAIEvent::SuitIntro, Suit.Id, true);
		}
	}
	ArenaAudio::PlayAt(this, Tpl == ETemplate::Air ? FName(TEXT("thruster_flight_fast")) : FName(TEXT("suitup_servo")), Root0.GetLocation(), bShort ? 0.6f : 0.9f);
	UE_LOG(LogIronManArena, Log, TEXT("Suit-up cinematic: %s (%s, template %d, %d plates, %.1fs)"), *Suit.Id.ToString(), bShort ? TEXT("short") : TEXT("full"), int32(Tpl), Flyers.Num(), Duration);
}

void ASuitUpCinematic::SpawnFlyers()
{
	AIronManCharacter* P = Player.Get();
	USceneComponent* Root = P ? P->GetSuitRoot() : nullptr;
	if (!Root) { return; }
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	const FTransform RootXf = Root->GetComponentTransform();
	const TArray<FArenaPaintedPart>& Parts = P->GetSuitParts();

	TArray<const FArenaPaintedPart*> Candidates;
	float MaxZ = 0.f;
	for (const FArenaPaintedPart& Part : Parts)
	{
		if (Part.Component && Part.Role != ESuitPaintRole::Fixed && Part.Component->IsVisible() && !Part.Component->bHiddenInGame && Part.Component->GetStaticMesh())
		{
			Candidates.Add(&Part);
			MaxZ = FMath::Max(MaxZ, float(RootXf.InverseTransformPosition(Part.Component->Bounds.Origin + FVector(0, 0, Part.Component->Bounds.BoxExtent.Z)).Z));
		}
	}
	SuitH = FMath::Clamp(MaxZ, 120.f, 400.f);
	const float H = SuitH;

	// Keep every head plate (faceplate / eyes need all of them); thin out the body plates for the budget.
	TArray<const FArenaPaintedPart*> Head, Body;
	for (const FArenaPaintedPart* Part : Candidates)
	{
		const FVector L = RootXf.InverseTransformPosition(Part->Component->GetComponentLocation());
		if (L.Z > 0.8f * H) { Head.Add(Part); } else { Body.Add(Part); }
	}
	const int32 MaxBody = bShort ? 40 : 110;
	const int32 Stride = FMath::Max(1, FMath::DivideAndRoundUp(Body.Num(), MaxBody));
	TArray<const FArenaPaintedPart*> Picked = Head;
	for (int32 i = 0; i < Body.Num(); i += Stride) { Picked.Add(Body[i]); }

	// Face group bounds -> hinge (top-back edge of the faceplate, Mark III hinge line) and the face centre.
	FBox FaceBox(ForceInit);
	for (const FArenaPaintedPart* Part : Picked)
	{
		const FString N = Part->Component->GetName();
		const FVector L = RootXf.InverseTransformPosition(Part->Component->GetComponentLocation());
		if (L.Z > 0.78f * H && (IsFaceName(N) || IsEyeName(N)))
		{
			const FBox WB = Part->Component->Bounds.GetBox();
			FaceBox += RootXf.InverseTransformPosition(WB.Min);
			FaceBox += RootXf.InverseTransformPosition(WB.Max);
		}
	}
	if (FaceBox.IsValid)
	{
		HingeRel = FVector(FaceBox.Min.X - 2.f, 0.f, FaceBox.Max.Z + 2.f);
		FaceRel = FaceBox.GetCenter();
	}
	else
	{
		HingeRel = FVector(-6.f, 0.f, H + 1.f);
		FaceRel = FVector(10.f, 0.f, H - 13.f);
	}
	ReactorRel = FVector(14.f, 0.f, 0.73f * H);

	// Timeline per template (normalized). Windows: [start, end] of each group's flights.
	float Win[GNum][2] = { {0.17f, 0.27f}, {0.24f, 0.35f}, {0.36f, 0.50f}, {0.52f, 0.68f}, {0.69f, 0.75f}, {0.69f, 0.75f}, {0.f, 0.f} };
	float Fly = 0.05f;
	bool bPooled = false;
	FVector2D Pool(0.f, 1.f);
	switch (bShort ? ETemplate::Suitcase : Tpl)
	{
	case ETemplate::Garage:
		FaceStart = 0.76f; FaceEnd = 0.81f; EyesAt = 0.83f; RevealAt = 0.86f; Fly = 0.05f;
		break;
	case ETemplate::Air:
		Win[GBoots][0] = 0.08f; Win[GBoots][1] = 0.30f; Win[GLegs][0] = 0.10f; Win[GLegs][1] = 0.32f;
		Win[GArms][0] = 0.18f; Win[GArms][1] = 0.42f; Win[GTorso][0] = 0.44f; Win[GTorso][1] = 0.52f;
		Win[GHelmet][0] = 0.56f; Win[GHelmet][1] = 0.64f; Win[GFace][0] = 0.56f; Win[GFace][1] = 0.64f;
		FaceStart = 0.66f; FaceEnd = 0.72f; EyesAt = 0.74f; RevealAt = 0.84f; Fly = 0.09f;
		break;
	case ETemplate::Nano:
		bPooled = true; Pool = FVector2D(0.14f, 0.60f);
		Win[GFace][0] = 0.58f; Win[GFace][1] = 0.72f;
		FaceStart = FaceEnd = 0.72f; EyesAt = 0.76f; RevealAt = 0.82f; Fly = 0.06f;
		break;
	case ETemplate::Suitcase:
	default:
		bPooled = true; Pool = FVector2D(0.30f, 0.60f);
		Win[GFace][0] = 0.55f; Win[GFace][1] = 0.63f;
		FaceStart = 0.64f; FaceEnd = 0.74f; EyesAt = 0.77f; RevealAt = 0.82f; Fly = 0.07f;
		break;
	}
	if (bShort)
	{
		bPooled = true; Pool = FVector2D(0.04f, 0.46f); Fly = 0.13f;
		Win[GFace][0] = 0.38f; Win[GFace][1] = 0.50f;
		FaceStart = 0.52f; FaceEnd = 0.64f; EyesAt = 0.68f; RevealAt = 0.74f;
		if (Tpl == ETemplate::Nano) { Win[GFace][0] = 0.44f; Win[GFace][1] = 0.62f; FaceStart = FaceEnd = 0.62f; EyesAt = 0.66f; RevealAt = 0.72f; }
	}

	// Build the flyers.
	TArray<int32> ByGroup[GNum];
	for (const FArenaPaintedPart* Part : Picked)
	{
		const UStaticMeshComponent* Src = Part->Component;
		const FTransform Target = Src->GetComponentTransform().GetRelativeTransform(RootXf);
		const FVector L = Target.GetLocation();
		const FString N = Src->GetName();
		uint8 G;
		if (L.Z > 0.78f * H && IsEyeName(N)) { G = GEyes; }
		else if (L.Z > 0.78f * H && IsFaceName(N)) { G = GFace; }
		else if (L.Z > 0.8f * H) { G = GHelmet; }
		else if (L.Z < 0.12f * H) { G = GBoots; }
		else if (L.Z < 0.5f * H) { G = GLegs; }
		else if (FMath::Abs(L.Y) > 0.13f * H) { G = GArms; }
		else { G = GTorso; }

		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(this);
		Copy->SetStaticMesh(Src->GetStaticMesh());
		for (int32 m = 0; m < Src->GetNumMaterials(); ++m) { Copy->SetMaterial(m, Src->GetMaterial(m)); } // shares the part's MID
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetMobility(EComponentMobility::Movable);
		Copy->SetVisibility(false);
		Copy->SetupAttachment(SceneRoot);
		Copy->RegisterComponent();
		FlyingParts.Add(Copy);

		FFlyer F;
		F.Copy = Copy;
		F.RealMat = Src->GetMaterial(0);
		F.RelToRoot = Target;
		F.Group = G;
		F.StartRot = Target.GetRotation();
		const FVector Out = FVector(L.X, L.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector(1.f, 0.f, 0.f));
		const float Side = L.Y >= 0.f ? 1.f : -1.f;
		const FVector TL = (G == GFace && Tpl != ETemplate::Nano) ? (FQuat(FRotator(100.f, 0.f, 0.f)) * (L - HingeRel) + HingeRel) : L; // faces fly to the open pose

		const ETemplate Use = bShort ? (Tpl == ETemplate::Nano ? ETemplate::Nano : ETemplate::Suitcase) : Tpl;
		if (bShort && Use != ETemplate::Nano)
		{
			// Quick swap: plates snap in from close by (outward + up), the faceplate still swings shut.
			F.StartRel = TL + Out * 70.f + FVector(0.f, 0.f, 25.f);
			F.CtrlRel = (F.StartRel + TL) * 0.5f + Out * 25.f + FVector(0.f, 0.f, 20.f);
			F.StartRot = Target.GetRotation() * FQuat(FRotator(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f));
			F.StartScale = 0.6f;
		}
		else switch (Use)
		{
		case ETemplate::Garage:
			if (G == GHelmet || G == GFace)
			{
				F.StartRel = TL + FVector(-6.f, 0.f, 95.f);
				F.CtrlRel = TL + FVector(-4.f, 0.f, 50.f);
			}
			else
			{
				// Presented by the robot arm from the side, then pressed on.
				F.Arm = (G == GArms ? 2 : 0) + (Side > 0.f ? 1 : 0);
				F.StartRel = TL + FVector(G == GArms ? -25.f : 30.f, Side * 75.f, 18.f);
				F.CtrlRel = (F.StartRel + TL) * 0.5f + FVector(0.f, Side * 20.f, 14.f);
				F.StartRot = Target.GetRotation() * FQuat(FRotator(0.f, Side * 35.f, Side * 20.f));
			}
			break;
		case ETemplate::Air:
		{
			FVector D = FMath::VRand(); D.Z = FMath::Abs(D.Z) * 0.6f + 0.15f; D.Normalize();
			if (FVector::DotProduct(D, FVector(1.f, 0.f, 0.f)) > 0.6f) { D.X = -D.X; } // not straight through the camera
			F.StartRel = TL + D * FMath::FRandRange(900.f, 1400.f);
			F.CtrlRel = (F.StartRel + TL) * 0.5f + FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal() * FMath::FRandRange(-320.f, 320.f) + FVector(0.f, 0.f, 160.f);
			F.StartRot = FQuat(FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f)));
			break;
		}
		case ETemplate::Nano:
		{
			const FVector From = (G == GFace || G == GHelmet || G == GEyes) ? FVector(4.f, 0.f, 0.8f * H) : ReactorRel; // helmet grows up from the neck
			F.StartRel = From;
			F.CtrlRel = (From + L) * 0.5f + Out * 16.f;
			F.StartScale = 0.04f;
			break;
		}
		case ETemplate::Suitcase:
		default:
			F.StartRel = CaseRel + FVector(0.f, 0.f, 18.f);
			F.CtrlRel = FVector(TL.X + 55.f, (TL.Y + CaseRel.Y) * 0.5f, TL.Z * 0.55f + 12.f);
			F.StartRot = Target.GetRotation() * FQuat(FRotator(90.f, 0.f, 0.f)); // folded flat in the case
			F.StartScale = 0.35f;
			break;
		}
		if (G == GEyes)
		{
			// Eyes ignite in place once the faceplate is shut.
			if (UMaterialInstanceDynamic* MID = ArenaVisuals::PaintComponent(this, Copy, Suit.EmissiveColor, 1.f, EArenaSurface::Glow))
			{
				F.GlowMID = MID;
				float Nits = 1.f;
				if (MID->GetScalarParameterValue(NitsParam, Nits)) { F.BaseNits = FMath::Max(0.01f, Nits); }
				MID->SetScalarParameterValue(NitsParam, 0.f);
			}
		}
		else if (Tpl == ETemplate::Nano)
		{
			// Wet nanite glow until the plate settles.
			F.GlowMID = ArenaVisuals::PaintComponent(this, Copy, FMath::Lerp(Suit.EmissiveColor, Suit.PrimaryColor, 0.35f), 5.f, EArenaSurface::Glow);
		}
		if (Tpl == ETemplate::Air && !bShort && G != GEyes)
		{
			UStaticMeshComponent* Trail = NewObject<UStaticMeshComponent>(this);
			Trail->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
			Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Trail->SetCastShadow(false);
			Trail->SetVisibility(false);
			Trail->SetupAttachment(SceneRoot);
			Trail->RegisterComponent();
			ArenaVisuals::PaintComponent(this, Trail, FLinearColor(0.75f, 0.85f, 1.f), 3.f, EArenaSurface::Glow);
			FlyingParts.Add(Trail);
			F.Trail = Trail;
		}
		ByGroup[G].Add(Flyers.Add(F));
	}

	// Schedule.
	auto Schedule = [this, Fly](TArray<int32>& Idx, float S, float E, TFunctionRef<float(const FFlyer&)> Key)
	{
		Idx.Sort([this, &Key](int32 A, int32 B) { return Key(Flyers[A]) < Key(Flyers[B]); });
		const int32 Num = Idx.Num();
		const float FT = FMath::Min(Fly, (E - S) * 0.7f);
		for (int32 i = 0; i < Num; ++i)
		{
			FFlyer& F = Flyers[Idx[i]];
			F.FlyTime = FT * FMath::FRandRange(0.85f, 1.15f);
			F.Delay = FMath::Lerp(S, FMath::Max(S, E - F.FlyTime), Num > 1 ? float(i) / float(Num - 1) : 0.f);
		}
	};
	auto ByZ = [](const FFlyer& F) { return float(F.RelToRoot.GetLocation().Z); };
	const FVector Reactor = ReactorRel;
	auto ByReactor = [Reactor](const FFlyer& F) { return float(FVector::Dist(F.RelToRoot.GetLocation(), Reactor)); };
	if (bPooled)
	{
		TArray<int32> All;
		for (int32 g = GBoots; g <= GHelmet; ++g) { All.Append(ByGroup[g]); }
		if (Tpl == ETemplate::Nano) { Schedule(All, Pool.X, Pool.Y, ByReactor); } else { Schedule(All, Pool.X, Pool.Y, ByZ); }
	}
	else
	{
		for (int32 g = GBoots; g <= GHelmet; ++g) { Schedule(ByGroup[g], Win[g][0], Win[g][1], ByZ); }
	}
	Schedule(ByGroup[GFace], Win[GFace][0], Win[GFace][1], ByZ);
	for (int32 i : ByGroup[GEyes]) { Flyers[i].Delay = EyesAt; Flyers[i].FlyTime = 0.01f; }
}

void ASuitUpCinematic::SpawnProps()
{
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	const float Sc = Root0.GetScale3D().X;
	const FTransform F0 = FrameXf(0.f, true);
	if (bShort) { return; }

	if (Tpl == ETemplate::Garage)
	{
		// Platform: steel disc with a glowing ring (lit when the boots lock).
		if (UStaticMeshComponent* Disc = Prop(EArenaShape::Cylinder, GantrySteel, 0.f, FVector(2.4f, 2.4f, 0.06f) * Sc)) { Disc->SetWorldLocation(F0.TransformPosition(FVector(0, 0, -2.f))); }
		if (UStaticMeshComponent* Ring = Prop(EArenaShape::Cylinder, Suit.EmissiveColor, 2.5f, FVector(2.55f, 2.55f, 0.02f) * Sc)) { Ring->SetWorldLocation(F0.TransformPosition(FVector(0, 0, -4.f))); }
		// Four robot arms on columns + the ceiling carrier rail for the helmet.
		const FVector Bases[4] = { FVector(70.f, -135.f, 0.f), FVector(70.f, 135.f, 0.f), FVector(-70.f, -130.f, 0.f), FVector(-70.f, 130.f, 0.f) };
		for (int32 a = 0; a < 4; ++a)
		{
			FRobotArm A;
			A.BaseRel = Bases[a];
			A.RestRel = Bases[a] * FVector(0.8f, 0.6f, 0.f) + FVector(0.f, 0.f, 175.f);
			A.ClawRel = A.RestRel;
			A.Column = Prop(EArenaShape::Cylinder, GantrySteel, 0.f, FVector(0.3f, 0.3f, 1.6f) * Sc);
			if (A.Column.IsValid()) { A.Column->SetWorldLocation(F0.TransformPosition(Bases[a] + FVector(0, 0, 80.f))); }
			A.Light = Prop(EArenaShape::Cylinder, GantryYellow, 4.f, FVector(0.34f, 0.34f, 0.05f) * Sc);
			if (A.Light.IsValid()) { A.Light->SetWorldLocation(F0.TransformPosition(Bases[a] + FVector(0, 0, 158.f))); }
			A.Upper = Prop(EArenaShape::Cylinder, GantrySteel, 0.f, FVector::OneVector);
			A.Fore = Prop(EArenaShape::Cylinder, FLinearColor(0.09f, 0.09f, 0.1f), 0.f, FVector::OneVector);
			A.ClawA = Prop(EArenaShape::Cube, GantryYellow, 0.f, FVector(0.06f, 0.03f, 0.14f) * Sc);
			A.ClawB = Prop(EArenaShape::Cube, GantryYellow, 0.f, FVector(0.06f, 0.03f, 0.14f) * Sc);
			Arms.Add(A);
		}
		if (UStaticMeshComponent* Rail = Prop(EArenaShape::Cube, GantrySteel, 0.f, FVector(0.25f, 3.6f, 0.2f) * Sc)) { Rail->SetWorldLocation(F0.TransformPosition(FVector(-10.f, 0.f, SuitH + 120.f))); Rail->SetWorldRotation(F0.GetRotation()); }
		if (UStaticMeshComponent* Strip = Prop(EArenaShape::Cube, FLinearColor(0.9f, 0.95f, 1.f), 3.f, FVector(0.08f, 3.4f, 0.03f) * Sc)) { Strip->SetWorldLocation(F0.TransformPosition(FVector(-10.f, 0.f, SuitH + 108.f))); Strip->SetWorldRotation(F0.GetRotation()); }
	}
	else if (Tpl == ETemplate::Suitcase)
	{
		// Mark V case: body + lid on a hinge + handle.
		CaseHinge = NewObject<USceneComponent>(this);
		CaseHinge->SetupAttachment(SceneRoot);
		CaseHinge->RegisterComponent();
		CaseHinge->SetWorldLocationAndRotation(F0.TransformPosition(CaseRel + FVector(-20.f, 0.f, 34.f)), F0.GetRotation());
		CaseProp = Prop(EArenaShape::Cube, FLinearColor(0.11f, 0.11f, 0.12f), 0.f, FVector(0.4f, 0.62f, 0.32f) * Sc);
		if (CaseProp) { CaseProp->SetWorldLocationAndRotation(F0.TransformPosition(CaseRel + FVector(0, 0, 16.f)), F0.GetRotation()); }
		CaseLid = Prop(EArenaShape::Cube, FLinearColor(0.13f, 0.13f, 0.14f), 0.f, FVector(0.4f, 0.62f, 0.05f) * Sc);
		if (CaseLid) { CaseLid->AttachToComponent(CaseHinge, FAttachmentTransformRules::KeepWorldTransform); CaseLid->SetWorldLocation(F0.TransformPosition(CaseRel + FVector(0, 0, 34.f))); }
		if (UStaticMeshComponent* Handle = Prop(EArenaShape::Cube, FLinearColor(0.6f, 0.6f, 0.62f), 0.f, FVector(0.04f, 0.16f, 0.04f) * Sc))
		{
			Handle->AttachToComponent(CaseHinge, FAttachmentTransformRules::KeepWorldTransform);
			Handle->SetWorldLocationAndRotation(F0.TransformPosition(CaseRel + FVector(21.f, 0.f, 30.f)), F0.GetRotation());
		}
		if (UStaticMeshComponent* Glow = Prop(EArenaShape::Cube, Suit.EmissiveColor, 3.f, FVector(0.36f, 0.58f, 0.01f) * Sc)) { Glow->SetWorldLocationAndRotation(F0.TransformPosition(CaseRel + FVector(0, 0, 32.5f)), F0.GetRotation()); }
	}

	// Thrusters (Air catches the fall; Garage boots puff when they lock).
	if (Tpl == ETemplate::Air || Tpl == ETemplate::Garage)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* Fl = Prop(EArenaShape::Cone, ArenaVisuals::Thruster, 8.f, FVector(0.11f, 0.11f, 0.4f) * Sc);
			if (Fl) { Fl->SetCastShadow(false); Fl->SetVisibility(false); Thrusters.Add(Fl); }
		}
	}
}

void ASuitUpCinematic::BuildShots()
{
	const float K = SuitH / 185.f;
	auto V = [K](float X, float Y, float Z) { return FVector(X, Y, Z) * K; };
	auto Add = [this](float Start, float Focal, const FVector& PA, const FVector& PB, const FVector& LA, const FVector& LB, bool bWorld = false, float OA = 0.f, float OB = 0.f, float Whip = 0.f)
	{
		FCamShot S; S.Start = Start; S.Focal = Focal; S.PosA = PA; S.PosB = PB; S.LookA = LA; S.LookB = LB; S.bWorld = bWorld; S.OrbitA = OA; S.OrbitB = OB; S.Whip = Whip;
		Shots.Add(S);
	};
	const FVector Face = FaceRel;
	if (bShort)
	{
		Add(0.f, 32.f, V(300, 0, 120), V(290, 0, 125), V(0, 0, 110), V(0, 0, 120), false, -35.f, -10.f);
		Add(FaceStart - 0.04f, 85.f, Face + V(130, 18, 2), Face + V(112, 6, 0), Face, Face);
		Add(RevealAt, 35.f, V(340, -40, 100), V(310, -20, 108), V(0, 0, 112), V(0, 0, 116));
		return;
	}
	switch (Tpl)
	{
	case ETemplate::Garage:   // A1 crane wide, A4 low boots, A2 85 mm chest insert, A3 50 mm dolly, A5 85 mm faceplate, A7 35 mm push-in
		Add(0.f, 35.f, V(430, -270, 330), V(390, -230, 230), V(0, 0, 110), V(0, 0, 100));
		Add(0.17f, 24.f, V(150, -95, 16), V(140, -70, 28), V(0, 0, 30), V(0, 0, 70));
		Add(0.355f, 85.f, V(175, 45, 140), V(160, 25, 138), ReactorRel, ReactorRel);
		Add(0.515f, 50.f, V(250, -230, 145), V(250, 140, 152), V(0, 0, 130), V(0, 0, 135));
		Add(0.69f, 85.f, Face + V(150, 22, 4), Face + V(118, 6, 0), Face + V(0, 0, 6), Face);
		Add(0.84f, 35.f, V(440, 0, 92), V(305, 0, 104), V(0, 0, 112), V(0, 0, 118));
		break;
	case ETemplate::Air:      // B1 world-anchored wide looking up, B2 tracking side, B3 chest clamp, B4 face, B5 thrusters catch
		Add(0.f, 24.f, V(420, -160, -40), V(400, -140, 0), V(0, 0, 520), V(0, 0, 300), true);
		Add(0.21f, 35.f, V(260, -280, 175), V(235, -310, 120), V(0, 0, 110), V(0, 0, 112));
		Add(0.43f, 50.f, V(205, 30, 150), V(188, 12, 142), ReactorRel, ReactorRel);
		Add(0.57f, 85.f, Face + V(145, -24, 6), Face + V(118, -6, 0), Face + V(0, 0, 4), Face);
		Add(0.755f, 28.f, V(450, -130, 30), V(400, -90, 50), V(0, 0, 200), V(0, 0, 110), true);
		break;
	case ETemplate::Nano:     // N1 reactor tap close, N2 orbit as the flow covers the body, N3 helmet forms, N4 push-in
		Add(0.f, 85.f, ReactorRel + V(115, 4, 4), ReactorRel + V(98, 0, 2), ReactorRel, ReactorRel);
		Add(0.155f, 50.f, V(280, 0, 125), V(280, 0, 120), V(0, 0, 112), V(0, 0, 108), false, -45.f, 125.f);
		Add(0.6f, 85.f, Face + V(140, 14, 2), Face + V(116, 0, 0), Face, Face);
		Add(RevealAt - 0.04f, 35.f, V(410, -60, 90), V(295, -18, 104), V(0, 0, 112), V(0, 0, 118));
		break;
	case ETemplate::Suitcase: // S1 low tracking on the case, S2 case opens, S3 tilt up the unfolding rings, S4 faceplate, S5 whip pan
	default:
		Add(0.f, 35.f, CaseRel + V(140, -240, 22), CaseRel + V(120, -130, 24), CaseRel + V(0, 0, 18), CaseRel + V(0, 0, 18));
		Add(0.18f, 50.f, CaseRel + V(170, -120, 75), CaseRel + V(160, -100, 70), CaseRel + V(0, 0, 25), CaseRel + V(0, 0, 25));
		Add(0.30f, 35.f, V(310, -160, 50), V(300, -150, 175), V(0, 0, 30), V(0, 0, 150));
		Add(FaceStart - 0.05f, 85.f, Face + V(140, 18, 3), Face + V(116, 4, 0), Face, Face);
		Add(RevealAt - 0.02f, 28.f, V(380, 0, 88), V(340, 0, 100), V(0, 0, 112), V(0, 0, 116), false, -85.f, 0.f, 0.22f);
		break;
	}
}

void ASuitUpCinematic::TickFlyers(float Alpha, float Dt)
{
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	const FTransform Frame = FrameXf(Alpha);
	const float Sec = Duration;
	int32 LandedNow = 0;

	for (FFlyer& F : Flyers)
	{
		UStaticMeshComponent* Copy = F.Copy.Get();
		if (!Copy) { continue; }
		const FVector L = F.RelToRoot.GetLocation();
		const FQuat R = F.RelToRoot.GetRotation();
		FVector PosRel = L;
		FQuat RotRel = R;
		float Scale = 1.f;
		bool bVisible = true;

		if (F.Group == GEyes)
		{
			bVisible = Alpha >= EyesAt;
			if (bVisible && F.GlowMID.IsValid())
			{
				// Ignition: dark -> flare 25 -> settle 12 (with a flicker on the way up).
				const float T = (Alpha - EyesAt) * Sec;
				float G = T < 0.18f ? FMath::Lerp(0.f, 25.f, T / 0.18f) * (FMath::FRand() < 0.25f ? 0.4f : 1.f) : FMath::Lerp(25.f, 12.f, FMath::Clamp((T - 0.18f) / 0.5f, 0.f, 1.f));
				F.GlowMID->SetScalarParameterValue(NitsParam, F.BaseNits * G);
			}
		}
		else
		{
			const float T = (Alpha - F.Delay) / FMath::Max(0.005f, F.FlyTime);
			const bool bGarageHeld = Tpl == ETemplate::Garage && !bShort && F.Arm >= 0 && T > -0.6f; // held by the arm before the press
			if (T <= 0.f && !bGarageHeld && !(F.Group == GFace && T > -0.3f && Tpl == ETemplate::Garage && !bShort))
			{
				bVisible = false;
			}
			else
			{
				const float Tc = FMath::Clamp(T, 0.f, 1.f);
				const bool bHinge = F.Group == GFace && Tpl != ETemplate::Nano;
				const FVector Dest = bHinge ? (FQuat(FRotator(100.f, 0.f, 0.f)) * (L - HingeRel) + HingeRel) : L;
				const FQuat DestRot = bHinge ? FQuat(FRotator(100.f, 0.f, 0.f)) * R : R;
				float E;
				switch (Tpl)
				{
				case ETemplate::Nano: E = EaseOutBack(Tc); break;
				case ETemplate::Air: E = EaseOutCubic(Tc); break;
				default: E = EaseInOut(Tc); break;
				}
				if (Tpl == ETemplate::Nano && !bShort)
				{
					PosRel = Bezier(F.StartRel, F.CtrlRel, Dest, FMath::Clamp(E, 0.f, 1.f));
					Scale = FMath::Lerp(F.StartScale, 1.f, FMath::Clamp(E, 0.f, 1.08f));
				}
				else
				{
					PosRel = Bezier(F.StartRel, F.CtrlRel, Dest, FMath::Clamp(E, 0.f, 1.f));
					Scale = FMath::Lerp(F.StartScale, 1.f, FMath::Clamp(E, 0.f, 1.f));
				}
				RotRel = FQuat::Slerp(F.StartRot, DestRot, FMath::Clamp(E, 0.f, 1.f));
				// Clamp punch: last few % press 1.5 cm deeper then settle.
				if (Tc >= 1.f && !F.bLanded)
				{
					F.bLanded = true;
					++LandedNow;
					if (F.Group == GTorso && !bChestHit && (Tpl == ETemplate::Garage || Tpl == ETemplate::Air) && !bShort && L.Z > 0.55f * SuitH)
					{
						bChestHit = true;
						HitStopLeft = 0.35f;
						Shake(Tpl == ETemplate::Air ? 9.f : 5.f);
						ArenaAudio::PlayAt(this, TEXT("suitup_servo_heavy"), Frame.TransformPosition(L), 1.f);
						AArenaBlastFX::Spawn(GetWorld(), Frame.TransformPosition(ReactorRel), 45.f * SizeScale, Suit.EmissiveColor, 0.25f, false);
					}
					if (Tpl == ETemplate::Nano && (LandedNow % 3) == 0)
					{
						AArenaBlastFX::Spawn(GetWorld(), Frame.TransformPosition(L), 7.f * SizeScale, Suit.EmissiveColor, 0.12f, false);
					}
				}
				if (Tpl == ETemplate::Nano && F.GlowMID.IsValid() && T > 1.1f)
				{
					// Settled: the wet glow hardens into the real plate.
					for (int32 m = 0; m < Copy->GetNumMaterials(); ++m) { if (UMaterialInterface* RM = F.RealMat.Get()) { Copy->SetMaterial(m, RM); } }
					F.GlowMID = nullptr;
				}
				// Faceplate: swings shut on the hinge (ease-in = it snaps closed).
				if (F.Group == GFace && Tpl != ETemplate::Nano)
				{
					const float C = FaceEnd > FaceStart ? EaseInCubic((Alpha - FaceStart) / (FaceEnd - FaceStart)) : 1.f;
					if (Alpha >= FaceStart)
					{
						const float Ang = 100.f * (1.f - C);
						const FQuat Q(FRotator(Ang, 0.f, 0.f));
						PosRel = Q * (L - HingeRel) + HingeRel;
						RotRel = Q * R;
					}
				}
			}
			// Contrail.
			if (UStaticMeshComponent* Tr = F.Trail.Get())
			{
				const FVector W = Frame.TransformPosition(PosRel);
				const FVector Vel = (W - F.PrevLoc) / FMath::Max(Dt, 0.001f);
				const float Len = FMath::Clamp(float(Vel.Size()) * 0.06f, 0.f, 320.f * SizeScale);
				const bool bShowTrail = bVisible && !F.bLanded && Len > 15.f && !F.PrevLoc.IsZero();
				Tr->SetVisibility(bShowTrail);
				if (bShowTrail) { PlaceBeam(Tr, W - Vel.GetSafeNormal() * Len, W, 2.5f * SizeScale); }
				F.PrevLoc = W;
			}
		}
		Copy->SetVisibility(bVisible);
		if (bVisible)
		{
			const FTransform Rel(RotRel, PosRel, F.RelToRoot.GetScale3D() * Scale);
			Copy->SetWorldTransform(Rel * Frame);
		}
	}
	if (LandedNow > 0 && Clock - LastLockSfx > 0.08f)
	{
		LastLockSfx = Clock;
		ArenaAudio::PlayAt(this, TEXT("suitup_plate_lock"), Frame.GetLocation() + FVector(0, 0, SuitH * 0.5f), Tpl == ETemplate::Nano ? 0.35f : 0.7f, 0.12f);
	}
}

void ASuitUpCinematic::TickProps(float Alpha, float Dt)
{
	const FTransform Frame = FrameXf(Alpha);
	const FTransform World0 = FrameXf(Alpha, true);
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	const float Sc = Root0.GetScale3D().X;

	// Robot arms: claw follows the plate it carries (2-bone IK, elbow up), else returns to rest.
	for (int32 a = 0; a < Arms.Num(); ++a)
	{
		FRobotArm& A = Arms[a];
		FVector Goal = A.RestRel;
		float Best = 1e9f;
		for (const FFlyer& F : Flyers)
		{
			if (F.Arm != a || F.bLanded) { continue; }
			const float T = (Alpha - F.Delay) / FMath::Max(0.005f, F.FlyTime);
			if (T < -0.6f || T > 1.f) { continue; }
			if (T < Best && F.Copy.IsValid())
			{
				Best = T;
				Goal = World0.InverseTransformPosition(F.Copy->GetComponentLocation()) + FVector(0.f, A.BaseRel.Y > 0 ? 9.f : -9.f, 0.f);
			}
		}
		A.ClawRel = FMath::VInterpTo(A.ClawRel, Goal, Dt, 14.f);
		const float L1 = 115.f * Sc, L2 = 110.f * Sc;
		const FVector S = World0.TransformPosition(A.BaseRel + FVector(0, 0, 160.f));
		const FVector T = World0.TransformPosition(A.ClawRel);
		FVector D = T - S;
		const float Dist = FMath::Clamp(float(D.Size()), 10.f, (L1 + L2) * 0.99f);
		D = D.GetSafeNormal();
		const float CosA = FMath::Clamp((L1 * L1 + Dist * Dist - L2 * L2) / (2.f * L1 * Dist), -1.f, 1.f);
		const float Ang = FMath::RadiansToDegrees(FMath::Acos(CosA));
		FVector Axis = FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal();
		if (Axis.IsNearlyZero()) { Axis = World0.GetRotation().GetRightVector(); }
		const FVector U1 = D.RotateAngleAxis(Ang, Axis), U2 = D.RotateAngleAxis(-Ang, Axis);
		const FVector Up = U1.Z > U2.Z ? U1 : U2;
		const FVector Elbow = S + Up * L1;
		const FVector Reach = S + D * Dist;
		PlaceBeam(A.Upper.Get(), S, Elbow, 14.f * Sc);
		PlaceBeam(A.Fore.Get(), Elbow, Reach, 10.f * Sc);
		const FVector Fd = (Reach - Elbow).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Fd, FVector::UpVector).GetSafeNormal() * 5.f * Sc;
		const FRotator CR = FRotationMatrix::MakeFromZ(Fd).Rotator();
		if (A.ClawA.IsValid()) { A.ClawA->SetWorldLocationAndRotation(Reach + Side, CR); }
		if (A.ClawB.IsValid()) { A.ClawB->SetWorldLocationAndRotation(Reach - Side, CR); }
	}

	// Suitcase lid.
	if (CaseHinge)
	{
		const float Open = EaseOutBack(FMath::Clamp((Alpha - 0.2f) / 0.09f, 0.f, 1.f));
		CaseHinge->SetWorldRotation((FQuat(World0.GetRotation()) * FQuat(FRotator(-110.f * Open, 0.f, 0.f))).Rotator());
		if (Open > 0.05f && !bCaseOpened) { bCaseOpened = true; ArenaAudio::PlayAt(this, TEXT("suitup_servo"), World0.TransformPosition(CaseRel), 0.9f); }
		if (Alpha > 0.62f)
		{
			if (CaseProp) { CaseProp->SetVisibility(false, true); }
			CaseHinge->SetVisibility(false, true);
		}
	}

	// Thrusters: Air catches the fall (palms + boots), Garage puffs the boots once they lock.
	if (Thrusters.Num() == 4)
	{
		const float On = Tpl == ETemplate::Air ? (Alpha > 0.74f ? 1.f : 0.f) : ((Alpha > 0.3f && Alpha < 0.36f) ? 0.7f : 0.f);
		if (On > 0.f && !bThrust && Tpl == ETemplate::Air) { bThrust = true; ArenaAudio::PlayAt(this, TEXT("thruster_landing"), Frame.GetLocation(), 1.f); Shake(6.f); }
		const FVector Spots[4] = { FVector(0.f, -11.f, -2.f), FVector(0.f, 11.f, -2.f), FVector(0.f, -0.21f * SuitH, 0.47f * SuitH), FVector(0.f, 0.21f * SuitH, 0.47f * SuitH) };
		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* Fl = Thrusters[i];
			const bool bShow = On > 0.f && (i < 2 || Tpl == ETemplate::Air);
			Fl->SetVisibility(bShow);
			if (!bShow) { continue; }
			const float Flick = 0.8f + 0.3f * FMath::Sin(Elapsed * 47.f + i) + 0.15f * FMath::Sin(Elapsed * 91.f);
			const FVector Tip = Spots[i] + FVector(0.f, 0.f, -22.f * Flick * On);
			Fl->SetWorldLocationAndRotation(Frame.TransformPosition((Spots[i] + Tip) * 0.5f), (FQuat(Frame.GetRotation()) * FQuat(FRotator(180.f, 0.f, 0.f))).Rotator());
			Fl->SetWorldScale3D(FVector(0.1f, 0.1f, 0.45f * Flick * On) * Sc);
		}
	}

	// Pilot follows the (falling) frame until the armour is complete.
	if (PilotBody)
	{
		PilotBody->SetActorLocation(Frame.GetLocation());
		PilotBody->SetActorHiddenInGame(Alpha >= RevealAt - 0.01f);
	}
}

void ASuitUpCinematic::TickBeats(float Alpha)
{
	const FSuitDef& Suit = USuitLibrary::GetSuit(SuitIndex);
	const FTransform Frame = FrameXf(Alpha);

	// Nano: tap the reactor.
	if (Tpl == ETemplate::Nano && !bTapped && Alpha > (bShort ? 0.01f : 0.08f))
	{
		bTapped = true;
		AArenaBlastFX::Spawn(GetWorld(), Frame.TransformPosition(ReactorRel), 28.f * SizeScale, Suit.EmissiveColor, 0.3f, false);
		ArenaAudio::PlayAt(this, TEXT("hud_beep_confirm"), Frame.TransformPosition(ReactorRel), 0.7f);
	}
	// Garage: weld sparks on the chest insert.
	if (Tpl == ETemplate::Garage && !bShort && Alpha > 0.38f && Alpha < 0.5f && FMath::FRand() < 0.35f)
	{
		const FVector P = ReactorRel + FVector(-2.f, FMath::FRandRange(-22.f, 22.f), FMath::FRandRange(-25.f, 20.f));
		AArenaBlastFX::Spawn(GetWorld(), Frame.TransformPosition(P), FMath::FRandRange(4.f, 9.f) * SizeScale, WeldColor, 0.1f, false);
	}
	// Faceplate shut.
	if (!bFaceClosed && Alpha >= FaceEnd)
	{
		bFaceClosed = true;
		ArenaAudio::PlayAt(this, TEXT("suitup_helmet_close"), Frame.TransformPosition(FaceRel), 1.f);
		Shake(bShort ? 2.f : 3.5f);
	}
	// Eyes ignite + HUD boot.
	if (!bEyesLit && Alpha >= EyesAt)
	{
		bEyesLit = true;
		ArenaAudio::Play2D(this, TEXT("hud_beep_confirm"), 0.8f);
		AArenaBlastFX::Spawn(GetWorld(), Frame.TransformPosition(FaceRel + FVector(8.f, 0.f, 2.f)), 10.f * SizeScale, Suit.EmissiveColor, 0.15f, false);
	}
	// Real armour replaces the copies.
	if (!bBodyShown && Alpha >= RevealAt)
	{
		bBodyShown = true;
		DestroyFlyers();
		if (AIronManCharacter* P = Player.Get()) { P->SetBodyHiddenForCinematic(false); }
		if (PilotBody) { PilotBody->SetActorHiddenInGame(true); }
	}
}

void ASuitUpCinematic::UpdateCamera(float Alpha, float DeltaSeconds)
{
	if (!CineCam || Shots.Num() == 0) { return; }
	int32 Idx = 0;
	for (int32 i = 0; i < Shots.Num(); ++i) { if (Alpha >= Shots[i].Start) { Idx = i; } }
	const FCamShot& S = Shots[Idx];
	const float End = Shots.IsValidIndex(Idx + 1) ? Shots[Idx + 1].Start : 1.f;
	float T = FMath::Clamp((Alpha - S.Start) / FMath::Max(0.01f, End - S.Start), 0.f, 1.f);
	const FTransform Frame = FrameXf(Alpha, S.bWorld);
	FVector PosRel = FMath::Lerp(S.PosA, S.PosB, EaseInOut(T));
	if (S.OrbitA != S.OrbitB)
	{
		const float OT = S.Whip > 0.f ? EaseOutCubic(T / S.Whip) : EaseInOut(T);
		PosRel = FVector(PosRel.X, PosRel.Y, 0.f).RotateAngleAxis(FMath::Lerp(S.OrbitA, S.OrbitB, OT), FVector::UpVector) + FVector(0.f, 0.f, PosRel.Z);
	}
	const FVector LookRel = FMath::Lerp(S.LookA, S.LookB, EaseInOut(T));
	FVector CamLoc = Frame.TransformPosition(PosRel);
	FVector LookAt = (S.bWorld ? FrameXf(Alpha) : Frame).TransformPosition(LookRel);
	if (S.bWorld && Tpl == ETemplate::Air) { LookAt = FrameXf(Alpha).TransformPosition(FVector(0.f, 0.f, 0.55f * SuitH)); } // keep the falling pilot framed

	// Handheld shake (decays); Ultron glitch jumps.
	ShakeAmp = FMath::FInterpTo(ShakeAmp, 0.f, DeltaSeconds, 5.f);
	const float Tm = Elapsed;
	FVector Jit = FVector(FMath::Sin(Tm * 37.f), FMath::Sin(Tm * 29.f + 1.3f), FMath::Sin(Tm * 43.f + 2.1f)) * ShakeAmp;
	if (Tpl == ETemplate::Air && !bShort && Alpha < 0.74f) { Jit += FVector(FMath::Sin(Tm * 23.f), FMath::Sin(Tm * 19.f), 0.f) * 1.5f; } // wind buffet
	UCineCameraComponent* Cam = CineCam->GetCineCameraComponent();
	if (Style == ECinematicStyle::Ultron)
	{
		const bool bGlitch = FMath::FRand() < 0.15f;
		if (bGlitch) { Jit += FMath::VRand() * 10.f; }
		Cam->PostProcessSettings.SceneFringeIntensity = bGlitch ? FMath::FRandRange(3.f, 5.f) : 1.5f;
	}
	else if (Style == ECinematicStyle::Stealth)
	{
		Cam->PostProcessSettings.AutoExposureBias = FMath::Lerp(-2.f, -0.5f, Alpha);
	}
	CamLoc += Jit;
	CineCam->SetActorLocationAndRotation(CamLoc, (LookAt - CamLoc).Rotation());
	Cam->SetCurrentFocalLength(S.Focal);
	Cam->CurrentAperture = S.Focal >= 70.f ? 2.8f : 4.f;
	Cam->FocusSettings.ManualFocusDistance = FMath::Max(30.f, float(FVector::Dist(CamLoc, LookAt)));
	LastShot = Idx;
	LastCamLoc = CamLoc;
}

void ASuitUpCinematic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) { return; }
	if (!Player.IsValid()) { Finish(true); return; }

	Elapsed += DeltaSeconds;
	// Hit-stop on the chest clamp (0.25x for a moment), otherwise real time.
	const float Rate = HitStopLeft > 0.f ? 0.25f : 1.f;
	HitStopLeft = FMath::Max(0.f, HitStopLeft - DeltaSeconds);
	Clock += DeltaSeconds * Rate;
	const float Alpha = FMath::Clamp(Clock / FMath::Max(0.1f, Duration), 0.f, 1.f);

	TickFlyers(Alpha, DeltaSeconds);
	TickProps(Alpha, DeltaSeconds);
	TickBeats(Alpha);
	UpdateCamera(Alpha, DeltaSeconds);

	if (Overlay)
	{
		const float BarIn = FMath::Clamp(Elapsed / 0.4f, 0.f, 1.f);
		const float BarOut = FMath::Clamp((Duration - Clock) / 0.35f, 0.f, 1.f);
		const float TitleIn = FMath::Clamp((Alpha - (bShort ? 0.72f : RevealAt + 0.02f)) / 0.06f, 0.f, 1.f);
		const float TitleOut = FMath::Clamp((Duration - Clock) / 0.3f, 0.f, 1.f);
		Overlay->SetState(FMath::Min(BarIn, BarOut), FMath::Min(TitleIn, TitleOut), Style == ECinematicStyle::Ultron ? 1.f : 0.f);
		// HUD boot from the inside of the helmet: rises with the eyes, fades as the camera pulls out.
		const float HudLen = bShort ? 0.14f : 0.1f;
		const float H = Alpha < EyesAt ? 0.f : FMath::Clamp(1.f - (Alpha - EyesAt - HudLen * 0.5f) / (HudLen * 0.5f), 0.f, 1.f) * FMath::Clamp((Alpha - EyesAt) / 0.02f, 0.f, 1.f);
		Overlay->SetHudBoot(H, FText::GetEmpty(), USuitLibrary::GetSuit(SuitIndex).EmissiveColor);
	}

	if (Clock >= Duration) { Finish(false); }
}

void ASuitUpCinematic::Skip()
{
	if (!bFinished && Elapsed > 0.15f) { Finish(true); } // ignore the click that started the game
}

void ASuitUpCinematic::Abort()
{
	Finish(true);
}

void ASuitUpCinematic::DestroyFlyers()
{
	for (UStaticMeshComponent* C : FlyingParts) { if (C) { C->DestroyComponent(); } }
	FlyingParts.Empty();
	Flyers.Empty();
}

void ASuitUpCinematic::Finish(bool bSkipped)
{
	if (bFinished) { return; }
	bFinished = true;
	DestroyFlyers();
	for (UStaticMeshComponent* C : Props) { if (C) { C->DestroyComponent(); } }
	Props.Empty();
	Thrusters.Empty();
	Arms.Empty();
	if (CaseHinge) { CaseHinge->DestroyComponent(); CaseHinge = nullptr; }
	CaseProp = nullptr; CaseLid = nullptr;
	if (PilotBody) { PilotBody->Destroy(); PilotBody = nullptr; }

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
		if (PilotBody) { PilotBody->Destroy(); PilotBody = nullptr; }
		if (Overlay) { Overlay->RemoveFromParent(); Overlay = nullptr; }
		if (AIronManCharacter* P = Player.Get()) { P->SetCinematicMode(false, nullptr); }
	}
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
