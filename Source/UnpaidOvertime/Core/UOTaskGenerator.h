// UNPAID: OVERTIME - deterministic, seed-driven task generator.

#pragma once

#include "CoreMinimal.h"
#include "UOTaskTypes.h"

/** Station anchors available in a hand-built level, per type */
struct FUOStationSlots
{
	int32 Printers = 0;
	int32 CoffeeMakers = 0;
	int32 FilingCabinets = 0;
	int32 InboxTrays = 0;
	int32 SupplyClosets = 0;

	int32 Get(EUOStationType Type) const;
	void Add(EUOStationType Type, int32 Count = 1);
	int32 Total() const { return Printers + CoffeeMakers + FilingCabinets + InboxTrays + SupplyClosets; }
};

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

	/**
	 *  Sanity limit for the work time multiplier (a 4 s coffee refill tops out near 24 s of holding).
	 *  The multiplier approaches it but never reaches it, so work keeps growing after the sub-objective cap.
	 */
	static constexpr float MaxWorkSecondsScale = 6.f;

	/** Work time multiplier growth per difficulty point at D = 1 (slows down near MaxWorkSecondsScale) */
	static constexpr float WorkSecondsScalePerDifficulty = 0.2f;

	/** Multiplier applied to base work seconds: 1 at D = 1, strictly rising, below MaxWorkSecondsScale */
	static float GetWorkSecondsScale(float Difficulty);

	/** Base hold time at the destination station for a task type */
	static float GetBaseWorkSeconds(EUOTaskType TaskType);

	/** Station where a task type's sub-objectives are finished */
	static EUOStationType GetDestinationStationType(EUOTaskType TaskType);

	/** Station that hands out an item */
	static EUOStationType GetItemSourceStationType(EUOItemType ItemType);

	/** Localized zone name ("North wing") */
	static FText GetZoneName(int32 ZoneIndex);

	/** "Printer_03": stable id of the Nth (1-based) station of a type */
	static FName MakeStationId(EUOStationType Type, int32 Ordinal);

	/** Generates exactly TasksPerDay tasks (clamped to 4..8) and one new station per need (procedural office) */
	static FUOGeneratedDay Generate(int32 Seed, int32 TasksPerDay, float Difficulty);

	/**
	 *  Generates exactly TasksPerDay tasks (clamped to 4..8) for an office with fixed station anchors.
	 *  Never uses more printers / coffee makers than sub-objectives of that type allow, nor more filing cabinets
	 *  than report tasks; stations are picked from the anchors deterministically by seed. Types whose stations
	 *  (or item sources) are missing are skipped. Only when the office is too small for TasksPerDay tasks with
	 *  one sub-objective each are stations shared. Every item source anchor is listed in the result.
	 */
	static FUOGeneratedDay GenerateForSlots(int32 Seed, int32 TasksPerDay, float Difficulty, const FUOStationSlots& Slots);
};
