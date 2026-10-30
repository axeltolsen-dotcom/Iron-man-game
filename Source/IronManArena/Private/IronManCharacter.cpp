#include "IronManCharacter.h"
#include "ArenaFeel.h"
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
	FollowCamera->SetFieldOfView(90.f);

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

	SuitRig::BuildGauntlets(this, FirstPersonCamera, Rig, FPParts);
	for (FArenaPaintedPart& P : FPParts)
	{
		P.Component->SetOnlyOwnerSee(true);
		P.Component->CastShadow = false;
	}

	FPPalmLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FPPalmLight"));
	FPPalmLight->SetupAttachment(Rig.FPArmR);
	FPPalmLight->SetRelativeLocation(FVector(50.f, 0.f, 6.f));
	FPPalmLight->SetIntensity(0.f);
	FPPalmLight->SetAttenuationRadius(250.f);
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
	ArcLight->SetLightColor(ArenaVisuals::ArcBlue);
	ArcLight->SetIntensity(3000.f);
	ArcLight->SetAttenuationRadius(300.f);
	ArcLight->SetCastShadows(false);

	RepulsorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RepulsorLight"));
	RepulsorLight->SetupAttachment(PalmR);
	RepulsorLight->SetLightColor(ArenaVisuals::ArcBlue);
	RepulsorLight->SetIntensity(0.f);
	RepulsorLight->SetAttenuationRadius(400.f);
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
		default:                               return Emissive;
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

void AIronManCharacter::ResetForNewGame()
{
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

	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	SetActorEnableCollision(true);

	ApplySuit(SuitIndex); // stats, colours, visibility
	Health = MaxHealth;
	Energy = MaxEnergy;
	SetFlying(true);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(SpawnTransform.Rotator());
	}
}

// =====================================================================================
// Suits
// =====================================================================================

void AIronManCharacter::ApplySuit(int32 Index)
{
	const int32 Num = USuitLibrary::GetNumSuits();
	SuitIndex = Num > 0 ? FMath::Clamp(Index, 0, Num - 1) : 0;
	const FSuitDef& S = GetSuit();

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
	if (ASuitUpCinematic* C = ActiveCinematic.Get()) { C->Skip(); }
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
	FollowCamera->SetFieldOfView(FirstPersonFOV - 5.f);
	UpdateCameraForSuit();
}

