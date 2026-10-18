// UNPAID: OVERTIME - deterministic, seed-driven task generator.

#pragma once

#include "CoreMinimal.h"
#include "UOTaskTypes.h"

/**
 *  Produces the day's tasks and the stations the office needs.
 *  The task count never changes between days; difficulty D(day) scales sub-objective count and work time.
 */
struct FUOTaskGenerator
{
	static constexpr int32 MinSubObjectives = 2;
	static constexpr int32 MaxSubObjectives = 4;

	/** Wall slots offered by the office builder (the supply closet has its own corner) */
	static constexpr int32 MaxWallStationSlots = 36;

	/** Named office zones used as task suffixes ("North wing", ...) */
	static constexpr int32 NumZones = 8;

	/** D(day): 1.0 on day 1, growing every day */
	static float GetDifficultyForDay(int32 DayNumber);

	/** Inclusive sub-objective count range for a difficulty */
	static void GetSubObjectiveRange(float Difficulty, int32& OutMin, int32& OutMax);

	/** Multiplier applied to base work seconds */
	static float GetWorkSecondsScale(float Difficulty);

	/** Base hold time at the destination station for a task type */
	static float GetBaseWorkSeconds(EUOTaskType TaskType);

	/** Station where a task type's sub-objectives are finished */
	static EUOStationType GetDestinationStationType(EUOTaskType TaskType);

	/** Station that hands out an item */
	static EUOStationType GetItemSourceStationType(EUOItemType ItemType);

	/** Localized zone name ("North wing") */
	static FText GetZoneName(int32 ZoneIndex);

	/** Generates exactly TasksPerDay tasks (clamped to 4..8) */
	static FUOGeneratedDay Generate(int32 Seed, int32 TasksPerDay, float Difficulty);
};
