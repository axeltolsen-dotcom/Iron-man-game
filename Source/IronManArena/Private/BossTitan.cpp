#include "BossTitan.h"
#include "ArenaFeel.h"
#include "ArenaVisuals.h"
#include "StoryDirector.h"
#include "IronManCharacter.h"
#include "IronProjectile.h"
#include "ArenaGameMode.h"
#include "ArenaBlastFX.h"
#include "DroneEnemy.h"
#include "JarvisComponent.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/DamageType.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ABossTitan::ABossTitan()
{
	Kind = EEnemyKind::Boss;
	MaxHealth = BaseHealth;
	ScoreValue = 5000;
	DeathBlastRadius = 900.f;
	HitFlashTime = 0.06f;
	Collision->SetSphereRadius(280.f);

	VolleyBolts    = { 0, 2, 3, 4 };
	VolleyCooldown = { 0.f, 2.0f, 1.6f, 1.3f };

	using namespace ArenaVisuals;
	const FLinearColor Obsidian(0.06f, 0.05f, 0.09f);
	const FLinearColor Plate(0.2f, 0.17f, 0.28f);
	const FLinearColor Violet(0.65f, 0.3f, 1.f);

	AddPart(this, VisualRoot, TEXT("Torso"), EArenaShape::Cube, FVector(0.f, 0.f, 0.f), FVector(2.2f, 3.2f, 2.4f), FRotator::ZeroRotator, Obsidian, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("ChestPlate"), EArenaShape::Cube, FVector(95.f, 0.f, 40.f), FVector(0.4f, 2.6f, 1.2f), FRotator::ZeroRotator, Plate, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("Head"), EArenaShape::Cube, FVector(30.f, 0.f, 165.f), FVector(1.0f, 1.0f, 0.8f), FRotator::ZeroRotator, Plate, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("Visor"), EArenaShape::Cube, FVector(82.f, 0.f, 170.f), FVector(0.05f, 0.8f, 0.15f), FRotator::ZeroRotator, Violet, 10.f, Parts);
	AddPart(this, VisualRoot, TEXT("ShoulderL"), EArenaShape::Sphere, FVector(0.f, -190.f, 90.f), FVector(1.3f), FRotator::ZeroRotator, Plate, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("ShoulderR"), EArenaShape::Sphere, FVector(0.f, 190.f, 90.f), FVector(1.3f), FRotator::ZeroRotator, Plate, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("PodL"), EArenaShape::Cube, FVector(-20.f, -190.f, 170.f), FVector(0.8f, 0.6f, 0.5f), FRotator::ZeroRotator, Obsidian, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("PodR"), EArenaShape::Cube, FVector(-20.f, 190.f, 170.f), FVector(0.8f, 0.6f, 0.5f), FRotator::ZeroRotator, Obsidian, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("ArmL"), EArenaShape::Cube, FVector(40.f, -230.f, -40.f), FVector(0.9f, 0.7f, 2.0f), FRotator::ZeroRotator, Obsidian, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("ArmR"), EArenaShape::Cube, FVector(40.f, 230.f, -40.f), FVector(0.9f, 0.7f, 2.0f), FRotator::ZeroRotator, Obsidian, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("BarrelL"), EArenaShape::Cylinder, FVector(150.f, -230.f, -110.f), FVector(0.45f, 0.45f, 2.0f), FRotator(-90.f, 0.f, 0.f), Plate, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("BarrelR"), EArenaShape::Cylinder, FVector(150.f, 230.f, -110.f), FVector(0.45f, 0.45f, 2.0f), FRotator(-90.f, 0.f, 0.f), Plate, 0.f, Parts);
	AddPart(this, VisualRoot, TEXT("EngineL"), EArenaShape::Cylinder, FVector(-110.f, -90.f, -130.f), FVector(0.6f, 0.6f, 0.4f), FRotator::ZeroRotator, FLinearColor(1.f, 0.4f, 0.9f), 6.f, Parts);
	AddPart(this, VisualRoot, TEXT("EngineR"), EArenaShape::Cylinder, FVector(-110.f, 90.f, -130.f), FVector(0.6f, 0.6f, 0.4f), FRotator::ZeroRotator, FLinearColor(1.f, 0.4f, 0.9f), 6.f, Parts);
	Core = AddPart(this, VisualRoot, TEXT("Core"), EArenaShape::Sphere, FVector(115.f, 0.f, 40.f), FVector(0.7f), FRotator::ZeroRotator, Violet, 10.f, Parts);

	CannonL = CreateDefaultSubobject<USceneComponent>(TEXT("CannonL"));
	CannonL->SetupAttachment(VisualRoot);
	CannonL->SetRelativeLocation(FVector(260.f, -230.f, -110.f));
	CannonR = CreateDefaultSubobject<USceneComponent>(TEXT("CannonR"));
	CannonR->SetupAttachment(VisualRoot);
	CannonR->SetRelativeLocation(FVector(260.f, 230.f, -110.f));

	// Invulnerability shield bubble (not part of Parts so hit-flash does not touch it).
	Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
	Shield->SetupAttachment(VisualRoot);
	Shield->SetStaticMesh(GetShapeMesh(EArenaShape::Sphere));
	Shield->SetRelativeScale3D(FVector(6.4f));
	Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shield->CastShadow = false;

	CoreLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreLight"));
	CoreLight->SetupAttachment(Core);
	CoreLight->SetLightColor(Violet);
	CoreLight->SetIntensity(20000.f);
	CoreLight->SetAttenuationRadius(1200.f);
	CoreLight->SetCastShadows(false);
}

