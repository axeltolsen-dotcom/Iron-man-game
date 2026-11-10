#include "IronManCharacter.h"
#include "SuitSuperComponent.h"
#include "CareerSubsystem.h"
#include "ArenaAudio.h"
#include "ArenaCombat.h"
#include "CityLife.h"
#include "Components/AudioComponent.h"
#include "ArenaFeel.h"
#include "HUDDataSubsystem.h"
#include "IronManArena.h"
#include "IronProjectile.h"
#include "EnemyBase.h"
#include "ArenaGameMode.h"
#include "ArenaPlayerController.h"
#include "ArenaBlastFX.h"
#include "JarvisComponent.h"
#include "SuitUpCinematic.h"
#include "ReactorComponent.h"
#include "ArenaSaveGame.h"
#include "SuitArmorKit.h"
#include "SuitRig.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

#define LOCTEXT_NAMESPACE "IronManCharacter"

namespace SuitLightLumens
{
	// Physical light output (lumens) of the two suit point lights; emissive surfaces carry the visible glow.
	constexpr float Reactor = 300.f;
	constexpr float PalmFlash = 400.f;
}

// =====================================================================================
// Construction: camera rig + placeholder suit
// =====================================================================================

AIronManCharacter::AIronManCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(BaseCapsuleRadius, BaseCapsuleHalfHeight);
	// Enemy aim traces use Visibility; make sure the player blocks it too.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Shooter-style: the body always faces where the camera looks.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxFlySpeed = FlySpeed;
	Move->JumpZVelocity = JumpVelocity;
	Move->AirControl = 0.6f;
	Move->BrakingDecelerationFlying = FlightBraking;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->MaxAcceleration = 4000.f;
	Move->GravityScale = 1.6f;

	// --- Camera ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->SocketOffset = CameraShoulderOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 14.f;
	CameraBoom->ProbeSize = 16.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(85.f); // third person = first-person FOV - 10 (Vison camera_notes #7: 85 deg at 95)

	// The inherited skeletal Mesh stays empty; assign one in a Blueprint child to replace the shapes.
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -BaseCapsuleHalfHeight));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // UE mannequin convention

	// --- Placeholder suit (feet at SuitRoot Z = 0; all sizes in cm, basic shape = 100 cm) ---
	SuitRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SuitRoot"));
	SuitRoot->SetupAttachment(GetCapsuleComponent());
	SuitRoot->SetRelativeLocation(FVector(0.f, 0.f, -BaseCapsuleHalfHeight));

	// Body + every part variant (helmets, reactors, shoulders, backpacks, wings, Hulkbuster plates).
	SuitRig::Build(this, SuitRoot, Rig, Parts);
	ShoulderL = Rig.ShoulderL;
	ShoulderR = Rig.ShoulderR;
	HipL = Rig.HipL;
	HipR = Rig.HipR;
	PalmL = Rig.PalmL;
	PalmR = Rig.PalmR;
	ArcReactor = Rig.ArcReactor;
	FlameBootL = Rig.FlameBootL;
	FlameBootR = Rig.FlameBootR;
	FlameHandL = Rig.FlameHandL;
	FlameHandR = Rig.FlameHandR;
	HulkbusterPlates = Rig.HulkbusterPlates;

	// --- First-person camera inside the helmet + gauntlets in view ---
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(12.f, 0.f, FirstPersonEyeHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(FirstPersonFOV);
	// Helmet view: EDITH's visor already adds barrel warp, a 1.5 px chromatic split, 0.02 noise and a warning
	// vignette, so the scene versions are off here (no doubling), grain is halved, and there is no motion blur
	// (camera decision 6). Camera post-process blends over the environment volume.
	{
		FPostProcessSettings& PP = FirstPersonCamera->PostProcessSettings;
		PP.bOverride_VignetteIntensity = true;   PP.VignetteIntensity = 0.f;
		PP.bOverride_SceneFringeIntensity = true; PP.SceneFringeIntensity = 0.f;
		PP.bOverride_FilmGrainIntensity = true;  PP.FilmGrainIntensity = 0.015f; // half the 0.03 gameplay grain
		PP.bOverride_MotionBlurAmount = true;    PP.MotionBlurAmount = 0.f;
		FirstPersonCamera->PostProcessBlendWeight = 1.f;
	}

	SuitRig::BuildGauntlets(this, FirstPersonCamera, Rig, FPParts);
	for (FArenaPaintedPart& P : FPParts)
	{
		P.Component->SetOnlyOwnerSee(true);
		P.Component->CastShadow = false;
	}

	FPPalmLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FPPalmLight"));
	FPPalmLight->SetupAttachment(Rig.FPArmR);
	FPPalmLight->SetRelativeLocation(FVector(50.f, 0.f, 6.f));
	FPPalmLight->SetIntensityUnits(ELightUnits::Lumens);
	FPPalmLight->SetIntensity(0.f);
	FPPalmLight->SetAttenuationRadius(250.f);
	FPPalmLight->SetSourceRadius(2.f);
	FPPalmLight->SetCastShadows(false);

	// --- Shield special: six glowing orbs orbiting the suit (hidden until used) ---
	ShieldRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShieldRoot"));
	ShieldRoot->SetupAttachment(GetCapsuleComponent());
	for (int32 i = 0; i < 6; ++i)
	{
		UStaticMeshComponent* Orb = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ShieldOrb%d"), i));
		Orb->SetupAttachment(ShieldRoot);
		Orb->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Sphere));
		const float A = i * 60.f;
		Orb->SetRelativeLocation(FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 110.f, FMath::Sin(FMath::DegreesToRadians(A)) * 110.f, (i % 2 ? 30.f : -30.f)));
		Orb->SetRelativeScale3D(FVector(0.16f));
		Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Orb->CastShadow = false;
		Orb->SetOwnerNoSee(true);
		Orb->SetVisibility(false);
		ShieldOrbs.Add(Orb);
	}

	QuickSlotSuitIds = { TEXT("mark-iii"), TEXT("mark-xlii"), TEXT("mark-l"), TEXT("war-machine"), TEXT("ultron") };

	// Lights
	ArcLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ArcLight"));
	ArcLight->SetupAttachment(ArcReactor);
	// Suit light budget (Vison suits #4): two point lights per suit - the reactor and ONE palm slot, which is a
	// pool of two components (third-person RepulsorLight / first-person FPPalmLight); only the one for the current
	// view is visible (the other is out of the renderer). Real-world scale: reactor <= 300 lm, palm flash 400 lm,
	// 2-4 cm sources, no shadows (High and below; Epic too - the emissive and Lumen carry the look).
	ArcLight->SetLightColor(ArenaVisuals::ArcBlue);
	ArcLight->SetIntensityUnits(ELightUnits::Lumens);
	ArcLight->SetIntensity(SuitLightLumens::Reactor);
	ArcLight->SetAttenuationRadius(300.f);
	ArcLight->SetSourceRadius(4.f);
	ArcLight->SetCastShadows(false);

	RepulsorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RepulsorLight"));
	RepulsorLight->SetupAttachment(PalmR);
	RepulsorLight->SetLightColor(ArenaVisuals::ArcBlue);
	RepulsorLight->SetIntensityUnits(ELightUnits::Lumens);
	RepulsorLight->SetIntensity(0.f);
	RepulsorLight->SetAttenuationRadius(400.f);
	RepulsorLight->SetSourceRadius(3.f);
	RepulsorLight->SetCastShadows(false);

	// Chest beam (attached to the capsule so the Hulkbuster scale doesn't stretch it).
	BeamPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamPivot"));
	BeamPivot->SetupAttachment(GetCapsuleComponent());
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(BeamPivot);
	BeamMesh->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
	BeamMesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // cylinder Z -> beam X
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->CastShadow = false;
	BeamMesh->SetVisibility(false);

	Reactor = CreateDefaultSubobject<UReactorComponent>(TEXT("Reactor"));
}

// =====================================================================================
// BeginPlay / reset
// =====================================================================================

namespace
{
	const FLinearColor CloakColor(0.015f, 0.02f, 0.03f);

	FLinearColor PrimaryWeaponColor(ESuitPrimaryWeapon W, const FLinearColor& Emissive)
	{
		switch (W)
		{
		case ESuitPrimaryWeapon::Sonic:        return FLinearColor(0.8f, 0.9f, 1.f);
		case ESuitPrimaryWeapon::Flamethrower: return ArenaVisuals::Thruster;
		case ESuitPrimaryWeapon::Frost:        return FLinearColor(0.6f, 0.92f, 1.f);
		case ESuitPrimaryWeapon::Minigun:      return FLinearColor(1.f, 0.8f, 0.3f);
		case ESuitPrimaryWeapon::RedBlast:     return FLinearColor(1.f, 0.08f, 0.04f);
		case ESuitPrimaryWeapon::Laser:        return FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.35f, 0.25f), Emissive, 0.35f);
		default:
			// Mark-style cyan-white core from reactor/suit emissive (not a soft orb hue).
			return FLinearColor::LerpUsingHSV(FLinearColor(0.82f, 0.96f, 1.f), Emissive, 0.4f);
		}
	}
}

void AIronManCharacter::BeginPlay()
{
	Super::BeginPlay();

	ArenaVisuals::PaintAll(this, Parts);
	ArenaVisuals::PaintAll(this, FPParts);
	BeamMID = ArenaVisuals::PaintComponent(this, BeamMesh, FLinearColor(0.6f, 0.95f, 1.f), 30.f);
	ShieldMIDs.Reset();
	for (UStaticMeshComponent* Orb : ShieldOrbs)
	{
		ShieldMIDs.Add(ArenaVisuals::PaintComponent(this, Orb, ArenaVisuals::ArcBlue, 25.f));
	}

	BaseCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	BaseCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SpawnTransform = GetActorTransform();
	FloorZAtSpawn = GetActorLocation().Z - BaseCapsuleHalfHeight;

	const int32 Default = USuitLibrary::FindSuitIndex(DefaultSuitId);
	Reactor->OnOverheatChanged.AddUniqueDynamic(this, &AIronManCharacter::HandleReactorOverheat);
	ApplySuit(Default != INDEX_NONE ? Default : 0);
	ApplyRewards();
	Health = MaxHealth;
	Energy = MaxEnergy;
	SetFirstPerson(bStartInFirstPerson);
	SetFlying(true); // reference game starts airborne
}

bool AIronManCharacter::HasPlaidDecal() const
{
	return PlaidDecal != nullptr && PlaidDecal->IsVisible();
}

void AIronManCharacter::ApplyRewards()
{
	const AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
	const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(GM ? GM->SaveSlotName : FString(TEXT("IronManArena")));
	if (!Save) { return; }

	if (Reactor) { Reactor->SetPermanentUpgrades(Save->HasUpgrade(ArenaRewards::CapacitorBank()), Save->HasUpgrade(ArenaRewards::HeatSinkFins())); }
	SuitRig::SetWhiteTrim(Save->IsCosmeticActive(ArenaRewards::WhiteTrim()));
	SuitRig::SetPaleBlueTrim(Save->IsCosmeticActive(ArenaRewards::PaleBlueTrim()));

	const bool bPlaid = Save->IsCosmeticActive(ArenaRewards::PlaidDecal());
	if (bPlaid && !PlaidDecal)
	{
		// Built by Content/Python/build_realism_materials.py; no material = no patch (logged once).
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Realism/Materials/M_PlaidDecal.M_PlaidDecal")))
		{
			PlaidDecal = NewObject<UDecalComponent>(this, TEXT("PlaidDecal"));
			PlaidDecal->SetupAttachment(GetRootComponent());
			PlaidDecal->SetDecalMaterial(M);
			PlaidDecal->DecalSize = FVector(6.f, 7.f, 7.f);
			// Left shoulder plate, projected inward (+Y) from just outside the arm.
			PlaidDecal->SetRelativeLocationAndRotation(FVector(0.f, -30.f, 52.f), FRotator(0.f, 90.f, 0.f));
			PlaidDecal->RegisterComponent();
		}
		else
		{
			UE_LOG(LogIronManArena, Log, TEXT("Rewards: garage-plaid-decal earned but M_PlaidDecal is not built yet"));
		}
	}
	bPlaidWanted = bPlaid;
	if (PlaidDecal) { PlaidDecal->SetVisibility(bPlaid && !IsFirstPerson()); }

	// Repaint (trim) and recompute max energy (capacitor bank) for the current suit.
	ApplySuit(GetSuitIndex());
}

void AIronManCharacter::SetPlayArea(const FTransform& StartTransform, const FVector& AreaCenter, float AreaRadius, float AreaCeilingZ)
{
	bPlayAreaOverride = true;
	PlayAreaStart = StartTransform;
	PlayAreaCenter = AreaCenter;
	PlayAreaRadius = FMath::Max(AreaRadius, ArenaBoundaryRadius);
	PlayAreaCeilingZ = AreaCeilingZ;
}

void AIronManCharacter::ClearPlayArea()
{
	bPlayAreaOverride = false;
}

void AIronManCharacter::ResetForNewGame()
{
	TrySkipCinematic(); // B08: don't leak suit-up state into the next run
	CloseSuitWheel(false);
	if (IsHulkbusterActive()) { RevertHulkbuster(); }
	HulkbusterTimeLeft = 0.f;
	HulkbusterCharges = 0;
	bDead = false;
	bFlightLocked = false;
	bFireHeld = false;
	InvulnerableLeft = 1.f;
	BeamTimeLeft = 0.f;
	bBeamWhip = false;
	PunchTimeLeft = 0.f;
	OverdriveLeft = 0.f;
	ShieldLeft = 0.f;
	CloakLeft = 0.f;
	SwapCooldownLeft = 0.f;
	bBoostHeld = false;
	bBoostActive = false;
	Reactor->ResetReactor();
	bWarnedLowEnergy = false;
	bWarnedLowHealth = false;
	BeamMesh->SetVisibility(false);
	BeamDamageAccum.Empty();
	for (UStaticMeshComponent* Orb : ShieldOrbs) { Orb->SetVisibility(false); }

	const FTransform& StartAt = bPlayAreaOverride ? PlayAreaStart : SpawnTransform;
	SetActorTransform(StartAt, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	SetActorEnableCollision(true);

	ApplySuit(SuitIndex); // stats, colours, visibility
	Health = MaxHealth;
	Energy = MaxEnergy;
	SetFlying(true);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(StartAt.Rotator());
	}
}

// =====================================================================================
// Suits
// =====================================================================================

