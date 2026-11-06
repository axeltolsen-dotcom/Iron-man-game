#include "SuitSuperComponent.h"
#include "ArenaAudio.h"
#include "ArenaBlastFX.h"
#include "ArenaCombat.h"
#include "ArenaFeel.h"
#include "ArenaVisuals.h"
#include "CareerSubsystem.h"
#include "CareerWidgets.h"
#include "CityLife.h"
#include "EnemyBase.h"
#include "IronManCharacter.h"
#include "IronProjectile.h"
#include "PilotAvatar.h"
#include "ReactorComponent.h"
#include "SuitTypes.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/AudioComponent.h"
#include "Misc/App.h"

namespace
{
	float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }
	/** Eased 0..1 between A and B (ease in-out). */
	float Ph(float T, float A, float B) { return B <= A ? (T >= B ? 1.f : 0.f) : FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((T - A) / (B - A), 0.f, 1.f), 2.f); }
	FRotator LerpR(const FRotator& A, const FRotator& B, float X) { return FQuat::Slerp(A.Quaternion(), B.Quaternion(), FMath::Clamp(X, 0.f, 1.f)).Rotator(); }
	FVector LerpV(const FVector& A, const FVector& B, float X) { return FMath::Lerp(A, B, FMath::Clamp(X, 0.f, 1.f)); }

	float DurationOf(ESuperKind K)
	{
		switch (K)
		{
		case ESuperKind::BeamStomp: return 2.2f;
		case ESuperKind::NanoBlades: return 1.5f;
		case ESuperKind::SpiderArms: return 1.7f;
		case ESuperKind::MissileBarrage: return 1.6f;
		case ESuperKind::RemoteStrike: return 1.9f;
		case ESuperKind::SupportPulse: return 1.4f;
		case ESuperKind::GroundPound: return 1.9f;
		case ESuperKind::MinigunPod: return 3.6f;
		case ESuperKind::CloakSlash: return 1.3f;
		default: return 1.5f;
		}
	}
}

// ============================================================================================ table

const TArray<FSuperDef>& USuitSuperComponent::Table()
{
	static TArray<FSuperDef> T;
	if (T.Num()) { return T; }
	auto A = [](const TCHAR* Id, ESuperKind K, const TCHAR* Name, float Cd, FLinearColor Tint) { FSuperDef D; D.SuitId = FName(Id); D.Kind = K; D.Name = Name; D.Cooldown = Cd; D.Tint = Tint; T.Add(D); };
	const FLinearColor Arc(0.45f, 0.9f, 1.f), Gold(1.f, 0.8f, 0.3f), Red(1.f, 0.3f, 0.2f), Nano(0.5f, 0.85f, 1.f), Pink(1.f, 0.45f, 0.85f), Green(0.4f, 1.f, 0.6f);
	A(TEXT("mark-lxxxv"), ESuperKind::BeamStomp, TEXT("Endgame: Nano Blades / Beam Stomp"), 14.f, Nano);
	A(TEXT("mark-l"), ESuperKind::NanoBlades, TEXT("Nanotech Blades"), 10.f, Nano);
	A(TEXT("arachnid"), ESuperKind::SpiderArms, TEXT("Iron Spider: Waldoes / Nano Blades"), 9.f, Gold);
	A(TEXT("mark-iii"), ESuperKind::HeavyBlast, TEXT("Unibeam Overcharge"), 12.f, Arc);
	A(TEXT("mark-i"), ESuperKind::GroundPound, TEXT("Cave Stomp"), 12.f, FLinearColor(1.f, 0.5f, 0.1f));
	A(TEXT("mark-ii"), ESuperKind::HeavyBlast, TEXT("Prototype Unibeam"), 12.f, Arc);
	A(TEXT("mark-iv"), ESuperKind::HeavyBlast, TEXT("Unibeam"), 12.f, Arc);
	A(TEXT("mark-v"), ESuperKind::GroundPound, TEXT("Suitcase Slam"), 11.f, Arc);
	A(TEXT("mark-vi"), ESuperKind::HeavyBlast, TEXT("Triangle Unibeam"), 12.f, Arc);
	A(TEXT("mark-vii"), ESuperKind::MissileBarrage, TEXT("Shoulder-Missile Barrage"), 13.f, Red);
	A(TEXT("mark-xlii"), ESuperKind::RemoteStrike, TEXT("Remote-Piece Strike"), 12.f, Gold);
	A(TEXT("mark-xli-bones"), ESuperKind::RemoteStrike, TEXT("Bones Barrage"), 12.f, FLinearColor::White);
	A(TEXT("rescue"), ESuperKind::SupportPulse, TEXT("Rescue Pulse"), 11.f, FLinearColor(0.5f, 0.7f, 1.f));
	A(TEXT("rescue-mk2"), ESuperKind::SupportPulse, TEXT("Morgan's Rescue Pulse"), 11.f, Pink);
	A(TEXT("mark-xxxviii-igor"), ESuperKind::GroundPound, TEXT("Igor Mega Stomp"), 14.f, Gold);
	A(TEXT("war-machine"), ESuperKind::MinigunPod, TEXT("Minigun + Micro-Rocket Pod"), 13.f, Gold);
	A(TEXT("iron-patriot"), ESuperKind::MinigunPod, TEXT("Patriot Minigun + Rockets"), 13.f, FLinearColor(0.4f, 0.6f, 1.f));
	A(TEXT("mark-xv-sneaky"), ESuperKind::CloakSlash, TEXT("Stealth Cloak Slash"), 10.f, FLinearColor(0.6f, 0.8f, 1.f));
	A(TEXT("nightshade"), ESuperKind::CloakSlash, TEXT("Nightshade Cloak Slash"), 10.f, FLinearColor(0.7f, 0.4f, 1.f));
	A(TEXT("mark-xvi-nightclub"), ESuperKind::CloakSlash, TEXT("Blackout Slash"), 10.f, FLinearColor(0.5f, 0.5f, 1.f));
	A(TEXT("mark-xxiii-shades"), ESuperKind::MissileBarrage, TEXT("Shades Salvo"), 12.f, Red);
	A(TEXT("mark-xxi-midas"), ESuperKind::MissileBarrage, TEXT("Midas Salvo"), 12.f, Gold);
	A(TEXT("mark-xxv-striker"), ESuperKind::GroundPound, TEXT("Striker Pile-Driver"), 12.f, FLinearColor(1.f, 0.6f, 0.2f));
	A(TEXT("mark-xvii-heartbreaker"), ESuperKind::HeavyBlast, TEXT("Heartbreaker Mega-Unibeam"), 13.f, FLinearColor(1.f, 1.f, 1.f));
	A(TEXT("mark-xx-python"), ESuperKind::HeavyBlast, TEXT("Python Long-Range Beam"), 12.f, Arc);
	A(TEXT("mark-xxii-hot-rod"), ESuperKind::HeavyBlast, TEXT("Hot Rod Blast"), 12.f, Red);
	A(TEXT("mark-xxvii-disco"), ESuperKind::SupportPulse, TEXT("Disco Strobe Pulse"), 11.f, Pink);
	A(TEXT("mark-xxxiii-silver-centurion"), ESuperKind::HeavyBlast, TEXT("Centurion Unibeam"), 12.f, Red);
	A(TEXT("mark-xl-shotgun"), ESuperKind::MissileBarrage, TEXT("Shotgun Swarm"), 12.f, Arc);
	A(TEXT("mark-xliii"), ESuperKind::RemoteStrike, TEXT("Avengers Piece Strike"), 12.f, Gold);
	A(TEXT("mark-xlv"), ESuperKind::HeavyBlast, TEXT("Ultron-era Unibeam"), 12.f, Arc);
	A(TEXT("mark-xlvi"), ESuperKind::NanoBlades, TEXT("Civil War Gauntlet Blades"), 10.f, Arc);
	A(TEXT("mark-xlvii"), ESuperKind::RemoteStrike, TEXT("Homecoming Remote Strike"), 12.f, Gold);
	A(TEXT("ultron"), ESuperKind::BeamStomp, TEXT("Ultron Annihilation Stomp"), 14.f, Red);
	A(TEXT("mark-iii-alt"), ESuperKind::HeavyBlast, TEXT("Unibeam"), 12.f, Arc);
	A(TEXT("valkyrie"), ESuperKind::GroundPound, TEXT("Valkyrie Dive"), 11.f, Pink);
	A(TEXT("aurora"), ESuperKind::SupportPulse, TEXT("Aurora Pulse"), 11.f, Green);
	A(TEXT("ironheart"), ESuperKind::HeavyBlast, TEXT("Ironheart Overcharge"), 12.f, Red);
	A(TEXT("deep-space"), ESuperKind::HeavyBlast, TEXT("Deep-Space Ion Blast"), 12.f, Arc);
	A(TEXT("impulse"), ESuperKind::CloakSlash, TEXT("Impulse Blink Strike"), 9.f, FLinearColor(0.4f, 1.f, 1.f));
	return T;
}

FSuperDef USuitSuperComponent::SuperFor(FName SuitId)
{
	for (const FSuperDef& D : Table()) { if (D.SuitId == SuitId) { return D; } }
	FSuperDef D;
	D.SuitId = SuitId;
	const int32 Idx = USuitLibrary::FindSuitIndex(SuitId);
	const ESuitSpecialWeapon W = Idx != INDEX_NONE ? USuitLibrary::GetSuit(Idx).SpecialWeapon : ESuitSpecialWeapon::Unibeam;
	switch (W)
	{
	case ESuitSpecialWeapon::Missiles: case ESuitSpecialWeapon::DroneSwarm: D.Kind = ESuperKind::MissileBarrage; D.Name = TEXT("Missile Barrage"); break;
	case ESuitSpecialWeapon::NanoBlades: D.Kind = ESuperKind::NanoBlades; D.Name = TEXT("Nano Blades"); break;
	case ESuitSpecialWeapon::Cloak: D.Kind = ESuperKind::CloakSlash; D.Name = TEXT("Cloak Slash"); break;
	case ESuitSpecialWeapon::Shield: D.Kind = ESuperKind::SupportPulse; D.Name = TEXT("Shield Pulse"); break;
	default: D.Kind = ESuperKind::HeavyBlast; D.Name = TEXT("Unibeam Overcharge"); break;
	}
	return D;
}