void ABossTitan::BeginPlay()
{
	// MaxHealth may have been raised by InitForWave before BeginPlay (deferred spawn).
	Super::BeginPlay();
	ShieldMID = ArenaVisuals::PaintComponent(this, Shield, FLinearColor(0.5f, 0.3f, 1.f), 1.5f);

	// Entering: spawn high in the sky, descend to hover height.
	State = EBossState::Entering;
	InvulnerableLeft = 99.f;
	EnterElapsed = 0.f;
	EnterStuckTime = 0.f;
	EnterPrevLoc = GetActorLocation();
	EnterTarget = GetActorLocation() - FVector(0.f, 0.f, 1500.f);
	float Z = 0.f;
	if (FindFloorZ(GetActorLocation(), Z)) { EnterTarget.Z = Z + HoverHeight; }
	OrbitDir = FMath::RandBool() ? 1.f : -1.f;
}

void ABossTitan::InitForWave(int32 InWave)
{
	Super::InitForWave(InWave);
	const int32 Appearance = FMath::Max(1, InWave / 4);
	MaxHealth = FMath::RoundToFloat(BaseHealth * (1.f + HealthPerAppearance * (Appearance - 1)));
	Health = MaxHealth;
}

float ABossTitan::ModifyIncomingDamage(float Amount)
{
	if (InvulnerableLeft > 0.f)
	{
		return 0.f; // shield sparks only
	}
	if (State == EBossState::Venting) { Amount *= VentDamageMultiplier; }
	// Check for a phase change caused by this hit.
	const float After = (Health - Amount) / FMath::Max(1.f, MaxHealth);
	if (Health - Amount > 0.f)
	{
		const int32 NewPhase = After <= Phase3Threshold ? 3 : (After <= Phase2Threshold ? 2 : 1);
		if (NewPhase > Phase)
		{
			EnterPhase(NewPhase);
		}
	}
	return Amount;
}

void ABossTitan::EnterPhase(int32 NewPhase)
{
	Phase = NewPhase;
	InvulnerableLeft = PhaseChangeInvulnerability;
	if (State == EBossState::Venting) { State = EBossState::Fighting; VentLeft = 0.f; SetCoreColor(FLinearColor(0.65f, 0.3f, 1.f)); }
	if (UArenaFeelSubsystem* Feel = UArenaFeelSubsystem::Get(this))
	{
		Feel->HitStop(0.12f, 0.1f);
		Feel->PlayShake(TEXT("hit_heavy"));
	}
	AArenaBlastFX::Spawn(GetWorld(), GetActorLocation(), 600.f, FLinearColor(0.85f, 0.6f, 1.f), 0.6f);
	DropPickup(EPickupType::Health, FVector(0, 0, -150));
	UJarvisComponent::Notify(this, NewPhase >= 3 ? EAIEvent::BossPhase3 : EAIEvent::BossPhase2);
	// Story comms: the guy in the chair cracks the armour open on phase 2.
	if (NewPhase == 2) { UJarvisComponent::Notify(this, EAIEvent::Hack); }

	if (AArenaGameMode* GM = GetArenaGameMode())
	{
		if (AStoryDirector* Director = GM->GetStoryDirector()) { Director->NotifyBossPhase(NewPhase); }
		const FText Sub = (NewPhase == 2)
			? NSLOCTEXT("IronManArena", "BossP2", "ARMOR BREACHED - MISSILE PODS ONLINE")
			: NSLOCTEXT("IronManArena", "BossP3", "CORE CRITICAL - RAMMING SPEED");
		GM->ShowBanner(FText::Format(NSLOCTEXT("IronManArena", "BossPhase", "{0} - PHASE {1}"), BossName, FText::AsNumber(NewPhase)), Sub, 2.2f);

		if (NewPhase == 3)
		{
			for (int32 i = 0; i < EscortDrones; ++i)
			{
				const FVector Off(Rand(-300.f, 300.f), Rand(-300.f, 300.f), 150.f);
				GM->SpawnEnemyAt(GM->DroneClass, GetActorLocation() + Off);
			}
		}
	}
}

