// UNPAID: OVERTIME - task model: tasks, sub-objectives and station placement requirements.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOTaskTypes.generated.h"

/** One unit of work. Credit goes to whoever performs the final interaction. */
USTRUCT(BlueprintType)
struct FUOSubObjective
{
	GENERATED_BODY()

	/** Unique within the day */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	int32 Id = INDEX_NONE;

	/** Task this sub-objective belongs to */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	int32 TaskId = INDEX_NONE;

	/** Station where the final interaction happens */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category="Task")
	bool bRequiresItem = false;

	/** Item that must be carried to the station (when bRequiresItem) */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	EUOItemType ItemType = EUOItemType::None;

	/** Seconds of work (hold interact) at the station */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	float WorkSeconds = 1.f;

	UPROPERTY(BlueprintReadOnly, Category="Task")
	EUOSubObjectiveState State = EUOSubObjectiveState::Open;

	/** Employee holding the claim (AI coordination), INDEX_NONE if unclaimed */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	int32 ClaimedBy = INDEX_NONE;

	/** Employee credited with the completion, INDEX_NONE if not done */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	int32 CompletedBy = INDEX_NONE;

	/** Shift time (seconds) of the completion, negative if not done */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	float CompletedAtTime = -1.f;

	bool IsDone() const { return State == EUOSubObjectiveState::Done; }
};

/** A cooperative task split into sub-objectives that any alive employee can take */
USTRUCT(BlueprintType)
struct FUOTaskInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Task")
	int32 Id = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Task")
	EUOTaskType Type = EUOTaskType::CoffeeRefill;

	/** e.g. "Coffee refill - North wing" */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	FText DisplayName;

	/** Office zone the task lives in (drives the zone suffix and station placement) */
	UPROPERTY(BlueprintReadOnly, Category="Task")
	int32 ZoneIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Task")
	TArray<FUOSubObjective> SubObjectives;

	int32 CountDone() const;
	bool IsComplete() const;
};

/** A station the office builder has to place */
USTRUCT(BlueprintType)
struct FUOStationSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Station")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category="Station")
	EUOStationType Type = EUOStationType::Printer;

	/** Preferred zone, INDEX_NONE for fixed placements (supply closet corner) */
	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 ZoneIndex = INDEX_NONE;

	/** 1-based number among stations of the same type, used for labels ("Printer 3") */
	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 Ordinal = 1;
};

/** Station placement requirements produced by the task generator */
USTRUCT(BlueprintType)
struct FUOStationRequirements
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 Printers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 CoffeeMakers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 FilingCabinets = 0;

	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 InboxTrays = 0;

	UPROPERTY(BlueprintReadOnly, Category="Station")
	int32 SupplyClosets = 0;

	/** Every station to place, in generation order */
	UPROPERTY(BlueprintReadOnly, Category="Station")
	TArray<FUOStationSpec> Stations;

	/** Stations that need a wall slot (everything except the supply closet corner) */
	int32 GetWallStationCount() const { return Printers + CoffeeMakers + FilingCabinets + InboxTrays; }

	int32 GetCountForType(EUOStationType Type) const;

	const FUOStationSpec* FindStation(FName StationId) const;
};

/** Output of the task generator for one day */
USTRUCT(BlueprintType)
struct FUOGeneratedDay
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Task")
	TArray<FUOTaskInstance> Tasks;

	UPROPERTY(BlueprintReadOnly, Category="Task")
	FUOStationRequirements Stations;
};