// ============================================================================================ component

USuitSuperComponent::USuitSuperComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

USuitSuperComponent* USuitSuperComponent::Get(const AActor* Owner) { return Owner ? Owner->FindComponentByClass<USuitSuperComponent>() : nullptr; }
AIronManCharacter* USuitSuperComponent::Char() const { return Cast<AIronManCharacter>(GetOwner()); }

FSuperDef USuitSuperComponent::CurrentDef() const
{
	const AIronManCharacter* C = Char();
	if (!C) { return FSuperDef(); }
	if (C->IsHulkbusterActive()) { FSuperDef D; D.Kind = ESuperKind::GroundPound; D.Name = TEXT("Hulkbuster Mega Stomp"); D.Cooldown = 12.f; D.Tint = FLinearColor(1.f, 0.6f, 0.2f); return D; }
	return SuperFor(C->GetSuit().Id);
}

float USuitSuperComponent::GetCooldownFraction() const { return CooldownLeft <= 0.f ? 1.f : 1.f - CooldownLeft / FMath::Max(0.1f, CooldownTotal); }
bool USuitSuperComponent::CanSwing() const { const AIronManCharacter* C = Char(); return C && C->GetSuit().Id == FName(TEXT("arachnid")); }

void USuitSuperComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearParts();
	if (Face) { Face->Destroy(); Face = nullptr; }
	Super::EndPlay(Reason);
}

UStaticMeshComponent* USuitSuperComponent::MakePart(USceneComponent* Parent, int32 Shape, const FVector& Loc, const FVector& Scale, const FRotator& Rot, const FLinearColor& Color, float Glow)
{
	AActor* O = GetOwner();
	if (!O) { return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(O);
	C->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape(Shape)));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->CastShadow = Glow <= 0.f;
	if (Parent) { C->SetupAttachment(Parent); }
	C->RegisterComponent();
	if (!Parent) { C->SetAbsolute(true, true, true); C->SetWorldLocationAndRotation(Loc, Rot); C->SetWorldScale3D(Scale); }
	else { C->SetRelativeLocationAndRotation(Loc, Rot); C->SetRelativeScale3D(Scale); }
	TempMID.Add(ArenaVisuals::PaintComponent(O, C, Color, Glow, Glow > 0.f ? EArenaSurface::Glow : EArenaSurface::Auto));
	Temp.Add(C);
	return C;
}

void USuitSuperComponent::ClearParts()
{
	for (UStaticMeshComponent* C : Temp)
	{
		if (!C) { continue; }
		if (USceneComponent* P = C->GetAttachParent()) { if (!Temp.Contains(Cast<UStaticMeshComponent>(P)) && P->GetOwner() == GetOwner() && P != GetOwner()->GetRootComponent() && P->GetName().StartsWith(TEXT("SuperPivot"))) { P->DestroyComponent(); } }
		C->DestroyComponent();
	}
	Temp.Empty();
	TempMID.Empty();
	PieceVel.Empty();
}

void USuitSuperComponent::SetPose(float W, const FRotator& Root, const FVector& Off, const FRotator& SL, const FRotator& SR, const FRotator& HL, const FRotator& HR)
{
	if (AIronManCharacter* C = Char())
	{
		AIronManCharacter::FPoseOverride& P = C->PoseOverride;
		P.Weight = W; P.Root = Root; P.RootOffset = Off; P.ShoulderL = SL; P.ShoulderR = SR; P.HipL = HL; P.HipR = HR;
	}
}

void USuitSuperComponent::Punch(float ArmLength, float FovDelta, float Seconds)
{
	AIronManCharacter* C = Char();
	if (!C || !C->CameraBoom) { return; }
	if (PunchLeft <= 0.f) { BaseArm = C->CameraBoom->TargetArmLength; }
	PunchArm = ArmLength; PunchFov = FovDelta; PunchLeft = PunchTotal = FMath::Max(0.1f, Seconds);
}

void USuitSuperComponent::TickCameraPunch(float Dt)
{
	AIronManCharacter* C = Char();
	if (!C || !C->CameraBoom || PunchLeft <= 0.f) { if (C) { C->SuperFov = FMath::FInterpTo(C->SuperFov, 0.f, Dt, 8.f); } return; }
	PunchLeft -= Dt;
	const float X = 1.f - FMath::Clamp(PunchLeft / PunchTotal, 0.f, 1.f);
	// Fast in (first 25 %), hold, ease out: a punch-in on the hero beat, then back to the play camera.
	const float W = X < 0.25f ? Smooth(0.f, 0.25f, X) : 1.f - Smooth(0.55f, 1.f, X);
	C->CameraBoom->TargetArmLength = FMath::Lerp(BaseArm, PunchArm, W);
	C->SuperFov = PunchFov * W;
	if (PunchLeft <= 0.f) { C->CameraBoom->TargetArmLength = BaseArm; }
}

void USuitSuperComponent::Beat(float SlowSeconds, float Dilation, FName Shake, float ShakeScale, float RumbleAmt)
{
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this))
	{
		if (SlowSeconds > 0.f) { Feel->HitStop(SlowSeconds, Dilation); }
		if (!Shake.IsNone()) { if (!Feel->PlayShake(Shake, ShakeScale)) { Feel->AddTrauma(0.5f * ShakeScale); } }
	}
	if (RumbleAmt > 0.f) { ArenaCombat::Rumble(this, RumbleAmt, 0.3f, true); }
}

AEnemyBase* USuitSuperComponent::FindTarget(float MaxRange, float MaxAngleDeg) const
{
	const AIronManCharacter* C = Char();
	if (!C || !C->GetController()) { return nullptr; }
	const FVector Eye = C->GetActorLocation();
	const FVector Fwd = C->GetController()->GetControlRotation().Vector();
	const float CosLim = FMath::Cos(FMath::DegreesToRadians(MaxAngleDeg));
	AEnemyBase* Best = nullptr; float BestScore = -1e9f;
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) { continue; }
		const FVector To = It->GetActorLocation() - Eye;
		const float D = float(To.Size());
		if (D > MaxRange) { continue; }
		const float Dot = float(FVector::DotProduct(To / FMath::Max(1.f, D), Fwd));
		if (Dot < CosLim) { continue; }
		const float S = Dot * 2.f - D / MaxRange;
		if (S > BestScore) { BestScore = S; Best = *It; }
	}
	return Best;
}

int32 USuitSuperComponent::AreaDamage(const FVector& Center, float Radius, float Damage, float Knock, bool bCity)
{
	AIronManCharacter* C = Char();
	UWorld* W = GetWorld();
	if (!C || !W) { return 0; }
	int32 N = 0;
	bool bKill = false;
	TArray<AEnemyBase*> Hit;
	for (TActorIterator<AEnemyBase> It(W); It; ++It) { if (!It->IsDead() && FVector::Dist(It->GetActorLocation(), Center) < Radius + It->GetSimpleCollisionRadius()) { Hit.Add(*It); } }
	for (AEnemyBase* E : Hit)
	{
		const FVector Loc = E->GetActorLocation();
		UGameplayStatics::ApplyDamage(E, Damage, C->GetController(), C, UDamageType::StaticClass());
		++N;
		if (!IsValid(E) || E->IsDead()) { bKill = true; AArenaSparks::SpawnDebris(W, Loc, Loc - Center, FLinearColor(0.35f, 0.36f, 0.4f), 80.f, 8); }
		else { ArenaCombat::Knockback(E, Loc - Center, Knock); }
	}
	if (bCity) { CityLife::Damage(W, Center, Radius, Damage, nullptr, true); }
	if (N) { ArenaCombat::RegisterHit(bKill); }
	return N;
}

int32 USuitSuperComponent::ConeDamage(const FVector& Origin, const FVector& Dir, float Range, float HalfAngleDeg, float Damage, float Knock)
{
	AIronManCharacter* C = Char();
	UWorld* W = GetWorld();
	if (!C || !W) { return 0; }
	const float CosLim = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
	const FVector D = Dir.GetSafeNormal();
	int32 N = 0; bool bKill = false;
	TArray<AEnemyBase*> Hit;
	for (TActorIterator<AEnemyBase> It(W); It; ++It)
	{
		if (It->IsDead()) { continue; }
		const FVector To = It->GetActorLocation() - Origin;
		const float Dist = float(To.Size()) - It->GetSimpleCollisionRadius();
		if (Dist < Range && FVector::DotProduct(To.GetSafeNormal(), D) > CosLim) { Hit.Add(*It); }
	}
	for (AEnemyBase* E : Hit)
	{
		const FVector Loc = E->GetActorLocation();
		UGameplayStatics::ApplyDamage(E, Damage, C->GetController(), C, UDamageType::StaticClass());
		AArenaSparks::Spawn(W, Loc, -D, CurrentDef().Tint, 12, 1300.f);
		++N;
		if (!IsValid(E) || E->IsDead()) { bKill = true; AArenaSparks::SpawnDebris(W, Loc, D, FLinearColor(0.35f, 0.36f, 0.4f), 80.f, 8); }
		else { ArenaCombat::Knockback(E, D, Knock); }
	}
	CityLife::Damage(W, Origin + D * Range * 0.6f, Range * 0.5f, Damage * 0.5f, nullptr, true);
	if (N) { ArenaCombat::RegisterHit(bKill); }
	return N;
}