void AIronManCharacter::ApplySuit(int32 Index)
{
	const int32 Num = USuitLibrary::GetNumSuits();
	SuitIndex = Num > 0 ? FMath::Clamp(Index, 0, Num - 1) : 0;
	// Career: shop upgrades into the tunables, avatar paint / helmet / armour silhouette into the drawn suit.
	SuitLook = USuitLibrary::GetSuit(SuitIndex);
	bSuitLookOverride = UCareerSubsystem::ApplyToCharacter(this, SuitLook);
	const FSuitDef& S = GetSuit();

	// U's armour kit: real plates replace the placeholder shapes (runtime meshes, cached per suit).
	if (GetWorld() && GetWorld()->IsGameWorld() && SuitArmorKit::IsLoaded())
	{
		SuitArmorKit::Build(this, S, Rig, Parts, GetMesh());
	}
	SuitRig::ApplyColors(S, Parts);
	SuitRig::ApplyColors(S, FPParts);
	if (CloakLeft > 0.f) { ArenaVisuals::FlashAll(Parts, CloakColor, false); }

	// Stats (keep the current fill fraction across a swap).
	const float HFrac = MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 1.f;
	const float EFrac = MaxEnergy > 0.f ? FMath::Clamp(Energy / MaxEnergy, 0.f, 1.f) : 1.f;
	MaxHealth = FMath::Max(1.f, BaseMaxHealth * S.Armor);
	Reactor->SetMountedUnit(Reactor->HasStage() || S.Reactor == ESuitReactor::Mounted);
	MaxEnergy = FMath::Max(10.f, S.MaxEnergy * Reactor->GetCapacityMultiplier());
	Health = bDead ? 0.f : MaxHealth * HFrac;
	Energy = MaxEnergy * EFrac;
	GroundEnergyRegen = S.Regen;
	SuitSpeedMul = FMath::Max(0.3f, S.Speed);
	SuitDamageMul = FMath::Max(0.1f, S.WeaponDamage);

	// Lights / beam follow the suit's emissive colour (Ultron: red unibeam).
	ArcLight->SetLightColor(S.EmissiveColor);
	RepulsorLight->SetLightColor(PrimaryWeaponColor(S.PrimaryWeapon, S.EmissiveColor));
	FPPalmLight->SetLightColor(PrimaryWeaponColor(S.PrimaryWeapon, S.EmissiveColor));
	if (BeamMID) { BeamMID->SetVectorParameterValue(TEXT("Color"), S.EmissiveColor); }
	for (UMaterialInstanceDynamic* MID : ShieldMIDs)
	{
		if (MID) { MID->SetVectorParameterValue(TEXT("Color"), S.EmissiveColor); }
	}

	UpdateSuitRootTransform();
	RefreshSuitVisibility();
	ApplySuitMovementSettings();
}

bool AIronManCharacter::RequestSuitSwap(int32 Index)
{
	if (bDead || bInCinematic || SwapCooldownLeft > 0.f) { return false; }
	if (!USuitLibrary::GetAllSuits().IsValidIndex(Index) || Index == SuitIndex) { return false; }
	if (!IsSuitAllowed(Index))
	{
		Banner(LOCTEXT("SuitLocked", "SUIT LOCKED"), LOCTEXT("SuitLockedSub", "NOT BUILT YET"), 1.2f);
		return false;
	}
	if (UGameplayStatics::IsGamePaused(this)) { return false; }

	SwapCooldownLeft = SuitSwapCooldown;
	if (!ASuitUpCinematic::Play(this, Index, /*bShortVersion*/ true))
	{
		// Fallback without the cinematic.
		ApplySuit(Index);
		if (UJarvisComponent* J = UJarvisComponent::Get(this)) { J->SetPersona(GetSuit().AI, true); }
	}
	return true;
}

void AIronManCharacter::SetCinematicMode(bool bEnable, ASuitUpCinematic* Cinematic)
{
	bInCinematic = bEnable;
	ActiveCinematic = Cinematic;
	if (!bEnable) { bCinematicHideBody = false; }
	if (bEnable)
	{
		CloseSuitWheel(false);
		bFireHeld = false;
		BeamTimeLeft = 0.f;
		PunchTimeLeft = 0.f;
		BeamMesh->SetVisibility(false);
		GetCharacterMovement()->StopMovementImmediately();
	}
	else
	{
		InvulnerableLeft = FMath::Max(InvulnerableLeft, 0.75f);
	}
	RefreshSuitVisibility();
}

bool AIronManCharacter::TrySkipCinematic()
{
	if (!bInCinematic) { return false; }
	// Abort (not Skip): Skip ignores the first 0.15 s, which left B08 leaks on early Quit/Restart.
	if (ASuitUpCinematic* C = ActiveCinematic.Get()) { C->Abort(); }
	else if (CinematicSkipHandler) { CinematicSkipHandler(false); }
	return true;
}

bool AIronManCharacter::ShowsMountedReactor() const
{
	return (Reactor && Reactor->HasStage()) || GetSuit().Reactor == ESuitReactor::Mounted;
}

void AIronManCharacter::SetAllowedSuits(const TArray<FName>& SuitIds)
{
	SelectableSuits.Reset();
	const int32 Num = USuitLibrary::GetNumSuits();
	if (SuitIds.Num() == 0)
	{
		for (int32 i = 0; i < Num; ++i) { SelectableSuits.Add(i); }
	}
	else
	{
		for (const FName& Id : SuitIds)
		{
			const int32 Idx = USuitLibrary::FindSuitIndex(Id);
			if (Idx != INDEX_NONE) { SelectableSuits.AddUnique(Idx); }
		}
	}
	WheelPage = FMath::Clamp(WheelPage, 0, GetWheelPageCount() - 1);
}

bool AIronManCharacter::IsSuitAllowed(int32 Index) const
{
	return SelectableSuits.Num() == 0 || SelectableSuits.Contains(Index);
}

void AIronManCharacter::RefreshSuitVisibility()
{
	const bool bBody = bUsePlaceholderVisuals && !bDead && !bCinematicHideBody;
	SuitRig::ApplyVisibility(GetSuit(), Rig, bBody, IsHulkbusterActive());
	SuitRig::ApplyMountedReactor(Rig, ShowsMountedReactor(), Reactor && Reactor->HasStage() && Reactor->GetMount() == EReactorMount::Forearm);

	// First person: the owner doesn't see the body (other views and shadows still do).
	const bool bHideBodyForOwner = bFirstPerson && !bInCinematic;
	for (FArenaPaintedPart& P : Parts)
	{
		if (P.Component) { P.Component->SetOwnerNoSee(bHideBodyForOwner); }
	}

	const bool bFP = bBody && bFirstPerson && !bInCinematic;
	if (Rig.FPRoot) { Rig.FPRoot->SetVisibility(bFP, true); }
	const bool bFists = bFP && IsHulkbusterActive();
	if (Rig.FPBusterL) { Rig.FPBusterL->SetVisibility(bFists, true); }
	if (Rig.FPBusterR) { Rig.FPBusterR->SetVisibility(bFists, true); }
	if (Rig.FPRoot) { Rig.FPRoot->SetRelativeScale3D(FVector(IsHulkbusterActive() ? 1.25f : 1.f)); }
	// Makeshift mounted reactor: forearm readout in first person (hidden under the Hulkbuster fists).
	if (Rig.FPMountedReactor)
	{
		Rig.FPMountedReactor->SetVisibility(bFP && !IsHulkbusterActive() && ShowsMountedReactor(), true);
	}
}

void AIronManCharacter::SetCameraFOV(float NewFOV)
{
	FirstPersonFOV = FMath::Clamp(NewFOV, 60.f, 120.f);
	FollowCamera->SetFieldOfView(FirstPersonFOV - 10.f); // 95 -> 85 deg third person (less edge stretch on a 4.2 m chase)
	UpdateCameraForSuit();
}

void AIronManCharacter::UpdateSuitRootTransform()
{
	const float P = SuitRig::PlatingScale(GetSuit().Plating);
	const float H = IsHulkbusterActive() ? HulkbusterScale : 1.f;
	const float Half = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SuitRoot->SetRelativeLocation(FVector(0.f, 0.f, -Half));
	SuitRoot->SetRelativeScale3D(FVector(P * H, P * H, H) * CareerBodyScale);
	UpdateCameraForSuit();
}

void AIronManCharacter::UpdateCameraForSuit()
{
	const float H = IsHulkbusterActive() ? HulkbusterScale : 1.f;
	FirstPersonCamera->SetRelativeLocation(FVector(12.f * H, 0.f, FirstPersonEyeHeight * H));
	FirstPersonCamera->SetFieldOfView(FirstPersonFOV);
}

void AIronManCharacter::SetFirstPerson(bool bEnable)
{
	bFirstPerson = bEnable;
	FirstPersonCamera->SetActive(bEnable);
	FollowCamera->SetActive(!bEnable);
	RefreshSuitVisibility();
	if (PlaidDecal) { PlaidDecal->SetVisibility(bPlaidWanted && !bEnable && !IsHulkbusterActive()); }
}

UCameraComponent* AIronManCharacter::GetActiveCamera() const
{
	return bFirstPerson ? FirstPersonCamera.Get() : FollowCamera.Get();
}

int32 AIronManCharacter::GetWheelPageCount() const
{
	const int32 Count = SelectableSuits.Num() > 0 ? SelectableSuits.Num() : USuitLibrary::GetNumSuits();
	return FMath::Max(1, FMath::DivideAndRoundUp(Count, WheelSlots));
}

int32 AIronManCharacter::GetWheelSuit(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= WheelSlots) { return INDEX_NONE; }
	const int32 Idx = WheelPage * WheelSlots + SlotIndex;
	if (SelectableSuits.Num() > 0) { return SelectableSuits.IsValidIndex(Idx) ? SelectableSuits[Idx] : INDEX_NONE; }
	return Idx < USuitLibrary::GetNumSuits() ? Idx : INDEX_NONE;
}

int32 AIronManCharacter::GetQuickSlotSuit(int32 SlotIndex) const
{
	if (SelectableSuits.Num() > 0 && SelectableSuits.Num() < USuitLibrary::GetNumSuits())
	{
		// Story: 1-5 pick from the unlocked suits, newest last.
		return SelectableSuits.IsValidIndex(SlotIndex) ? SelectableSuits[SlotIndex] : INDEX_NONE;
	}
	return QuickSlotSuitIds.IsValidIndex(SlotIndex) ? USuitLibrary::FindSuitIndex(QuickSlotSuitIds[SlotIndex]) : INDEX_NONE;
}

float AIronManCharacter::GetSpecialCost() const
{
	if (IsHulkbusterActive()) { return BeamEnergyCost; }
	switch (GetSuit().SpecialWeapon)
	{
	case ESuitSpecialWeapon::Missiles:   return 25.f;
	case ESuitSpecialWeapon::Overdrive:  return 30.f;
	case ESuitSpecialWeapon::Shield:     return 25.f;
	case ESuitSpecialWeapon::DroneSwarm: return 30.f;
	case ESuitSpecialWeapon::NanoBlades: return 15.f;
	case ESuitSpecialWeapon::Cloak:      return 25.f;
	case ESuitSpecialWeapon::LaserWhip:  return 20.f;
	default:                             return BeamEnergyCost;
	}
}

FText AIronManCharacter::GetSpecialName() const
{
	if (IsHulkbusterActive()) { return LOCTEXT("BusterBeam", "CHEST BEAM"); }
	return UEnum::GetDisplayValueAsText(GetSuit().SpecialWeapon).ToUpper();
}

FText AIronManCharacter::GetPrimaryName() const
{
	if (IsHulkbusterActive()) { return LOCTEXT("BusterPunch", "PUNCH"); }
	return UEnum::GetDisplayValueAsText(GetSuit().PrimaryWeapon).ToUpper();
}

float AIronManCharacter::GetActiveEffectTime(FText& OutName) const
{
	if (ShieldLeft > 0.f)    { OutName = LOCTEXT("FxShield", "SHIELD"); return ShieldLeft; }
	if (CloakLeft > 0.f)     { OutName = LOCTEXT("FxCloak", "CLOAKED"); return CloakLeft; }
	if (OverdriveLeft > 0.f) { OutName = LOCTEXT("FxOverdrive", "OVERDRIVE"); return OverdriveLeft; }
	OutName = FText::GetEmpty();
	return 0.f;
}

void AIronManCharacter::NotifyAI(uint8 Event) const
{
	if (Event < static_cast<uint8>(EAIEvent::Count))
	{
		UJarvisComponent::Notify(this, static_cast<EAIEvent>(Event));
	}
}

// =====================================================================================
// Input (Enhanced Input, built at runtime)
// =====================================================================================

void AIronManCharacter::CreateDefaultInputIfNeeded()
{
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type, bool bWhenPaused = false)
	{
		UInputAction* A = NewObject<UInputAction>(this, FName(Name));
		A->ValueType = Type;
		A->bTriggerWhenPaused = bWhenPaused;
		return A;
	};

	if (!MoveAction)         { MoveAction         = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D); }
	if (!LookAction)         { LookAction         = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D); }
	if (!AscendAction)       { AscendAction       = MakeAction(TEXT("IA_Ascend"), EInputActionValueType::Axis1D); }
	if (!DescendAction)      { DescendAction      = MakeAction(TEXT("IA_Descend"), EInputActionValueType::Axis1D); }
	if (!ToggleFlightAction) { ToggleFlightAction = MakeAction(TEXT("IA_ToggleFlight"), EInputActionValueType::Boolean); }
	if (!FireAction)         { FireAction         = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean); }
	if (!BeamAction)         { BeamAction         = MakeAction(TEXT("IA_Special"), EInputActionValueType::Boolean); }
	if (!HulkbusterAction)   { HulkbusterAction   = MakeAction(TEXT("IA_Hulkbuster"), EInputActionValueType::Boolean); }
	if (!PauseAction)        { PauseAction        = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean, true); }
	if (!ToggleViewAction)   { ToggleViewAction   = MakeAction(TEXT("IA_ToggleView"), EInputActionValueType::Boolean); }
	if (!SuitWheelAction)    { SuitWheelAction    = MakeAction(TEXT("IA_SuitWheel"), EInputActionValueType::Boolean); }
	if (!WheelPageAction)    { WheelPageAction    = MakeAction(TEXT("IA_WheelPage"), EInputActionValueType::Axis1D); }
	if (!BoostAction)        { BoostAction        = MakeAction(TEXT("IA_Boost"), EInputActionValueType::Boolean); }
	if (!VentAction)         { VentAction         = MakeAction(TEXT("IA_Vent"), EInputActionValueType::Boolean); }
	if (!AllyAction)         { AllyAction         = MakeAction(TEXT("IA_Ally"), EInputActionValueType::Boolean); }
	while (QuickSlotActions.Num() < 5)
	{
		const FString Name = FString::Printf(TEXT("IA_QuickSlot%d"), QuickSlotActions.Num() + 1);
		QuickSlotActions.Add(MakeAction(*Name, EInputActionValueType::Boolean));
	}

	if (DefaultMappingContext) { return; }

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_IronMan"));

	// Helpers that add modifiers to a fresh mapping.
	auto Swizzle = [IMC](FEnhancedActionKeyMapping& M)
	{
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(IMC);
		S->Order = EInputAxisSwizzle::YXZ; // put the 1D key value on the Y axis (forward)
		M.Modifiers.Add(S);
	};
	auto Negate = [IMC](FEnhancedActionKeyMapping& M)
	{
		M.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
	};

	// Move: value.X = right, value.Y = forward.
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(MoveAction, EKeys::W); Swizzle(M); }
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(MoveAction, EKeys::S); Swizzle(M); Negate(M); }
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(MoveAction, EKeys::A); Negate(M); }
	IMC->MapKey(MoveAction, EKeys::D);
	IMC->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	// Look: mouse delta (X right, Y up). Gamepad stick scaled to degrees/second.
	IMC->MapKey(LookAction, EKeys::Mouse2D);
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(LookAction, EKeys::Gamepad_Right2D);
		UInputModifierScalar* Scale = NewObject<UInputModifierScalar>(IMC);
		Scale->Scalar = FVector(160.f, 110.f, 1.f);
		M.Modifiers.Add(Scale);
		M.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(IMC));
	}

	IMC->MapKey(AscendAction, EKeys::SpaceBar);
	IMC->MapKey(AscendAction, EKeys::Gamepad_FaceButton_Bottom);
	IMC->MapKey(DescendAction, EKeys::LeftControl);
	IMC->MapKey(DescendAction, EKeys::C);
	IMC->MapKey(DescendAction, EKeys::Gamepad_FaceButton_Right);
	IMC->MapKey(ToggleFlightAction, EKeys::F);
	IMC->MapKey(ToggleFlightAction, EKeys::Gamepad_FaceButton_Top);
	IMC->MapKey(FireAction, EKeys::LeftMouseButton);
	IMC->MapKey(FireAction, EKeys::Gamepad_RightTrigger);
	IMC->MapKey(BeamAction, EKeys::RightMouseButton);
	IMC->MapKey(BeamAction, EKeys::Gamepad_LeftTrigger);
	IMC->MapKey(HulkbusterAction, EKeys::Q);
	IMC->MapKey(HulkbusterAction, EKeys::Gamepad_FaceButton_Left);
	IMC->MapKey(PauseAction, EKeys::P);
	IMC->MapKey(PauseAction, EKeys::Escape); // in PIE, Esc also ends the session; use P there
	IMC->MapKey(PauseAction, EKeys::Gamepad_Special_Right);
	IMC->MapKey(ToggleViewAction, EKeys::V);
	IMC->MapKey(ToggleViewAction, EKeys::Gamepad_RightThumbstick);
	IMC->MapKey(SuitWheelAction, EKeys::T);
	IMC->MapKey(SuitWheelAction, EKeys::Gamepad_LeftShoulder);
	IMC->MapKey(WheelPageAction, EKeys::MouseWheelAxis);
	IMC->MapKey(WheelPageAction, EKeys::Tab);
	IMC->MapKey(WheelPageAction, EKeys::Gamepad_RightShoulder);
	IMC->MapKey(BoostAction, EKeys::LeftShift);
	IMC->MapKey(BoostAction, EKeys::Gamepad_LeftThumbstick);
	IMC->MapKey(VentAction, EKeys::X);
	IMC->MapKey(VentAction, EKeys::Gamepad_DPad_Down);
	IMC->MapKey(AllyAction, EKeys::G);
	IMC->MapKey(AllyAction, EKeys::Gamepad_DPad_Up);
	const FKey SlotKeys[5] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	for (int32 i = 0; i < 5; ++i)
	{
		IMC->MapKey(QuickSlotActions[i], SlotKeys[i]);
	}

	DefaultMappingContext = IMC;
}

void AIronManCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateDefaultInputIfNeeded();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogIronManArena, Error, TEXT("Enhanced Input component missing: check Config/DefaultInput.ini (DefaultInputComponentClass)."));
		return;
	}

	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AIronManCharacter::Input_Move);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AIronManCharacter::Input_Look);
	EIC->BindAction(AscendAction, ETriggerEvent::Triggered, this, &AIronManCharacter::Input_Ascend);
	EIC->BindAction(AscendAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_AscendStarted);
	EIC->BindAction(AscendAction, ETriggerEvent::Completed, this, &AIronManCharacter::Input_VertReleased);
	EIC->BindAction(AscendAction, ETriggerEvent::Canceled, this, &AIronManCharacter::Input_VertReleased);
	EIC->BindAction(DescendAction, ETriggerEvent::Triggered, this, &AIronManCharacter::Input_Descend);
	EIC->BindAction(DescendAction, ETriggerEvent::Completed, this, &AIronManCharacter::Input_VertReleased);
	EIC->BindAction(DescendAction, ETriggerEvent::Canceled, this, &AIronManCharacter::Input_VertReleased);
	EIC->BindAction(ToggleFlightAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_ToggleFlight);
	EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_FireStarted);
	EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AIronManCharacter::Input_FireCompleted);
	EIC->BindAction(FireAction, ETriggerEvent::Canceled, this, &AIronManCharacter::Input_FireCompleted);
	EIC->BindAction(BeamAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_Beam);
	EIC->BindAction(HulkbusterAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_Hulkbuster);
	EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_Pause);
	EIC->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_ToggleView);
	EIC->BindAction(SuitWheelAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_WheelOpen);
	EIC->BindAction(SuitWheelAction, ETriggerEvent::Completed, this, &AIronManCharacter::Input_WheelClose);
	EIC->BindAction(SuitWheelAction, ETriggerEvent::Canceled, this, &AIronManCharacter::Input_WheelClose);
	EIC->BindAction(WheelPageAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_WheelPage);
	EIC->BindAction(QuickSlotActions[0], ETriggerEvent::Started, this, &AIronManCharacter::Input_Slot1);
	EIC->BindAction(QuickSlotActions[1], ETriggerEvent::Started, this, &AIronManCharacter::Input_Slot2);
	EIC->BindAction(QuickSlotActions[2], ETriggerEvent::Started, this, &AIronManCharacter::Input_Slot3);
	EIC->BindAction(QuickSlotActions[3], ETriggerEvent::Started, this, &AIronManCharacter::Input_Slot4);
	EIC->BindAction(QuickSlotActions[4], ETriggerEvent::Started, this, &AIronManCharacter::Input_Slot5);
	EIC->BindAction(BoostAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_BoostStarted);
	EIC->BindAction(BoostAction, ETriggerEvent::Completed, this, &AIronManCharacter::Input_BoostCompleted);
	EIC->BindAction(BoostAction, ETriggerEvent::Canceled, this, &AIronManCharacter::Input_BoostCompleted);
	EIC->BindAction(VentAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_Vent);
	EIC->BindAction(AllyAction, ETriggerEvent::Started, this, &AIronManCharacter::Input_Ally);
}

void AIronManCharacter::Input_Move(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || !Controller) { return; }
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Mul = IsBeamActive() ? BeamMoveMultiplier : (PunchTimeLeft > 0.f ? 0.3f : 1.f);

	if (IsFlying()) { FlightMoveInput = Axis * Mul; return; } // force-based flight reads it in TickFlightPhysics
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	AddMovementInput(Forward, Axis.Y * Mul);
	AddMovementInput(Right, Axis.X * Mul);
}

void AIronManCharacter::Input_Look(const FInputActionValue& Value)
{
	if (bInCinematic) { return; }
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) { return; }

	if (bWheelOpen)
	{
		// The mouse drives the wheel cursor instead of the camera.
		WheelCursor += Value.Get<FVector2D>() * 0.03f;
		if (WheelCursor.Size() > 1.f) { WheelCursor = WheelCursor.GetSafeNormal(); }
		WheelSelection = INDEX_NONE;
		if (WheelCursor.Size() > 0.3f)
		{
			// 0 = up, clockwise (X right, Y up).
			float Deg = FMath::RadiansToDegrees(FMath::Atan2(WheelCursor.X, WheelCursor.Y));
			if (Deg < 0.f) { Deg += 360.f; }
			const int32 SlotIndex = FMath::RoundToInt(Deg / (360.f / WheelSlots)) % WheelSlots;
			if (GetWheelSuit(SlotIndex) != INDEX_NONE) { WheelSelection = SlotIndex; }
		}
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>() * LookSensitivity;

	// Apply directly to the control rotation so the result doesn't depend on the engine's
	// legacy input-scale settings (which flip the pitch sign between configurations).
	FRotator Rot = PC->GetControlRotation();
	Rot.Yaw += Axis.X;
	const float PitchDelta = bInvertLookY ? -Axis.Y : Axis.Y;
	Rot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rot.Pitch + (double)PitchDelta), -80.0, 75.0);
	Rot.Roll = 0.f;
	PC->SetControlRotation(Rot);
}

void AIronManCharacter::Input_Ascend(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || !IsFlying()) { return; }
	// Sticky vertical: write held value (do not accumulate+clear each physics tick — F9).
	FlightVertHeld = FMath::Clamp(Value.Get<float>() * VerticalFlightInputScale, -1.f, 1.f);
	FlightVertInput = FlightVertHeld;
}

void AIronManCharacter::Input_AscendStarted(const FInputActionValue& Value)
{
	if (TrySkipCinematic()) { return; } // Space skips the suit-up
	if (bDead || IsFlying()) { return; }
	Jump(); // ground mode: Space = jump
}

void AIronManCharacter::Input_Descend(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || !IsFlying()) { return; }
	FlightVertHeld = FMath::Clamp(-Value.Get<float>() * VerticalFlightInputScale, -1.f, 1.f);
	FlightVertInput = FlightVertHeld;
}

void AIronManCharacter::Input_VertReleased(const FInputActionValue& Value)
{
	(void)Value;
	FlightVertHeld = 0.f;
	FlightVertInput = 0.f;
}

void AIronManCharacter::Input_ToggleFlight(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || bCareerSuitOff) { return; }
	if (const USuitSuperComponent* Sup = USuitSuperComponent::Get(this)) { if (Sup->IsSwingMode()) { return; } } // Iron Spider swing mode: B returns to flight
	SetFlying(!IsFlying());
}

void AIronManCharacter::Input_FireStarted(const FInputActionValue& Value)
{
	if (TrySkipCinematic()) { return; } // click skips the suit-up
	if (bCareerSuitOff) { return; }
	if (bDead) { return; }
	if (bWheelOpen) { CloseSuitWheel(true); return; } // click confirms the wheel
	bFireHeld = true;
	if (IsHulkbusterActive()) { StartPunch(); }
}

void AIronManCharacter::Input_FireCompleted(const FInputActionValue& Value)
{
	bFireHeld = false;
}

void AIronManCharacter::Input_Beam(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || bWheelOpen || bCareerSuitOff || bSuperActive) { return; }
	// Super ready: the component decides tap (normal special) vs hold 0.3 s (super move).
	if (USuitSuperComponent* Sup = USuitSuperComponent::Get(this)) { if (Sup->HandleSpecialPressed()) { return; } }
	UseSpecial();
}

void AIronManCharacter::Input_Hulkbuster(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || bCareerSuitOff) { return; }
	ActivateHulkbuster();
}

void AIronManCharacter::Input_Pause(const FInputActionValue& Value)
{
	CloseSuitWheel(false);
	// Suit-up or story cutscene: Esc skips instead of opening pause (B08).
	if (bInCinematic)
	{
		TrySkipCinematic();
		return;
	}
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetController()))
	{
		PC->TogglePauseMenu();
	}
}

void AIronManCharacter::Input_ToggleView(const FInputActionValue& Value)
{
	if (bInCinematic) { return; }
	SetFirstPerson(!bFirstPerson);
}

void AIronManCharacter::Input_WheelOpen(const FInputActionValue& Value)
{
	if (bDead || bInCinematic || bWheelOpen || bCareerSuitOff || UGameplayStatics::IsGamePaused(this)) { return; }
	bWheelOpen = true;
	WheelCursor = FVector2D::ZeroVector;
	WheelSelection = INDEX_NONE;
	WheelPage = FMath::Clamp(SuitIndex / WheelSlots, 0, GetWheelPageCount() - 1);
	bFireHeld = false;
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this))
	{
		Feel->PushDilation(TEXT("SuitWheel"), FMath::Clamp(SuitWheelTimeDilation, 0.05f, 1.f));
	}
	else
	{
		UGameplayStatics::SetGlobalTimeDilation(this, FMath::Clamp(SuitWheelTimeDilation, 0.05f, 1.f));
	}
}

void AIronManCharacter::Input_WheelClose(const FInputActionValue& Value)
{
	CloseSuitWheel(true);
}

void AIronManCharacter::Input_WheelPage(const FInputActionValue& Value)
{
	if (!bWheelOpen) { return; }
	const float V = Value.Get<float>();
	const int32 Pages = GetWheelPageCount();
	WheelPage = (WheelPage + (V < 0.f ? Pages - 1 : 1)) % Pages;
	WheelSelection = INDEX_NONE;
	WheelCursor = FVector2D::ZeroVector;
}

void AIronManCharacter::CloseSuitWheel(bool bApply)
{
	if (!bWheelOpen) { return; }
	bWheelOpen = false;
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this))
	{
		Feel->PopDilation(TEXT("SuitWheel"));
	}
	else
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}
	const int32 Target = GetWheelSuit(WheelSelection);
	WheelSelection = INDEX_NONE;
	if (bApply && Target != INDEX_NONE && Target != SuitIndex)
	{
		if (!RequestSuitSwap(Target) && SwapCooldownLeft > 0.f)
		{
			Banner(LOCTEXT("SwapCooldown", "SUIT SWAP RECHARGING"),
				FText::Format(LOCTEXT("SwapCooldownSub", "{0}s"), FText::AsNumber(FMath::CeilToInt(SwapCooldownLeft))), 1.f);
		}
	}
}

void AIronManCharacter::QuickSlot(int32 SlotIndex)
{
	if (bDead || bInCinematic || bWheelOpen) { return; }
	const int32 Target = GetQuickSlotSuit(SlotIndex);
	if (Target == INDEX_NONE || Target == SuitIndex) { return; }
	if (!RequestSuitSwap(Target) && SwapCooldownLeft > 0.f)
	{
		Banner(LOCTEXT("SwapCooldown", "SUIT SWAP RECHARGING"),
			FText::Format(LOCTEXT("SwapCooldownSub", "{0}s"), FText::AsNumber(FMath::CeilToInt(SwapCooldownLeft))), 1.f);
	}
}

void AIronManCharacter::Input_BoostStarted(const FInputActionValue& Value)
{
	if (!bBoostHeld && IsFlying() && !bCareerSuitOff) { ArenaAudio::PlayAt(this, TEXT("thruster_boost"), GetActorLocation(), 1.f); ArenaCombat::Rumble(this, 0.4f, 0.3f, true); }
	bBoostHeld = true;
}

void AIronManCharacter::Input_BoostCompleted(const FInputActionValue& Value)
{
	bBoostHeld = false;
}

void AIronManCharacter::Input_Ally(const FInputActionValue& Value)
{
	if (bDead || bInCinematic) { return; }
	OnSummonAllyPressed.Broadcast();
}

void AIronManCharacter::Input_Vent(const FInputActionValue& Value)
{
	if (bDead || bInCinematic) { return; }
	if (Reactor->Vent())
	{
		BeamTimeLeft = 0.f;
		VentPuffTimer = 0.f;
		Banner(LOCTEXT("Venting", "VENTING REACTOR"), FText::GetEmpty(), 0.8f);
	}
	else if (Reactor->GetVentCooldownLeft() > 0.f)
	{
		Banner(LOCTEXT("VentCooldown", "VENTS RESEALING"),
			FText::Format(LOCTEXT("VentCooldownSub", "{0}s"), FText::AsNumber(FMath::CeilToInt(Reactor->GetVentCooldownLeft()))), 0.8f);
	}
}

void AIronManCharacter::HandleReactorOverheat(bool bOverheated)
{
	if (bOverheated)
	{
		BeamTimeLeft = 0.f;
		Banner(LOCTEXT("Overheat", "REACTOR OVERHEAT"), LOCTEXT("OverheatSub", "WEAPONS OFFLINE  -  VENT [X]"), 1.5f);
		// No dedicated voice event yet: low-energy lines read as a power warning.
		UJarvisComponent::Notify(this, EAIEvent::LowEnergy);
	}
	else if (!bDead)
	{
		Banner(LOCTEXT("ReactorStable", "REACTOR STABLE"), FText::GetEmpty(), 0.8f);
	}
}

// =====================================================================================
// Flight / movement
// =====================================================================================

bool AIronManCharacter::IsFlying() const
{
	return GetCharacterMovement() && GetCharacterMovement()->MovementMode == MOVE_Flying;
}

