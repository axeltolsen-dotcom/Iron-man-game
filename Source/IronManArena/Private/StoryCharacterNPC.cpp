#include "StoryCharacterNPC.h"
#include "SuitArmorKit.h"

#include "IronManArena.h"
#include "SuitTypes.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"

using namespace ArenaVisuals;

const TCHAR* AStoryCharacterNPC::DefaultMannyPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny");
const TCHAR* AStoryCharacterNPC::DefaultQuinnPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn");

const TCHAR* const* AStoryCharacterNPC::GetDefaultAnimClassPaths(int32& OutNum)
{
	// Third Person template animation blueprints across engine versions (first found wins).
	static const TCHAR* const Paths[] =
	{
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"),   // 5.4+ template
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"),          // 5.0 - 5.3 template
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Quinn.ABP_Quinn_C"),
	};
	OutNum = UE_ARRAY_COUNT(Paths);
	return Paths;
}

namespace
{
	const FLinearColor Skin(0.72f, 0.52f, 0.4f);
	const FLinearColor Hair(0.12f, 0.08f, 0.05f);
	const FLinearColor Denim(0.12f, 0.16f, 0.25f);
	const FLinearColor RobotGrey(0.35f, 0.36f, 0.38f);
	const FLinearColor RobotDark(0.08f, 0.08f, 0.09f);
	const FLinearColor SynthRed(0.45f, 0.06f, 0.1f);
	const FLinearColor SynthGreen(0.08f, 0.35f, 0.2f);
	const FLinearColor Gem(1.f, 0.85f, 0.25f);

	FString PackageOf(const FString& ObjectPath)
	{
		FString Package = ObjectPath;
		int32 Dot;
		if (Package.FindChar(TEXT('.'), Dot)) { Package.LeftInline(Dot); }
		return Package;
	}
}

