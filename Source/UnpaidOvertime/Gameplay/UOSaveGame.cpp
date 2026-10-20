// UNPAID: OVERTIME - persisted records.

#include "UOSaveGame.h"
#include "Kismet/GameplayStatics.h"

UUOSaveGame* UUOSaveGame::LoadOrCreate()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		if (UUOSaveGame* Loaded = Cast<UUOSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)))
		{
			return Loaded;
		}
	}
	return Cast<UUOSaveGame>(UGameplayStatics::CreateSaveGameObject(UUOSaveGame::StaticClass()));
}

bool UUOSaveGame::Save()
{
	return UGameplayStatics::SaveGameToSlot(this, SlotName, UserIndex);
}
