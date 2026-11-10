#include "BossVFXComponent.h"

#include "IronManArena.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

namespace
{
	// Resolve a soft Niagara reference only if its package exists, so missing content never logs load errors.
	UNiagaraSystem* LoadBossFXIfPresent(const TSoftObjectPtr<UNiagaraSystem>& Soft)
	{
		if (Soft.IsNull()) { return nullptr; }
		if (UNiagaraSystem* Already = Soft.Get()) { return Already; }
		const FString PackageName = Soft.ToSoftObjectPath().GetLongPackageName();
		if (PackageName.IsEmpty() || !FPackageName::DoesPackageExist(PackageName)) { return nullptr; }
		return Soft.LoadSynchronous();
	}

	TSoftObjectPtr<UNiagaraSystem> BossFXPath(const TCHAR* Path)
	{
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Path));
	}
}

UBossVFXComponent::UBossVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Content layout from contrib/ultron/boss_vfx.md section 3.
	NS_PhaseBlast = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_PhaseBlast.NS_Boss_PhaseBlast"));
	NS_VolleyMuzzle = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_VolleyMuzzle.NS_Boss_VolleyMuzzle"));
	NS_VentSteam = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_VentSteam.NS_Boss_VentSteam"));
	NS_CoreVent = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_CoreVent.NS_Boss_CoreVent"));
	NS_ChargeTelegraph = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_ChargeTelegraph.NS_Boss_ChargeTelegraph"));
	NS_RamHeat = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_RamHeat.NS_Boss_RamHeat"));
	NS_ShieldHit = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_ShieldHit.NS_Boss_ShieldHit"));
	NS_PlateShear = BossFXPath(TEXT("/Game/FX/Boss/NS_PlateShear.NS_PlateShear"));
	NS_HoverJet = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_HoverJet.NS_Boss_HoverJet"));
	NS_ExplosionLarge = BossFXPath(TEXT("/Game/FX/World/NS_Explosion_Large.NS_Explosion_Large"));
	NS_ExplosionSmall = BossFXPath(TEXT("/Game/FX/World/NS_Explosion_Small.NS_Explosion_Small"));
	NS_ArenaShock = BossFXPath(TEXT("/Game/FX/Boss/NS_Boss_ArenaShock.NS_Boss_ArenaShock"));
}

void UBossVFXComponent::BeginPlay()
{
	Super::BeginPlay();
	SoftLoadSystems();
	if (NumLoaded == 0)
	{
		UE_LOG(LogIronManArena, Verbose, TEXT("Boss VFX: no /Game/FX/Boss Niagara systems found - using the BlastFX fallback"));
		return;
	}
	UE_LOG(LogIronManArena, Log, TEXT("Boss VFX: %d Niagara systems loaded"), NumLoaded);
	if (LoadedHoverJet)
	{
		HoverJetL = SpawnAttachedFX(LoadedHoverJet, FindHostPart(TEXT("EngineL")));
		HoverJetR = SpawnAttachedFX(LoadedHoverJet, FindHostPart(TEXT("EngineR")));
	}
	if (LoadedCoreVent)
	{
		CoreVentFX = SpawnAttachedFX(LoadedCoreVent, FindHostPart(TEXT("Core")));
		if (CoreVentFX) { CoreVentFX->SetFloatParameter(TEXT("Vent01"), 0.f); }
	}
}

void UBossVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(VentEndTimer); }
	ClearTransient();
	Super::EndPlay(EndPlayReason);
}