void AIronManCharacter::SetFlying(bool bFly, bool bForced)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move) { return; }
	const bool bWasFlying = IsFlying();

	if (bFly)
	{
		if (Energy <= 0.f || (bFlightLocked && Energy < MinFlyEnergy))
		{
			Banner(LOCTEXT("NoPower", "INSUFFICIENT POWER"),
				FText::Format(LOCTEXT("NoPowerSub", "RECHARGE TO {0} ENERGY ON THE GROUND"), FText::AsNumber(FMath::RoundToInt(MinFlyEnergy))), 1.5f);
			return;
		}
		bFlightLocked = false;
		Move->SetMovementMode(MOVE_Flying);
		Move->bUseSeparateBrakingFriction = true;
		Move->BrakingFriction = 0.f;
		ApplySuitMovementSettings();
		ApplyFlightCamera(true);
		FlightState = 0;
		LastAppliedFlightCamState = -1; // force ApplyFlightCameraForState on next physics tick
		// Soft lift (F8): blend toward 320 instead of hard-setting 250 (less takeoff pop).
		if (Move->IsMovingOnGround() || Move->Velocity.Z < 250.f)
		{
			Move->Velocity.Z = FMath::Max(float(Move->Velocity.Z), FMath::Lerp(float(Move->Velocity.Z), 320.f, 0.55f));
		}
		if (!bWasFlying && HasActorBegunPlay() && !bInCinematic) { UJarvisComponent::Notify(this, EAIEvent::ToFlight); }
	}
	else
	{
		// Landing flare (F7): clamp dive speed, then near-floor soft touchdown + brief XY brake.
		Move->Velocity.Z = FMath::Max(float(Move->Velocity.Z), -900.f);
		{
			FHitResult FloorHit;
			const FVector Loc = GetActorLocation();
			if (GetWorld() && GetWorld()->LineTraceSingleByChannel(FloorHit, Loc, Loc - FVector(0, 0, 2000), ECC_WorldStatic,
				FCollisionQueryParams(SCENE_QUERY_STAT(FlightLandAssist), false, this)))
			{
				if ((Loc.Z - FloorHit.ImpactPoint.Z) < 350.f)
				{
					Move->Velocity.Z = FMath::Max(float(Move->Velocity.Z), -420.f);
					LandingAssistLeft = 0.12f;
					// Immediate XY bleed so the first falling frame is not a dump.
					Move->Velocity.X *= 0.65f;
					Move->Velocity.Y *= 0.65f;
				}
			}
		}
		// MOVE_Falling transitions to MOVE_Walking automatically on touchdown.
		Move->SetMovementMode(MOVE_Falling);
		Move->bUseSeparateBrakingFriction = false;
		ApplySuitMovementSettings();
		ApplyFlightCamera(false);
		FlightMoveInput = FVector2D::ZeroVector;
		FlightMoveSmoothed = FVector2D::ZeroVector;
		FlightVertHeld = 0.f;
		FlightVertInput = 0.f;
		CruiseStickHold = 0.f;
		BoostFov = 0.f;
		if (bForced)
		{
			bFlightLocked = true;
			Banner(LOCTEXT("Depleted", "POWER DEPLETED"), LOCTEXT("DepletedSub", "FORCED LANDING"), 2.f);
			OnForcedLanding.Broadcast();
		}
		if (bWasFlying && !bInCinematic) { UJarvisComponent::Notify(this, EAIEvent::ToGround); }
	}
}

void AIronManCharacter::ApplySuitMovementSettings()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bBuster = IsHulkbusterActive();
	const float SpeedMul = SuitSpeedMul * (OverdriveLeft > 0.f ? 1.3f : 1.f) * (bBoostActive ? BoostSpeedMultiplier : 1.f);
	Move->MaxWalkSpeed = (bBuster ? HulkbusterWalkSpeed : WalkSpeed) * SpeedMul;
	Move->MaxFlySpeed = (bBuster ? HulkbusterFlySpeed : FlySpeed) * SpeedMul * FMath::Max(0.5f, CareerFlightMul);
	Move->JumpZVelocity = bBuster ? HulkbusterJumpVelocity : JumpVelocity;
	Move->BrakingDecelerationFlying = FlightBraking;
	if (IsFlying())
	{
		// Force-based flight integrates the velocity itself; CMC only moves + slides along walls.
		Move->MaxFlySpeed = 20000.f;
		Move->BrakingDecelerationFlying = 0.f;
	}
	CameraBoom->TargetArmLength = bBuster ? HulkbusterCameraDistance : CameraDistance;
	CameraBoom->SocketOffset = CameraShoulderOffset * (bBuster ? 1.4f : 1.f);
}

void AIronManCharacter::TickEnergy(float DeltaSeconds)
{
	const bool bOnGround = GetCharacterMovement()->IsMovingOnGround();
	Reactor->bOnGround = bOnGround;

	// Flight boost: only while flying, moving and the reactor can deliver.
	const bool bWantBoost = bBoostHeld && IsFlying() && Reactor->CanDrawPower() && Energy > 1.f &&
		GetCharacterMovement()->Velocity.SizeSquared() > 100.f * 100.f && !bWheelOpen;
	if (bWantBoost != bBoostActive)
	{
		bBoostActive = bWantBoost;
		ApplySuitMovementSettings();
	}
	if (bBoostActive) { Reactor->AddHeat(Reactor->HeatPerBoostSecond * DeltaSeconds); }

	if (IsFlying())
	{
		const float Mul = (IsHulkbusterActive() ? HulkbusterFlightDrainMultiplier : 1.f) * (bBoostActive ? BoostEnergyDrainMultiplier : 1.f)
			* (Reactor->IsOverheated() ? OverheatFlightDrainMultiplier : 1.f);
		Energy -= FlightEnergyDrain * Mul * DeltaSeconds;
		if (Energy <= 0.f)
		{
			Energy = 0.f;
			SetFlying(false, true);
		}
	}
	else if (bOnGround)
	{
		// Energy is the reactor's output: no regen while it is overheated, more with Output upgrades.
		Energy = FMath::Min(MaxEnergy, Energy + GroundEnergyRegen * Reactor->GetRegenMultiplier() * DeltaSeconds);
	}

	// Vent: steam puffs from the back (and the forearm unit) while the reactor dumps heat.
	if (Reactor->IsVenting())
	{
		VentPuffTimer -= DeltaSeconds;
		if (VentPuffTimer <= 0.f)
		{
			VentPuffTimer = 0.12f;
			const FVector Back = GetActorLocation() - GetActorForwardVector() * 45.f + FVector(0.f, 0.f, 40.f);
			AArenaBlastFX::Spawn(GetWorld(), Back + FMath::VRand() * 15.f, 55.f, FLinearColor(0.85f, 0.9f, 0.95f), 0.35f);
		}
	}

	// AI warnings (with hysteresis; the component adds its own cooldowns).
	const float EFrac = MaxEnergy > 0.f ? Energy / MaxEnergy : 0.f;
	if (!bWarnedLowEnergy && EFrac < 0.2f) { bWarnedLowEnergy = true; UJarvisComponent::Notify(this, EAIEvent::LowEnergy); }
	else if (EFrac > 0.4f) { bWarnedLowEnergy = false; }

	const float HFrac = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	if (!bWarnedLowHealth && HFrac < 0.3f) { bWarnedLowHealth = true; UJarvisComponent::Notify(this, EAIEvent::LowHealth); }
	else if (HFrac > 0.5f) { bWarnedLowHealth = false; }
}

void AIronManCharacter::TickSuitWear(float DeltaSeconds)
{
	SuitWearTimer -= DeltaSeconds;
	if (SuitWearTimer > 0.f) { return; }
	SuitWearTimer = 0.2f;
	const float Damage = MaxHealth > 0.f ? FMath::Clamp(1.f - Health / MaxHealth, 0.f, 1.f) * 0.8f : 0.f;
	const float Heat = Reactor ? FMath::Clamp((Reactor->GetHeatFraction() - 0.6f) / 0.4f, 0.f, 1.f) : 0.f;
	if (FMath::Abs(Damage - ShownSuitDamage) > 0.02f)
	{
		ShownSuitDamage = Damage;
		ArenaVisuals::SetScalarOnAll(Parts, TEXT("Damage"), Damage);
		ArenaVisuals::SetScalarOnAll(FPParts, TEXT("Damage"), Damage);
	}
	if (FMath::Abs(Heat - ShownSuitHeat) > 0.02f)
	{
		ShownSuitHeat = Heat;
		ArenaVisuals::SetScalarOnAll(Parts, TEXT("HeatGlow"), Heat);
		ArenaVisuals::SetScalarOnAll(FPParts, TEXT("HeatGlow"), Heat);
	}
}

void AIronManCharacter::TickBoundary()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Loc = GetActorLocation();

	// Ceiling (F5): soft bleed in the last 200 cm, hard zero only 50 cm past.
	const float Ceiling = bPlayAreaOverride ? PlayAreaCeilingZ : FloorZAtSpawn + MaxFlightAltitude;
	if (Move && Move->Velocity.Z > 0.f)
	{
		const float SoftStart = Ceiling - 200.f;
		if (Loc.Z > Ceiling + 50.f) { Move->Velocity.Z = 0.f; }
		else if (Loc.Z > SoftStart)
		{
			const float Alpha = FMath::Clamp((Loc.Z - SoftStart) / 200.f, 0.f, 1.f);
			const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : (1.f / 60.f);
			Move->Velocity.Z *= FMath::Exp(-6.f * Alpha * Dt);
		}
	}

	// Soft circular wall around the spawn point.
	const FVector Center = bPlayAreaOverride ? PlayAreaCenter : SpawnTransform.GetLocation();
	const float Limit = bPlayAreaOverride ? PlayAreaRadius : ArenaBoundaryRadius;
	FVector Flat(Loc.X - Center.X, Loc.Y - Center.Y, 0.f);
	const float Dist = Flat.Size();
	if (Dist > Limit)
	{
		const FVector Out = Flat / Dist;
		const float Outward = FVector::DotProduct(Move->Velocity, Out);
		if (Outward > 0.f) { Move->Velocity -= Out * Outward; }
		SetActorLocation(FVector(Center.X, Center.Y, Loc.Z) + Out * Limit, true);
	}
}

// =====================================================================================
// Tick
// =====================================================================================

void AIronManCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Camera decision 6: no motion blur while aiming (arm raised after a shot, or the beam is on) in third person;
	// the helmet camera has it off permanently.
	if (FollowCamera)
	{
		const bool bAimingNow = !bInCinematic && (AimPoseTime > 0.f || IsBeamActive());
		FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = bAimingNow;
		FollowCamera->PostProcessSettings.MotionBlurAmount = 0.f;
	}

	if (!bDead && !bInCinematic)
	{
		InvulnerableLeft = FMath::Max(0.f, InvulnerableLeft - DeltaSeconds);
		TickEnergy(DeltaSeconds);
		TickFlightPhysics(DeltaSeconds);
		TickAimFeel(DeltaSeconds);
		TickWeapons(DeltaSeconds);
		TickBeam(DeltaSeconds);
		TickHulkbuster(DeltaSeconds);
		TickEffects(DeltaSeconds);
		TickBoundary();
		TickSuitWear(DeltaSeconds);
	}
	TickVisuals(DeltaSeconds);
	TickFirstPerson(DeltaSeconds);
}

void AIronManCharacter::TickEffects(float DeltaSeconds)
{
	SwapCooldownLeft = FMath::Max(0.f, SwapCooldownLeft - DeltaSeconds);

	if (OverdriveLeft > 0.f)
	{
		OverdriveLeft -= DeltaSeconds;
		if (OverdriveLeft <= 0.f) { OverdriveLeft = 0.f; ApplySuitMovementSettings(); }
	}

	if (ShieldLeft > 0.f)
	{
		ShieldLeft -= DeltaSeconds;
		const bool bOn = ShieldLeft > 0.f && (ShieldLeft > 0.8f || FMath::Fmod(ShieldLeft, 0.2f) > 0.1f);
		ShieldRoot->AddLocalRotation(FRotator(0.f, 240.f * DeltaSeconds, 0.f));
		for (UStaticMeshComponent* Orb : ShieldOrbs) { Orb->SetVisibility(bOn); }
		if (ShieldLeft <= 0.f) { ShieldLeft = 0.f; }
	}

	if (CloakLeft > 0.f)
	{
		CloakLeft -= DeltaSeconds;
		if (CloakLeft <= 0.f)
		{
			CloakLeft = 0.f;
			ArenaVisuals::FlashAll(Parts, FLinearColor::White, true);
			Banner(LOCTEXT("Decloak", "CLOAK DISENGAGED"), FText::GetEmpty(), 1.f);
		}
	}
}

// =====================================================================================
// Weapons
// =====================================================================================

FVector AIronManCharacter::ComputeAimPoint() const
{
	const UCameraComponent* Cam = GetActiveCamera();
	const FVector CamLoc = Cam->GetComponentLocation();
	const FVector CamDir = Cam->GetForwardVector();
	const float MaxRange = 20000.f;

	// 1) Aim assist: enemy closest to the crosshair within the cone.
	if (AimAssistDegrees > 0.f)
	{
		const float CosLimit = FMath::Cos(FMath::DegreesToRadians(AimAssistDegrees * (1.f + 0.8f * FocusAlpha)));
		AEnemyBase* Best = nullptr;
		float BestDot = CosLimit;
		for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
		{
			if (It->IsDead()) { continue; }
			const FVector To = It->GetAimPoint() - CamLoc;
			const float Dot = FVector::DotProduct(To.GetSafeNormal(), CamDir);
			if (Dot > BestDot && To.Size() < MaxRange) { BestDot = Dot; Best = *It; }
		}
		if (Best) { return Best->GetAimPoint(); }
	}

	// 2) Whatever is under the crosshair.
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(IronManAim), false, this);
	// Start past the character so the trace never hits our own body.
	const float StartOffset = bFirstPerson ? 60.f : CameraBoom->TargetArmLength + 50.f;
	const FVector Start = CamLoc + CamDir * StartOffset;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, CamLoc + CamDir * MaxRange, ECC_Visibility, Q))
	{
		return Hit.ImpactPoint;
	}
	return CamLoc + CamDir * MaxRange;
}

void AIronManCharacter::TickWeapons(float DeltaSeconds)
{
	FireCooldown -= DeltaSeconds;
	AimPoseTime = FMath::Max(0.f, AimPoseTime - DeltaSeconds);
	if (bSuperActive) { return; } // the super move owns the arms

	// Hulkbuster punches.
	if (PunchTimeLeft > 0.f)
	{
		PunchTimeLeft -= DeltaSeconds;
		if (!bPunchLanded && (1.f - PunchTimeLeft / PunchDuration) > 0.47f)
		{
			bPunchLanded = true;
			PunchImpact();
		}
	}

	if (IsHulkbusterActive())
	{
		// Holding LMB keeps punching.
		if (bFireHeld && PunchTimeLeft <= 0.f) { StartPunch(); }
		return;
	}

	// Focus + hold = charge a heavy repulsor blast (released on let go); otherwise hold = auto fire at the weapon's rate.
	const ESuitPrimaryWeapon PW = GetSuit().PrimaryWeapon;
	const bool bChargeable = PW == ESuitPrimaryWeapon::Repulsor || PW == ESuitPrimaryWeapon::RedBlast || PW == ESuitPrimaryWeapon::Laser;
	if (bFireHeld && FocusAlpha > 0.85f && bChargeable && !IsBeamActive() && !bWheelOpen)
	{
		if (!bCharging) { bCharging = true; ChargeTime = 0.f; ArenaAudio::PlayAt(this, TEXT("repulsor_charge"), GetActorLocation(), 0.9f, 0.f); }
		ChargeTime += DeltaSeconds;
		if (ChargeTime > 0.3f) { ArenaCombat::Rumble(this, 0.12f * FMath::Min(1.f, ChargeTime / 0.7f), 0.05f); }
		return;
	}
	if (bCharging)
	{
		bCharging = false;
		if (!bFireHeld && Reactor->CanDrawPower() && !IsBeamActive() && !bWheelOpen)
		{
			bNextShotCharged = ChargeTime >= 0.7f;
			FireRepulsor();
			bNextShotCharged = false;
		}
		ChargeTime = 0.f;
		return;
	}
	// B03: carry the cooldown remainder and catch up (cap ~4 shots/frame) so fire rate is fps-independent.
	if (bFireHeld && !IsBeamActive() && !bWheelOpen && Reactor->CanDrawPower())
	{
		int32 ShotsThisFrame = 0;
		while (FireCooldown <= 0.f && ShotsThisFrame < 4 && bFireHeld && !IsBeamActive() && !bWheelOpen && Reactor->CanDrawPower())
		{
			FireRepulsor(); // adds Interval onto FireCooldown
			++ShotsThisFrame;
		}
	}
	else
	{
		FireCooldown = FMath::Max(0.f, FireCooldown); // don't bank negative while not firing
	}
}

