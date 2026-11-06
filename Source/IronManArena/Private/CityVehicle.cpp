#include "CityVehicle.h"

#include "CareerSubsystem.h"
#include "CityLife.h"
#include "IronManCharacter.h"
#include "PilotAvatar.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

ACityVehicle::ACityVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	SetRootComponent(Box);
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Box);
	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Box);
	Arm->bEnableCameraLag = true;
	Arm->bEnableCameraRotationLag = true;
	Arm->CameraLagSpeed = 8.f;
	Arm->CameraRotationLagSpeed = 6.f;
	Arm->bDoCollisionTest = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm);
	Camera->SetFieldOfView(80.f);
}

ACityVehicle* ACityVehicle::SpawnVehicle(UWorld* World, FName Id, const FTransform& Xf)
{
	if (!World || !Career::FindVehicle(Id)) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ACityVehicle* V = World->SpawnActor<ACityVehicle>(ACityVehicle::StaticClass(), Xf, SP);
	if (V) { V->Setup(Id); }
	return V;
}

UStaticMeshComponent* ACityVehicle::Part(USceneComponent* Parent, EArenaShape Shape, const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color,
	EArenaSurface Surface, float Emissive, const FRotator& Rot)
{
	UStaticMeshComponent* C = ArenaVisuals::AddRuntimePart(this, Parent ? Parent : Body.Get(), Shape, Loc, SizeCm / 100.f, Rot, Color, Emissive, Surface, false, Parts);
	if (C) { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	return C;
}

void ACityVehicle::Setup(FName Id)
{
	VehicleId = Id;
	const FVehicleDef* D = Career::FindVehicle(Id);
	if (!D) { return; }
	Kind = D->Kind;
	TopSpeed = D->TopSpeedKmh / 0.036f;
	HalfSize = D->Size * 0.5f;
	switch (Kind)
	{
	case ECityVehicleKind::Car: BuildCar(*D); break;
	case ECityVehicleKind::Bike: BuildBike(*D); break;
	case ECityVehicleKind::Plane: BuildPlane(*D, D->Id == FName(TEXT("private-jet"))); break;
	case ECityVehicleKind::Helicopter: BuildHeli(*D); break;
	}
	Box->SetBoxExtent(FVector(HalfSize.X, HalfSize.Y, FMath::Max(40.f, HalfSize.Z)));
	Body->SetRelativeLocation(FVector(0, 0, -FMath::Max(40.f, HalfSize.Z)));
	const float Len = D->Size.X;
	Arm->TargetArmLength = Kind == ECityVehicleKind::Plane ? Len * 1.6f + 800.f : Len * 1.4f + 350.f;
	Arm->SetRelativeLocation(FVector(0, 0, HalfSize.Z + 80.f));
	Arm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	Arm->bInheritRoll = false;
}

void ACityVehicle::BuildCar(const FVehicleDef& D)
{
	const FVector S = D.Size;
	const FLinearColor Glass(0.02f, 0.025f, 0.03f);
	const bool bSuper = D.Id == FName(TEXT("supercar")), bMuscle = D.Id == FName(TEXT("muscle")), bSuv = D.Id == FName(TEXT("suv"));
	const float BodyH = S.Z * (bSuv ? 0.55f : 0.45f);
	const float WheelR = bSuv ? 42.f : 34.f;
	Part(Body, EArenaShape::Cube, FVector(0, 0, WheelR + BodyH * 0.5f - 6.f), FVector(S.X, S.Y, BodyH), D.Paint, EArenaSurface::SuitPaint);
	const float CabL = S.X * (bSuper ? 0.42f : bSuv ? 0.62f : 0.5f);
	const float CabH = S.Z - BodyH - WheelR + 10.f;
	Part(Body, EArenaShape::Cube, FVector(bSuper ? -10.f : -S.X * 0.06f, 0, WheelR + BodyH + CabH * 0.5f - 10.f), FVector(CabL, S.Y * 0.88f, CabH), Glass, EArenaSurface::Steel);
	Part(Body, EArenaShape::Cube, FVector(bSuper ? -10.f : -S.X * 0.06f, 0, WheelR + BodyH + CabH - 8.f), FVector(CabL * 0.92f, S.Y * 0.86f, 6.f), D.Paint, EArenaSurface::SuitPaint);
	if (bMuscle) { Part(Body, EArenaShape::Cube, FVector(0, 0, WheelR + BodyH - 4.f), FVector(S.X + 1.f, 34.f, 2.f), FLinearColor(0.9f, 0.9f, 0.9f)); Part(Body, EArenaShape::Cube, FVector(S.X * 0.25f, 0, WheelR + BodyH + 6.f), FVector(70.f, 60.f, 18.f), FLinearColor(0.1f, 0.1f, 0.1f), EArenaSurface::Steel); }
	if (bSuper) { Part(Body, EArenaShape::Cube, FVector(-S.X * 0.47f, 0, WheelR + BodyH + 30.f), FVector(40.f, S.Y * 0.9f, 5.f), FLinearColor(0.03f, 0.03f, 0.03f), EArenaSurface::Steel); }
	if (bSuv) { Part(Body, EArenaShape::Cube, FVector(-S.X * 0.06f, 0, S.Z + 4.f), FVector(CabL * 0.8f, S.Y * 0.8f, 5.f), FLinearColor(0.1f, 0.1f, 0.1f), EArenaSurface::Steel); }
	Part(Body, EArenaShape::Cube, FVector(S.X * 0.5f, 0, WheelR + BodyH * 0.6f), FVector(4.f, S.Y * 0.8f, 12.f), FLinearColor(1.f, 0.95f, 0.85f), EArenaSurface::Plain, 1.f);
	Part(Body, EArenaShape::Cube, FVector(-S.X * 0.5f, 0, WheelR + BodyH * 0.7f), FVector(4.f, S.Y * 0.8f, 10.f), FLinearColor(1.f, 0.02f, 0.02f), EArenaSurface::Plain, 0.6f);
	for (const FVector2D& W : { FVector2D(0.32f, 0.44f), FVector2D(0.32f, -0.44f), FVector2D(-0.32f, 0.44f), FVector2D(-0.32f, -0.44f) })
	{
		USceneComponent* Hub = NewObject<USceneComponent>(this);
		Hub->SetupAttachment(Body);
		Hub->SetRelativeLocation(FVector(W.X * S.X, W.Y * S.Y, WheelR));
		Hub->RegisterComponent();
		Wheels.Add(Hub);
		Part(Hub, EArenaShape::Cylinder, FVector::ZeroVector, FVector(WheelR * 2.f, WheelR * 2.f, bSuv ? 30.f : 26.f), FLinearColor(0.015f, 0.015f, 0.015f), EArenaSurface::Plain, 0.f, FRotator(0.f, 0.f, 90.f));
		Part(Hub, EArenaShape::Cylinder, FVector(0, W.Y > 0 ? 14.f : -14.f, 0), FVector(WheelR * 1.2f, WheelR * 1.2f, 2.f), FLinearColor(0.6f, 0.6f, 0.62f), EArenaSurface::Steel, 0.f, FRotator(0.f, 0.f, 90.f));
	}
}

void ACityVehicle::BuildBike(const FVehicleDef& D)
{
	const FVector S = D.Size;
	const bool bCruiser = D.Id == FName(TEXT("cruiser-bike"));
	for (const float X : { S.X * 0.36f, -S.X * 0.36f })
	{
		USceneComponent* Hub = NewObject<USceneComponent>(this);
		Hub->SetupAttachment(Body);
		Hub->SetRelativeLocation(FVector(X, 0, 32.f));
		Hub->RegisterComponent();
		Wheels.Add(Hub);
		Part(Hub, EArenaShape::Cylinder, FVector::ZeroVector, FVector(64.f, 64.f, 14.f), FLinearColor(0.015f, 0.015f, 0.015f), EArenaSurface::Plain, 0.f, FRotator(0.f, 0.f, 90.f));
	}
	Part(Body, EArenaShape::Cube, FVector(0, 0, 62.f), FVector(S.X * 0.55f, 28.f, 34.f), D.Paint, EArenaSurface::SuitPaint, 0.f, FRotator(bCruiser ? 0.f : -6.f, 0.f, 0.f));
	Part(Body, EArenaShape::Cube, FVector(-S.X * 0.12f, 0, 84.f), FVector(60.f, 26.f, 8.f), FLinearColor(0.03f, 0.03f, 0.03f));
	Part(Body, EArenaShape::Cube, FVector(S.X * 0.3f, 0, 96.f), FVector(8.f, bCruiser ? 80.f : 62.f, 5.f), FLinearColor(0.5f, 0.5f, 0.52f), EArenaSurface::Steel);
	Part(Body, EArenaShape::Cube, FVector(S.X * 0.33f, 0, 66.f), FVector(6.f, 8.f, 60.f), FLinearColor(0.5f, 0.5f, 0.52f), EArenaSurface::Steel, 0.f, FRotator(-20.f, 0.f, 0.f));
	Part(Body, EArenaShape::Cube, FVector(S.X * 0.42f, 0, 78.f), FVector(4.f, 16.f, 10.f), FLinearColor(1.f, 0.95f, 0.85f), EArenaSurface::Plain, 1.f);
	Part(Body, EArenaShape::Cylinder, FVector(-S.X * 0.15f, 18.f, 45.f), FVector(12.f, 12.f, 70.f), FLinearColor(0.6f, 0.6f, 0.62f), EArenaSurface::Steel, 0.f, FRotator(90.f, 0.f, 0.f));
	HalfSize.Y = 45.f;
}

void ACityVehicle::BuildPlane(const FVehicleDef& D, bool bJet)
{
	const FVector S = D.Size;
	const FLinearColor White(0.85f, 0.86f, 0.88f);
	const float FusR = bJet ? 160.f : 110.f;
	Part(Body, EArenaShape::Cylinder, FVector(0, 0, 160.f), FVector(FusR, FusR, S.X * 0.92f), White, EArenaSurface::SuitPaint, 0.f, FRotator(90.f, 0.f, 0.f));
	Part(Body, EArenaShape::Sphere, FVector(S.X * 0.45f, 0, 160.f), FVector(FusR * 1.6f, FusR, FusR), White, EArenaSurface::SuitPaint);
	Part(Body, EArenaShape::Cube, FVector(S.X * 0.38f, 0, 160.f + FusR * 0.3f), FVector(FusR * 0.9f, FusR * 0.8f, FusR * 0.35f), FLinearColor(0.02f, 0.025f, 0.03f), EArenaSurface::Steel);
	Part(Body, EArenaShape::Cube, FVector(0, 0, 160.f - FusR * 0.1f), FVector(S.X * 0.93f, FusR * 0.25f, FusR * 0.25f), D.Paint, EArenaSurface::SuitPaint); // cheat line
	const float WingZ = bJet ? 120.f : 160.f + FusR * 0.45f;
	Part(Body, EArenaShape::Cube, FVector(bJet ? -S.X * 0.05f : S.X * 0.1f, 0, WingZ), FVector(bJet ? 420.f : 200.f, S.Y, 14.f), White, EArenaSurface::SuitPaint, 0.f, FRotator(0.f, 0.f, 0.f));
	Part(Body, EArenaShape::Cube, FVector(-S.X * 0.44f, 0, 160.f + FusR), FVector(160.f, 16.f, FusR * 2.2f), D.Paint, EArenaSurface::SuitPaint, 0.f, FRotator(-20.f, 0.f, 0.f));
	Part(Body, EArenaShape::Cube, FVector(-S.X * 0.44f, 0, 170.f), FVector(110.f, S.Y * 0.35f, 10.f), White, EArenaSurface::SuitPaint);
	if (bJet)
	{
		for (const float Y : { -FusR - 70.f, FusR + 70.f })
		{
			Part(Body, EArenaShape::Cylinder, FVector(-S.X * 0.3f, Y, 200.f), FVector(90.f, 90.f, 300.f), FLinearColor(0.5f, 0.5f, 0.52f), EArenaSurface::Steel, 0.f, FRotator(90.f, 0.f, 0.f));
		}
		for (int32 w = -4; w <= 4; ++w) { Part(Body, EArenaShape::Cube, FVector(w * 90.f, FusR - 4.f, 175.f), FVector(30.f, 4.f, 22.f), FLinearColor(0.02f, 0.025f, 0.03f), EArenaSurface::Steel); Part(Body, EArenaShape::Cube, FVector(w * 90.f, -FusR + 4.f, 175.f), FVector(30.f, 4.f, 22.f), FLinearColor(0.02f, 0.025f, 0.03f), EArenaSurface::Steel); }
	}
	else
	{
		Spinner = NewObject<USceneComponent>(this);
		Spinner->SetupAttachment(Body);
		Spinner->SetRelativeLocation(FVector(S.X * 0.5f + 30.f, 0, 160.f));
		Spinner->RegisterComponent();
		Part(Spinner, EArenaShape::Cube, FVector::ZeroVector, FVector(6.f, 220.f, 16.f), FLinearColor(0.05f, 0.05f, 0.05f));
		Part(Body, EArenaShape::Sphere, FVector(S.X * 0.5f + 20.f, 0, 160.f), FVector(40.f), FLinearColor(0.7f, 0.1f, 0.1f), EArenaSurface::SuitPaint);
	}
	// Gear.
	for (const FVector& G : { FVector(S.X * 0.3f, 0, 30.f), FVector(-S.X * 0.05f, 150.f, 30.f), FVector(-S.X * 0.05f, -150.f, 30.f) })
	{
		Part(Body, EArenaShape::Cylinder, G, FVector(60.f, 60.f, 20.f), FLinearColor(0.015f, 0.015f, 0.015f), EArenaSurface::Plain, 0.f, FRotator(0.f, 0.f, 90.f));
		Part(Body, EArenaShape::Cube, G + FVector(0, 0, 60.f), FVector(10.f, 10.f, 90.f), FLinearColor(0.5f, 0.5f, 0.52f), EArenaSurface::Steel);
	}
	Part(Body, EArenaShape::Sphere, FVector(0, S.Y * 0.5f, WingZ), FVector(18.f), FLinearColor(0.1f, 1.f, 0.1f), EArenaSurface::Plain, 2.f);
	Part(Body, EArenaShape::Sphere, FVector(0, -S.Y * 0.5f, WingZ), FVector(18.f), FLinearColor(1.f, 0.1f, 0.1f), EArenaSurface::Plain, 2.f);
	HalfSize = FVector(S.X * 0.5f, FusR + 20.f, 110.f);
}

void ACityVehicle::BuildHeli(const FVehicleDef& D)
{
	const FVector S = D.Size;
	Part(Body, EArenaShape::Sphere, FVector(60.f, 0, 170.f), FVector(420.f, 200.f, 210.f), D.Paint, EArenaSurface::SuitPaint);
	Part(Body, EArenaShape::Sphere, FVector(190.f, 0, 190.f), FVector(190.f, 170.f, 150.f), FLinearColor(0.02f, 0.025f, 0.03f), EArenaSurface::Steel);
	Part(Body, EArenaShape::Cylinder, FVector(-330.f, 0, 200.f), FVector(50.f, 50.f, 520.f), D.Paint, EArenaSurface::SuitPaint, 0.f, FRotator(90.f, 0.f, 0.f));
	Part(Body, EArenaShape::Cube, FVector(-580.f, 0, 260.f), FVector(80.f, 10.f, 140.f), D.Paint, EArenaSurface::SuitPaint);
	for (const float Y : { -95.f, 95.f })
	{
		Part(Body, EArenaShape::Cylinder, FVector(40.f, Y, 18.f), FVector(12.f, 12.f, 380.f), FLinearColor(0.2f, 0.2f, 0.22f), EArenaSurface::Steel, 0.f, FRotator(90.f, 0.f, 0.f));
		Part(Body, EArenaShape::Cube, FVector(90.f, Y, 55.f), FVector(8.f, 8.f, 80.f), FLinearColor(0.2f, 0.2f, 0.22f), EArenaSurface::Steel);
		Part(Body, EArenaShape::Cube, FVector(-30.f, Y, 55.f), FVector(8.f, 8.f, 80.f), FLinearColor(0.2f, 0.2f, 0.22f), EArenaSurface::Steel);
	}
	Part(Body, EArenaShape::Cylinder, FVector(40.f, 0, 290.f), FVector(30.f, 30.f, 50.f), FLinearColor(0.2f, 0.2f, 0.22f), EArenaSurface::Steel);
	Spinner = NewObject<USceneComponent>(this);
	Spinner->SetupAttachment(Body);
	Spinner->SetRelativeLocation(FVector(40.f, 0, 320.f));
	Spinner->RegisterComponent();
	for (int32 b = 0; b < 4; ++b)
	{
		const FRotator R(0.f, b * 90.f, 0.f);
		Part(Spinner, EArenaShape::Cube, R.RotateVector(FVector(280.f, 0, 0)), FVector(560.f, 28.f, 4.f), FLinearColor(0.04f, 0.04f, 0.04f), EArenaSurface::Plain, 0.f, R);
	}
	Part(Body, EArenaShape::Sphere, FVector(-600.f, 0, 330.f), FVector(14.f), FLinearColor(1.f, 0.1f, 0.1f), EArenaSurface::Plain, 2.f);
	HalfSize = FVector(300.f, 120.f, 130.f);
}

float ACityVehicle::GetAltitudeM() const
{
	float Z; FVector N;
	return TraceGround(GetActorLocation(), Z, N) ? (GetActorLocation().Z - Z) * 0.01f : 0.f;
}

bool ACityVehicle::Enter(AIronManCharacter* InDriver)
{
	if (!InDriver || Driver.IsValid()) { return false; }
	APlayerController* PC = Cast<APlayerController>(InDriver->GetController());
	if (!PC) { return false; }
	Driver = InDriver;
	InDriver->GetCharacterMovement()->StopMovementImmediately();
	InDriver->GetCharacterMovement()->DisableMovement();
	InDriver->SetActorEnableCollision(false);
	InDriver->SetActorHiddenInGame(true);
	InDriver->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InDriver->DisableInput(PC);
	PC->SetViewTargetWithBlend(this, 0.35f);
	Speed = 0.f;
	if (Kind == ECityVehicleKind::Bike && !Rider)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Rider = GetWorld()->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), GetActorTransform(), SP);
		if (Rider)
		{
			if (const UCareerSubsystem* C = UCareerSubsystem::Get(this)) { Rider->ApplyConfig(C->GetAvatar(), false); }
			Rider->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
			Rider->SetActorRelativeLocation(FVector(-25.f, 0.f, -10.f));
			Rider->SetActorRelativeRotation(FRotator(-12.f, 0.f, 0.f));
		}
	}
	return true;
}