AStoryCharacterNPC::AStoryCharacterNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); // actors on a stage, not obstacles
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // mannequin convention
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetVisibility(false);

	// Placeholder bodies: feet at ShapeRoot Z = 0, facing +X, ~175 cm.
	ShapeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShapeRoot"));
	ShapeRoot->SetupAttachment(GetCapsuleComponent());
	ShapeRoot->SetRelativeLocation(FVector(0.f, 0.f, -88.f));

	auto MakeRoot = [this](const TCHAR* Name) -> USceneComponent*
	{
		USceneComponent* C = CreateDefaultSubobject<USceneComponent>(Name);
		C->SetupAttachment(ShapeRoot);
		return C;
	};
	HumanRoot = MakeRoot(TEXT("HumanRoot"));
	RobotRoot = MakeRoot(TEXT("RobotRoot"));
	SynthRoot = MakeRoot(TEXT("SynthRoot"));
	SuitRoot = MakeRoot(TEXT("SuitRoot"));

	auto Add = [this](USceneComponent* Parent, const TCHAR* Name, EArenaShape Shape, FVector Loc, FVector Scale,
		const FLinearColor& Color, float Emissive = 0.f, FRotator Rot = FRotator::ZeroRotator) -> int32
	{
		AddPart(this, Parent, FName(Name), Shape, Loc, Scale, Rot, Color, Emissive, Parts);
		return Parts.Num() - 1;
	};

	// --- Human (also used for the hologram) ---
	Add(HumanRoot, TEXT("HLegL"), EArenaShape::Cube, FVector(0, -9, 45), FVector(0.13f, 0.13f, 0.88f), Denim);
	Add(HumanRoot, TEXT("HLegR"), EArenaShape::Cube, FVector(0, 9, 45), FVector(0.13f, 0.13f, 0.88f), Denim);
	TintParts.Add(Add(HumanRoot, TEXT("HTorso"), EArenaShape::Cube, FVector(0, 0, 118), FVector(0.22f, 0.36f, 0.55f), FLinearColor::Gray));
	TintParts.Add(Add(HumanRoot, TEXT("HArmL"), EArenaShape::Cube, FVector(0, -23, 113), FVector(0.09f, 0.09f, 0.58f), FLinearColor::Gray));
	TintParts.Add(Add(HumanRoot, TEXT("HArmR"), EArenaShape::Cube, FVector(0, 23, 113), FVector(0.09f, 0.09f, 0.58f), FLinearColor::Gray));
	Add(HumanRoot, TEXT("HHandL"), EArenaShape::Sphere, FVector(0, -23, 82), FVector(0.085f), Skin);
	Add(HumanRoot, TEXT("HHandR"), EArenaShape::Sphere, FVector(0, 23, 82), FVector(0.085f), Skin);
	HeadPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadPivot"));
	HeadPivot->SetupAttachment(HumanRoot);
	HeadPivot->SetRelativeLocation(FVector(0, 0, 148));
	Add(HeadPivot, TEXT("HNeck"), EArenaShape::Cylinder, FVector(0, 0, 0), FVector(0.08f, 0.08f, 0.1f), Skin);
	Add(HeadPivot, TEXT("HHead"), EArenaShape::Sphere, FVector(0, 0, 15), FVector(0.21f, 0.19f, 0.24f), Skin);
	Add(HeadPivot, TEXT("HHair"), EArenaShape::Sphere, FVector(-3, 0, 21), FVector(0.22f, 0.2f, 0.17f), Hair);

	// --- Robot arm (U / DUM-E): wheeled base, column, boom, claw, "eye" light ---
	Add(RobotRoot, TEXT("RBase"), EArenaShape::Cylinder, FVector(0, 0, 10), FVector(0.55f, 0.55f, 0.2f), RobotDark);
	Add(RobotRoot, TEXT("RColumn"), EArenaShape::Cylinder, FVector(0, 0, 60), FVector(0.16f, 0.16f, 0.85f), RobotGrey);
	Add(RobotRoot, TEXT("RJoint"), EArenaShape::Sphere, FVector(0, 0, 105), FVector(0.22f), RobotDark);
	Add(RobotRoot, TEXT("RBoom"), EArenaShape::Cube, FVector(28, 0, 120), FVector(0.6f, 0.1f, 0.1f), RobotGrey, 0.f, FRotator(25, 0, 0));
	Add(RobotRoot, TEXT("RClawA"), EArenaShape::Cube, FVector(58, -5, 133), FVector(0.18f, 0.03f, 0.06f), RobotDark, 0.f, FRotator(10, 15, 0));
	Add(RobotRoot, TEXT("RClawB"), EArenaShape::Cube, FVector(58, 5, 133), FVector(0.18f, 0.03f, 0.06f), RobotDark, 0.f, FRotator(10, -15, 0));
	GemPart = INDEX_NONE;
	Add(RobotRoot, TEXT("REye"), EArenaShape::Sphere, FVector(48, 0, 138), FVector(0.05f), FLinearColor(0.3f, 0.8f, 1.f), 12.f);

	// --- Synthezoid (Vison): a nod to the Vision Quest - red / green plating, gold gem, cape ---
	Add(SynthRoot, TEXT("SLegL"), EArenaShape::Cube, FVector(0, -9, 46), FVector(0.13f, 0.13f, 0.9f), SynthGreen);
	Add(SynthRoot, TEXT("SLegR"), EArenaShape::Cube, FVector(0, 9, 46), FVector(0.13f, 0.13f, 0.9f), SynthGreen);
	Add(SynthRoot, TEXT("STorso"), EArenaShape::Cube, FVector(0, 0, 122), FVector(0.22f, 0.38f, 0.58f), SynthGreen);
	Add(SynthRoot, TEXT("SArmL"), EArenaShape::Cube, FVector(0, -24, 116), FVector(0.09f, 0.09f, 0.6f), SynthGreen);
	Add(SynthRoot, TEXT("SArmR"), EArenaShape::Cube, FVector(0, 24, 116), FVector(0.09f, 0.09f, 0.6f), SynthGreen);
	Add(SynthRoot, TEXT("SHead"), EArenaShape::Sphere, FVector(0, 0, 168), FVector(0.2f, 0.18f, 0.24f), SynthRed);
	Add(SynthRoot, TEXT("SCape"), EArenaShape::Cube, FVector(-15, 0, 112), FVector(0.03f, 0.42f, 0.95f), FLinearColor(0.75f, 0.6f, 0.1f));
	Add(SynthRoot, TEXT("SGem"), EArenaShape::Sphere, FVector(10, 0, 176), FVector(0.05f), Gem, 25.f);
	GemPart = Parts.Num() - 1;

	// --- Suit (Morgan in Rescue for the finale): the shared placeholder armour ---
	SuitRig::Build(this, SuitRoot, Rig, SuitParts);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(ShapeRoot);
	GlowLight->SetRelativeLocation(FVector(30, 0, 160));
	GlowLight->SetIntensity(0.f);
	GlowLight->SetAttenuationRadius(250.f);
	GlowLight->SetCastShadows(false);

	HumanRoot->SetVisibility(false, true);
	RobotRoot->SetVisibility(false, true);
	SynthRoot->SetVisibility(false, true);
	SuitRoot->SetVisibility(false, true);
}

void AStoryCharacterNPC::BeginPlay()
{
	Super::BeginPlay();
	PaintAll(this, Parts);
	PaintAll(this, SuitParts);
}