void AIronManCharacter::Landed(const FHitResult& Hit)
{
	// Vertical speed is still the falling speed here (CharacterMovement calls Landed before zeroing it).
	const float DownSpeed = -float(GetVelocity().Z);
	Super::Landed(Hit);
	if (DownSpeed < 600.f || bInCinematic || IsDead()) { return; } // camera_shake.json: > 6 m/s
	UWorld* World = GetWorld();
	if (!World) { return; }
	++HardLandings;
	const float K = FMath::Clamp((DownSpeed - 600.f) / 1400.f, 0.f, 1.f); // 6..20 m/s
	const FVector Ground = Hit.ImpactPoint;
	// Same frame: dust ring flash (no shake of its own), scorch mark, then the landing shake.
	AArenaBlastFX::Spawn(World, Ground + Hit.ImpactNormal * 5.f, FMath::Lerp(90.f, 180.f, K), FLinearColor(0.55f, 0.5f, 0.45f), 0.35f, false);
	ArenaFeel::SpawnImpactDecal(World, Ground, Hit.ImpactNormal, FMath::Lerp(80.f, 160.f, K), 10.f);
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this)) { Feel->PlayShake(TEXT("superhero_landing"), FMath::Lerp(0.6f, 1.f, K)); }
}

void AIronManCharacter::FireRepulsor()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const FSuitDef& S = GetSuit();

	// Per-weapon tuning (Repulsor = the tunables on this class).
	float Interval = RepulsorFireInterval, Speed = RepulsorSpeed, Life = RepulsorLifetime;
	float Damage = RepulsorDamage, Blast = 0.f, Radius = 10.f, Spread = 0.f;
	int32 Count = 1;
	float FanDegrees = 0.f;
	switch (S.PrimaryWeapon)
	{
	case ESuitPrimaryWeapon::Laser:        Interval = 0.32f; Speed = 6500.f; Damage = 2.2f; Life = 0.8f; Radius = 7.f; break;
	case ESuitPrimaryWeapon::Sonic:        Interval = 0.45f; Speed = 2600.f; Damage = 1.2f; Life = 0.6f; Blast = 120.f; Radius = 16.f; Count = 3; FanDegrees = 7.f; break;
	case ESuitPrimaryWeapon::Flamethrower: Interval = 0.06f; Speed = 1800.f; Damage = 0.35f; Life = 0.35f; Radius = 14.f; Spread = 6.f; break;
	case ESuitPrimaryWeapon::Frost:        Interval = 0.2f;  Speed = 3000.f; Damage = 1.1f; Radius = 11.f; break;
	case ESuitPrimaryWeapon::Minigun:      Interval = 0.06f; Speed = 4200.f; Damage = 0.45f; Life = 0.9f; Radius = 6.f; Spread = 2.5f; break;
	case ESuitPrimaryWeapon::RedBlast:     Interval = 0.22f; Speed = 3000.f; Damage = 1.4f; Blast = 150.f; Radius = 12.f; break;
	default: break;
	}
	// Heat scales with time spent firing, so fast and slow weapons heat equally per second.
	// B10: halve the interval for overdrive first, so overdrive doesn't double heat per second.
	if (OverdriveLeft > 0.f) { Interval *= 0.5f; }
	Reactor->AddHeat(Reactor->HeatPerFiringSecond * Interval);
	FireCooldown += Interval; // B03: preserve remainder instead of resetting
	Damage *= SuitDamageMul * (CloakLeft > 0.f ? 1.5f : 1.f);
	const FLinearColor Color = PrimaryWeaponColor(S.PrimaryWeapon, S.EmissiveColor);

	AimPoseTime = 0.45f;
	bUseLeftHand = !bUseLeftHand;
	UStaticMeshComponent* Palm = bUseLeftHand ? PalmL : PalmR;

	// Muzzle: the visible gauntlet palm in first person, the body's palm in third person.
	FVector Muzzle;
	const int32 FPIdx = bUseLeftHand ? Rig.FPPalmPartL : Rig.FPPalmPartR;
	if (bFirstPerson && FPParts.IsValidIndex(FPIdx) && FPParts[FPIdx].Component)
	{
		Muzzle = FPParts[FPIdx].Component->GetComponentLocation() + FirstPersonCamera->GetForwardVector() * 10.f;
		(bUseLeftHand ? FlashL : FlashR) = 1.f;
		FPKick = 1.f;
		FPPalmLight->AttachToComponent(bUseLeftHand ? Rig.FPArmL.Get() : Rig.FPArmR.Get(), FAttachmentTransformRules::KeepRelativeTransform);
		FPPalmLight->SetRelativeLocation(FVector(50.f, 0.f, 6.f));
	}
	else
	{
		// Make sure the arm is already pointing forward before reading the palm position.
		TickVisuals(0.f);
		Muzzle = Palm->GetComponentLocation() + GetActorForwardVector() * 20.f;
	}
	const FVector Target = ComputeAimPoint();
	const FVector Dir = (Target - Muzzle).GetSafeNormal();

	const bool bRapidFireWeapon = S.PrimaryWeapon == ESuitPrimaryWeapon::Minigun || S.PrimaryWeapon == ESuitPrimaryWeapon::Flamethrower;
	if (!bRapidFireWeapon && !bInCinematic)
	{
		if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this)) { Feel->PlayShake(TEXT("repulsor_fire"), 0.6f); }
	}
	// Edith combat-feel: ALL player primaries are film-style hitscan beams/blasts (no glowing balls).
	// Sonic = short fan of thick tracers; Flamethrower = short cone of soft orange streaks.
	const bool bCharged = bNextShotCharged;
	if (bCharged) { Damage *= 3.5f; Blast = FMath::Max(Blast, 260.f); Radius *= 2.f; FireCooldown = FMath::Max(FireCooldown, 0.45f); }
	Spread += ShotBloom * (bRapidFireWeapon ? 2.5f : 1.2f) * (1.f - 0.7f * FocusAlpha);
	ShotBloom = FMath::Min(1.f, ShotBloom + (bRapidFireWeapon ? 0.08f : 0.25f));
	float Range = 11000.f;
	float TracerW = bCharged ? 16.f : 5.2f;   // Pass 2: thinner Mark-style beams
	float TracerLife = bCharged ? 0.12f : 0.055f;
	FName FireSfx = TEXT("repulsor_fire");
	float FireVol = bRapidFireWeapon ? 0.45f : 1.f;
	float FirePitch = bRapidFireWeapon ? 0.12f : 0.06f;
	switch (S.PrimaryWeapon)
	{
	case ESuitPrimaryWeapon::Laser:        Range = 14000.f; TracerW = 3.2f; TracerLife = 0.05f; break;
	case ESuitPrimaryWeapon::Minigun:      Range = 10000.f; TracerW = 2.4f; TracerLife = 0.035f; break;
	case ESuitPrimaryWeapon::Frost:        Range = 4800.f; TracerW = 6.5f; TracerLife = 0.07f; break;
	case ESuitPrimaryWeapon::RedBlast:     Range = 9000.f; TracerW = 7.5f; TracerLife = 0.07f; break;
	case ESuitPrimaryWeapon::Sonic:
		Range = 2200.f; TracerW = 12.f; TracerLife = 0.075f;
		FireSfx = TEXT("unibeam_impact"); FireVol = 0.55f; FirePitch = 0.1f;
		break;
	case ESuitPrimaryWeapon::Flamethrower:
		Range = 950.f; TracerW = 10.f; TracerLife = 0.04f;
		FireSfx = TEXT("thruster_boost"); FireVol = 0.35f; FirePitch = 0.12f;
		break;
	default: break;
	}
	if (bCharged) { TracerW = FMath::Max(TracerW, 16.f); TracerLife = FMath::Max(TracerLife, 0.12f); }
	// Gold rim from suit secondary (champagne / gold trim); A>0 enables explicit rim in AArenaTracer.
	FLinearColor BeamRim = S.SecondaryColor;
	BeamRim.A = 1.f;
	bool bAnyKill = false, bAnyHit = false;
	for (int32 i = 0; i < Count; ++i)
	{
		FVector ShotDir = Dir;
		if (Count > 1) { ShotDir = Dir.RotateAngleAxis((i - (Count - 1) * 0.5f) * FanDegrees, FVector::UpVector); }
		if (Spread > 0.f) { ShotDir = FMath::VRandCone(ShotDir, FMath::DegreesToRadians(Spread)); }
		const ArenaCombat::FShot Shot = ArenaCombat::FireHitscan(this, GetController(), Muzzle, ShotDir, Range, Damage, Blast, Radius, Color, bCharged ? 1400.f : 420.f);
		AArenaTracer::Spawn(World, Muzzle, Shot.End, Color, TracerW, TracerLife, BeamRim);
		bAnyKill |= Shot.bKill;
		bAnyHit |= Shot.bEnemy;
	}
	// Tight muzzle flash (plate + shock), not a soft orb.
	AArenaBlastFX::Spawn(World, Muzzle, bCharged ? 38.f : bRapidFireWeapon ? 9.f : (S.PrimaryWeapon == ESuitPrimaryWeapon::Sonic ? 24.f : 14.f), Color, bCharged ? 0.1f : 0.045f, false);
	ArenaAudio::PlayAt(this, FireSfx, Muzzle, FireVol, FirePitch);
	if (bCharged) { ArenaAudio::PlayAt(this, TEXT("explosion_near"), Muzzle, 0.6f); }
	ArenaCombat::Rumble(this, bCharged ? 0.9f : bRapidFireWeapon ? 0.15f : 0.35f, bCharged ? 0.25f : 0.07f, bCharged);
	if (bAnyKill) { ArenaCombat::Rumble(this, 0.7f, 0.18f, true); ArenaAudio::Play2D(this, TEXT("hud_target_lock"), 0.6f); }
	else if (bAnyHit) { ArenaAudio::Play2D(this, TEXT("hud_beep_select"), 0.35f, 1.2f); }
	if (bCharged && bAnyHit) { if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this)) { Feel->HitStop(0.06f, 0.15f); Feel->PlayShake(TEXT("repulsor_fire"), 1.4f); } }
	if (AController* Ctrl = GetController())
	{
		const float Kick = (bCharged ? 1.6f : bRapidFireWeapon ? 0.12f : 0.35f) * (1.f - 0.5f * FocusAlpha);
		Ctrl->SetControlRotation(Ctrl->GetControlRotation() + FRotator(Kick, FMath::FRandRange(-0.3f, 0.3f) * Kick, 0.f));
	}
	FPKick = bCharged ? 1.6f : 1.f;
	(void)Speed; (void)Life; // projectile path retired for player primaries (missiles / supers still use AIronProjectile)
	RepulsorLight->AttachToComponent(Palm, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RepulsorFlash = 1.f;
	RepulsorLight->SetIntensity(bFirstPerson ? 0.f : SuitLightLumens::PalmFlash);

	// Shots also knock down boss missiles they pass close to (cheap proximity check).
	for (TActorIterator<AIronProjectile> It(World); It; ++It)
	{
		if (It->IsActiveShot() && It->IsHostile() && It->bDestroyableByPlayer)
		{
			const FVector ToMissile = It->GetActorLocation() - Muzzle;
			const float Along = FVector::DotProduct(ToMissile, Dir);
			if (Along > 0.f && Along < Speed * Life &&
				(ToMissile - Dir * Along).Size() < 60.f)
			{
				It->Intercept();
			}
		}
	}
}

void AIronManCharacter::UseSpecial()
{
	if (BeamTimeLeft > 0.f) { return; }
	if (!Reactor->CanDrawPower())
	{
		Banner(Reactor->IsOverheated() ? LOCTEXT("TooHot", "REACTOR TOO HOT") : LOCTEXT("VentBusy", "VENTING"),
			Reactor->IsOverheated() ? LOCTEXT("TooHotSub", "VENT [X] OR WAIT") : FText::GetEmpty(), 0.8f);
		return;
	}
	const ESuitSpecialWeapon Special = IsHulkbusterActive() ? ESuitSpecialWeapon::Unibeam : GetSuit().SpecialWeapon;
	const float Cost = GetSpecialCost();

	// Timed specials can't be stacked.
	if ((Special == ESuitSpecialWeapon::Overdrive && OverdriveLeft > 0.f) ||
		(Special == ESuitSpecialWeapon::Shield && ShieldLeft > 0.f) ||
		(Special == ESuitSpecialWeapon::Cloak && CloakLeft > 0.f))
	{
		return;
	}
	if (Energy < Cost)
	{
		Banner(LOCTEXT("LowPower", "LOW POWER"),
			FText::Format(LOCTEXT("LowPowerSub", "{0} NEEDS {1} ENERGY"), GetSpecialName(), FText::AsNumber(FMath::RoundToInt(Cost))), 1.f);
		return;
	}
	Energy -= Cost;
	const FSuitDef& S = GetSuit();
	// Beams heat per second in TickBeam; everything else is one spike.
	if (Special != ESuitSpecialWeapon::Unibeam && Special != ESuitSpecialWeapon::LaserWhip)
	{
		Reactor->AddHeat(Reactor->HeatPerSpecial);
	}

	switch (Special)
	{
	case ESuitSpecialWeapon::Missiles:
		FireHoming(6 + FMath::Max(0, CareerExtraMissiles), 1800.f, 6000.f, 3.f, 200.f, 12.f, 3.5f, S.AccentColor);
		break;
	case ESuitSpecialWeapon::DroneSwarm:
		FireHoming(4, 1100.f, 3800.f, 2.5f, 160.f, 20.f, 5.f, S.EmissiveColor);
		break;
	case ESuitSpecialWeapon::Overdrive:
		OverdriveLeft = 5.f;
		ApplySuitMovementSettings();
		Banner(LOCTEXT("Overdrive", "OVERDRIVE"), LOCTEXT("OverdriveSub", "DOUBLE FIRE RATE"), 1.f);
		break;
	case ESuitSpecialWeapon::Shield:
		ShieldLeft = 4.f;
		Banner(LOCTEXT("Shield", "ENERGY SHIELD"), LOCTEXT("ShieldSub", "NO DAMAGE FOR 4s"), 1.f);
		break;
	case ESuitSpecialWeapon::NanoBlades:
		NanoBladeSweep();
		break;
	case ESuitSpecialWeapon::Cloak:
		CloakLeft = 4.f;
		ArenaVisuals::FlashAll(Parts, CloakColor, false);
		Banner(LOCTEXT("Cloak", "CLOAK ENGAGED"), LOCTEXT("CloakSub", "ENEMIES LOSE TRACK  -  SHOTS +50%"), 1.f);
		break;
	case ESuitSpecialWeapon::LaserWhip:
		bBeamWhip = true;
		BeamTimeLeft = 0.6f;
		BeamDamageAccum.Empty();
		break;
	case ESuitSpecialWeapon::Unibeam:
	default:
		bBeamWhip = false;
		BeamTimeLeft = BeamDuration;
		ArenaAudio::PlayAt(this, TEXT("unibeam_charge"), GetActorLocation(), 1.f, 0.f);
		BeamSfxLeft = BeamWindup;
		if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this)) { Feel->PlayShake(TEXT("unibeam")); }
		BeamDamageAccum.Empty();
		break;
	}
}