void USuitSuperComponent::FireSalvo(int32 Count, float Speed, float Homing, float Damage, float Blast, const FLinearColor& Color, bool bFromShoulders)
{
	AIronManCharacter* C = Char();
	UWorld* W = GetWorld();
	if (!C || !W) { return; }
	TArray<AEnemyBase*> Targets;
	for (TActorIterator<AEnemyBase> It(W); It; ++It) { if (!It->IsDead()) { Targets.Add(*It); } }
	const FVector Me = C->GetActorLocation();
	const FVector Fwd = C->GetController() ? C->GetController()->GetControlRotation().Vector() : C->GetActorForwardVector();
	Targets.Sort([&](const AEnemyBase& A, const AEnemyBase& B) { return FVector::DistSquared(A.GetActorLocation(), Me) < FVector::DistSquared(B.GetActorLocation(), Me); });
	const FVector Right = C->GetActorRightVector();
	for (int32 i = 0; i < Count; ++i)
	{
		const float Side = (i % 2) ? 1.f : -1.f;
		const FVector Origin = Me + FVector(0, 0, bFromShoulders ? 75.f : 40.f) + Right * Side * (bFromShoulders ? 32.f : 20.f);
		const FVector Dir = Homing > 0.f ? (Fwd + FVector(0, 0, 0.5f) + Right * Side * 0.4f * (1 + i / 2)).GetSafeNormal() : FMath::VRandCone(Fwd, FMath::DegreesToRadians(3.f));
		AEnemyBase* Tg = Targets.Num() ? Targets[i % Targets.Num()] : nullptr;
		if (AIronProjectile* P = AIronProjectile::SpawnPooled(W, Origin, Dir.Rotation(), C, C, false))
		{
			P->LifeSeconds = 4.f;
			P->Configure(Damage, false, Blast, Color, 11.f);
			P->Launch(Dir * Speed, 0.f, (Tg && Homing > 0.f) ? Tg->GetRootComponent() : nullptr, Tg ? Homing : 0.f);
		}
		AArenaBlastFX::Spawn(W, Origin, 30.f, FLinearColor(1.f, 0.6f, 0.2f), 0.08f, false);
	}
	ArenaAudio::PlayAt(C, TEXT("missile_launch"), Me, 1.f);
}

// -------------------------------------------------------------------------------------------- begin / end

bool USuitSuperComponent::HandleSpecialPressed()
{
	if (!IsReady()) { return false; }
	bRightDown = true;
	RightDownTime = 0.f;
	return true;
}

void USuitSuperComponent::Begin(ESuperKind Kind)
{
	AIronManCharacter* C = Char();
	if (!C || bActive || C->IsDead() || C->IsInCinematic() || C->bCareerSuitOff) { return; }
	// Context switches.
	AEnemyBase* Near = FindTarget(700.f, 70.f);
	if (Kind == ESuperKind::BeamStomp && Near && C->GetSuit().Id == FName(TEXT("mark-lxxxv"))) { Kind = ESuperKind::NanoBlades; }
	if (Kind == ESuperKind::SpiderArms && !FindTarget(1200.f, 120.f)) { Kind = ESuperKind::NanoBlades; }
	bActive = true;
	ActiveKind = Kind;
	T = 0.f;
	Fired.Reset();
	bLanded = false;
	HoldTime = 0.f;
	Spin = 0.f;
	Pinned.Reset(); PinPoints.Reset();
	Target = FindTarget(Kind == ESuperKind::CloakSlash ? 2600.f : 3000.f, 50.f);
	if (!Target.IsValid()) { Target = Near; }
	StartLoc = C->GetActorLocation();
	TargetPoint = Target.IsValid() ? Target->GetActorLocation() : C->ComputeAimPoint();
	C->bSuperActive = true;
	C->bFireHeld = false;
	const FSuperDef D = CurrentDef();
	CooldownTotal = D.Cooldown;
	ArenaAudio::Play2D(C, TEXT("hud_target_lock"), 0.6f);
	if (UCareerSubsystem* CS = UCareerSubsystem::Get(C)) { CS->Toast(FText::FromString(FString(D.Name).ToUpper()), D.Tint); }
	// Supers are staged in third person so the body animation reads (back to the helmet view afterwards).
	bWasFP = C->bFirstPerson;
	if (bWasFP) { C->SetFirstPerson(false); }
}

void USuitSuperComponent::End()
{
	AIronManCharacter* C = Char();
	bActive = false;
	CooldownLeft = CooldownTotal;
	ClearParts();
	if (C)
	{
		C->bSuperActive = false;
		C->PoseOverride.Weight = 0.f;
		if (C->SuitRoot) { C->SuitRoot->SetVisibility(true, true); }
		if (bWasFP && !bMaskOpen) { C->SetFirstPerson(true); }
	}
	bWasFP = false;
	if (MinigunAudio) { MinigunAudio->FadeOut(0.2f, 0.f); MinigunAudio = nullptr; }
}

// -------------------------------------------------------------------------------------------- the moves