void ABossTitan::TickEnemy(float DeltaSeconds, AIronManCharacter* Player)
{
	InvulnerableLeft = FMath::Max(0.f, InvulnerableLeft - DeltaSeconds);
	ContactTimer = FMath::Max(0.f, ContactTimer - DeltaSeconds);

	// Shield bubble visible while invulnerable (pulsing).
	Shield->SetVisibility(InvulnerableLeft > 0.f && bUsePlaceholderVisuals);
	if (ShieldMID) { ShieldMID->SetScalarParameterValue(TEXT("EmissiveStrength"), ArenaVisuals::EmissiveToNits(1.f + 0.6f * FMath::Sin(Age * 30.f))); }

	const FVector Loc = GetActorLocation();
	const bool bRage = Phase >= 3;

	if (State == EBossState::Entering)
	{
		const FVector To = EnterTarget - Loc;
		MoveBy(To.GetClampedToMaxSize(700.f * DeltaSeconds));
		if (Player) { FaceToward(Player->GetActorLocation(), DeltaSeconds, 60.f); }
		// B05: timeout / no-progress so a blocked descent can't leave the boss shielded forever.
		EnterElapsed += DeltaSeconds;
		const float Progress = FVector::Dist(Loc, EnterPrevLoc);
		EnterStuckTime = (Progress < 1.f) ? (EnterStuckTime + DeltaSeconds) : 0.f;
		EnterPrevLoc = Loc;
		const bool bArrived = To.Size() < 30.f || EnterElapsed >= 4.f || EnterStuckTime >= 0.5f;
		if (bArrived)
		{
			State = EBossState::Fighting;
			InvulnerableLeft = 0.f;
			if (Player)
			{
				const FVector From = Loc - Player->GetActorLocation();
				OrbitAngle = FMath::RadiansToDegrees(FMath::Atan2(From.Y, From.X));
			}
		}
		return;
	}

	if (!Player || Player->IsDead())
	{
		Velocity *= 0.95f;
		MoveBy(Velocity * DeltaSeconds);
		return;
	}

	const FVector PlayerLoc = Player->GetActorLocation();

	switch (State)
	{
	case EBossState::Charging:
	{
		// Telegraph: slow down, core flashes white, then dash at the player.
		StateTimer += DeltaSeconds;
		Velocity *= FMath::Max(0.f, 1.f - 6.f * DeltaSeconds);
		MoveBy(Velocity * DeltaSeconds);
		FaceToward(PlayerLoc, DeltaSeconds, 180.f);
		if (Parts.Num() > 0 && Parts.Last().MID)
		{
			Parts.Last().MID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(0.65f, 0.3f, 1.f), FLinearColor::White, FMath::Abs(FMath::Sin(Age * 25.f))));
		}
		if (StateTimer <= DeltaSeconds)
		{
			// Telegraph where it will ram: a ring that tracks the player and fills as the charge completes.
			AArenaTelegraph::Spawn(GetWorld(), PlayerLoc, TelegraphRadius * 1.2f, DashChargeTime, FLinearColor(1.f, 0.12f, 0.05f), Player);
		}
		if (StateTimer >= DashChargeTime)
		{
			State = EBossState::Dashing;
			StateTimer = 0.f;
			Velocity = (PlayerLoc - Loc).GetSafeNormal() * DashSpeed;
			ArenaVisuals::FlashAll(Parts, FLinearColor::White, true);
		}
		break;
	}
	case EBossState::Dashing:
	{
		StateTimer += DeltaSeconds;
		MoveBy(Velocity * DeltaSeconds);
		if (StateTimer >= DashDuration)
		{
			Velocity *= 0.2f;
			OpenVent(VentAfterDashSeconds);
		}
		break;
	}
	case EBossState::Venting:
	{
		// Exposed core: drift, hold fire, glow hot. Shoot it.
		VentLeft -= DeltaSeconds;
		Velocity *= FMath::Max(0.f, 1.f - 2.5f * DeltaSeconds);
		Velocity.Z += FMath::Sin(Age * 3.f) * 20.f * DeltaSeconds;
		MoveBy(Velocity * DeltaSeconds);
		FaceToward(PlayerLoc, DeltaSeconds, 30.f);
		SetCoreColor(FMath::Lerp(FLinearColor(1.f, 0.45f, 0.1f), FLinearColor(1.f, 0.9f, 0.6f), 0.5f + 0.5f * FMath::Sin(Age * 14.f)));
		if (VentLeft <= 0.f)
		{
			State = EBossState::Fighting;
			SetCoreColor(FLinearColor(0.65f, 0.3f, 1.f));
		}
		break;
	}
	default: // Fighting
	{
		// Switch orbit side every 5-8 s.
		SideTimer -= DeltaSeconds;
		if (SideTimer <= 0.f) { SideTimer = Rand(5.f, 8.f); OrbitDir *= -1.f; }
		OrbitAngle += OrbitDir * 14.f * DeltaSeconds;

		const float Dist = bRage ? HoverDistanceRage : HoverDistance;
		const float Rad = FMath::DegreesToRadians(OrbitAngle);
		float FloorZ = PlayerLoc.Z;
		FindFloorZ(PlayerLoc, FloorZ);
		const FVector Target(PlayerLoc.X + FMath::Cos(Rad) * Dist, PlayerLoc.Y + FMath::Sin(Rad) * Dist,
			FloorZ + HoverHeight + (bRage ? 150.f : 0.f) + FMath::Sin(Age * 1.3f) * 120.f);

		Velocity += ((Target - Loc) * 1.3f - Velocity * 1.5f) * DeltaSeconds;
		MoveBy(Velocity * DeltaSeconds);
		FaceToward(PlayerLoc, DeltaSeconds, 90.f);

		// Arm-cannon volleys.
		VolleyTimer -= DeltaSeconds;
		if (VolleyTimer <= 0.f)
		{
			VolleyTimer = VolleyCooldown.IsValidIndex(Phase) ? VolleyCooldown[Phase] : 1.5f;
			FireVolley(Player);
		}

		// Missile pods from phase 2.
		if (Phase >= 2)
		{
			MissileTimer -= DeltaSeconds;
			if (MissileTimer <= 0.f)
			{
				MissileTimer = bRage ? MissileCooldownPhase3 : MissileCooldownPhase2;
				FireMissiles(Player);
				OpenVent(VentAfterMissilesSeconds);
			}
		}

		// Ramming dash in phase 3.
		if (bRage && State == EBossState::Fighting)
		{
			DashTimer -= DeltaSeconds;
			if (DashTimer <= 0.f)
			{
				DashTimer = Rand(DashIntervalRange.X, DashIntervalRange.Y);
				State = EBossState::Charging;
				StateTimer = 0.f;
			}
		}
		break;
	}
	}

	// Ramming / contact damage with its own cooldown.
	if (ContactTimer <= 0.f && FVector::Dist(PlayerLoc, GetActorLocation()) < ContactRange)
	{
		ContactTimer = ContactCooldown;
		UGameplayStatics::ApplyDamage(Player, ContactDamage, nullptr, this, UDamageType::StaticClass());
	}
}

