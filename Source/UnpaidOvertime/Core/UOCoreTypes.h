// UNPAID: OVERTIME - shared enums used by pure game logic and gameplay code.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.generated.h"

/** How long the shift lasts before overtime starts */
UENUM(BlueprintType)
enum class EUODayLength : uint8
{
	Short,
	Normal,
	Long
};

/** Base difficulty of AI employees (scaled further by day number) */
UENUM(BlueprintType)
enum class EUOAIDifficulty : uint8
{
	Easy,
	Medium,
	Hard
};

/** Phases of a single working day */
UENUM(BlueprintType)
enum class EUODayPhase : uint8
{
	PreGame,
	Shift,
	Overtime,
	Evaluation,
	/** Emergency meeting called mid-shift (a body was found): the clock stops until it ends */
	Meeting
};

/** Cooperative task archetypes */
UENUM(BlueprintType)
enum class EUOTaskType : uint8
{
	PrinterMaintenance,
	CoffeeRefill,
	FileReports
};

/** Items an employee can carry (one at a time) */
UENUM(BlueprintType)
enum class EUOItemType : uint8
{
	None,
	InkCartridge,
	Report
};

/** Interactable station archetypes placed by the office builder */
UENUM(BlueprintType)
enum class EUOStationType : uint8
{
	Printer,
	CoffeeMaker,
	FilingCabinet,
	InboxTray,
	SupplyCloset
};

/** Lifecycle of a sub-objective */
UENUM(BlueprintType)
enum class EUOSubObjectiveState : uint8
{
	Open,
	Claimed,
	Done
};

/** What happens to an employee depending on the stars they received */
UENUM(BlueprintType)
enum class EUOStarConsequence : uint8
{
	Fired,
	CutVoteCandidate,
	Safe,
	PromotionAvailable
};

/** Localizable display names for the shared enums */
namespace UOText
{
	FText GetItemName(EUOItemType Item);
	FText GetStationTypeName(EUOStationType StationType);
	FText GetTaskTypeName(EUOTaskType TaskType);
	FText GetDayLengthName(EUODayLength DayLength);
	FText GetDifficultyName(EUOAIDifficulty Difficulty);
	FText GetPhaseName(EUODayPhase Phase);
}