void USuitSuperComponent::TickMove(float Dt)
{
	AIronManCharacter* C = Char();
	UWorld* W = GetWorld();
	if (!C || !W) { return; }
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const FSuperDef Def = CurrentDef();
	const FLinearColor Tint = Def.Tint;
	T += Dt;
	const float Dur = DurationOf(ActiveKind);
	// Pose weight envelope: blend in over 0.12 s, out over the last 0.3 s (no popping).
	const float Env = Ph(T, 0.f, 0.12f) * (1.f - Ph(T, Dur - 0.3f, Dur));
	const FVector Me = C->GetActorLocation();
	const FRotator CR = C->GetController() ? C->GetController()->GetControlRotation() : C->GetActorRotation();
	const FVector Look = CR.Vector();
	const FVector Flat = FRotator(0.f, CR.Yaw, 0.f).Vector();
	const FVector Chest = C->ArcReactor ? C->ArcReactor->GetComponentLocation() : Me + FVector(0, 0, 40.f);
	if (Target.IsValid() && !Target->IsDead()) { TargetPoint = Target->GetActorLocation(); }
	const FVector ToT = (TargetPoint - Me);
	const FVector ToTFlat = FVector(ToT.X, ToT.Y, 0.f).GetSafeNormal();
	const FRotator Z = FRotator::ZeroRotator;
	const float HalfH = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bGround = Move && Move->IsMovingOnGround();

	switch (ActiveKind)
	{
	// ------------------------------------------------------------------ Beam Stomp (Mark LXXXV / Ultron)
	case ESuperKind::BeamStomp:
	{
		if (Once(0))
		{
			ArenaAudio::PlayAt(C, TEXT("unibeam_charge"), Me, 1.f, 0.f);
			ArenaAudio::PlayAt(C, TEXT("suitup_servo_heavy"), Me, 0.8f);
			if (Move) { C->LaunchCharacter(FVector(0, 0, bGround ? 950.f : 400.f), false, true); }
			Punch(300.f, -6.f, 0.7f);
		}
		// Anticipation 0..0.45: rise with arms raised overhead, knees tucked.
		const float Up = Ph(T, 0.f, 0.4f), Slam = Ph(T, 0.42f, 0.55f), Kneel = bLanded ? Ph(T, 0.f, 99.f) : 0.f;
		if (T > 0.45f && !bLanded)
		{
			if (Once(1)) { ArenaAudio::PlayAt(C, TEXT("thruster_boost"), Me, 1.f); }
			if (Move) { if (Move->IsFlying()) { Move->SetMovementMode(MOVE_Falling); } Move->Velocity = FVector(0.f, 0.f, -4200.f); }
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(Stomp), false, C);
			const bool bNearGround = W->LineTraceSingleByChannel(H, Me, Me - FVector(0, 0, HalfH + 60.f), ECC_WorldStatic, Q);
			if (bNearGround || bGround || T > 1.1f)
			{
				bLanded = true;
				const FVector G = bNearGround ? FVector(H.ImpactPoint) : Me - FVector(0, 0, HalfH);
				// HERO BEAT: fist hits the ground.
				AreaDamage(G, 950.f, 10.f, 1600.f);
				AArenaTelegraph::Spawn(W, G, 950.f, 0.45f, Tint);
				AArenaBlastFX::Spawn(W, G + FVector(0, 0, 30.f), 520.f, Tint, 0.45f, true);
				ArenaFeel::SpawnImpactDecal(W, G, FVector::UpVector, 520.f, 14.f);
				AArenaSparks::Spawn(W, G, FVector::UpVector, Tint, 16, 1600.f);
				AArenaSparks::SpawnDebris(W, G, FVector::UpVector, FLinearColor(0.2f, 0.2f, 0.2f), 70.f, 10);
				ArenaAudio::PlayAt(C, TEXT("explosion_large"), G, 1.2f);
				ArenaAudio::PlayAt(C, TEXT("melee_hit_heavy"), G, 1.f);
				Beat(0.12f, 0.2f, TEXT("explosion_nearby"), 1.3f, 1.f);
				Punch(190.f, -10.f, 0.55f);
				T = 0.7f; // continue from the landing beat
			}
		}
		// Beam: 0.85..1.75 chest unibeam sweep (from the kneel, rising to a stand).
		if (bLanded && T > 0.85f && T < 1.75f)
		{
			if (Once(2)) { ArenaAudio::PlayAt(C, TEXT("unibeam_fire"), Chest, 1.f, 0.f); }
			const FVector Dir = (Look + FVector(0, 0, -0.08f)).GetSafeNormal();
			for (int32 k = -1; k <= 1; ++k)
			{
				const FVector Off = FRotationMatrix(CR).GetUnitAxis(EAxis::Y) * (k * 35.f);
				const ArenaCombat::FShot S = ArenaCombat::FireHitscan(C, C->GetController(), Chest + Off, Dir, 9000.f, 28.f * Dt, 0.f, 40.f, Tint, 250.f);
				if (k == 0) { AArenaTracer::Spawn(W, Chest, S.End, Tint, 70.f, 0.05f); if (S.bHit && FMath::FRand() < 0.4f) { AArenaSparks::Spawn(W, S.End, -Dir, Tint, 6, 1400.f); CityLife::Damage(W, S.End, 160.f, 2.f, nullptr, true); } }
			}
			ArenaCombat::Rumble(C, 0.5f, 0.06f, true);
		}
		const FRotator SL = !bLanded ? LerpR(FRotator(-30.f, 0.f, -25.f), FRotator(170.f, 0.f, -10.f), Up) : (T < 0.85f ? FRotator(60.f, 0.f, -30.f) : FRotator(-35.f, 0.f, -25.f));
		const FRotator SR = !bLanded ? LerpR(FRotator(-30.f, 0.f, 25.f), FRotator(170.f, 0.f, 10.f), Up) : (T < 0.85f ? FRotator(5.f, 0.f, 10.f) : FRotator(-35.f, 0.f, 25.f));
		const FRotator Root = !bLanded ? LerpR(FRotator(8.f, 0.f, 0.f), FRotator(-25.f, 0.f, 0.f), Slam) : (T < 0.85f ? FRotator(-22.f, 0.f, 0.f) : LerpR(FRotator(-22.f, 0.f, 0.f), FRotator(12.f, 0.f, 0.f), Ph(T, 0.85f, 1.05f)));
		const FVector Off = bLanded ? LerpV(FVector(0, 0, -45.f), FVector(0, 0, -8.f), Ph(T, 0.85f, 1.2f)) : FVector(0, 0, -15.f * Up);
		const FRotator HL = bLanded ? LerpR(FRotator(-35.f, 0.f, 0.f), FRotator(-8.f, 0.f, 0.f), Ph(T, 0.85f, 1.2f)) : FRotator(40.f * Up, 0.f, 0.f);
		const FRotator HR = bLanded ? LerpR(FRotator(65.f, 0.f, 0.f), FRotator(8.f, 0.f, 0.f), Ph(T, 0.85f, 1.2f)) : FRotator(55.f * Up, 0.f, 0.f);
		SetPose(Env, Root, Off, SL, SR, HL, HR);
		(void)Kneel;
		break;
	}

	// ------------------------------------------------------------------ Nano Blades (Mark L / LXXXV near / Iron Spider far)
	case ESuperKind::NanoBlades:
	{
		const bool bSpider = C->GetSuit().Id == FName(TEXT("arachnid"));
		if (Once(0))
		{
			// Blades flow out of the forearms (nanotech): flat glowing cylinders on both arms, growing from the wrist.
			for (int32 s = 0; s < 2; ++s)
			{
				USceneComponent* Sh = s == 0 ? C->ShoulderL.Get() : C->ShoulderR.Get();
				MakePart(Sh, int32(EArenaShape::Cylinder), FVector(0.f, 0.f, -95.f), FVector(0.07f, 0.015f, 0.01f), Z, Tint, 9.f);
			}
			ArenaAudio::PlayAt(C, TEXT("suitup_plate_lock"), Me, 1.f);
			ArenaAudio::PlayAt(C, TEXT("hb_deploy"), Me, 0.6f);
			Punch(240.f, -6.f, 0.45f); // close-up: blades extending
		}
		const float Grow = Ph(T, 0.05f, 0.25f) * (1.f - Ph(T, Dur - 0.3f, Dur - 0.1f));
		for (int32 s = 0; s < Temp.Num() && s < 2; ++s)
		{
			if (!Temp[s]) { continue; }
			const float L = 110.f * Grow;
			Temp[s]->SetRelativeLocation(FVector(0.f, 0.f, -78.f - L * 0.5f));
			Temp[s]->SetRelativeScale3D(FVector(0.07f, 0.015f, FMath::Max(0.01f, L / 100.f)));
		}
		// Three slashes: R (down-right), L (down-left), R overhead finisher. Each: wind-up -> strike -> follow-through.
		const float Hits[3] = { 0.32f, 0.66f, 1.02f };
		FRotator SL(60.f, 0.f, -20.f), SR(60.f, 0.f, 20.f), Root = FRotator(-8.f, 0.f, 0.f);
		for (int32 h = 0; h < 3; ++h)
		{
			const float A = Hits[h] - 0.16f, B = Hits[h] + 0.12f;
			if (T < A || T > B + 0.1f) { continue; }
			const float X = Ph(T, A, B);
			const bool bRight = h != 1;
			const FRotator Wind = h == 2 ? FRotator(175.f, 0.f, 0.f) : FRotator(150.f, 0.f, bRight ? 55.f : -55.f);
			const FRotator Fol = h == 2 ? FRotator(10.f, 0.f, 0.f) : FRotator(25.f, 0.f, bRight ? -40.f : 40.f);
			(bRight ? SR : SL) = LerpR(Wind, Fol, X);
			Root = FRotator(LerpR(FRotator(6.f, bRight ? 20.f : -20.f, 0.f), FRotator(-14.f, bRight ? -25.f : 25.f, 0.f), X));
			if (T >= Hits[h] && Once(1 + h))
			{
				// Dash into the strike (lunge), then the hit.
				const FVector DashDir = Target.IsValid() ? ToTFlat : Flat;
				const float Dist = Target.IsValid() ? float(FVector::Dist2D(TargetPoint, Me)) : 400.f;
				if (Move) { Move->Velocity = DashDir * FMath::Clamp(Dist * 3.f, 600.f, bSpider && h == 0 ? 3600.f : 2200.f) + FVector(0, 0, bGround ? 120.f : 0.f); }
				ArenaAudio::PlayAt(C, TEXT("melee_swish"), Me, 1.f);
				const int32 N = ConeDamage(Me, DashDir, 420.f, 70.f, h == 2 ? 10.f : 6.f, h == 2 ? 1500.f : 700.f);
				if (N) { ArenaAudio::PlayAt(C, TEXT("melee_hit_heavy"), Me + DashDir * 200.f, 1.f); }
				AArenaTracer::Spawn(W, Me + DashDir * 60.f + FVector(0, 0, h == 2 ? 120.f : 60.f), Me + DashDir * 320.f + FVector(0, 0, h == 2 ? -40.f : 20.f), Tint, 14.f, 0.12f);
				Beat(h == 2 ? 0.12f : (N ? 0.06f : 0.f), 0.2f, TEXT("repulsor_fire"), h == 2 ? 1.6f : 0.8f, N ? 0.7f : 0.3f);
				if (h == 2) { Punch(210.f, -9.f, 0.5f); }
			}
		}
		SetPose(Env, Root, FVector(0, 0, -12.f), SL, SR, FRotator(25.f, 0.f, 0.f), FRotator(-20.f, 0.f, 0.f));
		break;
	}

	// ------------------------------------------------------------------ Iron Spider waldoes: stab & pin / sweep
	case ESuperKind::SpiderArms:
	{
		if (Once(0))
		{
			for (int32 l = 0; l < 4; ++l)
			{
				USceneComponent* Pivot = NewObject<USceneComponent>(C, *FString::Printf(TEXT("SuperPivot%d"), l));
				Pivot->SetupAttachment(C->SuitRoot);
				Pivot->RegisterComponent();
				Pivot->SetRelativeLocation(FVector(-16.f, (l % 2 ? 1.f : -1.f) * 13.f, l < 2 ? 140.f : 118.f));
				Pivot->SetRelativeRotation(FRotator(-150.f, 0.f, (l % 2 ? 1.f : -1.f) * 20.f));
				MakePart(Pivot, int32(EArenaShape::Cylinder), FVector(0, 0, 30.f), FVector(0.06f, 0.06f, 0.6f), Z, FLinearColor(0.8f, 0.6f, 0.15f), 0.f);
				MakePart(Pivot, int32(EArenaShape::Cone), FVector(0, 0, 62.f), FVector(0.07f, 0.07f, 0.12f), Z, FLinearColor(1.f, 0.75f, 0.3f), 3.f);
			}
			ArenaAudio::PlayAt(C, TEXT("suitup_servo"), Me, 1.f);
			Punch(260.f, -5.f, 0.5f);
		}
		const bool bStab = Target.IsValid() && FVector::Dist(TargetPoint, Me) < 520.f;
		const float Deploy = Ph(T, 0.05f, 0.35f) * (1.f - Ph(T, Dur - 0.35f, Dur - 0.05f));
		Spin += Dt * (bStab ? 0.f : (T > 0.5f && T < 1.1f ? 720.f : 0.f));
		for (int32 l = 0; l < 4; ++l)
		{
			const int32 PI0 = l * 2;
			if (!Temp.IsValidIndex(PI0 + 1) || !Temp[PI0]) { continue; }
			USceneComponent* Pivot = Temp[PI0]->GetAttachParent();
			if (!Pivot) { continue; }
			const float Side = (l % 2) ? 1.f : -1.f;
			FRotator WorldWant;
			float Len = 60.f + 120.f * Deploy;
			if (bStab)
			{
				// Two legs per stab (0.45 and 0.8), the others brace on the ground.
				const bool bStabber = (l < 2 && T > 0.38f && T < 0.62f) || (l >= 2 && T > 0.72f && T < 0.98f);
				const FVector Root = Pivot->GetComponentLocation();
				const FVector Aim = bStabber ? (TargetPoint - Root) : (Me + C->GetActorRightVector() * Side * 160.f - C->GetActorForwardVector() * 40.f - FVector(0, 0, HalfH) - Root);
				WorldWant = FRotationMatrix::MakeFromZ(Aim.GetSafeNormal()).Rotator();
				if (bStabber) { Len = FMath::Lerp(Len, FMath::Clamp(float(Aim.Size()), 60.f, 460.f), Ph(T, l < 2 ? 0.38f : 0.72f, l < 2 ? 0.46f : 0.8f)); }
				else { Len = FMath::Clamp(float(Aim.Size()), 60.f, 240.f) * Deploy + 60.f * (1.f - Deploy); }
			}
			else
			{
				// Sweep: legs fan out horizontally and spin around the body.
				const float Yaw = float(CR.Yaw) + Spin + l * 90.f;
				WorldWant = FRotationMatrix::MakeFromZ(FRotator(0.f, Yaw, 0.f).Vector() + FVector(0, 0, -0.25f)).Rotator();
				Len = 60.f + 300.f * Deploy;
			}
			const FRotator Folded = C->SuitRoot->GetComponentRotation().RotateVector(FRotator(-150.f, 0.f, Side * 20.f).Vector()).Rotation();
			Pivot->SetWorldRotation(LerpR(Pivot->GetComponentRotation(), Deploy > 0.05f ? WorldWant : Folded, FMath::Min(1.f, Dt * 18.f)));
			Temp[PI0]->SetRelativeLocation(FVector(0, 0, Len * 0.5f));
			Temp[PI0]->SetRelativeScale3D(FVector(0.06f, 0.06f, Len / 100.f));
			Temp[PI0 + 1]->SetRelativeLocation(FVector(0, 0, Len + 4.f));
		}
		if (bStab)
		{
			for (int32 s = 0; s < 2; ++s)
			{
				if (T >= (s == 0 ? 0.46f : 0.8f) && Once(1 + s))
				{
					AEnemyBase* E = Target.Get();
					ArenaAudio::PlayAt(C, TEXT("punch"), TargetPoint, 1.f);
					ArenaAudio::PlayAt(C, TEXT("hit_metal"), TargetPoint, 1.f);
					AArenaSparks::Spawn(W, TargetPoint, (Me - TargetPoint).GetSafeNormal(), Tint, 14, 1200.f);
					if (E)
					{
						UGameplayStatics::ApplyDamage(E, 8.f, C->GetController(), C, UDamageType::StaticClass());
						const bool bDead = !IsValid(E) || E->IsDead();
						ArenaCombat::RegisterHit(bDead);
						if (!bDead && s == 0) { Pinned.Add(E); PinPoints.Add(E->GetActorLocation()); }
					}
					Beat(0.09f, 0.2f, TEXT("repulsor_fire"), 1.3f, 0.8f);
					if (s == 1) { Punch(220.f, -8.f, 0.45f); }
				}
			}
			// Pin: hold the target where the first stab caught it until the second.
			for (int32 p = 0; p < Pinned.Num(); ++p) { if (Pinned[p].IsValid() && !Pinned[p]->IsDead() && T < 1.2f) { Pinned[p]->SetActorLocation(PinPoints[p]); } }
		}
		else
		{
			for (int32 s = 0; s < 3; ++s)
			{
				if (T >= 0.55f + s * 0.18f && Once(1 + s))
				{
					ArenaAudio::PlayAt(C, TEXT("melee_swish"), Me, 1.f);
					if (AreaDamage(Me, 430.f, 4.f, 1100.f)) { ArenaAudio::PlayAt(C, TEXT("melee_hit_heavy"), Me, 0.9f); if (s == 0) { Beat(0.08f, 0.25f, TEXT("repulsor_fire"), 1.f, 0.6f); } }
				}
			}
		}
		SetPose(Env, FRotator(-10.f, 0.f, 0.f), FVector(0, 0, -22.f), FRotator(75.f, 0.f, -25.f), FRotator(75.f, 0.f, 25.f), FRotator(30.f, 0.f, -10.f), FRotator(-25.f, 0.f, 10.f));
		break;
	}

	// ------------------------------------------------------------------ Shoulder-missile barrage (Mark VII)
	case ESuperKind::MissileBarrage:
	{
		if (Once(0))
		{
			for (int32 s = 0; s < 2; ++s) { MakePart(C->SuitRoot, int32(EArenaShape::Cube), FVector(-6.f, (s ? 1.f : -1.f) * 26.f, 150.f), FVector(0.16f, 0.14f, 0.05f), Z, C->GetSuit().SecondaryColor, 0.f); }
			ArenaAudio::PlayAt(C, TEXT("suitup_servo"), Me, 1.f);
			if (Move && !Move->IsFlying()) { C->LaunchCharacter(FVector(0, 0, 500.f), false, true); }
		}
		// Pods rise out of the shoulders (anticipation), then four salvos.
		const float Open = Ph(T, 0.05f, 0.35f) * (1.f - Ph(T, Dur - 0.35f, Dur - 0.1f));
		for (int32 s = 0; s < Temp.Num() && s < 2; ++s) { if (Temp[s]) { Temp[s]->SetRelativeLocation(FVector(-6.f, (s ? 1.f : -1.f) * 26.f, 150.f + 16.f * Open)); Temp[s]->SetRelativeRotation(FRotator(-35.f * Open, 0.f, 0.f)); } }
		for (int32 v = 0; v < 4; ++v)
		{
			if (T >= 0.4f + v * 0.2f && Once(1 + v))
			{
				FireSalvo(3, 2400.f, 7000.f, 3.f, 190.f, FLinearColor(1.f, 0.5f, 0.2f), true);
				Beat(v == 0 ? 0.1f : 0.f, 0.3f, TEXT("repulsor_fire"), 0.9f, 0.5f);
				if (v == 0) { Punch(320.f, -6.f, 0.7f); }
			}
		}
		if (Move && Move->IsFalling()) { Move->Velocity.Z = FMath::Max(float(Move->Velocity.Z), -120.f); } // hang in the air while firing
		SetPose(Env, FRotator(10.f, 0.f, 0.f), FVector::ZeroVector, FRotator(20.f, 0.f, -70.f * Open), FRotator(20.f, 0.f, 70.f * Open), FRotator(-15.f, 0.f, 0.f), FRotator(-5.f, 0.f, 0.f));
		break;
	}

	// ------------------------------------------------------------------ Remote-piece strike (Mark XLII)
	case ESuperKind::RemoteStrike:
	{
		if (Once(0))
		{
			const FSuitDef& S = C->GetSuit();
			for (int32 p = 0; p < 6; ++p)
			{
				const float A = -70.f + p * 28.f;
				const FVector From = Me - Flat * 220.f + FRotator(0.f, CR.Yaw + 180.f + A, 0.f).Vector() * 120.f + FVector(0, 0, 60.f + 25.f * (p % 3));
				MakePart(nullptr, int32(p % 2 ? EArenaShape::Cube : EArenaShape::Cylinder), From, FVector(0.22f, 0.16f, 0.3f), FRotator(FMath::FRand() * 360.f, FMath::FRand() * 360.f, 0.f), p % 2 ? S.SecondaryColor : S.PrimaryColor, 0.f);
				PieceVel.Add(FVector::ZeroVector);
			}
			ArenaAudio::PlayAt(C, TEXT("suitup_servo"), Me, 1.f);
			Punch(300.f, -5.f, 0.6f);
		}
		for (int32 p = 0; p < Temp.Num(); ++p)
		{
			UStaticMeshComponent* Pc = Temp[p];
			if (!Pc || !Pc->IsVisible()) { continue; }
			const float Launch = 0.45f + p * 0.13f;
			FVector L = Pc->GetComponentLocation();
			if (T < Launch) { Pc->AddWorldRotation(FRotator(0.f, 360.f * Dt, 0.f)); Pc->SetWorldLocation(L + FVector(0, 0, FMath::Sin((T + p) * 6.f) * 20.f * Dt)); continue; }
			if (PieceVel[p].IsZero()) { PieceVel[p] = (TargetPoint - L).GetSafeNormal() * 1500.f + FVector(0, 0, 500.f); ArenaAudio::PlayAt(C, TEXT("thruster_boost"), L, 0.5f); }
			const FVector Want = (TargetPoint - L).GetSafeNormal() * 5200.f;
			PieceVel[p] = FMath::VInterpTo(PieceVel[p], Want, Dt, 6.f);
			const FVector Next = L + PieceVel[p] * Dt;
			AArenaTracer::Spawn(W, L, Next, FLinearColor(1.f, 0.6f, 0.2f), 6.f, 0.08f);
			Pc->SetWorldLocationAndRotation(Next, PieceVel[p].Rotation());
			if (FVector::Dist(Next, TargetPoint) < 120.f || T > Launch + 1.f)
			{
				Pc->SetVisibility(false);
				AArenaBlastFX::Spawn(W, Next, 200.f, FLinearColor(1.f, 0.5f, 0.15f), 0.3f, p == 0);
				AreaDamage(Next, 220.f, 4.f, 900.f);
				ArenaAudio::PlayAt(C, TEXT("hit_metal"), Next, 1.f);
				if (p == 0) { Beat(0.1f, 0.25f, TEXT("explosion_nearby"), 0.8f, 0.6f); }
			}
		}
		// Pose: the classic point-and-send (right arm extended toward the target, left braced).
		const FRotator Point(90.f + FMath::Clamp(float(FRotator::NormalizeAxis(CR.Pitch)), -40.f, 40.f), 0.f, 0.f);
		SetPose(Env, FRotator(4.f, 0.f, 0.f), FVector::ZeroVector, FRotator(30.f, 0.f, -25.f), LerpR(FRotator(20.f, 0.f, 20.f), Point, Ph(T, 0.1f, 0.4f)), FRotator(8.f, 0.f, 0.f), FRotator(-8.f, 0.f, 0.f));
		break;
	}

	// ------------------------------------------------------------------ Support pulse (Rescue)
	case ESuperKind::SupportPulse:
	{
		if (Once(0)) { ArenaAudio::PlayAt(C, TEXT("unibeam_charge"), Me, 0.7f, 0.f); }
		const float Cross = Ph(T, 0.f, 0.45f);
		if (T >= 0.5f && Once(1))
		{
			C->Heal(35.f);
			C->AddEnergy(30.f);
			AreaDamage(Me, 1200.f, 3.f, 1900.f);
			AArenaTelegraph::Spawn(W, Me - FVector(0, 0, HalfH), 1200.f, 0.5f, Tint);
			AArenaBlastFX::Spawn(W, Me, 700.f, Tint, 0.5f, true);
			ArenaAudio::PlayAt(C, TEXT("unibeam_impact"), Me, 1.f);
			ArenaAudio::PlayAt(C, TEXT("pickup_health"), Me, 1.f);
			Beat(0.1f, 0.3f, TEXT("explosion_nearby"), 0.7f, 0.6f);
			Punch(260.f, -8.f, 0.5f);
		}
		const float Open = Ph(T, 0.5f, 0.65f);
		SetPose(Env, LerpR(FRotator(-12.f, 0.f, 0.f), FRotator(10.f, 0.f, 0.f), Open), FVector(0, 0, -20.f * (1.f - Open)),
			LerpR(FRotator(100.f, 40.f, 0.f), FRotator(60.f, 0.f, -75.f), Open), LerpR(FRotator(100.f, -40.f, 0.f), FRotator(60.f, 0.f, 75.f), Open),
			FRotator(15.f * (1.f - Open), 0.f, -8.f * Open), FRotator(-10.f * (1.f - Open), 0.f, 8.f * Open));
		(void)Cross;
		break;
	}

	// ------------------------------------------------------------------ Ground pound / mega stomp (Hulkbuster, Igor, Mark I)
	case ESuperKind::GroundPound:
	{
		const bool bBig = C->IsHulkbusterActive() || C->GetSuit().Id == FName(TEXT("mark-xxxviii-igor"));
		if (Once(0))
		{
			ArenaAudio::PlayAt(C, TEXT("suitup_servo_heavy"), Me, 1.f);
			C->LaunchCharacter(FVector(0, 0, bGround ? 1300.f : 300.f) + Flat * 300.f, false, true);
			if (Move && Move->IsFlying()) { Move->SetMovementMode(MOVE_Falling); }
		}
		const float Up = Ph(T, 0.f, 0.45f);
		if (T > 0.5f && !bLanded)
		{
			if (Once(1)) { ArenaAudio::PlayAt(C, TEXT("thruster_boost"), Me, 1.f); }
			if (Move) { Move->Velocity = FVector(Move->Velocity.X * 0.5f, Move->Velocity.Y * 0.5f, -5200.f); }
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(Pound), false, C);
			const bool bHit = W->LineTraceSingleByChannel(H, Me, Me - FVector(0, 0, HalfH + 80.f), ECC_WorldStatic, Q);
			if (bHit || bGround || T > 1.3f)
			{
				bLanded = true;
				const FVector G = bHit ? FVector(H.ImpactPoint) : Me - FVector(0, 0, HalfH);
				const float R = bBig ? 1300.f : 950.f;
				AreaDamage(G, R, bBig ? 16.f : 11.f, 2300.f);
				AArenaTelegraph::Spawn(W, G, R, 0.5f, FLinearColor(1.f, 0.5f, 0.15f));
				AArenaBlastFX::Spawn(W, G + FVector(0, 0, 40.f), R * 0.6f, FLinearColor(1.f, 0.55f, 0.2f), 0.5f, true);
				ArenaFeel::SpawnImpactDecal(W, G, FVector::UpVector, R * 0.55f, 16.f);
				AArenaSparks::SpawnDebris(W, G, FVector::UpVector, FLinearColor(0.25f, 0.24f, 0.22f), 90.f, 12);
				ArenaAudio::PlayAt(C, TEXT("explosion_large"), G, 1.3f);
				ArenaAudio::PlayAt(C, bBig ? FName(TEXT("hb_footstep_concrete")) : FName(TEXT("melee_hit_heavy")), G, 1.2f);
				Beat(0.15f, 0.15f, TEXT("explosion_nearby"), 1.6f, 1.f);
				Punch(bBig ? 380.f : 200.f, -12.f, 0.6f);
				T = 1.0f;
			}
		}
		const float Rec = bLanded ? Ph(T, 1.05f, 1.6f) : 0.f;
		const FRotator Arms = !bLanded ? LerpR(FRotator(-20.f, 0.f, 0.f), FRotator(175.f, 0.f, 0.f), Up) : LerpR(FRotator(10.f, 0.f, 0.f), FRotator(-10.f, 0.f, 0.f), Rec);
		SetPose(Env, !bLanded ? FRotator(-10.f * Up, 0.f, 0.f) : LerpR(FRotator(-30.f, 0.f, 0.f), Z, Rec), bLanded ? LerpV(FVector(0, 0, -50.f), FVector::ZeroVector, Rec) : FVector::ZeroVector,
			FRotator(Arms.Pitch, 0.f, -15.f), FRotator(Arms.Pitch, 0.f, 15.f), bLanded ? LerpR(FRotator(-40.f, 0.f, -15.f), Z, Rec) : FRotator(45.f * Up, 0.f, 0.f), bLanded ? LerpR(FRotator(60.f, 0.f, 15.f), Z, Rec) : FRotator(35.f * Up, 0.f, 0.f));
		break;
	}

	// ------------------------------------------------------------------ Minigun + micro-rocket pod (War Machine / Iron Patriot)
	case ESuperKind::MinigunPod:
	{
		if (Once(0))
		{
			USceneComponent* R = C->SuitRoot;
			MakePart(R, int32(EArenaShape::Cylinder), FVector(10.f, 30.f, 158.f), FVector(0.09f, 0.09f, 0.55f), FRotator(-90.f, 0.f, 0.f), FLinearColor(0.12f, 0.12f, 0.13f), 0.f);
			MakePart(R, int32(EArenaShape::Cube), FVector(-4.f, -28.f, 156.f), FVector(0.2f, 0.16f, 0.14f), Z, FLinearColor(0.2f, 0.2f, 0.22f), 0.f);
			ArenaAudio::PlayAt(C, TEXT("suitup_servo"), Me, 1.f);
			MinigunAudio = ArenaAudio::Attach(TEXT("wm_minigun_fire_loop"), C->GetRootComponent(), 0.8f, true);
			Punch(300.f, -4.f, 0.5f);
		}
		const APlayerController* PC = Cast<APlayerController>(C->GetController());
		const bool bHeld = PC && (PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_LeftTrigger));
		const float Spinup = Ph(T, 0.1f, 0.35f);
		if (Temp.IsValidIndex(0) && Temp[0]) { Temp[0]->AddLocalRotation(FRotator(0.f, 0.f, 1800.f * Spinup * Dt)); }
		if (T > 0.35f && T < 3.f && (bHeld || T < 1.2f))
		{
			if (MinigunAudio && !MinigunAudio->IsPlaying()) { MinigunAudio->Play(); }
			GunAcc += Dt;
			while (GunAcc > 0.055f)
			{
				GunAcc -= 0.055f;
				const FVector Muz = Temp.IsValidIndex(0) && Temp[0] ? Temp[0]->GetComponentLocation() + Look * 40.f : Chest;
				const FVector Dir = FMath::VRandCone((C->ComputeAimPoint() - Muz).GetSafeNormal(), FMath::DegreesToRadians(1.6f));
				const ArenaCombat::FShot S = ArenaCombat::FireHitscan(C, C->GetController(), Muz, Dir, 9000.f, 0.7f, 0.f, 6.f, FLinearColor(1.f, 0.8f, 0.4f), 120.f);
				AArenaTracer::Spawn(W, Muz, S.End, FLinearColor(1.f, 0.85f, 0.5f), 3.5f, 0.05f);
				AArenaBlastFX::Spawn(W, Muz, 14.f, FLinearColor(1.f, 0.7f, 0.3f), 0.04f, false);
				ArenaCombat::Rumble(C, 0.2f, 0.05f);
				if (C->GetController()) { C->GetController()->SetControlRotation(C->GetController()->GetControlRotation() + FRotator(0.06f, FMath::FRandRange(-0.05f, 0.05f), 0.f)); }
			}
		}
		else if (T > 0.35f && Once(1))
		{
			// Release: spin-down + micro-rocket pod salvo, the hero beat.
			if (MinigunAudio) { MinigunAudio->FadeOut(0.25f, 0.f); MinigunAudio = nullptr; }
			FireSalvo(8, 6200.f, 0.f, 3.f, 180.f, FLinearColor(1.f, 0.55f, 0.2f), true);
			Beat(0.1f, 0.3f, TEXT("explosion_nearby"), 0.8f, 0.8f);
			Punch(260.f, -8.f, 0.45f);
			T = FMath::Max(T, Dur - 0.5f);
		}
		SetPose(Env, FRotator(-6.f, 0.f, 0.f), FVector(0, 0, -6.f), FRotator(85.f, 0.f, -10.f), FRotator(95.f, 0.f, 10.f), FRotator(18.f, 0.f, -6.f), FRotator(-14.f, 0.f, 6.f));
		break;
	}

	// ------------------------------------------------------------------ Stealth cloak slash
	case ESuperKind::CloakSlash:
	{
		if (Once(0)) { ArenaAudio::PlayAt(C, TEXT("hud_low_power"), Me, 0.7f); C->CloakLeft = FMath::Max(C->CloakLeft, 1.2f); }
		// 0..0.32: shimmer out (flicker), 0.32..0.45: invisible blink behind the target, 0.45: reveal + slash.
		const bool bHidden = T > 0.32f && T < 0.45f;
		const bool bFlicker = T < 0.32f && FMath::Fmod(T * 30.f, 1.f) < 0.5f && T > 0.12f;
		if (C->SuitRoot) { C->SuitRoot->SetVisibility(!(bHidden || bFlicker), true); }
		if (T >= 0.38f && Once(1))
		{
			const FVector Dest = Target.IsValid() ? TargetPoint - ToTFlat * -170.f + FVector(0, 0, 20.f) : Me + Flat * 900.f;
			C->SetActorLocation(Dest, true);
			if (C->GetController() && Target.IsValid()) { C->GetController()->SetControlRotation(FRotator(CR.Pitch, (TargetPoint - Dest).Rotation().Yaw, 0.f)); }
			ArenaAudio::PlayAt(C, TEXT("thruster_boost"), Dest, 0.6f);
		}
		if (T >= 0.45f && Once(2))
		{
			if (C->SuitRoot) { C->SuitRoot->SetVisibility(true, true); }
			const FVector Fwd = C->GetActorForwardVector();
			ArenaAudio::PlayAt(C, TEXT("melee_swish"), C->GetActorLocation(), 1.f);
			const int32 N = ConeDamage(C->GetActorLocation(), Fwd, 420.f, 75.f, 15.f, 1300.f);
			if (N) { ArenaAudio::PlayAt(C, TEXT("melee_hit_heavy"), C->GetActorLocation() + Fwd * 150.f, 1.f); }
			AArenaTracer::Spawn(W, C->GetActorLocation() + Fwd * 60.f + C->GetActorRightVector() * 120.f + FVector(0, 0, 60.f), C->GetActorLocation() + Fwd * 160.f - C->GetActorRightVector() * 140.f, Tint, 16.f, 0.16f);
			Beat(0.12f, 0.15f, TEXT("repulsor_fire"), 1.5f, 0.9f);
			Punch(200.f, -10.f, 0.5f);
		}
		const float X = Ph(T, 0.45f, 0.6f);
		SetPose(Env, LerpR(FRotator(-18.f, 0.f, 0.f), FRotator(-8.f, -30.f, 0.f), X), FVector(0, 0, -25.f),
			FRotator(40.f, 0.f, -20.f), LerpR(FRotator(160.f, 0.f, 60.f), FRotator(30.f, 0.f, -45.f), X), FRotator(35.f, 0.f, 0.f), FRotator(-30.f, 0.f, 0.f));
		break;
	}

	// ------------------------------------------------------------------ Heavy blast / unibeam overcharge (Mark III family)
	case ESuperKind::HeavyBlast:
	default:
	{
		if (Once(0)) { ArenaAudio::PlayAt(C, TEXT("unibeam_charge"), Me, 1.f, 0.f); ArenaAudio::PlayAt(C, TEXT("reactor_overheat_vent"), Me, 0.4f); Punch(230.f, -7.f, 0.75f); }
		const float Charge = Ph(T, 0.f, 0.6f);
		if (T < 0.62f && FMath::FRand() < 0.5f) { AArenaSparks::Spawn(W, Chest + FMath::VRand() * 60.f, (Chest - Me).GetSafeNormal(), Tint, 2, 300.f); }
		if (T >= 0.62f && Once(1))
		{
			const FVector Dir = Look;
			const ArenaCombat::FShot S = ArenaCombat::FireHitscan(C, C->GetController(), Chest, Dir, 12000.f, 20.f, 380.f, 60.f, Tint, 2000.f);
			AArenaTracer::Spawn(W, Chest, S.End, Tint, 95.f, 0.35f);
			AArenaBlastFX::Spawn(W, Chest + Dir * 40.f, 90.f, Tint, 0.2f, false);
			ArenaAudio::PlayAt(C, TEXT("unibeam_fire"), Chest, 1.2f, 0.f);
			C->LaunchCharacter(-Dir * 900.f + FVector(0, 0, 150.f), false, false); // recoil
			Beat(0.12f, 0.2f, TEXT("unibeam"), 1.f, 1.f);
		}
		// Arms swing back (charge, chest out), snap forward on release, settle.
		const float Rel = Ph(T, 0.62f, 0.8f);
		SetPose(Env, LerpR(FRotator(14.f * Charge, 0.f, 0.f), FRotator(-6.f, 0.f, 0.f), Rel), FVector(0, 0, -18.f * Charge * (1.f - Rel)),
			LerpR(FRotator(-45.f * Charge, 0.f, -25.f * Charge), FRotator(35.f, 0.f, -20.f), Rel), LerpR(FRotator(-45.f * Charge, 0.f, 25.f * Charge), FRotator(35.f, 0.f, 20.f), Rel),
			FRotator(-18.f * Charge, 0.f, 0.f), FRotator(22.f * Charge, 0.f, 0.f));
		break;
	}
	}
	if (T >= Dur) { End(); }
}