void ACityVehicle::Exit()
{
	AIronManCharacter* D = Driver.Get();
	Driver = nullptr;
	if (Rider) { Rider->Destroy(); Rider = nullptr; }
	if (!D) { return; }
	D->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector Side = GetActorRightVector() * -(HalfSize.Y + 140.f);
	D->TeleportTo(GetActorLocation() + Side + FVector(0, 0, 120.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), false, true);
	D->SetActorHiddenInGame(false);
	D->SetActorEnableCollision(true);
	D->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (APlayerController* PC = Cast<APlayerController>(D->GetController()))
	{
		D->EnableInput(PC);
		PC->SetViewTargetWithBlend(D, 0.35f);
	}
	InForward = InRight = InUp = InYaw = InThrottleDelta = InMouseYaw = 0.f;
	bInBrake = false;
}

void ACityVehicle::SetInput(float F, float R, float U, float Y, bool bBrake, float ThrottleDelta, float MouseYaw)
{
	InForward = F; InRight = R; InUp = U; InYaw = Y; bInBrake = bBrake; InThrottleDelta = ThrottleDelta; InMouseYaw = MouseYaw;
}

bool ACityVehicle::TraceGround(const FVector& At, float& OutZ, FVector& OutNormal) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VehicleGround), false, this);
	if (Driver.IsValid()) { Q.AddIgnoredActor(Driver.Get()); }
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 200.f), At - FVector(0, 0, 100000.f), ECC_Visibility, Q))
	{
		OutZ = Hit.ImpactPoint.Z;
		OutNormal = Hit.ImpactNormal;
		return true;
	}
	return false;
}