void ABossTitan::FireVolley(AIronManCharacter* Player)
{
	const int32 N = VolleyBolts.IsValidIndex(Phase) ? VolleyBolts[Phase] : 2;
	const FVector Aim = Player->GetActorLocation();
	for (USceneComponent* Cannon : { CannonL.Get(), CannonR.Get() })
	{
		if (!Cannon) { continue; }
		const FVector O = Cannon->GetComponentLocation();
		const FRotator Base = (Aim - O).Rotation();
		for (int32 i = 0; i < N; ++i)
		{
			FRotator R = Base;
			R.Yaw += (i - (N - 1) * 0.5f) * VolleySpreadDegrees;
			FireShot(O, R.Vector() * BoltSpeed, BoltDamage, 20.f, FLinearColor(0.8f, 0.4f, 1.f));
		}
	}
	AArenaBlastFX::Spawn(GetWorld(), CannonL->GetComponentLocation(), 60.f, FLinearColor(0.8f, 0.4f, 1.f), 0.12f);
	AArenaBlastFX::Spawn(GetWorld(), CannonR->GetComponentLocation(), 60.f, FLinearColor(0.8f, 0.4f, 1.f), 0.12f);
}

void ABossTitan::FireMissiles(AIronManCharacter* Player)
{
	const int32 PerSide = Phase >= 3 ? 2 : 1;
	USceneComponent* HomingTarget = Player->GetRootComponent();
	// Incoming-missile warning ring that follows the player until the salvo arrives (~flight time).
	AArenaTelegraph::Spawn(GetWorld(), Player->GetActorLocation(), TelegraphRadius, 1.6f, FLinearColor(1.f, 0.3f, 0.05f), Player);
	for (const float Side : { -1.f, 1.f })
	{
		for (int32 k = 0; k < PerSide; ++k)
		{
			const FVector O = GetActorLocation() + GetActorRightVector() * Side * 190.f + FVector(0.f, 0.f, 230.f);
			// Pop upward and outward first, then the homing takes over.
			const FVector V = GetActorRightVector() * Side * Rand(200.f, 400.f) + FVector(0.f, 0.f, Rand(550.f, 750.f));
			if (AIronProjectile* M = FireShot(O, V, MissileDamage, 26.f, FLinearColor(1.f, 0.35f, 0.15f), 220.f, 0.f, HomingTarget, MissileHoming, 4.5f))
			{
				M->bDestroyableByPlayer = true;
			}
		}
	}
}