// -------------------------------------------------------------------------------------------- tick / input

void USuitSuperComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AIronManCharacter* C = Char();
	if (!C) { return; }
	// Real time for input timing (hit-stop slows the world, not the player's fingers).
	const float RealDt = FMath::Min(0.05f, FApp::GetDeltaTime());
	if (!bActive) { CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaTime); }
	if (C->IsDead() || C->IsInCinematic() || C->bCareerSuitOff)
	{
		if (bActive) { End(); }
		if (bWebAttached) { bWebAttached = false; if (WebLine) { WebLine->SetVisibility(false); } }
		if (bMaskOpen && C->bCareerSuitOff) { bMaskOpen = false; }
	}
	else
	{
		TickInput(RealDt);
		if (bActive) { TickMove(DeltaTime); }
	}
	TickSwing(DeltaTime);
	TickMask(RealDt);
	TickCameraPunch(RealDt);
}

void USuitSuperComponent::TickInput(float Dt)
{
	AIronManCharacter* C = Char();
	APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
	if (!PC || PC->IsPaused() || C->bWheelOpen) { return; }
	if (bRightDown)
	{
		RightDownTime += Dt;
		const bool bHeld = PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_LeftTrigger);
		if (bHeld && RightDownTime >= 0.3f) { bRightDown = false; Begin(CurrentDef().Kind); }
		else if (!bHeld) { bRightDown = false; C->UseSpecial(); } // tap: the suit's normal special
	}
	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastLeftClick < 0.28 && IsReady() && !bSwingMode) { Begin(CurrentDef().Kind); LastLeftClick = -10.0; }
		else { LastLeftClick = Now; }
	}
	// Gamepad: both triggers + right shoulder = super.
	if (PC->WasInputKeyJustPressed(EKeys::Gamepad_RightThumbstick) && PC->IsInputKeyDown(EKeys::Gamepad_LeftTrigger) && IsReady()) { Begin(CurrentDef().Kind); }
	if (PC->WasInputKeyJustPressed(UCareerInput::Key(TEXT("Faceplate"), EKeys::H)) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left)) { ToggleMask(); }
	if ((PC->WasInputKeyJustPressed(UCareerInput::Key(TEXT("WebSwing"), EKeys::B)) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right)) && CanSwing())
	{
		bSwingMode = !bSwingMode;
		if (bSwingMode && C->IsFlying()) { C->SetFlying(false); }
		if (!bSwingMode && bWebAttached) { bWebAttached = false; if (WebLine) { WebLine->SetVisibility(false); } }
		if (UCareerSubsystem* CS = UCareerSubsystem::Get(C)) { CS->Toast(FText::FromString(bSwingMode ? TEXT("WEB SWING  -  Space: swing  R: web-zip  B: fly") : TEXT("FLIGHT MODE")), FLinearColor(1.f, 0.35f, 0.25f)); }
		ArenaAudio::Play2D(C, TEXT("hud_beep_select"), 0.7f);
	}
	if (bSwingMode && !CanSwing()) { bSwingMode = false; }
}

