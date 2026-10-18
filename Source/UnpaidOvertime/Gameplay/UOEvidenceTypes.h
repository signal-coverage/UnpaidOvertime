// UNPAID: OVERTIME - evidence trail entries (raw material for the future investigation mechanic).

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOEvidenceTypes.generated.h"

/** What an employee did at a station */
UENUM(BlueprintType)
enum class EUOEvidenceAction : uint8
{
	PickedUpItem,
	DroppedItem,
	CompletedSubObjective
};

/** One line of the evidence log */
USTRUCT(BlueprintType)
struct FUOEvidenceEntry
{
	GENERATED_BODY()

	/** Shift time in seconds */
	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	float TimeSeconds = 0.f;

	/** Fake office clock at the moment of the action ("10:42") */
	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	FString ClockTime;

	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	int32 EmployeeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	FString EmployeeName;

	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	EUOEvidenceAction Action = EUOEvidenceAction::CompletedSubObjective;

	/** Item involved, if any */
	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	EUOItemType Item = EUOItemType::None;

	/** Sub-objective involved, INDEX_NONE for item actions */
	UPROPERTY(BlueprintReadOnly, Category="Evidence")
	int32 SubObjectiveId = INDEX_NONE;
};