void AIronManCharacter::FireHoming(int32 Count, float Speed, float Homing, float Damage, float Blast, float Radius, float Life, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// Targets: living enemies, those in front of the camera first.
	const UCameraComponent* Cam = GetActiveCamera();
	const FVector CamFwd = Cam->GetForwardVector();
	TArray<AEnemyBase*> Targets;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (!It->IsDead()) { Targets.Add(*It); }
	}
	const FVector Me = GetActorLocation();
	Targets.Sort([&](const AEnemyBase& A, const AEnemyBase& B)
	{
		const float DA = FVector::DotProduct((A.GetActorLocation() - Me).GetSafeNormal(), CamFwd) - FVector::Dist(A.GetActorLocation(), Me) / 20000.f;
		const float DB = FVector::DotProduct((B.GetActorLocation() - Me).GetSafeNormal(), CamFwd) - FVector::Dist(B.GetActorLocation(), Me) / 20000.f;
		return DA > DB;
	});

	const FVector Up = FVector::UpVector;
	const FVector Right = GetActorRightVector();
	ArenaAudio::PlayAt(this, TEXT("missile_launch"), Me, 1.f);
	ArenaCombat::Rumble(this, 0.5f, 0.2f, true);
	for (int32 i = 0; i < Count; ++i)
	{
		const float Side = (i % 2 == 0) ? 1.f : -1.f;
		const FVector Origin = Me + Up * 70.f + Right * Side * 40.f;
		// Launch up and outward, then home in.
		const FVector Dir = (CamFwd + Up * 0.6f + Right * Side * 0.5f * (1 + i / 2)).GetSafeNormal();
		AEnemyBase* Target = Targets.Num() > 0 ? Targets[i % Targets.Num()] : nullptr;
		if (AIronProjectile* P = AIronProjectile::SpawnPooled(World, Origin, Dir.Rotation(), this, this, false))
		{
			P->LifeSeconds = Life;
			P->Configure(Damage * SuitDamageMul, false, Blast, Color, Radius);
			P->Launch(Dir * Speed, 0.f, Target ? Target->GetRootComponent() : nullptr, Target ? Homing : 0.f);
		}
	}
	AimPoseTime = 0.45f;
}

void AIronManCharacter::NanoBladeSweep()
{
	const UCameraComponent* Cam = GetActiveCamera();
	const FVector Center = GetActorLocation() + Cam->GetForwardVector() * 220.f;
	const float Radius = 320.f;

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_Pawn);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NanoBlades), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Obj, FCollisionShape::MakeSphere(Radius), Q);

	TSet<AActor*> Seen;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || Seen.Contains(A)) { continue; }
		Seen.Add(A);
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(A))
		{
			UGameplayStatics::ApplyDamage(Enemy, 5.f * SuitDamageMul, GetController(), this, UDamageType::StaticClass());
		}
		else if (AIronProjectile* P = Cast<AIronProjectile>(A))
		{
			if (P->IsHostile()) { P->Intercept(); }
		}
	}
	AArenaBlastFX::Spawn(GetWorld(), Center, Radius, GetSuit().EmissiveColor, 0.3f);
	FlashL = FlashR = 1.f;
	AimPoseTime = 0.45f;
}

void AIronManCharacter::TickBeam(float DeltaSeconds)
{
	if (BeamTimeLeft <= 0.f)
	{
		BeamMesh->SetVisibility(false);
		bBeamWhip = false;
		if (BeamAudio) { BeamAudio->FadeOut(0.2f, 0.f); BeamAudio = nullptr; ArenaAudio::PlayAt(this, TEXT("unibeam_impact"), GetActorLocation(), 0.6f); }
		return;
	}
	BeamTimeLeft -= DeltaSeconds;
	Reactor->AddHeat(Reactor->HeatPerBeamSecond * DeltaSeconds); // may overheat -> HandleReactorOverheat stops the beam
	const float Duration = bBeamWhip ? 0.6f : BeamDuration;
	const float Windup = bBeamWhip ? 0.f : BeamWindup;
	const float Range = bBeamWhip ? 1600.f : BeamRange;
	const float Dps = bBeamWhip ? 40.f : BeamDamagePerSecond;
	const float Elapsed = Duration - BeamTimeLeft;
	const bool bBuster = IsHulkbusterActive();
	const float Radius = BeamRadius * (bBuster ? 1.4f : 1.f) * (bBeamWhip ? 0.45f : 1.f);

	FVector Origin = ArcReactor->GetComponentLocation();
	FVector Dir = (ComputeAimPoint() - Origin).GetSafeNormal();
	if (bBeamWhip)
	{
		// Whip: from the right palm, sweeping left to right across the view.
		Origin = PalmR->GetComponentLocation();
		const float Yaw = FMath::Lerp(-60.f, 60.f, FMath::Clamp(Elapsed / Duration, 0.f, 1.f));
		Dir = GetActiveCamera()->GetForwardVector().RotateAngleAxis(Yaw, FVector::UpVector);
	}

	// Beam stops at walls / floor.
	float Length = Range;
	FHitResult WallHit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ChestBeam), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(WallHit, Origin, Origin + Dir * Range, FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		Length = WallHit.Distance;
	}

	// Visual: wind-up is a thin flicker, then full width.
	const float Width = (Elapsed < Windup) ? Radius * 0.25f : Radius * (0.9f + 0.1f * FMath::Sin(Elapsed * 60.f));
	BeamPivot->SetWorldLocationAndRotation(Origin, Dir.Rotation());
	BeamMesh->SetRelativeLocation(FVector(Length * 0.5f, 0.f, 0.f));
	BeamMesh->SetRelativeScale3D(FVector(Width * 2.f / 100.f, Width * 2.f / 100.f, Length / 100.f));
	BeamMesh->SetVisibility(BeamTimeLeft > 0.f);

	if (Elapsed < Windup || BeamTimeLeft <= 0.f) { return; }
	if (!BeamAudio)
	{
		ArenaAudio::PlayAt(this, TEXT("unibeam_fire"), Origin, 1.f, 0.f);
		BeamAudio = ArenaAudio::Attach(TEXT("unibeam_fire_loop"), GetRootComponent(), 0.9f, true);
		if (BeamAudio) { BeamAudio->Play(); }
	}
	ArenaCombat::Rumble(this, 0.45f, 0.06f, true);
	BeamCityAcc += DeltaSeconds;
	if (BeamCityAcc > 0.15f)
	{
		BeamCityAcc = 0.f;
		const FVector BeamEnd = Origin + Dir * Length;
		CityLife::Damage(GetWorld(), BeamEnd, Radius * 2.5f, 3.f, nullptr, true);
		AArenaSparks::Spawn(GetWorld(), BeamEnd, -Dir, GetSuit().EmissiveColor, 9, 1300.f);
		if (Length < Range - 1.f) { ArenaFeel::SpawnImpactDecal(GetWorld(), BeamEnd, WallHit.ImpactNormal, Radius * 2.2f, 8.f); }
	}

	// Damage: capsule overlap along the beam, Pawn (enemies) + WorldDynamic (missiles).
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_Pawn);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector Mid = Origin + Dir * (Length * 0.5f);
	const FQuat Rot = FRotationMatrix::MakeFromZ(Dir).ToQuat();
	GetWorld()->OverlapMultiByObjectType(Overlaps, Mid, Rot, Obj,
		FCollisionShape::MakeCapsule(Radius, Length * 0.5f + Radius), Q);

	TSet<AActor*> Seen;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || Seen.Contains(A)) { continue; }
		Seen.Add(A);
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(A))
		{
			// Accumulate fractional damage and apply in whole points (hit flash + boss phases behave).
			float& Acc = BeamDamageAccum.FindOrAdd(Enemy);
			Acc += Dps * SuitDamageMul * DeltaSeconds;
			if (Acc >= 1.f)
			{
				const float Whole = FMath::FloorToFloat(Acc);
				Acc -= Whole;
				UGameplayStatics::ApplyDamage(Enemy, Whole, GetController(), this, UDamageType::StaticClass());
			}
		}
		else if (AIronProjectile* P = Cast<AIronProjectile>(A))
		{
			if (P->IsHostile() && P->bDestroyableByPlayer) { P->Intercept(); }
		}
	}
}

void AIronManCharacter::StartPunch()
{
	if (PunchTimeLeft > 0.f) { return; }
	PunchTimeLeft = PunchDuration;
	bPunchLanded = false;
	bPunchLeft = !bPunchLeft;
	AimPoseTime = PunchDuration;
	// Lunge forward along the camera direction (flat when on the ground).
	FVector Fwd = GetActiveCamera()->GetForwardVector();
	if (!IsFlying()) { Fwd.Z = 0.f; Fwd.Normalize(); }
	LaunchCharacter(Fwd * PunchLunge, false, false);
}

void AIronManCharacter::PunchImpact()
{
	const FVector Fwd = GetActiveCamera()->GetForwardVector();
	const FVector Center = GetActorLocation() + Fwd * PunchReach * HulkbusterScale * 0.6f;

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_Pawn);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HulkPunch), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Obj, FCollisionShape::MakeSphere(PunchRadius), Q);

	TSet<AActor*> Seen;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || Seen.Contains(A)) { continue; }
		Seen.Add(A);
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(A))
		{
			UGameplayStatics::ApplyDamage(Enemy, PunchDamage * SuitDamageMul, GetController(), this, UDamageType::StaticClass());
		}
		else if (AIronProjectile* P = Cast<AIronProjectile>(A))
		{
			// The shockwave swats enemy shots out of the air.
			if (P->IsHostile()) { P->Intercept(); }
		}
	}
	// City: cars dent / explode, pedestrians are knocked down.
	CityLife::Damage(GetWorld(), Center, PunchRadius, PunchDamage * SuitDamageMul, nullptr, true);
	ArenaAudio::PlayAt(this, TEXT("melee_hit_heavy"), Center, 1.f);
	ArenaCombat::Rumble(this, 0.7f, 0.18f, true);
	AArenaBlastFX::Spawn(GetWorld(), Center, PunchRadius * 0.8f, FLinearColor(1.f, 0.8f, 0.55f), 0.25f);
}

// =====================================================================================
// Hulkbuster
// =====================================================================================

void AIronManCharacter::GrantHulkbusterCharge(const FString& Reason)
{
	++HulkbusterCharges;
	Banner(LOCTEXT("BusterReady", "HULKBUSTER READY [Q]"), FText::FromString(Reason), 2.f);
	UJarvisComponent::Notify(this, EAIEvent::HulkbusterReady);
}

void AIronManCharacter::ActivateHulkbuster()
{
	if (IsHulkbusterActive())
	{
		Banner(LOCTEXT("BusterActive", "HULKBUSTER ACTIVE"),
			FText::Format(LOCTEXT("BusterLeft", "{0}s REMAINING"), FText::AsNumber(FMath::CeilToInt(HulkbusterTimeLeft))), 1.f);
		return;
	}
	if (HulkbusterCharges <= 0)
	{
		int32 PtsLeft = 0;
		if (AArenaGameMode* GM = GetArenaGameMode()) { PtsLeft = GM->GetPointsToNextHulkbuster(); }
		Banner(LOCTEXT("BusterLocked", "HULKBUSTER LOCKED"),
			FText::Format(LOCTEXT("BusterLockedSub", "{0} PTS OR A GOLD BEACON"), FText::AsNumber(PtsLeft)), 1.5f);
		return;
	}

	// B07: make sure the bigger capsule fits here (low roofs, corridors, walls) before spending the charge.
	const float BusterHalf = BaseCapsuleHalfHeight * HulkbusterScale;
	const float BusterRadius = BaseCapsuleRadius * HulkbusterScale;
	const float BusterLift = BusterHalf - BaseCapsuleHalfHeight;
	FVector BusterSpot = GetActorLocation() + FVector(0.f, 0.f, BusterLift);
	{
		UWorld* BusterWorld = GetWorld();
		FCollisionQueryParams BusterParams(SCENE_QUERY_STAT(HulkbusterClearance), false, this);
		const FCollisionShape BusterShape = FCollisionShape::MakeCapsule(BusterRadius, BusterHalf);
		const ECollisionChannel BusterChannel = GetCapsuleComponent()->GetCollisionObjectType();
		bool bFits = BusterWorld && !BusterWorld->OverlapBlockingTestByChannel(BusterSpot, FQuat::Identity, BusterChannel, BusterShape, BusterParams);
		if (!bFits && BusterWorld)
		{
			// Try nudging the lift lower / sideways a little before giving up.
			static const FVector Nudges[] = {
				FVector(0.f, 0.f, -0.5f), FVector(1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f),
				FVector(0.f, 1.f, 0.f), FVector(0.f, -1.f, 0.f) };
			for (const FVector& N : Nudges)
			{
				const FVector Try = BusterSpot + N * FVector(BusterRadius - BaseCapsuleRadius, BusterRadius - BaseCapsuleRadius, BusterLift);
				if (!BusterWorld->OverlapBlockingTestByChannel(Try, FQuat::Identity, BusterChannel, BusterShape, BusterParams))
				{
					// Only accept it if the small capsule can actually get there (no going through a thin wall).
					FHitResult PathHit;
					const bool bPathBlocked = BusterWorld->SweepSingleByChannel(PathHit, GetActorLocation(), Try - FVector(0.f, 0.f, BusterLift),
						FQuat::Identity, BusterChannel, GetCapsuleComponent()->GetCollisionShape(), BusterParams);
					if (!bPathBlocked) { BusterSpot = Try; bFits = true; break; }
				}
			}
		}
		if (!bFits)
		{
			Banner(LOCTEXT("BusterNoClearance", "NO CLEARANCE"), LOCTEXT("BusterNoClearanceSub", "MOVE TO OPEN GROUND - CHARGE KEPT"), 1.5f);
			return;
		}
	}

	--HulkbusterCharges;
	HulkbusterTimeLeft = HulkbusterDuration;
	Health = FMath::Min(MaxHealth, Health + HulkbusterActivationHeal);
	BeamTimeLeft = 0.f;
	bBeamWhip = false;

	// Grow the capsule; lift first so the bigger capsule doesn't start inside the floor.
	SetActorLocation(BusterSpot, false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCapsuleSize(BusterRadius, BusterHalf);
	UpdateSuitRootTransform();
	RefreshSuitVisibility();

	ApplySuitMovementSettings();
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 350.f, ArenaVisuals::SuitGold, 0.5f);
	Banner(LOCTEXT("BusterOnline", "HULKBUSTER ONLINE"), LOCTEXT("BusterOnlineSub", "LMB: PUNCH  -  RMB: CHEST BEAM"), 2.f);
	UJarvisComponent::Notify(this, EAIEvent::HulkbusterDeploy);
	OnHulkbusterActivated.Broadcast();
}

void AIronManCharacter::RevertHulkbuster()
{
	HulkbusterTimeLeft = 0.f;
	GetCapsuleComponent()->SetCapsuleSize(BaseCapsuleRadius, BaseCapsuleHalfHeight);
	UpdateSuitRootTransform();
	RefreshSuitVisibility();
	PunchTimeLeft = 0.f;
	ApplySuitMovementSettings();
	if (!bDead)
	{
		Banner(LOCTEXT("BusterOffline", "HULKBUSTER OFFLINE"), FText::GetEmpty(), 1.5f);
	}
}

void AIronManCharacter::TickHulkbuster(float DeltaSeconds)
{
	if (HulkbusterTimeLeft > 0.f)
	{
		HulkbusterTimeLeft -= DeltaSeconds;
		if (HulkbusterTimeLeft <= 0.f) { RevertHulkbuster(); }
	}
}

// =====================================================================================
// Health
// =====================================================================================

float AIronManCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || bInCinematic || InvulnerableLeft > 0.f || DamageAmount <= 0.f || bGodMode) { return 0.f; }
	if (ShieldLeft > 0.f)
	{
		AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 160.f, GetSuit().EmissiveColor, 0.15f);
		return 0.f;
	}
	const float Mul = IsHulkbusterActive() ? HulkbusterDamageTaken : 1.f;
	const float Applied = DamageAmount * Mul;
	Super::TakeDamage(Applied, DamageEvent, EventInstigator, DamageCauser);

	Health -= Applied;
	// FRIDAY HUD: directional damage arc + per-zone wear (contrib/friday/realism, HUDDataSubsystem).
	if (UHUDDataSubsystem* HUDData = UHUDDataSubsystem::Get(this))
	{
		HUDData->ReportPlayerHit(this, Applied, MaxHealth, DamageEvent, EventInstigator, DamageCauser);
	}
	InvulnerableLeft = InvulnerabilityAfterHit;
	HitFlashLeft = 0.15f;
	ArenaVisuals::FlashAll(Parts, FLinearColor(1.f, 0.5f, 0.3f), false);
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this))
	{
		// Vison shakes, driven by ArenaFeel trauma (the one gameplay shake source).
		Feel->PlayShake(Applied >= 25.f ? FName(TEXT("hit_heavy")) : FName(TEXT("hit_light")), FMath::Clamp(Applied / 25.f, 0.25f, 1.f));
	}

	if (Health <= 0.f)
	{
		Health = 0.f;
		Die();
	}
	return Applied;
}

void AIronManCharacter::Heal(float Amount)
{
	if (bDead) { return; }
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
}

void AIronManCharacter::AddEnergy(float Amount)
{
	Energy = FMath::Clamp(Energy + Amount, 0.f, MaxEnergy);
	if (bFlightLocked && Energy >= MinFlyEnergy) { bFlightLocked = false; }
}

void AIronManCharacter::Die()
{
	if (bDead) { return; }
	CloseSuitWheel(false);
	bDead = true;
	bFireHeld = false;
	BeamTimeLeft = 0.f;
	BeamMesh->SetVisibility(false);
	ShieldLeft = 0.f;
	CloakLeft = 0.f;
	OverdriveLeft = 0.f;
	for (UStaticMeshComponent* Orb : ShieldOrbs) { Orb->SetVisibility(false); }
	if (IsHulkbusterActive()) { RevertHulkbuster(); }

	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 500.f, FLinearColor(1.f, 0.5f, 0.3f), 0.8f);
	RefreshSuitVisibility(); // hides body + gauntlets
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);

	if (UJarvisComponent* J = UJarvisComponent::Get(this)) { J->Say(EAIEvent::Death, NAME_None, true); }
	if (AArenaGameMode* GM = GetArenaGameMode()) { GM->OnPlayerDied(); }
}

// =====================================================================================
// Procedural placeholder animation
// =====================================================================================

void AIronManCharacter::TickVisuals(float DeltaSeconds)
{
	if (!bUsePlaceholderVisuals) { return; }

	const FVector Vel = GetVelocity();
	const float Fwd = FVector::DotProduct(Vel, GetActorForwardVector());
	const float Side = FVector::DotProduct(Vel, GetActorRightVector());
	const bool bFly = IsFlying() && !bDead;
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Body lean: tilt forward into flight like a superhero.
	const float MaxSpeed = FMath::Max(1.f, GetCharacterMovement()->MaxFlySpeed);
	// Force-based flight: lean / bank come from TickFlightPhysics (pitch into speed, roll into turns).
	const float LeanPitch = (bFly && !bInCinematic) ? FlightLeanPitch : 0.f;
	const float LeanRoll = (bFly && !bInCinematic) ? FlightBank : 0.f;
	if (DeltaSeconds > 0.f)
	{
		const FRotator Cur = SuitRoot->GetRelativeRotation();
		// Faster SuitRoot bank (F3): physics already interps FlightBank at 8; keep one visual lag, not two soft ones.
		SuitRoot->SetRelativeRotation(FMath::RInterpTo(Cur, FRotator(LeanPitch, 0.f, LeanRoll), DeltaSeconds, 12.f));
	}

	// Arms: raised toward the crosshair while shooting / punching, otherwise hanging (or swept back in flight).
	float AimPitch = 0.f;
	if (Controller) { AimPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch); }
	const float BodyPitch = SuitRoot->GetRelativeRotation().Pitch;
	const float Raised = 90.f + AimPitch - BodyPitch;
	// Hover: arms almost at the sides (palms down for the repulsors); fast flight sweeps them back.
	const float Idle = bFly ? -FMath::Lerp(6.f, 28.f, FMath::Clamp(Vel.Size() / MaxSpeed, 0.f, 1.f)) : FMath::Sin(WalkCycle) * 25.f * FMath::Min(1.f, Vel.Size2D() / 500.f);

	const bool bAim = (AimPoseTime > 0.f || IsBeamActive()) && !bInCinematic;
	float PitchR = bAim ? Raised : Idle;
	// Flight: both arms swept slightly back together (was mirrored: one forward, one back). Ground: opposite swing.
	float PitchL = bAim ? Raised : (bFly ? Idle : -Idle);
	if (IsHulkbusterActive() && PunchTimeLeft > 0.f)
	{
		// Punch: alternate arms, quick extend.
		const float T = 1.f - PunchTimeLeft / PunchDuration;
		const float Ext = FMath::Sin(T * PI);
		if (bPunchLeft) { PitchL = FMath::Lerp(30.f, Raised, Ext); PitchR = 30.f; }
		else            { PitchR = FMath::Lerp(30.f, Raised, Ext); PitchL = 30.f; }
	}
	else if (bAim && !IsBeamActive())
	{
		// Only the firing hand is fully up; the other stays half raised.
		if (bUseLeftHand) { PitchR = FMath::Lerp(Idle, Raised, 0.5f); }
		else              { PitchL = FMath::Lerp(bFly ? Idle : -Idle, Raised, 0.5f); }
	}
	// Relaxed idle: arms hang at the sides, a few degrees out so the gauntlets clear the hips (never behind the back).
	const float ArmOut = bAim ? 0.f : bFly ? 12.f : 5.f;
	ShoulderR->SetRelativeRotation(FRotator(FMath::Clamp(PitchR, -35.f, 200.f), 0.f, ArmOut));
	ShoulderL->SetRelativeRotation(FRotator(FMath::Clamp(PitchL, -35.f, 200.f), 0.f, -ArmOut));

	// Legs: walk cycle on the ground, together and slightly bent back in flight.
	if (!bFly && GetCharacterMovement()->IsMovingOnGround())
	{
		WalkCycle += DeltaSeconds * Vel.Size2D() * 0.018f;
		const float Swing = FMath::Sin(WalkCycle) * 30.f * FMath::Min(1.f, Vel.Size2D() / 400.f);
		HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
		HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));
	}
	else
	{
		HipL->SetRelativeRotation(FRotator(bFly ? -10.f : 10.f, 0.f, 0.f));
		HipR->SetRelativeRotation(FRotator(bFly ? -6.f : -10.f, 0.f, 0.f));
	}

	// Super move / mask pose override (USuitSuperComponent keys it with anticipation / impact / recovery curves).
	if (PoseOverride.Weight > 0.001f)
	{
		const float W = FMath::Clamp(PoseOverride.Weight, 0.f, 1.f);
		auto Blend = [W](USceneComponent* C, const FRotator& R) { if (C) { C->SetRelativeRotation(FQuat::Slerp(C->GetRelativeRotation().Quaternion(), R.Quaternion(), W).Rotator()); } };
		Blend(ShoulderL, PoseOverride.ShoulderL);
		Blend(ShoulderR, PoseOverride.ShoulderR);
		Blend(HipL, PoseOverride.HipL);
		Blend(HipR, PoseOverride.HipR);
		Blend(SuitRoot, PoseOverride.Root);
		const float Half = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		SuitRoot->SetRelativeLocation(FVector(0.f, 0.f, -Half) + PoseOverride.RootOffset * W);
	}

	// Thruster flames flicker while flying (and only when the body is shown).
	const bool bBodyShown = SuitRoot->IsVisible();
	const float Flicker = 0.8f + 0.3f * FMath::Sin(Time * 47.f) + 0.15f * FMath::Sin(Time * 91.f);
	for (UStaticMeshComponent* Flame : { FlameBootL.Get(), FlameBootR.Get(), FlameHandL.Get(), FlameHandR.Get() })
	{
		if (!Flame) { continue; }
		const bool bHand = (Flame == FlameHandL || Flame == FlameHandR);
		const bool bShow = bBodyShown && bFly && !(bHand && bAim);
		Flame->SetVisibility(bShow);
		const float Base = bHand ? 0.1f : 0.14f;
		const float Len = (bHand ? 0.3f : 0.45f) * Flicker * (0.7f + 1.3f * FlightThrottleVis);
		Flame->SetRelativeScale3D(FVector(Base, Base, Len));
	}

	// Repulsor flash fades.
	if (RepulsorLight && DeltaSeconds > 0.f)
	{
		RepulsorFlash = FMath::Max(0.f, RepulsorFlash - 8.f * DeltaSeconds);
		RepulsorLight->SetIntensity(bFirstPerson ? 0.f : SuitLightLumens::PalmFlash * RepulsorFlash);
	}
	// Palm light pool: exactly one of the two palm components is in the scene.
	if (RepulsorLight && FPPalmLight)
	{
		const bool bFP = bFirstPerson && !bInCinematic;
		if (RepulsorLight->IsVisible() == bFP) { RepulsorLight->SetVisibility(!bFP); }
		if (FPPalmLight->IsVisible() != bFP) { FPPalmLight->SetVisibility(bFP); }
	}

	// Damage flash (back to the suit colours, or to the cloak tint).
	if (HitFlashLeft > 0.f && DeltaSeconds > 0.f)
	{
		HitFlashLeft -= DeltaSeconds;
		if (HitFlashLeft <= 0.f)
		{
			if (CloakLeft > 0.f) { ArenaVisuals::FlashAll(Parts, CloakColor, false); }
			else { ArenaVisuals::FlashAll(Parts, FLinearColor::White, true); }
		}
	}

	// Arc reactor pulse, and dimmer when energy is low (suits without a reactor barely glow).
	if (ArcLight)
	{
		const float EnergyFrac = MaxEnergy > 0.f ? Energy / MaxEnergy : 0.f;
		const float ReactorMul = GetSuit().Reactor == ESuitReactor::None ? 0.25f : 1.f;
		ArcLight->SetIntensity(SuitLightLumens::Reactor * ReactorMul * (0.25f + 0.75f * EnergyFrac * (0.85f + 0.15f * FMath::Sin(Time * 4.f))));
	}

	// Makeshift mounted reactor: the exposed core shifts blue -> orange -> red with heat and
	// flickers when overheated (body unit + first-person forearm readout).
	if (ShowsMountedReactor() && Reactor)
	{
		const float HeatF = Reactor->GetHeatFraction();
		// Story: the upgraded core (Tony's ring) burns a clean white-blue; makeshift cores use the suit glow.
		const FLinearColor Cool = (Reactor->HasStage() && Reactor->GetStage() == EReactorStage::Upgraded)
			? FLinearColor(0.75f, 0.92f, 1.f) : (Reactor->HasStage() ? FLinearColor(0.35f, 0.8f, 1.f) : GetSuit().EmissiveColor);
		const FLinearColor Core = Reactor->GetCoreColor(Cool);
		float CoreFlicker = Reactor->IsOverheated() ? (FMath::Fmod(Time, 0.16f) > 0.08f ? 1.f : 0.15f) : 1.f;
		CoreFlicker *= Reactor->SampleCoreBrightness(Time);
		const float Glow = (18.f + 40.f * HeatF + 6.f * FMath::Sin(Time * 7.f)) * CoreFlicker;
		auto UpdateCore = [&Core, Glow](TArray<FArenaPaintedPart>& Arr, int32 Idx)
		{
			if (!Arr.IsValidIndex(Idx) || !Arr[Idx].Component) { return; }
			Arr[Idx].Color = Core;
			ArenaVisuals::RepaintPart(Arr[Idx]);
			ArenaVisuals::SetPartGlow(Arr[Idx], Glow);
		};
		if (HitFlashLeft <= 0.f && CloakLeft <= 0.f)
		{
			UpdateCore(Parts, Rig.MountedCorePart);
			UpdateCore(Parts, Rig.ForearmCorePart);
		}
		UpdateCore(FPParts, Rig.FPMountedCorePart);
		if (ArcLight) { ArcLight->SetLightColor(Core); }
	}
}

void AIronManCharacter::TickFirstPerson(float DeltaSeconds)
{
	if (!Rig.FPRoot || !Rig.FPArmL || !Rig.FPArmR || DeltaSeconds <= 0.f) { return; }

	FlashL = FMath::Max(0.f, FlashL - 2.2f * DeltaSeconds);
	FlashR = FMath::Max(0.f, FlashR - 2.2f * DeltaSeconds);
	FPKick = FMath::Max(0.f, FPKick - 9.f * DeltaSeconds);
	if (!bFirstPerson || bInCinematic || bDead) { FPPalmLight->SetIntensity(0.f); return; }

	const float Time = GetWorld()->GetTimeSeconds();
	const FVector Vel = GetVelocity();
	const bool bGround = GetCharacterMovement()->IsMovingOnGround();
	const float BobAmt = bGround ? FMath::Min(1.f, Vel.Size2D() / 500.f) : 0.35f;
	const float BobFreq = bGround ? 9.f : 1.6f;
	const float BobZ = FMath::Sin(Time * BobFreq) * 1.4f * BobAmt;
	const float BobY = FMath::Cos(Time * BobFreq * 0.5f) * 0.9f * BobAmt;
	const bool bBeam = IsBeamActive() && !bBeamWhip;

	for (int32 SideIdx = 0; SideIdx < 2; ++SideIdx)
	{
		const bool bLeft = SideIdx == 0;
		const float Sign = bLeft ? -1.f : 1.f;
		USceneComponent* Arm = bLeft ? Rig.FPArmL.Get() : Rig.FPArmR.Get();
		const float Fire = bLeft ? FlashL : FlashR;
		const float Raise = FMath::Min(1.f, Fire * 2.5f);

		// Idle: low at the sides of the view. Firing: palm comes up toward the crosshair.
		FVector Loc = FMath::Lerp(FVector(8.f, 24.f * Sign, -30.f), FVector(18.f, 15.f * Sign, -20.f), Raise);
		float Pitch = FMath::Lerp(6.f, 10.f, Raise);
		float Yaw = FMath::Lerp(-4.f * Sign, -12.f * Sign, Raise);
		const bool bLastShot = (bLeft == bUseLeftHand);
		if (bLastShot) { Loc.X -= 7.f * FPKick; Pitch += 6.f * FPKick; }

		if (IsHulkbusterActive() && PunchTimeLeft > 0.f && bLeft == bPunchLeft)
		{
			const float T = 1.f - PunchTimeLeft / PunchDuration;
			const float Ext = FMath::Sin(T * PI);
			Loc.X += 40.f * Ext;
			Loc.Y = FMath::Lerp(Loc.Y, 6.f * Sign, Ext);
			Loc.Z = FMath::Lerp(Loc.Z, -14.f, Ext);
			Yaw = FMath::Lerp(Yaw, 0.f, Ext);
		}
		if (bBeam) { Loc.Z -= 12.f; Loc.Y += 8.f * Sign; }
		if (bBeamWhip && IsBeamActive() && !bLeft) { Loc = FVector(20.f, 12.f, -18.f); }
		Loc.Z += BobZ;
		Loc.Y += BobY;

		Arm->SetRelativeLocation(FMath::VInterpTo(Arm->GetRelativeLocation(), Loc, DeltaSeconds, 20.f));
		Arm->SetRelativeRotation(FMath::RInterpTo(Arm->GetRelativeRotation(), FRotator(Pitch, Yaw, 0.f), DeltaSeconds, 20.f));

		// Palm glow while firing.
		const int32 PalmIdx = bLeft ? Rig.FPPalmPartL : Rig.FPPalmPartR;
		if (FPParts.IsValidIndex(PalmIdx))
		{
			ArenaVisuals::SetPartGlow(FPParts[PalmIdx], 4.f + 60.f * Fire);
		}
	}
	FPPalmLight->SetIntensity(SuitLightLumens::PalmFlash * 0.75f * FMath::Max(FlashL, FlashR));
}