// -------------------------------------------------------------------------------------------- web swing (Iron Spider)

void USuitSuperComponent::TickSwing(float Dt)
{
	AIronManCharacter* C = Char();
	UWorld* W = GetWorld();
	if (!C || !W) { return; }
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	APlayerController* PC = Cast<APlayerController>(C->GetController());
	const bool bUsable = bSwingMode && PC && !C->IsDead() && !C->IsInCinematic() && !C->bCareerSuitOff && !bActive;
	auto Detach = [&](bool bFling)
	{
		if (!bWebAttached) { return; }
		bWebAttached = false;
		if (WebLine) { WebLine->SetVisibility(false); }
		if (bFling && Move) { Move->Velocity = Move->Velocity * 1.12f + FVector(0, 0, 420.f); ArenaAudio::PlayAt(C, TEXT("melee_swish"), C->GetActorLocation(), 0.7f); }
	};
	if (!bUsable)
	{
		Detach(false);
		CamRoll = FMath::FInterpTo(CamRoll, 0.f, Dt, 6.f);
		if (bSwingMode && C->IsFlying()) { C->SetFlying(false); }
		return;
	}
	if (C->IsFlying()) { C->SetFlying(false); } // swing replaces flight
	const FVector Me = C->GetActorLocation();
	const FRotator CR = PC->GetControlRotation();
	const FVector Look = CR.Vector();
	const FVector Hand = C->Rig.PalmR ? C->Rig.PalmR->GetComponentLocation() : Me + FVector(0, 0, 60.f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WebTrace), false, C);
	const bool bAir = Move && !Move->IsMovingOnGround();

	// Web-zip (R): pull to the aimed point.
	if (PC->WasInputKeyJustPressed(UCareerInput::Key(TEXT("WebZip"), EKeys::R)) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left))
	{
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, Me + Look * 60.f, Me + Look * 6000.f, ECC_Visibility, Q))
		{
			Detach(false);
			ZipTarget = H.ImpactPoint + H.ImpactNormal * 120.f;
			ZipLeft = 1.2f;
			Anchor = H.ImpactPoint;
			ArenaAudio::PlayAt(C, TEXT("missile_flyby"), Me, 0.6f);
		}
	}
	if (ZipLeft > 0.f && Move)
	{
		ZipLeft -= Dt;
		const FVector To = ZipTarget - Me;
		if (To.Size() < 180.f || ZipLeft <= 0.f) { ZipLeft = 0.f; Move->Velocity *= 0.35f; Move->Velocity.Z = FMath::Max(float(Move->Velocity.Z), 450.f); }
		else { if (Move->IsMovingOnGround()) { Move->SetMovementMode(MOVE_Falling); } Move->Velocity = FMath::VInterpTo(Move->Velocity, To.GetSafeNormal() * 4200.f, Dt, 10.f); }
	}

	// Space while airborne: shoot a web up-and-ahead and hang on it; release = fling.
	const bool bSpace = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
	if (bSpace && bAir && !bWebAttached && ZipLeft <= 0.f && Move && Move->Velocity.Z < 300.f)
	{
		const FVector Flat = FRotator(0.f, CR.Yaw, 0.f).Vector();
		const FVector Dirs[4] = { (Flat + FVector(0, 0, 1.1f)).GetSafeNormal(), (Flat * 0.6f + FVector(0, 0, 1.f)).GetSafeNormal(),
			(Flat + C->GetActorRightVector() * 0.6f + FVector(0, 0, 1.f)).GetSafeNormal(), (Flat - C->GetActorRightVector() * 0.6f + FVector(0, 0, 1.f)).GetSafeNormal() };
		FHitResult H;
		bool bFound = false;
		for (const FVector& D : Dirs) { if (W->LineTraceSingleByChannel(H, Me, Me + D * 4200.f, ECC_Visibility, Q)) { bFound = true; break; } }
		// Open sky: a high virtual anchor so swinging still works over parks / streets (movie licence).
		Anchor = bFound ? FVector(H.ImpactPoint) : Me + Flat * 1500.f + FVector(0, 0, 1800.f);
		RopeLen = FMath::Clamp(float(FVector::Dist(Anchor, Me)), 500.f, 3800.f);
		bWebAttached = true;
		ArenaAudio::PlayAt(C, TEXT("missile_flyby"), Me, 0.45f, 0.15f);
		ArenaAudio::PlayAt(C, TEXT("hud_beep_select"), Me, 0.3f);
	}
	if (bWebAttached && (!bSpace || !bAir)) { Detach(bAir); }

	if (bWebAttached && Move)
	{
		// Pendulum: gravity is the CMC's; enforce the rope as an inelastic constraint + pump along the swing.
		FVector ToMe = Me - Anchor;
		const float Dist = ToMe.Size();
		const FVector Radial = ToMe / FMath::Max(1.f, Dist);
		FVector V = Move->Velocity;
		if (Dist > RopeLen)
		{
			const float Out = FVector::DotProduct(V, Radial);
			if (Out > 0.f) { V -= Radial * Out; }
			C->SetActorLocation(Anchor + Radial * RopeLen, true);
		}
		// Pump: forward input adds tangential speed (the "kick" at the bottom of the arc).
		const FVector LookFlat = FVector(Look.X, Look.Y, 0.f).GetSafeNormal();
		const FVector Tangent = (LookFlat - Radial * FVector::DotProduct(LookFlat, Radial)).GetSafeNormal();
		V += Tangent * 1400.f * Dt;
		// Shorten the rope a little (reel in) so arcs gain height.
		RopeLen = FMath::Max(400.f, RopeLen - 120.f * Dt);
		Move->Velocity = V.GetClampedToMaxSize(5200.f);
		// Web line from the hand to the anchor.
		if (!WebLine)
		{
			WebLine = NewObject<UStaticMeshComponent>(C);
			WebLine->SetStaticMesh(ArenaVisuals::GetShapeMesh(EArenaShape::Cylinder));
			WebLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WebLine->CastShadow = false;
			WebLine->RegisterComponent();
			WebLine->SetAbsolute(true, true, true);
			ArenaVisuals::PaintComponent(C, WebLine, FLinearColor(0.85f, 0.88f, 0.95f), 2.f, EArenaSurface::Glow);
		}
		const FVector Mid = (Hand + Anchor) * 0.5f;
		const FVector Axis = (Anchor - Hand);
		WebLine->SetVisibility(true);
		WebLine->SetWorldLocationAndRotation(Mid, FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator());
		WebLine->SetWorldScale3D(FVector(0.018f, 0.018f, Axis.Size() / 100.f));
		// Pose: right arm up the web line, legs tucked; camera rolls into the arc.
		const FVector LocalUp = C->GetActorTransform().InverseTransformVectorNoScale(Axis.GetSafeNormal());
		const float ArmPitch = FMath::RadiansToDegrees(FMath::Atan2(LocalUp.X, -LocalUp.Z)) ;
		C->PoseOverride.Weight = FMath::FInterpTo(C->PoseOverride.Weight, 0.9f, Dt, 8.f);
		C->PoseOverride.ShoulderR = FRotator(FMath::Clamp(ArmPitch, 90.f, 185.f), 0.f, 10.f);
		C->PoseOverride.ShoulderL = FRotator(40.f, 0.f, -35.f);
		C->PoseOverride.HipL = FRotator(45.f, 0.f, 0.f);
		C->PoseOverride.HipR = FRotator(20.f, 0.f, 0.f);
		C->PoseOverride.Root = FRotator(0.f, 0.f, FMath::Clamp(float(FVector::DotProduct(V, C->GetActorRightVector())) * 0.01f, -25.f, 25.f));
		C->PoseOverride.RootOffset = FVector::ZeroVector;
		CamRoll = FMath::FInterpTo(CamRoll, FMath::Clamp(float(FVector::DotProduct(V, C->GetActorRightVector())) * 0.006f, -12.f, 12.f), Dt, 4.f);
	}
	else
	{
		if (!bActive && C->PoseOverride.Weight > 0.f) { C->PoseOverride.Weight = FMath::FInterpTo(C->PoseOverride.Weight, 0.f, Dt, 6.f); if (C->PoseOverride.Weight < 0.01f) { C->PoseOverride.Weight = 0.f; } }
		CamRoll = FMath::FInterpTo(CamRoll, 0.f, Dt, 5.f);
	}
	if (C->FollowCamera && (bWebAttached || FMath::Abs(CamRoll) > 0.01f)) { FRotator R = C->FollowCamera->GetRelativeRotation(); R.Roll = CamRoll; C->FollowCamera->SetRelativeRotation(R); }
}

