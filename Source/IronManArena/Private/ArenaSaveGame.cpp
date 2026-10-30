#include "ArenaSaveGame.h"
#include "Kismet/GameplayStatics.h"

UArenaSaveGame* UArenaSaveGame::LoadOrCreate(const FString& SlotName)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (UArenaSaveGame* S = Cast<UArenaSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			return S;
		}
	}
	return Cast<UArenaSaveGame>(UGameplayStatics::CreateSaveGameObject(UArenaSaveGame::StaticClass()));
}

bool UArenaSaveGame::Write(const FString& SlotName)
{
	return UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}