// =====================================================================================
// Helpers
// =====================================================================================

AArenaGameMode* AIronManCharacter::GetArenaGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
}

void AIronManCharacter::Banner(const FText& Title, const FText& Sub, float Seconds) const
{
	if (AArenaGameMode* GM = GetArenaGameMode())
	{
		GM->ShowBanner(Title, Sub, Seconds);
	}
}

#undef LOCTEXT_NAMESPACE

// ------------------------------------------------------------------------------------------------ aim feel / thrusters

void AIronManCharacter::TickAimFeel(float DeltaSeconds)
{
	// Focus: hold Left Alt (aim down / hover focus); automatic half focus while hovering slowly and shooting.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bAltHeld = PC && PC->IsInputKeyDown(EKeys::LeftAlt);
	const bool bHoverFocus = IsFlying() && GetVelocity().Size() < 350.f && AimPoseTime > 0.f;
	const float Want = bCareerSuitOff ? 0.f : bAltHeld ? 1.f : bHoverFocus ? 0.5f : 0.f;
	FocusAlpha = FMath::FInterpTo(FocusAlpha, Want, DeltaSeconds, 10.f);
	ShotBloom = FMath::Max(0.f, ShotBloom - DeltaSeconds * 2.2f);
	const float SpeedFov = IsFlying() ? FMath::GetMappedRangeValueClamped(FVector2D(2000.f, 3500.f), FVector2D(0.f, 8.f), float(GetVelocity().Size())) : 0.f;
	const float FovAdd = SpeedFov + BoostFov + SuperFov - 14.f * FocusAlpha;
	if (FollowCamera) { FollowCamera->SetFieldOfView(FMath::Clamp(FirstPersonFOV - 10.f + FovAdd, 50.f, 125.f)); }
	if (FirstPersonCamera) { FirstPersonCamera->SetFieldOfView(FMath::Clamp(FirstPersonFOV + FovAdd, 50.f, 125.f)); }
	if (bAltHeld && IsFlying())
	{
		// Hover focus: bleed speed so the suit steadies itself.
		if (UCharacterMovementComponent* M = GetCharacterMovement()) { M->Velocity *= FMath::Exp(-2.5f * DeltaSeconds); }
	}
	ArenaCombat::SetAimState(FocusAlpha > 0.5f, bCharging ? FMath::Clamp(ChargeTime / 0.7f, 0.f, 1.f) : 0.f, ShotBloom);

	// Light camera roll from bank (F3): boom relative roll tracks FlightBank; no second SuitRoot lag.
	if (CameraBoom && IsFlying() && !bInCinematic)
	{
		const float Roll = FMath::Clamp(FlightBank * 0.22f, -10.f, 10.f);
		const FRotator BoomRel = CameraBoom->GetRelativeRotation();
		CameraBoom->SetRelativeRotation(FMath::RInterpTo(BoomRel, FRotator(BoomRel.Pitch, BoomRel.Yaw, Roll), DeltaSeconds, 8.f));
	}
	else if (CameraBoom)
	{
		const FRotator BoomRel = CameraBoom->GetRelativeRotation();
		if (FMath::Abs(BoomRel.Roll) > 0.05f)
		{
			CameraBoom->SetRelativeRotation(FMath::RInterpTo(BoomRel, FRotator(BoomRel.Pitch, BoomRel.Yaw, 0.f), DeltaSeconds, 10.f));
		}
	}

	// Thruster bed: loops while flying, pitch / volume with speed.
	const bool bFly = IsFlying() && !bCareerSuitOff;
	if (bFly != bWasFlyingSfx)
	{
		bWasFlyingSfx = bFly;
		ArenaAudio::PlayAt(this, bFly ? FName(TEXT("thruster_takeoff")) : FName(TEXT("thruster_landing")), GetActorLocation(), 0.9f);
		if (bFly && !ThrusterAudio)
		{
			ThrusterAudio = ArenaAudio::Attach(TEXT("thruster_loop"), GetRootComponent(), 0.5f, true);
			if (ThrusterAudio) { ThrusterAudio->Play(); }
		}
		else if (!bFly && ThrusterAudio) { ThrusterAudio->FadeOut(0.4f, 0.f); ThrusterAudio = nullptr; }
	}
	if (ThrusterAudio)
	{
		const float K = FMath::Clamp(GetVelocity().Size() / 4000.f, 0.f, 1.f);
		ThrusterAudio->SetVolumeMultiplier(0.35f + 0.55f * K + (bBoostHeld ? 0.2f : 0.f));
		ThrusterAudio->SetPitchMultiplier(0.85f + 0.4f * K + (bBoostHeld ? 0.15f : 0.f));
	}
}

// ------------------------------------------------------------------------------------------------ force-based flight
// ULTRON flight feel (contrib/ultron/flight_feel): hysteresis, soft boost assist, per-state cam,
// sticky vertical, landing flare. Caps unchanged (Hover 900 / Cruise 3500 / Boost 7500 × K).

void AIronManCharacter::ApplyFlightCameraForState(int32 InState)
{
	if (!CameraBoom) { return; }
	const bool bFly = IsFlying();
	CameraBoom->bEnableCameraLag = true;
	if (!bFly)
	{
		CameraBoom->CameraLagSpeed = 14.f;
		CameraBoom->CameraLagMaxDistance = 0.f;
		CameraBoom->bEnableCameraRotationLag = false;
		LastAppliedFlightCamState = -1;
		return;
	}
	// 0 hover / 1 cruise / 2 boost — matches flight_params.json cameraByState
	struct Cam { float Lag, MaxD, Rot; };
	const Cam C = InState == 2 ? Cam{ 6.f, 260.f, 9.f } : InState == 1 ? Cam{ 9.f, 180.f, 12.f } : Cam{ 12.f, 120.f, 14.f };
	CameraBoom->CameraLagSpeed = C.Lag;
	CameraBoom->CameraLagMaxDistance = C.MaxD;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = C.Rot;
	LastAppliedFlightCamState = InState;
}

void AIronManCharacter::ApplyFlightCamera(bool bFly)
{
	ApplyFlightCameraForState(bFly ? FlightState : -1);
}

void AIronManCharacter::TickFlightPhysics(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Dt = FMath::Min(DeltaSeconds, 1.f / 20.f);

	// Landing assist continues briefly after SetFlying(false) (deepen: proposal only ran while flying).
	if (LandingAssistLeft > 0.f && Move)
	{
		LandingAssistLeft = FMath::Max(0.f, LandingAssistLeft - DeltaSeconds);
		Move->Velocity.X *= FMath::Exp(-3.5f * DeltaSeconds);
		Move->Velocity.Y *= FMath::Exp(-3.5f * DeltaSeconds);
		Move->Velocity.Z = FMath::Max(float(Move->Velocity.Z), -420.f);
	}

	if (!Move || !IsFlying() || !Controller || Dt <= 0.f)
	{
		FlightBank = FMath::FInterpTo(FlightBank, 0.f, DeltaSeconds, 6.f);
		FlightLeanPitch = FMath::FInterpTo(FlightLeanPitch, 0.f, DeltaSeconds, 6.f);
		FlightThrottleVis = 0.f;
		BoostFov = FMath::FInterpTo(BoostFov, 0.f, DeltaSeconds, 8.f);
		FlightMoveSmoothed = FMath::Vector2DInterpTo(FlightMoveSmoothed, FVector2D::ZeroVector, DeltaSeconds, 14.f);
		return;
	}
	if (Move->MaxFlySpeed < 10000.f) { ApplySuitMovementSettings(); } // something reset it (overdrive / buster)

	const FRotator CR = Controller->GetControlRotation();
	const FVector Look = CR.Vector();
	const FRotator YawRot(0.f, CR.Yaw, 0.f);
	const FVector Flat = YawRot.Vector();
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	FVector V = Move->Velocity;
	const float Speed = float(V.Size());

	// Per-suit scale: suit speed rating, shop upgrade, Hulkbuster is heavier.
	const float K = SuitSpeedMul * FMath::Max(0.5f, CareerFlightMul) * (OverdriveLeft > 0.f ? 1.2f : 1.f) * (IsHulkbusterActive() ? 0.67f : 1.f);
	const float HoverMax = 900.f * K, CruiseMax = 3500.f * K, BoostMax = 7500.f * K;

	// Smooth stick (raw axis still refreshed by Input_Move each Triggered frame). F9.
	FlightMoveSmoothed = FMath::Vector2DInterpTo(FlightMoveSmoothed, FlightMoveInput, Dt, 14.f);
	if (FlightMoveInput.IsNearlyZero(0.01f))
	{
		FlightMoveSmoothed = FMath::Vector2DInterpTo(FlightMoveSmoothed, FVector2D::ZeroVector, Dt, 14.f);
	}
	const FVector2D In = FlightMoveSmoothed;
	const bool bAnyInput = In.SizeSquared() > 0.0144f; // 0.12 dead zone

	// Sticky vertical: Input_Ascend/Descend write FlightVertHeld; Completed clears it. F6/F9.
	const float Vert = FlightVertHeld;

	// --- F1 hysteresis: sticky hover→cruise, no thrash at HoverMax*0.9 ---
	if (bBoostActive)
	{
		FlightState = 2;
		CruiseStickHold = 0.f;
	}
	else if (FlightState == 2)
	{
		// Leaving boost: drop to cruise if still fast / pushing forward, else hover.
		FlightState = (Speed > HoverMax * 1.05f || In.Y > 0.4f) ? 1 : 0;
	}
	else if (FlightState == 1)
	{
		if (Speed < HoverMax * 0.72f && In.Y < 0.35f) { FlightState = 0; }
	}
	else // hover
	{
		if (In.Y > 0.55f && Speed > 500.f) { CruiseStickHold += Dt; }
		else { CruiseStickHold = 0.f; }
		if (Speed > HoverMax * 1.05f || CruiseStickHold >= 0.15f) { FlightState = 1; }
	}

	if (LastAppliedFlightCamState != FlightState) { ApplyFlightCameraForState(FlightState); }

	struct FP { float Thrust, Drag, HMax, VMax; };
	const FP P = FlightState == 2 ? FP{ 12000.f, 0.18f, BoostMax, 4500.f * K }
		: FlightState == 1 ? FP{ 4500.f, 0.55f, CruiseMax, 2200.f * K }
		: FP{ 2600.f, 2.5f, HoverMax, 900.f * K };

	// Thrust: hover strafes on the horizontal plane; cruise / boost thrust where you look.
	FVector Thrust = FlightState == 0 ? (Flat * In.Y + Right * In.X) : (Look * In.Y + Right * In.X * 0.8f);
	// F2: mild boost assist only when stick is dead, never when pulling back.
	if (FlightState == 2 && In.Size() < 0.12f && In.Y >= -0.2f)
	{
		Thrust += Look * 0.35f;
	}
	Thrust = Thrust.GetClampedToMaxSize(1.f) * P.Thrust * K;
	FVector Accel = Thrust + FVector(0.f, 0.f, Vert * 3800.f * K); // F6: was 2800

	// Soft bumpers: glance off buildings instead of sticking.
	if (UWorld* World = GetWorld())
	{
		if (Speed > 200.f)
		{
			const float LookAhead = FMath::Max(300.f, Speed * 0.25f);
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(FlightBumper), false, this);
			const FVector From = GetActorLocation();
			if (World->SweepSingleByChannel(Hit, From, From + V / Speed * LookAhead, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(80.f), Query) && !Hit.bStartPenetrating)
			{
				const float Near = 1.f - FMath::Clamp(Hit.Distance / LookAhead, 0.f, 1.f);
				const FVector N = Hit.ImpactNormal;
				Accel += N * 6000.f * Near;
				const float Into = float(FVector::DotProduct(V, -N));
				if (Into > 0.f) { V += N * Into * (1.f - FMath::Exp(-6.f * Near * Dt)); }
			}
		}
	}
	V += Accel * Dt;

	// Drag (exponential = frame-rate independent); gentler altitude hold (F6: 1.2/s was 3).
	V *= FMath::Exp(-(bAnyInput || FlightState == 2 ? P.Drag : FMath::Max(P.Drag, 2.5f)) * Dt);
	if (FMath::Abs(Vert) < 0.05f && FlightState != 2) { V.Z *= FMath::Exp(-1.2f * Dt); }

	// Soft caps: excess bleeds off at 4/s instead of a hard clamp.
	FVector H2(V.X, V.Y, 0.f);
	const float HS = float(H2.Size());
	if (HS > P.HMax) { H2 *= FMath::Lerp(1.f, P.HMax / HS, 1.f - FMath::Exp(-4.f * Dt)); }
	float VZ = float(V.Z);
	if (FMath::Abs(VZ) > P.VMax) { VZ = FMath::Lerp(VZ, FMath::Sign(VZ) * P.VMax, 1.f - FMath::Exp(-4.f * Dt)); }
	V = FVector(H2.X, H2.Y, VZ);

	// Near-floor flare while still flying and descending hard (deepen on F7).
	if (V.Z < -500.f)
	{
		FHitResult FloorHit;
		const FVector Loc = GetActorLocation();
		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(FloorHit, Loc, Loc - FVector(0, 0, 600), ECC_WorldStatic,
			FCollisionQueryParams(SCENE_QUERY_STAT(FlightNearFloor), false, this)))
		{
			const float DistFloor = Loc.Z - FloorHit.ImpactPoint.Z;
			if (DistFloor < 350.f)
			{
				const float Pull = 1.f - FMath::Clamp(DistFloor / 350.f, 0.f, 1.f);
				V.Z = FMath::Lerp(V.Z, -420.f, Pull * (1.f - FMath::Exp(-4.f * Dt)));
			}
		}
	}

	Move->Velocity = V;

	// Bank into turns (yaw rate + strafe), lean into speed; boost tilts into a dive.
	const float Yaw = float(CR.Yaw);
	const float YawRate = FMath::FindDeltaAngleDegrees(LastControlYaw, Yaw) / Dt;
	LastControlYaw = Yaw;
	const float BankMax = FlightState == 2 ? 55.f : FlightState == 1 ? 35.f : 15.f;
	const float WantBank = FMath::Clamp(YawRate / 140.f * BankMax * FMath::Clamp(Speed / 1500.f, 0.25f, 1.f) + In.X * BankMax * 0.5f, -BankMax, BankMax);
	FlightBank = FMath::FInterpTo(FlightBank, WantBank, Dt, 8.f); // F3: single, faster interp
	const float Fwd = float(FVector::DotProduct(V, Flat));
	const float WantPitch = FlightState == 2 ? -65.f : -FMath::Clamp(Fwd / FMath::Max(1.f, CruiseMax), -0.4f, 1.f) * 40.f;
	FlightLeanPitch = FMath::FInterpTo(FlightLeanPitch, WantPitch, Dt, 5.f);
	FlightThrottleVis = FMath::FInterpTo(FlightThrottleVis, FlightState == 2 ? 1.f : FMath::Clamp(Thrust.Size() / 6000.f + FMath::Abs(Vert) * 0.4f, 0.f, 0.8f), Dt, 8.f);
	BoostFov = FMath::FInterpTo(BoostFov, FlightState == 2 ? 12.f : 0.f, Dt, FlightState == 2 ? 15.f : 8.f);

	// Raw axis cleared so a missed Triggered frame decays via smoothed path.
	FlightMoveInput = FVector2D::ZeroVector;
}