bool AStoryCharacterNPC::SpawnMetaHuman(const FStoryCastMember& Member)
{
	if (MetaHumanActor) { MetaHumanActor->Destroy(); MetaHumanActor = nullptr; }
	if (Member.MetaHumanClass.IsNull() || !FPackageName::DoesPackageExist(Member.MetaHumanClass.GetLongPackageName())) { return false; }
	UClass* Cls = Member.MetaHumanClass.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!Cls || !World) { return false; }

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// MetaHuman Blueprints have their origin at the feet; we are a capsule centred at half height.
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FTransform At(GetActorRotation(), GetActorLocation() - FVector(0.f, 0.f, HalfHeight));
	AActor* MH = World->SpawnActor<AActor>(Cls, At, Params);
	if (!MH) { return false; }
	MH->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	MH->SetActorEnableCollision(false);
	MetaHumanActor = MH;
	return true;
}

void AStoryCharacterNPC::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);
	if (MetaHumanActor) { MetaHumanActor->SetActorHiddenInGame(bNewHidden); }
}

void AStoryCharacterNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MetaHumanActor) { MetaHumanActor->Destroy(); MetaHumanActor = nullptr; }
	Super::EndPlay(EndPlayReason);
}

void AStoryCharacterNPC::SetupCast(const FStoryCastMember& Member)
{
	CastInfo = Member;
	SetActorScale3D(FVector(FMath::Clamp(Member.Scale, 0.3f, 3.f)));

	// 0) A full MetaHuman Blueprint wins: spawn it attached to us and hide our own visuals.
	bUsingMesh = false;
	const bool bMeshBody = Member.Body == EStoryBody::Human || Member.Body == EStoryBody::Synthezoid;
	if (bMeshBody && SpawnMetaHuman(Member))
	{
		bUsingMesh = true;
		GetMesh()->SetVisibility(false);
		HumanRoot->SetVisibility(false, true);
		RobotRoot->SetVisibility(false, true);
		SynthRoot->SetVisibility(false, true);
		SuitRoot->SetVisibility(false, true);
		return;
	}

	// 1) Skeletal mesh (humans / synthezoid; MetaHuman bodies work here too, without the face rig).
	if (bMeshBody && !Member.Mesh.IsNull() && FPackageName::DoesPackageExist(Member.Mesh.GetLongPackageName()))
	{
		if (USkeletalMesh* SK = Member.Mesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMeshAsset(SK);
			GetMesh()->SetVisibility(true);
			bUsingMesh = true;

			UClass* Anim = nullptr;
			if (!Member.AnimClass.IsNull() && FPackageName::DoesPackageExist(Member.AnimClass.GetLongPackageName()))
			{
				Anim = Member.AnimClass.LoadSynchronous();
			}
			if (!Anim)
			{
				int32 Num = 0;
				const TCHAR* const* Paths = GetDefaultAnimClassPaths(Num);
				for (int32 i = 0; i < Num && !Anim; ++i)
				{
					if (FPackageName::DoesPackageExist(PackageOf(Paths[i])))
					{
						Anim = LoadClass<UAnimInstance>(nullptr, Paths[i]);
					}
				}
			}
			if (Anim) { GetMesh()->SetAnimInstanceClass(Anim); }
		}
	}

	// 2) Placeholder shapes.
	HumanRoot->SetVisibility(!bUsingMesh && (Member.Body == EStoryBody::Human || Member.Body == EStoryBody::Hologram), true);
	RobotRoot->SetVisibility(Member.Body == EStoryBody::Robot, true);
	SynthRoot->SetVisibility(!bUsingMesh && Member.Body == EStoryBody::Synthezoid, true);
	SuitRoot->SetVisibility(Member.Body == EStoryBody::Suit, true);
	if (!bUsingMesh && Member.Body != EStoryBody::Suit) { GetMesh()->SetVisibility(false); }

	const bool bHolo = Member.Body == EStoryBody::Hologram;
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		FArenaPaintedPart& P = Parts[i];
		if (TintParts.Contains(i)) { P.Color = Member.Tint; }
		if (bHolo && P.Component && P.Component->IsAttachedTo(HumanRoot))
		{
			P.Color = FLinearColor(0.25f, 0.6f, 1.f);
			P.Emissive = 6.f;
		}
		RepaintPart(P);
	}

	if (Member.Body == EStoryBody::Suit)
	{
		const int32 Idx = USuitLibrary::FindSuitIndex(Member.SuitId);
		const FSuitDef& S = USuitLibrary::GetSuit(Idx == INDEX_NONE ? 0 : Idx);
		// U's armour plates over the placeholder rig (game worlds only; meshes are built at runtime).
		if (GetWorld() && GetWorld()->IsGameWorld() && SuitArmorKit::IsLoaded()) { SuitArmorKit::Build(this, S, Rig, SuitParts); }
		SuitRig::ApplyColors(S, SuitParts);
		for (FArenaPaintedPart& P : SuitParts) { RepaintPart(P); }
		SuitBaseColors.Reset();
		SuitBaseGlow.Reset();
		for (const FArenaPaintedPart& P : SuitParts) { SuitBaseColors.Add(P.Color); SuitBaseGlow.Add(P.Emissive); }
		AppliedDamage = 0.f;
		SuitRig::ApplyVisibility(S, Rig, true, false);
		if (Rig.FPRoot) { Rig.FPRoot->SetVisibility(false, true); }
		const float P = SuitRig::PlatingScale(S.Plating);
		SuitRoot->SetRelativeScale3D(FVector(P, P, 1.f));
		GlowLight->SetLightColor(S.EmissiveColor);
		GlowLight->SetIntensity(1500.f);
	}
	else if (Member.Body == EStoryBody::Synthezoid)
	{
		GlowLight->SetLightColor(Gem);
		GlowLight->SetIntensity(800.f);
	}
	else if (bHolo)
	{
		GlowLight->SetLightColor(FLinearColor(0.25f, 0.6f, 1.f));
		GlowLight->SetIntensity(2500.f);
	}
	else
	{
		GlowLight->SetIntensity(0.f);
	}
}