void ACityVehicle::HitThings(const FVector& From, const FVector& To)
{
	const float Kmh = GetSpeedKmh();
	if (Kmh < 15.f) { return; }
	if (ACityLife* L = ACityLife::Find(this))
	{
		for (ACityPedestrian* P : L->GetPeds())
		{
			if (P && !P->IsPooled() && !P->IsDeadPed() && FMath::PointDistToSegment(P->GetActorLocation(), From, To) < HalfSize.Y + 60.f)
			{
				P->ApplyCityDamage(Kmh > 35.f ? 2.f : 0.2f, From - GetActorForwardVector() * 200.f, true);
			}
		}
	}
}

void ACityVehicle::TickGround(float DeltaSeconds)
{
	const float Accel = TopSpeed / 5.f;
	if (FMath::Abs(InForward) > 0.05f)
	{
		const bool bBraking = (InForward < 0.f && Speed > 50.f) || (InForward > 0.f && Speed < -50.f);
		Speed += InForward * (bBraking ? Accel * 2.5f : Accel) * DeltaSeconds;
	}
	else { Speed = FMath::FInterpTo(Speed, 0.f, DeltaSeconds, 0.6f); }
	if (bInBrake) { Speed = FMath::FInterpTo(Speed, 0.f, DeltaSeconds, 3.f); }
	Speed = FMath::Clamp(Speed, -TopSpeed * 0.3f, TopSpeed);
	Steer = FMath::FInterpTo(Steer, InRight, DeltaSeconds, 6.f);
	const float SpeedFrac = FMath::Clamp(FMath::Abs(Speed) / 1500.f, 0.f, 1.f);
	const float TurnRate = (Kind == ECityVehicleKind::Bike ? 85.f : 70.f) * (bInBrake ? 1.6f : 1.f) * SpeedFrac / (1.f + FMath::Abs(Speed) / TopSpeed);
	FRotator Rot = GetActorRotation();
	Rot.Yaw += Steer * TurnRate * DeltaSeconds * FMath::Sign(Speed);
	const FVector Fwd = FRotator(0.f, Rot.Yaw, 0.f).Vector();
	const FVector From = GetActorLocation();
	FVector Next = From + Fwd * Speed * DeltaSeconds;
	float GZ; FVector N;
	if (TraceGround(Next, GZ, N))
	{
		const float TargetZ = GZ + FMath::Max(40.f, HalfSize.Z);
		Next.Z = (TargetZ > From.Z - 5.f) ? FMath::FInterpTo(From.Z, TargetZ, DeltaSeconds, 18.f) : FMath::Max(TargetZ, From.Z - 980.f * DeltaSeconds);
		const FVector Fl = FVector::VectorPlaneProject(Fwd, N).GetSafeNormal();
		Rot.Pitch = FMath::FInterpTo(Rot.Pitch, Fl.Rotation().Pitch, DeltaSeconds, 8.f);
	}
	Rot.Roll = Kind == ECityVehicleKind::Bike ? FMath::FInterpTo(Rot.Roll, -Steer * 28.f * SpeedFrac, DeltaSeconds, 5.f) : FMath::FInterpTo(Rot.Roll, Steer * 3.f * SpeedFrac, DeltaSeconds, 5.f);
	FHitResult Hit;
	SetActorLocationAndRotation(Next, Rot, true, &Hit);
	if (Hit.bBlockingHit)
	{
		if (ACityCar* Car = Cast<ACityCar>(Hit.GetActor())) { Car->ApplyCityDamage(FMath::Abs(Speed) / 1500.f, From, true); }
		Speed *= -0.25f;
	}
	HitThings(From, Next);
	WheelSpin += Speed * DeltaSeconds / 34.f * 57.3f;
	for (int32 i = 0; i < Wheels.Num(); ++i)
	{
		if (!Wheels[i]) { continue; }
		const bool bFront = Kind == ECityVehicleKind::Bike ? i == 0 : i < 2;
		Wheels[i]->SetRelativeRotation(FRotator(-WheelSpin, bFront ? Steer * 28.f : 0.f, 0.f));
	}
	Arm->SetRelativeRotation(FRotator(-12.f, 0.f, -Rot.Roll));
}

