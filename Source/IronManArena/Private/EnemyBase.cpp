#include "EnemyBase.h"
#include "ArenaFeel.h"
#include "BossTitan.h"
#include "IronManArena.h"
#include "IronManCharacter.h"
#include "IronProjectile.h"
#include "ArenaGameMode.h"
#include "ArenaBlastFX.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Controller.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ----------------------------------------------------------------------------- combat realism data

namespace
{
	EEnemyHitClass EnemyHitClassCurrent = EEnemyHitClass::Normal;

	TSharedPtr<FJsonObject> EnemyRealismObj(const TSharedPtr<FJsonObject>& From, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return (From.IsValid() && From->TryGetObjectField(Field, Out) && Out) ? *Out : TSharedPtr<FJsonObject>();
	}

	void EnemyRealismNum(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, float& InOut, float Scale = 1.f)
	{
		double V = 0.0;
		if (From.IsValid() && From->TryGetNumberField(Field, V)) { InOut = float(V) * Scale; }
	}

	void EnemyRealismRange(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, float& OutMin, float& OutMax)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (From.IsValid() && From->TryGetArrayField(Field, Arr) && Arr && Arr->Num() >= 2)
		{
			const float A = float((*Arr)[0]->AsNumber());
			const float B = float((*Arr)[1]->AsNumber());
			OutMin = FMath::Min(A, B);
			OutMax = FMath::Max(A, B);
		}
	}

	CombatRealism::FTuning EnemyRealismLoad()
	{
		CombatRealism::FTuning T;
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/combat_realism.json");
		if (!FFileHelper::LoadFileToString(Text, *Path)) { return T; }
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return T; }
		T.bLoadedFromFile = true;

		const TSharedPtr<FJsonObject> Hit = EnemyRealismObj(Root, TEXT("hitReaction"));
		const TSharedPtr<FJsonObject> Knock = EnemyRealismObj(Hit, TEXT("knockback"));
		EnemyRealismNum(Knock, TEXT("halfLifeSec"), T.KnockHalfLife);
		EnemyRealismNum(Knock, TEXT("droneAngularKickMaxDeg"), T.DroneKickMaxDeg);
		const TSharedPtr<FJsonObject> KnockScale = EnemyRealismObj(Knock, TEXT("cmPerSecPerDamage"));
		EnemyRealismNum(KnockScale, TEXT("drone"), T.KnockDrone);
		EnemyRealismNum(KnockScale, TEXT("tank"), T.KnockTank);
		EnemyRealismNum(KnockScale, TEXT("boss"), T.KnockBoss);
		const TSharedPtr<FJsonObject> StaggerSec = EnemyRealismObj(EnemyRealismObj(Hit, TEXT("stagger")), TEXT("sec"));
		EnemyRealismNum(StaggerSec, TEXT("drone"), T.StaggerDrone);
		EnemyRealismNum(StaggerSec, TEXT("tank"), T.StaggerTank);
		EnemyRealismNum(StaggerSec, TEXT("boss"), T.StaggerBoss);
		const TSharedPtr<FJsonObject> Stop = EnemyRealismObj(Hit, TEXT("hitStop"));
		EnemyRealismNum(Stop, TEXT("repulsorKillMs"), T.HitStopKillSec, 0.001f);
		EnemyRealismNum(Stop, TEXT("heavyHitMs"), T.HitStopHeavySec, 0.001f);
		EnemyRealismNum(Stop, TEXT("bossPhaseBreakMs"), T.HitStopBossPhaseSec, 0.001f);
		EnemyRealismNum(Stop, TEXT("timeDilation"), T.HitStopDilation);

		const TSharedPtr<FJsonObject> Drone = EnemyRealismObj(EnemyRealismObj(Root, TEXT("enemyDamage")), TEXT("drone"));
		EnemyRealismNum(Drone, TEXT("wobbleBelow"), T.DroneWobbleBelow);
		EnemyRealismNum(Drone, TEXT("wobbleDeg"), T.DroneWobbleDeg);
		EnemyRealismNum(Drone, TEXT("wobbleHz"), T.DroneWobbleHz);
		EnemyRealismNum(Drone, TEXT("aimSpreadMul"), T.DroneAimSpreadMul);
		EnemyRealismNum(Drone, TEXT("smokeBelow"), T.DroneSmokeBelow);
		EnemyRealismRange(Drone, TEXT("sparkIntervalSec"), T.DroneSparkMin, T.DroneSparkMax);
		EnemyRealismNum(Drone, TEXT("altitudeSagCm"), T.DroneSagCm);
		EnemyRealismRange(EnemyRealismObj(Drone, TEXT("death")), TEXT("spiralSec"), T.DroneSpiralMin, T.DroneSpiralMax);

		// Sanity clamps (a bad edit must not freeze the game or fling enemies across the map).
		T.KnockHalfLife = FMath::Clamp(T.KnockHalfLife, 0.02f, 1.f);
		T.KnockDrone = FMath::Clamp(T.KnockDrone, 0.f, 2000.f);
		T.KnockTank = FMath::Clamp(T.KnockTank, 0.f, 2000.f);
		T.KnockBoss = FMath::Clamp(T.KnockBoss, 0.f, 2000.f);
		T.DroneKickMaxDeg = FMath::Clamp(T.DroneKickMaxDeg, 0.f, 60.f);
		T.StaggerDrone = FMath::Clamp(T.StaggerDrone, 0.f, 3.f);
		T.StaggerTank = FMath::Clamp(T.StaggerTank, 0.f, 3.f);
		T.StaggerBoss = FMath::Clamp(T.StaggerBoss, 0.f, 3.f);
		T.HitStopKillSec = FMath::Clamp(T.HitStopKillSec, 0.f, 0.2f);
		T.HitStopHeavySec = FMath::Clamp(T.HitStopHeavySec, 0.f, 0.2f);
		T.HitStopBossPhaseSec = FMath::Clamp(T.HitStopBossPhaseSec, 0.f, 0.2f);
		T.HitStopDilation = FMath::Clamp(T.HitStopDilation, 0.02f, 1.f);
		T.DroneSpiralMin = FMath::Clamp(T.DroneSpiralMin, 0.f, 3.f);
		T.DroneSpiralMax = FMath::Clamp(T.DroneSpiralMax, T.DroneSpiralMin, 3.f);
		T.DroneSparkMin = FMath::Clamp(T.DroneSparkMin, 0.2f, 10.f);
		T.DroneSparkMax = FMath::Clamp(T.DroneSparkMax, T.DroneSparkMin, 10.f);
		UE_LOG(LogIronManArena, Log, TEXT("Combat realism: loaded %s (knock %.0f/%.0f/%.0f cm/s per dmg, half-life %.2f s)"),
			*Path, T.KnockDrone, T.KnockTank, T.KnockBoss, T.KnockHalfLife);
		return T;
	}
}

