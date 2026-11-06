#include "ParkedSuit.h"
#include "ArenaAudio.h"
#include "ArenaBlastFX.h"
#include "ArenaVisuals.h"
#include "IronManCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

AParkedSuit::AParkedSuit()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	SetActorEnableCollision(false);
}

AParkedSuit* AParkedSuit::SpawnFrom(AIronManCharacter* Char, bool bInHover)
{
	UWorld* W = Char ? Char->GetWorld() : nullptr;
	USceneComponent* SR = Char ? Char->GetSuitRoot() : nullptr;
	if (!W || !SR) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform RootXf(FRotator(0.f, SR->GetComponentRotation().Yaw, 0.f), SR->GetComponentLocation());
	AParkedSuit* S = W->SpawnActor<AParkedSuit>(AParkedSuit::StaticClass(), RootXf, SP);
	if (!S) { return nullptr; }
	S->bHover = bInHover;
	for (const FArenaPaintedPart& Part : Char->GetSuitParts())
	{
		const UStaticMeshComponent* Src = Part.Component;
		if (!Src || !Src->GetStaticMesh() || !Src->IsVisible() || Src->bHiddenInGame) { continue; }
		if (Part.Role == ESuitPaintRole::Fixed && Src->GetName().Contains(TEXT("Flame"))) { continue; }
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(S);
		C->SetStaticMesh(Src->GetStaticMesh());
		for (int32 m = 0; m < Src->GetNumMaterials(); ++m) { C->SetMaterial(m, Src->GetMaterial(m)); }
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(S->Body);
		C->RegisterComponent();
		C->SetRelativeTransform(Src->GetComponentTransform().GetRelativeTransform(RootXf));
		S->Parts.Add(C);
	}
	if (bInHover) { S->Thrust = ArenaAudio::Attach(TEXT("thruster_idle"), S->Root, 0.35f, true); }
	return S;
}

void AParkedSuit::Call(AActor* Target)
{
	CalledBy = Target;
	bArrived = false;
	if (!Thrust) { Thrust = ArenaAudio::Attach(TEXT("thruster_flight_cruise"), Root, 0.7f, true); }
	ArenaAudio::PlayAt(this, TEXT("thruster_takeoff"), GetActorLocation(), 1.f);
	Vel = FVector(0, 0, 900.f);
}

void AParkedSuit::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	if (AActor* T = CalledBy.Get())
	{
		// Fly in an arc: climb, cruise, then decelerate onto a spot 1.2 m beside the pilot.
		const FVector Goal = T->GetActorLocation() + T->GetActorRightVector() * 120.f - FVector(0, 0, T->GetSimpleCollisionHalfHeight());
		const FVector To = Goal - GetActorLocation();
		const float D = To.Size();
		const float Want = FMath::Clamp(D * 1.6f, 300.f, 4500.f);
		FVector Desired = To.GetSafeNormal() * Want;
		if (D > 1500.f) { Desired.Z += 600.f; }
		Vel = FMath::VInterpTo(Vel, Desired, Dt, 2.5f);
		SetActorLocation(GetActorLocation() + Vel * Dt);
		const FRotator Face = FRotator(FMath::Clamp(-Vel.Size() * 0.012f, -60.f, 0.f), Vel.Rotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), D < 400.f ? FRotator(0.f, T->GetActorRotation().Yaw, 0.f) : Face, Dt, 5.f));
		if (Thrust) { Thrust->SetPitchMultiplier(0.8f + FMath::Min(1.f, Vel.Size() / 4000.f) * 0.5f); }
		if (D < 90.f && !bArrived)
		{
			bArrived = true;
			ArenaAudio::PlayAt(this, TEXT("thruster_landing"), GetActorLocation(), 1.f);
			AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 120.f, FLinearColor(0.6f, 0.85f, 1.f), 0.25f, false);
		}
		return;
	}
	if (bHover) { Body->SetRelativeLocation(FVector(0.f, 0.f, 6.f * FMath::Sin(Age * 2.2f))); Body->SetRelativeRotation(FRotator(1.5f * FMath::Sin(Age * 1.3f), 0.f, 1.2f * FMath::Sin(Age * 1.7f))); }
}

void AParkedSuit::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Thrust) { Thrust->Stop(); Thrust = nullptr; }
	Super::EndPlay(Reason);
}