void ACityVehicle::TickPlane(float DeltaSeconds)
{
	Throttle = FMath::Clamp(Throttle + InThrottleDelta * DeltaSeconds * 0.5f, 0.f, 1.f);
	const float Stall = TopSpeed * 0.28f;
	const float TargetSpeed = Throttle * TopSpeed;
	Speed = FMath::FInterpTo(Speed, TargetSpeed, DeltaSeconds, 0.35f);
	if (bInBrake && bGrounded) { Speed = FMath::FInterpTo(Speed, 0.f, DeltaSeconds, 1.5f); }
	const float Ctrl = FMath::Clamp(Speed / Stall, 0.f, 1.2f);
	FRotator Rot = GetActorRotation();
	// Pitch (W = nose down, S = nose up, like a stick), roll, rudder; bank turns.
	const float PitchIn = -InForward;
	Rot.Pitch = FMath::Clamp(Rot.Pitch + PitchIn * 45.f * Ctrl * DeltaSeconds, -60.f, 60.f);
	Rot.Roll = FMath::Clamp(Rot.Roll + InRight * 90.f * Ctrl * DeltaSeconds, -80.f, 80.f);
	if (FMath::Abs(InRight) < 0.05f) { Rot.Roll = FMath::FInterpTo(Rot.Roll, 0.f, DeltaSeconds, 0.8f); }
	Rot.Yaw += (InYaw * 30.f * Ctrl + Rot.Roll * 0.5f * FMath::Min(Ctrl, 1.f) * (bGrounded ? 0.f : 1.f)) * DeltaSeconds;
	if (bGrounded) { Rot.Yaw += InRight * 25.f * FMath::Clamp(Speed / 800.f, 0.f, 1.f) * DeltaSeconds; Rot.Roll = 0.f; }
	FVector Vel = Rot.Vector() * Speed;
	const float Lift = FMath::Clamp(Speed / Stall, 0.f, 1.f);
	VerticalSpeed = FMath::FInterpTo(VerticalSpeed, -(1.f - Lift) * 1400.f, DeltaSeconds, 1.5f);
	Vel.Z += VerticalSpeed;
	const FVector From = GetActorLocation();
	FVector Next = From + Vel * DeltaSeconds;
	float GZ; FVector N;
	bGrounded = false;
	if (TraceGround(Next, GZ, N) && Next.Z <= GZ + HalfSize.Z + 5.f)
	{
		// On the ground: wheels hold it; a hard nose-down / high sink landing is a crash (bounce + stop).
		Next.Z = GZ + HalfSize.Z;
		const bool bHard = Vel.Z < -900.f || Rot.Pitch < -15.f;
		if (bHard) { Speed *= 0.3f; Throttle = 0.f; if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->Toast(NSLOCTEXT("Vehicle", "HardLanding", "Hard landing!"), FLinearColor(1.f, 0.4f, 0.3f)); } }
		Rot.Pitch = FMath::Max(0.f, Rot.Pitch);
		if (PitchIn <= 0.f || Speed < Stall) { Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaSeconds, 4.f); }
		VerticalSpeed = 0.f;
		bGrounded = true;
	}
	FHitResult Hit;
	SetActorLocationAndRotation(Next, Rot, true, &Hit);
	if (Hit.bBlockingHit) { Speed *= 0.2f; Throttle = 0.f; }
	if (Spinner) { RotorSpin += (300.f + Throttle * 2400.f) * DeltaSeconds; Spinner->SetRelativeRotation(FRotator(0.f, 0.f, RotorSpin)); }
	Arm->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
}