void UBossVFXComponent::SoftLoadSystems()
{
	NumLoaded = 0;
	auto LoadOne = [this](const TSoftObjectPtr<UNiagaraSystem>& Soft, TObjectPtr<UNiagaraSystem>& Out)
	{
		Out = LoadBossFXIfPresent(Soft);
		if (Out) { ++NumLoaded; }
	};
	LoadOne(NS_PhaseBlast, LoadedPhaseBlast);
	LoadOne(NS_VolleyMuzzle, LoadedVolleyMuzzle);
	LoadOne(NS_VentSteam, LoadedVentSteam);
	LoadOne(NS_CoreVent, LoadedCoreVent);
	LoadOne(NS_ChargeTelegraph, LoadedCharge);
	LoadOne(NS_RamHeat, LoadedRamHeat);
	LoadOne(NS_ShieldHit, LoadedShieldHit);
	LoadOne(NS_PlateShear, LoadedPlateShear);
	LoadOne(NS_HoverJet, LoadedHoverJet);
	LoadOne(NS_ExplosionLarge, LoadedExplosionLarge);
	LoadOne(NS_ExplosionSmall, LoadedExplosionSmall);
	LoadOne(NS_ArenaShock, LoadedArenaShock);
}

USceneComponent* UBossVFXComponent::FindHostPart(FName PartName) const
{
	const AActor* HostActor = GetOwner();
	if (!HostActor) { return nullptr; }
	TInlineComponentArray<USceneComponent*> SceneComps(HostActor);
	for (USceneComponent* AsScene : SceneComps)
	{
		if (AsScene && AsScene->GetFName() == PartName) { return AsScene; }
	}
	return nullptr;
}

UNiagaraComponent* UBossVFXComponent::SpawnAttachedFX(UNiagaraSystem* System, USceneComponent* AttachParent)
{
	if (!System || !AttachParent || !GetWorld()) { return nullptr; }
	return UNiagaraFunctionLibrary::SpawnSystemAttached(System, AttachParent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ true, /*bAutoActivate*/ true);
}

void UBossVFXComponent::ClearTransient()
{
	auto KillFX = [](TObjectPtr<UNiagaraComponent>& Comp)
	{
		if (Comp)
		{
			Comp->DeactivateImmediate();
			Comp->DestroyComponent();
			Comp = nullptr;
		}
	};
	KillFX(HoverJetL);
	KillFX(HoverJetR);
	KillFX(CoreVentFX);
	KillFX(RamHeatFX);
}

void UBossVFXComponent::PlayPhaseBreak(int32 NewPhase)
{
	CachedPhase = NewPhase;
	const AActor* HostActor = GetOwner();
	if (!HostActor || !GetWorld()) { return; }
	if (LoadedPhaseBlast) { UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), LoadedPhaseBlast, HostActor->GetActorLocation()); }
	if (LoadedPlateShear)
	{
		if (NewPhase >= 3) { SpawnAttachedFX(LoadedPlateShear, FindHostPart(TEXT("ChestPlate"))); }
		else
		{
			SpawnAttachedFX(LoadedPlateShear, FindHostPart(TEXT("ShoulderL")));
			SpawnAttachedFX(LoadedPlateShear, FindHostPart(TEXT("ShoulderR")));
		}
	}
}

void UBossVFXComponent::PlayVolleyMuzzle(USceneComponent* MuzzleAttach)
{
	if (LoadedVolleyMuzzle) { SpawnAttachedFX(LoadedVolleyMuzzle, MuzzleAttach); }
}

void UBossVFXComponent::PlayShieldHit(const FVector& HitLocation, const FVector& HitNormal)
{
	UWorld* World = GetWorld();
	if (!LoadedShieldHit || !World) { return; }
	const double Now = World->GetTimeSeconds();
	if (LastShieldHitTime >= 0.0 && Now - LastShieldHitTime < ShieldHitInterval) { return; }
	LastShieldHitTime = Now;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, LoadedShieldHit, HitLocation, HitNormal.Rotation());
}