const CombatRealism::FTuning& CombatRealism::Get()
{
	static const FTuning Tuning = EnemyRealismLoad();
	return Tuning;
}

FEnemyHitClassScope::FEnemyHitClassScope(EEnemyHitClass InHitClass)
	: PrevHitClass(EnemyHitClassCurrent)
{
	EnemyHitClassCurrent = InHitClass;
}

FEnemyHitClassScope::~FEnemyHitClassScope()
{
	EnemyHitClassCurrent = PrevHitClass;
}

EEnemyHitClass FEnemyHitClassScope::GetCurrent()
{
	return EnemyHitClassCurrent;
}

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(60.f);
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	// Make sure the player's aim trace (Visibility) can see enemies.
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Collision);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	ArenaVisuals::PaintAll(this, Parts);
	VisualRoot->SetVisibility(bUsePlaceholderVisuals, true);
}

float AEnemyBase::GetHitRadius() const
{
	return Collision ? Collision->GetScaledSphereRadius() : 60.f;
}

AArenaGameMode* AEnemyBase::GetArenaGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
}

void AEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) { return; }
	Age += DeltaSeconds;

	if (FlashLeft > 0.f)
	{
		FlashLeft -= DeltaSeconds;
		if (FlashLeft <= 0.f) { ArenaVisuals::FlashAll(Parts, FLinearColor::White, true); }
	}

	StaggerLeft = FMath::Max(0.f, StaggerLeft - DeltaSeconds);

	// Cloaked (stealth suits), in a cutscene, or dead: enemies lose track and idle (every TickEnemy handles nullptr).
	AIronManCharacter* Player = Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	TickEnemy(DeltaSeconds, (Player && !Player->IsHiddenFromEnemies()) ? Player : nullptr);

	// Combat realism: knockback rides on top of the behaviour's own move, swept so it never pushes
	// through geometry, and decays exponentially (half-life from combat_realism.json).
	if (!bDead && IsValid(this))
	{
		const float HalfLife = CombatRealism::Get().KnockHalfLife;
		const float Decay = FMath::Exp2(-FMath::Min(DeltaSeconds, 0.25f) / HalfLife);
		if (!KnockVelocity.IsNearlyZero(2.f))
		{
			MoveBy(KnockVelocity * FMath::Min(DeltaSeconds, 0.25f));
			KnockVelocity *= Decay;
		}
		else
		{
			KnockVelocity = FVector::ZeroVector;
		}
		KickPitch *= Decay;
		KickRoll *= Decay;
	}
}

