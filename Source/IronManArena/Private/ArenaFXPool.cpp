#include "ArenaFXPool.h"
#include "IronProjectile.h"
#include "ArenaBlastFX.h"

#include "Engine/World.h"

UArenaFXPool* UArenaFXPool::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UArenaFXPool>() : nullptr;
}

void UArenaFXPool::Prune()
{
	auto Clean = [](auto& Arr) { Arr.RemoveAll([](const auto& W) { return !W.IsValid(); }); };
	Clean(ParkedShots);
	Clean(LivePlayer);
	Clean(LiveHostile);
	Clean(ParkedBlasts);
	Clean(LiveBlasts);
}

AIronProjectile* UArenaFXPool::AcquireProjectile(const FVector& Location, const FRotator& Rotation, AActor* InOwner, APawn* InInstigator, bool bInHostile)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	Prune();

	TArray<TWeakObjectPtr<AIronProjectile>>& Live = bInHostile ? LiveHostile : LivePlayer;
	const int32 Cap = bInHostile ? MaxHostileShots : MaxPlayerShots;
	if (Live.Num() >= Cap && Live[0].IsValid())
	{
		Live[0]->Recycle(); // oldest live shot fizzles quietly and goes back to the pool
		Prune();
	}

	AIronProjectile* Shot = nullptr;
	while (!Shot && ParkedShots.Num() > 0)
	{
		Shot = ParkedShots.Pop().Get();
	}
	if (Shot)
	{
		Shot->ReactivateFromPool(Location, Rotation, InOwner, InInstigator);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = InOwner;
		Params.Instigator = InInstigator;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Shot = World->SpawnActor<AIronProjectile>(AIronProjectile::StaticClass(), Location, Rotation, Params);
	}
	if (Shot) { Live.Add(Shot); }
	return Shot;
}

void UArenaFXPool::ReleaseProjectile(AIronProjectile* Shot)
{
	if (!Shot) { return; }
	LivePlayer.Remove(Shot);
	LiveHostile.Remove(Shot);
	if (ParkedShots.Num() < MaxParkedPerType) { ParkedShots.AddUnique(Shot); }
	else { Shot->Destroy(); }
}

AArenaBlastFX* UArenaFXPool::AcquireBlast(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	Prune();
	if (LiveBlasts.Num() >= MaxBlasts && LiveBlasts[0].IsValid())
	{
		LiveBlasts[0]->Park();
		Prune();
	}
	AArenaBlastFX* FX = nullptr;
	while (!FX && ParkedBlasts.Num() > 0)
	{
		FX = ParkedBlasts.Pop().Get();
	}
	if (FX)
	{
		FX->ReactivateFromPool(Location);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FX = World->SpawnActor<AArenaBlastFX>(AArenaBlastFX::StaticClass(), Location, FRotator::ZeroRotator, Params);
	}
	if (FX) { LiveBlasts.Add(FX); }
	return FX;
}

void UArenaFXPool::ReleaseBlast(AArenaBlastFX* Blast)
{
	if (!Blast) { return; }
	LiveBlasts.Remove(Blast);
	if (ParkedBlasts.Num() < MaxParkedPerType) { ParkedBlasts.AddUnique(Blast); }
	else { Blast->Destroy(); }
}

bool UArenaFXPool::TryClaimLight()
{
	if (LitFX >= MaxFXLights) { return false; }
	++LitFX;
	return true;
}

void UArenaFXPool::ReturnLight()
{
	LitFX = FMath::Max(0, LitFX - 1);
}

void UArenaFXPool::Forget(AActor* Actor)
{
	if (!Actor) { return; }
	if (AIronProjectile* Shot = Cast<AIronProjectile>(Actor))
	{
		ParkedShots.Remove(Shot);
		LivePlayer.Remove(Shot);
		LiveHostile.Remove(Shot);
	}
	else if (AArenaBlastFX* FX = Cast<AArenaBlastFX>(Actor))
	{
		ParkedBlasts.Remove(FX);
		LiveBlasts.Remove(FX);
	}
}
