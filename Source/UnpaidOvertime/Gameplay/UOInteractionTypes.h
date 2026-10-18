// UNPAID: OVERTIME - shared interaction types between stations, the task manager and the interaction component.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOInteractionTypes.generated.h"

/** What an interaction at a station would do */
UENUM(BlueprintType)
enum class EUOInteractionKind : uint8
{
	None,
	PickUpItem,
	DeliverItem,
	Work
};

/** Outcome of a finished hold/press */
UENUM(BlueprintType)
enum class EUOInteractionResult : uint8
{
	/** The server accepted the interaction */
	Succeeded,
	/** The work attempt failed (AI precision roll) and should be retried */
	FailedAttempt,
	/** The server rejected the interaction (nothing to do, someone else finished it, out of reach...) */
	Rejected
};

/** What the task manager offers an employee at a station right now */
USTRUCT(BlueprintType)
struct FUOInteractionOption
{
	GENERATED_BODY()

	/** True when the interaction can be performed */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	EUOInteractionKind Kind = EUOInteractionKind::None;

	/** Action verb shown after "Hold E" / "Press E" */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FText Prompt;

	/** Why the interaction is not available (shown greyed out) */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FText BlockedReason;

	/** Seconds of holding at work speed 1 (0 = instant press) */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	float HoldSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	int32 SubObjectiveId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	EUOItemType Item = EUOItemType::None;

	bool IsSameAction(const FUOInteractionOption& Other) const
	{
		return Kind == Other.Kind && SubObjectiveId == Other.SubObjectiveId && Item == Other.Item;
	}
};