FVector AEnemyBase::ComputeHitDirection(FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const
{
	const FVector Here = GetActorLocation();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FVector ShotDir = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
		if (!ShotDir.IsNearlyZero()) { return ShotDir.GetSafeNormal(); }
	}
	if (DamageCauser)
	{
		const FVector Away = Here - DamageCauser->GetActorLocation();
		// A projectile that hit us directly pushes along its flight; splash pushes away from the blast.
		if (DamageCauser->IsA<AIronProjectile>() && Away.Size() < GetHitRadius() + 60.f)
		{
			const FVector Flight = DamageCauser->GetVelocity();
			if (Flight.SizeSquared() > 100.f) { return Flight.GetSafeNormal(); }
		}
		if (Away.SizeSquared() > 1.f) { return Away.GetSafeNormal(); }
	}
	if (EventInstigator && EventInstigator->GetPawn())
	{
		const FVector Away = Here - EventInstigator->GetPawn()->GetActorLocation();
		if (Away.SizeSquared() > 1.f) { return Away.GetSafeNormal(); }
	}
	return FVector::ZeroVector;
}

void AEnemyBase::ApplyHitReaction(float Applied, const FVector& HitDir, bool bStaggerHit)
{
	const CombatRealism::FTuning& Tune = CombatRealism::Get();
	float KindScale = Tune.KnockDrone;
	float MaxKnock = 1800.f;
	float StaggerSec = Tune.StaggerDrone;
	if (Kind == EEnemyKind::Tank) { KindScale = Tune.KnockTank; MaxKnock = 400.f; StaggerSec = Tune.StaggerTank; }
	else if (Kind == EEnemyKind::Boss) { KindScale = Tune.KnockBoss; MaxKnock = 160.f; StaggerSec = Tune.StaggerBoss; }

	if (!HitDir.IsNearlyZero())
	{
		LastHitDir = HitDir;
		FVector Push = HitDir * (Applied * KindScale);
		if (Kind == EEnemyKind::Tank) { Push.Z = 0.f; } // treads stay on the floor (the floor snap would fight it)
		KnockVelocity = (KnockVelocity + Push).GetClampedToMaxSize(MaxKnock);

		if (Kind == EEnemyKind::Drone && Tune.DroneKickMaxDeg > 0.f)
		{
			// Angular kick around the axis perpendicular to the hit: a hit from the front pitches the
			// nose up, a hit from the left rolls it right. ~12 deg per damage point, capped.
			const float Deg = FMath::Min(Tune.DroneKickMaxDeg, Applied * 12.f);
			const float Fwd = float(FVector::DotProduct(HitDir, GetActorForwardVector()));
			const float Side = float(FVector::DotProduct(HitDir, GetActorRightVector()));
			KickPitch = FMath::Clamp(KickPitch - Fwd * Deg, -Tune.DroneKickMaxDeg, Tune.DroneKickMaxDeg);
			KickRoll = FMath::Clamp(KickRoll + Side * Deg, -Tune.DroneKickMaxDeg, Tune.DroneKickMaxDeg);
		}
	}
	if (bStaggerHit && StaggerSec > 0.f) { StaggerLeft = FMath::Max(StaggerLeft, StaggerSec); }
}

void AEnemyBase::SpawnDeathFX()
{
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), DeathBlastRadius, FLinearColor(1.f, 0.55f, 0.15f), 0.5f);
}

float AEnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || DamageAmount <= 0.f) { return 0.f; }
	const float Applied = ModifyIncomingDamage(DamageAmount);
	if (Applied <= 0.f) { return 0.f; }

	Super::TakeDamage(Applied, DamageEvent, EventInstigator, DamageCauser);
	Health -= Applied;

	FlashLeft = HitFlashTime;
	ArenaVisuals::FlashAll(Parts, FLinearColor::White, false);

	// Combat realism: knockback / kick / stagger. Heavy = the weapon said so (punch, charged shot),
	// Beam = unibeam / whip (stagger only), or a single hit of 4+ damage (a Hulkbuster punch's size).
	const EEnemyHitClass HitClass = FEnemyHitClassScope::GetCurrent();
	const bool bHeavyHit = HitClass == EEnemyHitClass::Heavy || (HitClass == EEnemyHitClass::Normal && Applied >= 4.f);
	ApplyHitReaction(Applied, ComputeHitDirection(DamageEvent, EventInstigator, DamageCauser), bHeavyHit || HitClass == EEnemyHitClass::Beam);

	AArenaGameMode* GM = GetArenaGameMode();
	if (Health <= 0.f)
	{
		Health = 0.f;
		bDead = true;
		SetActorEnableCollision(false);
		OnKilled();
		const bool bBoss = IsA<ABossTitan>();
		if (bBoss)
		{
			// Kill confirm for the boss: the long freeze (unchanged).
			if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this)) { Feel->HitStop(0.22f, 0.08f); }
		}
		else if (GM)
		{
			GM->RequestHitStop(bHeavyHit ? EArenaHitStop::HeavyHit : EArenaHitStop::RepulsorKill);
		}
		// Bookkeeping stays immediate (kill count, combo, score, wave clear, smoke/test counters);
		// the drone's death spiral is a separate cosmetic actor.
		if (GM) { GM->OnEnemyKilled(this); }
		SpawnDeathFX();
		Destroy();
	}
	else if (bHeavyHit && GM)
	{
		GM->RequestHitStop(EArenaHitStop::HeavyHit);
	}
	return Applied;
}

void AEnemyBase::OnKilled()
{
	// Subclasses spawn their drops.
}

void AEnemyBase::DropPickup(EPickupType Type, const FVector& Offset)
{
	APickup::SpawnPickup(GetWorld(), Type, GetActorLocation() + Offset);
}

AIronProjectile* AEnemyBase::FireShot(const FVector& Origin, const FVector& InVelocity, float Damage, float Radius,
	const FLinearColor& Color, float BlastRadius, float GravityScale, USceneComponent* HomingTarget, float HomingAccel, float LifeSeconds)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	AIronProjectile* P = AIronProjectile::SpawnPooled(World, Origin, InVelocity.Rotation(), this, this, true);
	if (P)
	{
		P->LifeSeconds = LifeSeconds;
		P->Configure(Damage, true, BlastRadius, Color, Radius);
		P->Launch(InVelocity, GravityScale, HomingTarget, HomingAccel);
	}
	return P;
}

bool AEnemyBase::MoveBy(const FVector& Delta)
{
	FHitResult Hit;
	SetActorLocation(GetActorLocation() + Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		// Slide along whatever we bumped into.
		const FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
		SetActorLocation(GetActorLocation() + Slide, true);
		return false;
	}
	return true;
}

void AEnemyBase::FaceToward(const FVector& Target, float DeltaSeconds, float DegreesPerSecond)
{
	const FVector To = Target - GetActorLocation();
	if (To.SizeSquared2D() < 1.f) { return; }
	const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Want, DeltaSeconds, DegreesPerSecond));
}

FVector AEnemyBase::IntegrateSpringSteer(FVector& InOutVelocity, const FVector& From, const FVector& Target,
	float Stiffness, float DampingRate, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f) { return FVector::ZeroVector; }
	// Clamp huge hitches (e.g. after a level stream) so one frame can't fling the enemy.
	const float Total = FMath::Min(DeltaSeconds, 0.25f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Total * 60.f), 1, 16);
	const float H = Total / Steps;
	FVector Pos = From;
	for (int32 i = 0; i < Steps; ++i)
	{
		InOutVelocity += ((Target - Pos) * Stiffness - InOutVelocity * DampingRate) * H;
		Pos += InOutVelocity * H;
	}
	return Pos - From;
}

bool AEnemyBase::FindFloorZ(const FVector& Location, float& OutZ, float StartAbove) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyFloor), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, StartAbove), Location - FVector(0, 0, 20000),
		FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}