void ABossTitan::OpenVent(float Seconds)
{
	if (Seconds <= 0.f) { State = EBossState::Fighting; return; }
	State = EBossState::Venting;
	VentLeft = Seconds;
	StateTimer = 0.f;
	++VentWindowsOpened;
	// Steam puffs off the shoulders.
	for (const float Side : { -1.f, 1.f })
	{
		AArenaBlastFX::Spawn(GetWorld(), GetActorLocation() + GetActorRightVector() * Side * 170.f + FVector(0, 0, 200.f), 90.f, FLinearColor(0.9f, 0.9f, 0.95f), 0.5f);
	}
	if (!bVentHintShown)
	{
		bVentHintShown = true;
		if (AArenaGameMode* GM = GetArenaGameMode())
		{
			GM->ShowBanner(NSLOCTEXT("IronManArena", "BossVent", "CORE EXPOSED"),
				NSLOCTEXT("IronManArena", "BossVentSub", "It vents after every dash and missile salvo - double damage"), 1.6f);
		}
	}
}

void ABossTitan::SetCoreColor(const FLinearColor& C)
{
	if (Parts.Num() > 0 && Parts.Last().MID) { Parts.Last().MID->SetVectorParameterValue(TEXT("Color"), C); }
}

void ABossTitan::OnKilled()
{
	DropPickup(EPickupType::Hulkbuster, FVector(0, 0, 0));
	DropPickup(EPickupType::Health, FVector(150, 0, 0));
	for (int32 i = 0; i < 5; ++i)
	{
		DropPickup(EPickupType::Gem, FVector(Rand(-300.f, 300.f), Rand(-300.f, 300.f), Rand(-100.f, 100.f)));
	}
	// Chain of secondary explosions for drama.
	for (int32 i = 0; i < 5; ++i)
	{
		AArenaBlastFX::Spawn(GetWorld(), GetActorLocation() + FVector(Rand(-280.f, 280.f), Rand(-280.f, 280.f), Rand(-200.f, 200.f)),
			Rand(250.f, 400.f), FLinearColor(1.f, 0.7f, 0.3f), Rand(0.4f, 0.9f));
	}
}