void AStoryCharacterNPC::SetWounded(float Damage, float ReactorLevel)
{
	if (CastInfo.Body != EStoryBody::Suit || SuitBaseColors.Num() != SuitParts.Num()) { return; }
	const float D = FMath::Clamp(Damage, 0.f, 1.f);
	const float R = FMath::Clamp(ReactorLevel, 0.f, 1.f);
	const bool bRepaint = !FMath::IsNearlyEqual(D, AppliedDamage, 0.01f);
	// Soot and heat-blued scoring: darker, less saturated plating.
	const FLinearColor Soot(0.045f, 0.042f, 0.04f);
	for (int32 i = 0; i < SuitParts.Num(); ++i)
	{
		FArenaPaintedPart& P = SuitParts[i];
		if (SuitBaseGlow[i] > 0.f)
		{
			SetPartGlow(P, SuitBaseGlow[i] * R);
		}
		else if (bRepaint)
		{
			// Every third plate takes the worst of it so the damage reads as patches, not a uniform tint.
			const float Patch = (i % 3 == 0) ? 0.85f : 0.45f;
			P.Color = FMath::Lerp(SuitBaseColors[i], Soot, D * Patch);
			RepaintPart(P);
		}
	}
	if (bRepaint) { AppliedDamage = D; }
	if (GlowLight) { GlowLight->SetIntensity(1500.f * R * R); }
}

FVector AStoryCharacterNPC::GetFocusPoint() const
{
	const float S = GetActorScale3D().Z;
	float Eye = 160.f;
	if (CastInfo.Body == EStoryBody::Robot) { Eye = 125.f; }
	return GetActorLocation() + FVector(0.f, 0.f, (Eye - 88.f) * S);
}

void AStoryCharacterNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// Idle life on the placeholder: breathing, a nod while speaking, hologram shimmer.
	if (!bUsingMesh && HeadPivot)
	{
		const float Nod = bSpeaking ? 4.f * FMath::Sin(Time * 9.f) : 1.2f * FMath::Sin(Time * 1.3f);
		HeadPivot->SetRelativeRotation(FRotator(Nod, 6.f * FMath::Sin(Time * 0.7f), 0.f));
	}
	if (ShapeRoot)
	{
		const float Breath = 1.f + 0.012f * FMath::Sin(Time * 2.1f);
		ShapeRoot->SetRelativeScale3D(FVector(1.f, Breath, Breath));
	}
	if (CastInfo.Body == EStoryBody::Hologram)
	{
		GlowLight->SetIntensity(2000.f + 800.f * FMath::Sin(Time * 23.f) * FMath::Sin(Time * 3.f));
	}
	if (CastInfo.Body == EStoryBody::Robot && bSpeaking && RobotRoot)
	{
		RobotRoot->SetRelativeRotation(FRotator(0.f, 10.f * FMath::Sin(Time * 6.f), 0.f));
	}
	if (Parts.IsValidIndex(GemPart) && CastInfo.Body == EStoryBody::Synthezoid)
	{
		SetPartGlow(Parts[GemPart], 20.f + (bSpeaking ? 15.f : 5.f) * (0.5f + 0.5f * FMath::Sin(Time * 5.f)));
	}
}