void ACityVehicle::TickHeli(float DeltaSeconds)
{
	FRotator Rot = GetActorRotation();
	Rot.Yaw += (InYaw * 70.f + InMouseYaw * 2.f) * DeltaSeconds;
	const FRotator YawOnly(0.f, Rot.Yaw, 0.f);
	const FVector Want = YawOnly.RotateVector(FVector(InForward, InRight, 0.f)).GetClampedToMaxSize(1.f) * TopSpeed + FVector(0, 0, InUp * 900.f);
	HeliVel = FMath::VInterpTo(HeliVel, Want, DeltaSeconds, 1.2f);
	const FVector From = GetActorLocation();
	FVector Next = From + HeliVel * DeltaSeconds;
	float GZ; FVector N;
	bGrounded = false;
	if (TraceGround(Next, GZ, N) && Next.Z < GZ + HalfSize.Z)
	{
		Next.Z = GZ + HalfSize.Z;
		HeliVel.Z = FMath::Max(0.f, HeliVel.Z);
		HeliVel *= FVector(0.9f, 0.9f, 1.f);
		bGrounded = true;
	}
	const FVector LocalVel = YawOnly.UnrotateVector(HeliVel);
	Rot.Pitch = FMath::FInterpTo(Rot.Pitch, -LocalVel.X / FMath::Max(1.f, TopSpeed) * 18.f, DeltaSeconds, 3.f);
	Rot.Roll = FMath::FInterpTo(Rot.Roll, LocalVel.Y / FMath::Max(1.f, TopSpeed) * 18.f, DeltaSeconds, 3.f);
	FHitResult Hit;
	SetActorLocationAndRotation(Next, Rot, true, &Hit);
	if (Hit.bBlockingHit) { HeliVel *= -0.3f; }
	Speed = HeliVel.Size();
	if (Spinner) { RotorSpin += 1500.f * DeltaSeconds; Spinner->SetRelativeRotation(FRotator(0.f, RotorSpin, 0.f)); }
	Arm->SetRelativeRotation(FRotator(-15.f, 0.f, -Rot.Roll));
}

void ACityVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Driver.IsValid())
	{
		// Parked: coast to a stop, settle on the ground.
		InForward = InRight = InUp = InYaw = InThrottleDelta = InMouseYaw = 0.f;
		if (Kind == ECityVehicleKind::Helicopter) { InUp = -1.f; }
		if (FMath::Abs(Speed) < 5.f && bGrounded && Kind != ECityVehicleKind::Helicopter) { return; }
	}
	switch (Kind)
	{
	case ECityVehicleKind::Car:
	case ECityVehicleKind::Bike: TickGround(DeltaSeconds); bGrounded = true; break;
	case ECityVehicleKind::Plane: TickPlane(DeltaSeconds); break;
	case ECityVehicleKind::Helicopter: TickHeli(DeltaSeconds); break;
	}
}