void AIronManCharacter::UpdateSuitRootTransform()
{
	const float P = SuitRig::PlatingScale(GetSuit().Plating);
	const float H = IsHulkbusterActive() ? HulkbusterScale : 1.f;
	const float Half = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SuitRoot->SetRelativeLocation(FVector(0.f, 0.f, -Half));
	SuitRoot->SetRelativeScale3D(FVector(P * H, P * H, H));
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

int32 AIronManCharacter::GetWheelSuit(int32 Slot) const
{
	if (Slot < 0 || Slot >= WheelSlots) { return INDEX_NONE; }
	const int32 Idx = WheelPage * WheelSlots + Slot;
	if (SelectableSuits.Num() > 0) { return SelectableSuits.IsValidIndex(Idx) ? SelectableSuits[Idx] : INDEX_NONE; }
	return Idx < USuitLibrary::GetNumSuits() ? Idx : INDEX_NONE;
}

int32 AIronManCharacter::GetQuickSlotSuit(int32 Slot) const
{
	if (SelectableSuits.Num() > 0 && SelectableSuits.Num() < USuitLibrary::GetNumSuits())
	{
		// Story: 1-5 pick from the unlocked suits, newest last.
		return SelectableSuits.IsValidIndex(Slot) ? SelectableSuits[Slot] : INDEX_NONE;
	}
	return QuickSlotSuitIds.IsValidIndex(Slot) ? USuitLibrary::FindSuitIndex(QuickSlotSuitIds[Slot]) : INDEX_NONE;
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
	EIC->BindAction(DescendAction, ETriggerEvent::Triggered, this, &AIronManCharacter::Input_Descend);
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
			const int32 Slot = FMath::RoundToInt(Deg / (360.f / WheelSlots)) % WheelSlots;
			if (GetWheelSuit(Slot) != INDEX_NONE) { WheelSelection = Slot; }
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
	AddMovementInput(FVector::UpVector, Value.Get<float>() * VerticalFlightInputScale);
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
	AddMovementInput(FVector::UpVector, -Value.Get<float>() * VerticalFlightInputScale);
}

void AIronManCharacter::Input_ToggleFlight(const FInputActionValue& Value)
{
	if (bDead || bInCinematic) { return; }
	SetFlying(!IsFlying());
}

void AIronManCharacter::Input_FireStarted(const FInputActionValue& Value)
{
	if (TrySkipCinematic()) { return; } // click skips the suit-up
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
	if (bDead || bInCinematic || bWheelOpen) { return; }
	UseSpecial();
}

void AIronManCharacter::Input_Hulkbuster(const FInputActionValue& Value)
{
	if (bDead || bInCinematic) { return; }
	ActivateHulkbuster();
}

void AIronManCharacter::Input_Pause(const FInputActionValue& Value)
{
	CloseSuitWheel(false);
	// Story cutscene: Esc skips the whole scene instead of pausing.
	if (bInCinematic && !ActiveCinematic.IsValid() && CinematicSkipHandler)
	{
		CinematicSkipHandler(true);
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
	if (bDead || bInCinematic || bWheelOpen || UGameplayStatics::IsGamePaused(this)) { return; }
	bWheelOpen = true;
	WheelCursor = FVector2D::ZeroVector;
	WheelSelection = INDEX_NONE;
	WheelPage = FMath::Clamp(SuitIndex / WheelSlots, 0, GetWheelPageCount() - 1);
	bFireHeld = false;
	UGameplayStatics::SetGlobalTimeDilation(this, FMath::Clamp(SuitWheelTimeDilation, 0.05f, 1.f));
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
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
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

void AIronManCharacter::QuickSlot(int32 Slot)
{
	if (bDead || bInCinematic || bWheelOpen) { return; }
	const int32 Target = GetQuickSlotSuit(Slot);
	if (Target == INDEX_NONE || Target == SuitIndex) { return; }
	if (!RequestSuitSwap(Target) && SwapCooldownLeft > 0.f)
	{
		Banner(LOCTEXT("SwapCooldown", "SUIT SWAP RECHARGING"),
			FText::Format(LOCTEXT("SwapCooldownSub", "{0}s"), FText::AsNumber(FMath::CeilToInt(SwapCooldownLeft))), 1.f);
	}
}

void AIronManCharacter::Input_BoostStarted(const FInputActionValue& Value)
{
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
		// Small hop so take-off from the ground feels responsive.
		if (Move->Velocity.Z < 250.f) { Move->Velocity.Z = 250.f; }
		if (!bWasFlying && HasActorBegunPlay() && !bInCinematic) { UJarvisComponent::Notify(this, EAIEvent::ToFlight); }
	}
	else
	{
		// MOVE_Falling transitions to MOVE_Walking automatically on touchdown.
		Move->SetMovementMode(MOVE_Falling);
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
	Move->MaxFlySpeed = (bBuster ? HulkbusterFlySpeed : FlySpeed) * SpeedMul;
	Move->JumpZVelocity = bBuster ? HulkbusterJumpVelocity : JumpVelocity;
	Move->BrakingDecelerationFlying = FlightBraking;
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

	// Ceiling.
	const float Ceiling = FloorZAtSpawn + MaxFlightAltitude;
	if (Loc.Z > Ceiling && Move->Velocity.Z > 0.f) { Move->Velocity.Z = 0.f; }

	// Soft circular wall around the spawn point.
	const FVector Center = SpawnTransform.GetLocation();
	FVector Flat(Loc.X - Center.X, Loc.Y - Center.Y, 0.f);
	const float Dist = Flat.Size();
	if (Dist > ArenaBoundaryRadius)
	{
		const FVector Out = Flat / Dist;
		const float Outward = FVector::DotProduct(Move->Velocity, Out);
		if (Outward > 0.f) { Move->Velocity -= Out * Outward; }
		SetActorLocation(FVector(Center.X, Center.Y, Loc.Z) + Out * ArenaBoundaryRadius, true);
	}
}

// =====================================================================================
// Tick
// =====================================================================================

void AIronManCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDead && !bInCinematic)
	{
		InvulnerableLeft = FMath::Max(0.f, InvulnerableLeft - DeltaSeconds);
		TickEnergy(DeltaSeconds);
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
		const float CosLimit = FMath::Cos(FMath::DegreesToRadians(AimAssistDegrees));
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
	FireCooldown = FMath::Max(0.f, FireCooldown - DeltaSeconds);
	AimPoseTime = FMath::Max(0.f, AimPoseTime - DeltaSeconds);

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

	if (bFireHeld && FireCooldown <= 0.f && !IsBeamActive() && !bWheelOpen && Reactor->CanDrawPower())
	{
		FireRepulsor(); // sets FireCooldown for the suit's weapon
	}
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
	Reactor->AddHeat(Reactor->HeatPerFiringSecond * Interval);
	if (OverdriveLeft > 0.f) { Interval *= 0.5f; }
	FireCooldown = Interval;
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
	for (int32 i = 0; i < Count; ++i)
	{
		FVector ShotDir = Dir;
		if (Count > 1) { ShotDir = Dir.RotateAngleAxis((i - (Count - 1) * 0.5f) * FanDegrees, FVector::UpVector); }
		if (Spread > 0.f) { ShotDir = FMath::VRandCone(ShotDir, FMath::DegreesToRadians(Spread)); }
		if (AIronProjectile* P = AIronProjectile::SpawnPooled(World, Muzzle, ShotDir.Rotation(), this, this, false))
		{
			P->LifeSeconds = Life;
			// Perf budget §5.3: no point light on minigun / flamethrower shots (emissive only).
			P->Configure(Damage, false, Blast, Color, Radius, !bRapidFireWeapon);
			P->Launch(ShotDir * Speed);
		}
	}
	RepulsorLight->AttachToComponent(Palm, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RepulsorFlash = 1.f;
	RepulsorLight->SetIntensity(bFirstPerson ? 0.f : 8000.f);

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
		FireHoming(6, 1800.f, 6000.f, 3.f, 200.f, 12.f, 3.5f, S.AccentColor);
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

	--HulkbusterCharges;
	HulkbusterTimeLeft = HulkbusterDuration;
	Health = FMath::Min(MaxHealth, Health + HulkbusterActivationHeal);
	BeamTimeLeft = 0.f;
	bBeamWhip = false;

	// Grow the capsule; lift first so the bigger capsule doesn't start inside the floor.
	const float NewHalf = BaseCapsuleHalfHeight * HulkbusterScale;
	AddActorWorldOffset(FVector(0.f, 0.f, NewHalf - BaseCapsuleHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCapsuleSize(BaseCapsuleRadius * HulkbusterScale, NewHalf);
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
	if (bDead || bInCinematic || InvulnerableLeft > 0.f || DamageAmount <= 0.f) { return 0.f; }
	if (ShieldLeft > 0.f)
	{
		AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 160.f, GetSuit().EmissiveColor, 0.15f);
		return 0.f;
	}
	const float Mul = IsHulkbusterActive() ? HulkbusterDamageTaken : 1.f;
	const float Applied = DamageAmount * Mul;
	Super::TakeDamage(Applied, DamageEvent, EventInstigator, DamageCauser);

	Health -= Applied;
	InvulnerableLeft = InvulnerabilityAfterHit;
	HitFlashLeft = 0.15f;
	ArenaVisuals::FlashAll(Parts, FLinearColor(1.f, 0.5f, 0.3f), false);
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this)) { Feel->AddTrauma(FMath::Clamp(Applied / 25.f, 0.12f, 0.55f)); }

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
	const float LeanPitch = (bFly && !bInCinematic) ? -FMath::Clamp(Fwd / MaxSpeed, -0.5f, 1.f) * 35.f : 0.f;
	const float LeanRoll = (bFly && !bInCinematic) ? FMath::Clamp(Side / MaxSpeed, -1.f, 1.f) * 20.f : 0.f;
	if (DeltaSeconds > 0.f)
	{
		const FRotator Cur = SuitRoot->GetRelativeRotation();
		SuitRoot->SetRelativeRotation(FMath::RInterpTo(Cur, FRotator(LeanPitch, 0.f, LeanRoll), DeltaSeconds, 6.f));
	}

	// Arms: raised toward the crosshair while shooting / punching, otherwise hanging (or swept back in flight).
	float AimPitch = 0.f;
	if (Controller) { AimPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch); }
	const float BodyPitch = SuitRoot->GetRelativeRotation().Pitch;
	const float Raised = 90.f + AimPitch - BodyPitch;
	const float Idle = bFly ? -20.f : FMath::Sin(WalkCycle) * 25.f * FMath::Min(1.f, Vel.Size2D() / 500.f);

	const bool bAim = (AimPoseTime > 0.f || IsBeamActive()) && !bInCinematic;
	float PitchR = bAim ? Raised : Idle;
	float PitchL = bAim ? Raised : -Idle;
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
		else              { PitchL = FMath::Lerp(-Idle, Raised, 0.5f); }
	}
	ShoulderR->SetRelativeRotation(FRotator(PitchR, 0.f, bFly && !bAim ? 12.f : 0.f));
	ShoulderL->SetRelativeRotation(FRotator(PitchL, 0.f, bFly && !bAim ? -12.f : 0.f));

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
		const float Len = (bHand ? 0.3f : 0.45f) * Flicker;
		Flame->SetRelativeScale3D(FVector(Base, Base, Len));
	}

	// Repulsor flash fades.
	if (RepulsorLight && DeltaSeconds > 0.f)
	{
		RepulsorFlash = FMath::Max(0.f, RepulsorFlash - 8.f * DeltaSeconds);
		RepulsorLight->SetIntensity(bFirstPerson ? 0.f : 8000.f * RepulsorFlash);
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
		ArcLight->SetIntensity(ReactorMul * (1000.f + 3000.f * EnergyFrac * (0.85f + 0.15f * FMath::Sin(Time * 4.f))));
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
	FPPalmLight->SetIntensity(6000.f * FMath::Max(FlashL, FlashR));
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
