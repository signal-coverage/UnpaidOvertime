// UNPAID: OVERTIME - persisted records: best days survived per base AI difficulty.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UOEvaluationTypes.h"
#include "UOSaveGame.generated.h"

/**
 *  Local records (Steam leaderboards come later). Rules for updating them live in FUORecordBook (Core).
 */
UCLASS()
class UUOSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	static constexpr const TCHAR* SlotName = TEXT("UORecords");
	static constexpr int32 UserIndex = 0;

	UPROPERTY()
	TArray<FUORecordEntry> Records;

	/** Loads the slot, or a fresh save game when there is none */
	static UUOSaveGame* LoadOrCreate();

	/** Writes the slot; false on failure */
	bool Save();
};