// -------------------------------------------------------------------------------------------- faceplate

void USuitSuperComponent::ToggleMask()
{
	AIronManCharacter* C = Char();
	if (!C || C->IsDead() || C->IsInCinematic() || C->bCareerSuitOff || bActive) { return; }
	USceneComponent* Helmet = nullptr;
	for (USceneComponent* H : C->Rig.Helmets) { if (H && H->IsVisible()) { Helmet = H; break; } }
	if (!Helmet) { return; }
	if (!bMaskOpen)
	{
		// Faceplate parts = the helmet's front pieces (face / eyes / jaw / chin / cheeks / mouth).
		if (!MaskHinge || MaskHinge->GetAttachParent() != Helmet)
		{
			for (FMaskPart& M : MaskParts) { if (M.C.IsValid() && M.Parent.IsValid()) { M.C->AttachToComponent(M.Parent.Get(), FAttachmentTransformRules::KeepRelativeTransform); } }
			MaskParts.Reset();
			if (MaskHinge) { MaskHinge->DestroyComponent(); }
			MaskHinge = NewObject<USceneComponent>(C, TEXT("MaskHinge"));
			MaskHinge->SetupAttachment(Helmet);
			MaskHinge->RegisterComponent();
			MaskHinge->SetRelativeLocation(FVector(-1.f, 0.f, 186.f)); // top of the head, the Mark III hinge line
			TArray<USceneComponent*> Kids;
			Helmet->GetChildrenComponents(false, Kids);
			for (USceneComponent* K : Kids)
			{
				if (K == MaskHinge) { continue; }
				const FString N = K->GetName();
				if (N.Contains(TEXT("Face")) || N.Contains(TEXT("Eye")) || N.Contains(TEXT("Jaw")) || N.Contains(TEXT("Chin")) || N.Contains(TEXT("Cheek")) || N.Contains(TEXT("Mouth")))
				{
					FMaskPart M; M.C = K; M.Parent = Helmet;
					MaskParts.Add(M);
					K->AttachToComponent(MaskHinge, FAttachmentTransformRules::KeepWorldTransform);
				}
			}
		}
		// The pilot's face (head-only avatar) sits in the helmet opening.
		if (!Face && GetWorld())
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Face = GetWorld()->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), FTransform::Identity, SP);
			if (Face)
			{
				if (UCareerSubsystem* CS = UCareerSubsystem::Get(C)) { Face->ApplyConfig(CS->GetAvatar(), false); }
				const FVector HeadC = Face->SetHeadOnly();
				Face->AttachToComponent(Helmet, FAttachmentTransformRules::KeepRelativeTransform);
				Face->SetActorRelativeScale3D(FVector(0.9f));
				Face->SetActorRelativeLocation(FVector(4.f, 0.f, 171.f) - HeadC * 0.9f);
				Face->SetActorHiddenInGame(true);
			}
		}
		bMaskOpen = true;
		bWasFP = bWasFP || C->bFirstPerson;
		if (C->bFirstPerson) { C->SetFirstPerson(false); }
		ArenaAudio::PlayAt(C, TEXT("suitup_servo"), C->GetActorLocation(), 0.9f);
		Punch(170.f, -8.f, 0.9f);
	}
	else
	{
		bMaskOpen = false;
		ArenaAudio::PlayAt(C, TEXT("suitup_servo"), C->GetActorLocation(), 0.8f);
	}
}

