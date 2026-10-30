#include "StoryAlly.h"

#include "EnemyBase.h"
#include "ArenaBlastFX.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"

using namespace ArenaVisuals;

namespace
{
	const FLinearColor AllyRed(0.45f, 0.06f, 0.1f);
	const FLinearColor AllyGreen(0.08f, 0.35f, 0.2f);
	const FLinearColor AllyGold(1.f, 0.85f, 0.25f);
}

AStoryAlly::AStoryAlly()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeLocation(FVector(0, 0, -90)); // feet ~90 uu below the actor origin

	auto Add = [this](const TCHAR* Name, EArenaShape Shape, FVector Loc, FVector Scale, const FLinearColor& C, float E = 0.f, FRotator R = FRotator::ZeroRotator)
	{
		AddPart(this, Body, FName(Name), Shape, Loc, Scale, R, C, E, Parts);
	};
	Add(TEXT("LegL"), EArenaShape::Cube, FVector(0, -9, 46), FVector(0.13f, 0.13f, 0.9f), AllyGreen);
	Add(TEXT("LegR"), EArenaShape::Cube, FVector(0, 9, 46), FVector(0.13f, 0.13f, 0.9f), AllyGreen);
	Add(TEXT("Torso"), EArenaShape::Cube, FVector(0, 0, 122), FVector(0.22f, 0.38f, 0.58f), AllyGreen);
	Add(TEXT("ChestLine"), EArenaShape::Cube, FVector(11.5f, 0, 128), FVector(0.01f, 0.3f, 0.03f), AllyGold, 4.f);
	Add(TEXT("ArmL"), EArenaShape::Cube, FVector(0, -24, 116), FVector(0.09f, 0.09f, 0.6f), AllyRed);
	Add(TEXT("ArmR"), EArenaShape::Cube, FVector(0, 24, 116), FVector(0.09f, 0.09f, 0.6f), AllyRed);
	Add(TEXT("Head"), EArenaShape::Sphere, FVector(0, 0, 168), FVector(0.2f, 0.18f, 0.24f), AllyRed);
	Add(TEXT("Cape"), EArenaShape::Cube, FVector(-15, 0, 112), FVector(0.03f, 0.42f, 0.95f), FLinearColor(0.75f, 0.6f, 0.1f), 0.f, FRotator(-8, 0, 0));
	Add(TEXT("Gem"), EArenaShape::Sphere, FVector(10, 0, 176), FVector(0.05f), AllyGold, 30.f);

	GemLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GemLight"));
	GemLight->SetupAttachment(Body);
	GemLight->SetRelativeLocation(FVector(20, 0, 176));
	GemLight->SetLightColor(AllyGold);
	GemLight->SetIntensity(2500.f);
	GemLight->SetAttenuationRadius(500.f);
	GemLight->SetCastShadows(false);

	// Beam: a unit cylinder along +X from the gem, scaled to the target distance each frame.
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	BeamMesh->SetupAttachment(Root);
	BeamMesh->SetStaticMesh(GetShapeMesh(EArenaShape::Cylinder));
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetCastShadow(false);
	BeamMesh->SetVisibility(false);
}

void AStoryAlly::BeginPlay()
{
	Super::BeginPlay();
	PaintAll(this, Parts);
	BeamMID = PaintComponent(this, BeamMesh, AllyGold, 40.f);
}

void AStoryAlly::ApplyLook(FName AllyId)
{
	AllyKind = AllyId;
	if (AllyId != FName(TEXT("jocasta"))) { return; }
	// Jocasta: a borrowed chassis in pale silver with a quiet blue core, no cape.
	// Fights up close with short energy bursts (see contrib/jocasta/vision_quest_side_mission.md).
	const FLinearColor Silver(0.72f, 0.74f, 0.78f);
	const FLinearColor Pale(0.55f, 0.82f, 1.f);
	GlowColor = Pale;
	BeamRange = 1800.f;
	BeamDamagePerSecond = 85.f;
	FollowDistance = 300.f;
	for (FArenaPaintedPart& Part : Parts)
	{
		if (!Part.Component) { continue; }
		const FName N = Part.Component->GetFName();
		if (N == TEXT("Cape")) { Part.Component->SetVisibility(false); continue; }
		if (N == TEXT("Gem") || N == TEXT("ChestLine")) { Part.Color = Pale; }
		else if (N == TEXT("Head")) { Part.Color = Silver * 1.1f; }
		else { Part.Color = Silver; }
		Part.Color.A = 1.f;
		RepaintPart(Part);
	}
	if (GemLight) { GemLight->SetLightColor(Pale); }
	BeamMID = PaintComponent(this, BeamMesh, Pale, 40.f);
}