void UBossVFXComponent::PlayVent(float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (CoreVentFX) { CoreVentFX->SetFloatParameter(TEXT("Vent01"), 1.f); }
	if (LoadedVentSteam)
	{
		for (const TCHAR* Shoulder : { TEXT("ShoulderL"), TEXT("ShoulderR") })
		{
			if (UNiagaraComponent* Steam = SpawnAttachedFX(LoadedVentSteam, FindHostPart(Shoulder)))
			{
				Steam->SetFloatParameter(TEXT("Duration"), DurationSeconds);
			}
		}
	}
	if (DurationSeconds > 0.f && CoreVentFX)
	{
		TWeakObjectPtr<UBossVFXComponent> WeakThis(this);
		World->GetTimerManager().SetTimer(VentEndTimer, [WeakThis]()
		{
			UBossVFXComponent* Self = WeakThis.Get();
			if (Self && Self->CoreVentFX) { Self->CoreVentFX->SetFloatParameter(TEXT("Vent01"), 0.f); }
		}, DurationSeconds, false);
	}
}

void UBossVFXComponent::SetCombatState(uint8 BossStateValue, int32 Phase)
{
	CachedPhase = Phase;
	const float Thrust = BossStateValue == 3 ? 1.0f : BossStateValue == 2 ? 0.7f : BossStateValue == 4 ? 0.15f : 0.35f;
	if (HoverJetL) { HoverJetL->SetFloatParameter(TEXT("Thrust"), Thrust); }
	if (HoverJetR) { HoverJetR->SetFloatParameter(TEXT("Thrust"), Thrust); }

	const AActor* HostActor = GetOwner();
	if (BossStateValue == 2 && LoadedCharge && HostActor && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), LoadedCharge, HostActor->GetActorLocation());
	}
	if (BossStateValue == 3)
	{
		if (!RamHeatFX && LoadedRamHeat && HostActor) { RamHeatFX = SpawnAttachedFX(LoadedRamHeat, HostActor->GetRootComponent()); }
	}
	else if (RamHeatFX)
	{
		RamHeatFX->Deactivate();
		RamHeatFX = nullptr; // auto-destroys when the system finishes
	}
	if (CoreVentFX) { CoreVentFX->SetFloatParameter(TEXT("Vent01"), BossStateValue == 4 ? 1.f : 0.f); }
}

void UBossVFXComponent::PlayDeathCascade(float SlowMoSeconds)
{
	UWorld* World = GetWorld();
	const AActor* HostActor = GetOwner();
	if (!World || !HostActor || (!LoadedExplosionLarge && !LoadedExplosionSmall && !LoadedArenaShock)) { return; }
	const float Delay = SlowMoSeconds >= 0.f ? SlowMoSeconds : DeathSlowMoSeconds;

	// Timeline from boss_vfx_params.json NS_Boss_DeathCascade.sequence. Locations are captured now because
	// the boss actor (and this component) is destroyed right after OnKilled.
	struct FBossDeathBeat { float Time; UNiagaraSystem* System; FName PartName; };
	const FBossDeathBeat Beats[] = {
		{ 0.00f, LoadedExplosionLarge, TEXT("Core") },
		{ 0.35f, LoadedExplosionSmall, TEXT("ShoulderL") },
		{ 0.70f, LoadedExplosionSmall, TEXT("ShoulderR") },
		{ 1.05f, LoadedExplosionSmall, TEXT("ChestPlate") },
		{ 1.20f, LoadedArenaShock, NAME_None },
	};
	for (const FBossDeathBeat& Beat : Beats)
	{
		if (!Beat.System) { continue; }
		FVector SpawnLoc = HostActor->GetActorLocation();
		if (!Beat.PartName.IsNone())
		{
			if (const USceneComponent* Part = FindHostPart(Beat.PartName)) { SpawnLoc = Part->GetComponentLocation(); }
		}
		else { SpawnLoc.Z -= 200.f; } // arena shock at the ground projection
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TWeakObjectPtr<UNiagaraSystem> WeakSystem(Beat.System);
		FTimerHandle BeatTimer;
		World->GetTimerManager().SetTimer(BeatTimer, [WeakWorld, WeakSystem, SpawnLoc]()
		{
			if (WeakWorld.IsValid() && WeakSystem.IsValid())
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(WeakWorld.Get(), WeakSystem.Get(), SpawnLoc);
			}
		}, FMath::Max(0.01f, Delay + Beat.Time), false);
	}
}