void USuitSuperComponent::TickMask(float Dt)
{
	AIronManCharacter* C = Char();
	if (!C) { return; }
	if (bMaskOpen && (C->bCareerSuitOff || C->IsDead())) { bMaskOpen = false; }
	const float Prev = MaskAlpha;
	MaskAlpha = FMath::FInterpConstantTo(MaskAlpha, bMaskOpen ? 1.f : 0.f, Dt, 1.f / 0.55f);
	if (MaskAlpha == Prev && MaskAlpha <= 0.f) { return; }
	// Two-stage motion: unlock (small drop + forward pop, 0..0.25) then the lift over the head (0.25..1), ease in-out.
	const float Pop = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(MaskAlpha / 0.25f, 0.f, 1.f), 2.f);
	const float Lift = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((MaskAlpha - 0.25f) / 0.75f, 0.f, 1.f), 2.5f);
	if (MaskHinge)
	{
		MaskHinge->SetRelativeLocation(FVector(-1.f + 1.5f * Pop, 0.f, 186.f - 0.6f * Pop + 1.f * Lift));
		MaskHinge->SetRelativeRotation(FRotator(98.f * Lift, 0.f, 0.f));
	}
	if (Face) { Face->SetActorHiddenInGame(MaskAlpha < 0.2f); }
	// Eyes: dark while open, light up (HUD boot) when closing completes.
	for (FMaskPart& M : MaskParts)
	{
		if (UPrimitiveComponent* Pc = Cast<UPrimitiveComponent>(M.C.Get())) { if (M.C->GetName().Contains(TEXT("Eye"))) { Pc->SetVisibility(MaskAlpha < 0.05f || (!bMaskOpen && MaskAlpha < 0.12f)); } }
	}
	if (Prev > 0.f && MaskAlpha <= 0.f)
	{
		ArenaAudio::PlayAt(C, TEXT("suitup_helmet_close"), C->GetActorLocation(), 1.f);
		ArenaAudio::Play2D(C, TEXT("hud_beep_confirm"), 0.6f);
		if (bWasFP && !bActive) { C->SetFirstPerson(true); bWasFP = false; }
	}
}