AStoryAlly* AStoryAlly::Summon(UWorld* World, AActor* Player, float Duration, FName AllyId)
{
	if (!World || !Player) { return nullptr; }
	const FVector Loc = Player->GetActorLocation() + Player->GetActorRightVector() * 260.f + FVector(0, 0, 300.f);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStoryAlly* A = World->SpawnActor<AStoryAlly>(AStoryAlly::StaticClass(), Loc, Player->GetActorRotation(), P);
	if (A)
	{
		A->FollowTarget = Player;
		A->TimeLeft = FMath::Max(1.f, Duration);
		A->ApplyLook(AllyId);
		AArenaBlastFX::Spawn(World, Loc, 260.f, A->GlowColor, 0.5f);
	}
	return A;
}

void AStoryAlly::Dismiss()
{
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 220.f, GlowColor, 0.4f);
	Destroy();
}

AEnemyBase* AStoryAlly::FindTarget() const
{
	AEnemyBase* Best = nullptr;
	float BestD = BeamRange * BeamRange;
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) { continue; }
		const float D = FVector::DistSquared(It->GetAimPoint(), GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

void AStoryAlly::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	TimeLeft -= DeltaSeconds;
	if (TimeLeft <= 0.f || !FollowTarget.IsValid()) { Dismiss(); return; }

	// Follow: hover beside the player, drifting slowly around them.
	AActor* P = FollowTarget.Get();
	const FVector Side = P->GetActorRightVector() * OrbitSide * FollowDistance;
	const FVector Bob(0, 0, FollowHeight + 25.f * FMath::Sin(Time * 1.7f));
	const FVector Goal = P->GetActorLocation() + Side - P->GetActorForwardVector() * 80.f + Bob;
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), Goal, DeltaSeconds, 3.f));
	if (FMath::Fmod(Time, 9.f) < DeltaSeconds) { OrbitSide = -OrbitSide; }

	Retarget -= DeltaSeconds;
	if (Retarget <= 0.f || !Target.IsValid() || Target->IsDead())
	{
		Target = FindTarget();
		Retarget = 0.6f;
	}

	FRotator Face = P->GetActorRotation();
	if (AEnemyBase* T = Target.Get())
	{
		const FVector From = GetActorLocation() + FVector(0, 0, 86.f);
		const FVector To = T->GetAimPoint();
		const FVector Dir = To - From;
		const float Len = Dir.Size();
		Face = FRotator(0.f, Dir.Rotation().Yaw, 0.f);
		// Cylinder axis is Z: rotate Z onto Dir, centre halfway.
		BeamMesh->SetWorldLocation(From + Dir * 0.5f);
		BeamMesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
		const float Pulse = 0.06f + 0.02f * FMath::Sin(Time * 30.f);
		BeamMesh->SetWorldScale3D(FVector(Pulse, Pulse, Len / 100.f));
		BeamMesh->SetVisibility(true);
		// Damage in 0.25 s pulses (avoids re-triggering the hit flash every frame).
		DamageTick -= DeltaSeconds;
		if (DamageTick <= 0.f)
		{
			DamageTick = 0.25f;
			T->TakeDamage(BeamDamagePerSecond * 0.25f, FDamageEvent(UDamageType::StaticClass()), nullptr, this);
		}
		GemLight->SetIntensity(4000.f + 1500.f * FMath::Sin(Time * 25.f));
	}
	else
	{
		BeamMesh->SetVisibility(false);
		GemLight->SetIntensity(2000.f);
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Face, DeltaSeconds, 6.f));
}
