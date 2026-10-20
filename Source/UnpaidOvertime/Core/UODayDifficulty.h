// UNPAID: OVERTIME - per-day difficulty: the endless curve plus the hardcore overtime weekend.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"

/**
 *  Every knob of the hardcore overtime weekend (design doc 2.3), in one place. Applied ONLY on that day:
 *  the next Monday goes back to the curve value for its own day number. AI parameters are not boosted
 *  (the weekend hits everyone equally): tasks, the shift and the boss get harder instead.
 */
struct FUOHardcoreTuning
{
	/** Added to the task difficulty D (the curve adds 0.25 per day, so 1.5 = six days ahead) */
	float TaskDifficultyBonus = 1.5f;

	/** Multiplies the day's shift length (and so the overtime cap) */
	float ShiftScale = 0.8f;

	/** Boss question tiers (options, hard facts, close distractors) act as if the day were this much later */
	int32 BossTierDayBonus = 6;

	/** Multiplies boss question time limits */
	float BossTimeLimitScale = 0.75f;

	/** Weekend boss timers never go below this */
	float BossMinTimeLimitSeconds = 5.f;
};

struct FUODayDifficulty
{
	/** The shift never shrinks below this fraction of the configured length (before the weekend scale) */
	static constexpr float MinShiftScale = 0.6f;

	/** Initial shrink per day; it slows down as it approaches MinShiftScale, but never stops */
	static constexpr float ShiftShrinkPerDay = 0.004f;

	/** Sanity limit: a full team's pure hold work on the worst possible day stays below this fraction of the day */
	static constexpr float MaxHoldWorkFractionOfDay = 0.6f;

	static const FUOHardcoreTuning& GetHardcoreTuning();

	/** The overtime weekend */
	static bool IsHardcoreDay(int32 DayNumber);

	/** Task difficulty D actually used for the day: the curve, plus the hardcore bonus on the weekend */
	static float GetTaskDifficulty(int32 DayNumber);

	/** Shift length multiplier from the curve alone: 1 on day 1, approaching MinShiftScale */
	static float GetCurveShiftScale(int32 DayNumber);

	/** Shift length multiplier actually used for the day (curve, times the hardcore scale on the weekend) */
	static float GetShiftScale(int32 DayNumber);

	/** Shift seconds for the day. OverrideSeconds > 0 replaces the configured base length (day scaling still applies). */
	static float GetShiftSeconds(EUODayLength DayLength, int32 DayNumber, float OverrideSeconds = 0.f);

	/** Expected hold-work seconds of all the day's sub-objectives (average sub-objective count and task type) */
	static float GetExpectedWorkSeconds(float Difficulty, int32 TasksPerDay);

	/** Worst-case hold-work seconds: max sub-objectives, every task of the slowest type */
	static float GetMaxWorkSeconds(float Difficulty, int32 TasksPerDay);

	/** Work per alive employee over the available seconds */
	static float ComputeWorkloadRatio(float WorkSeconds, int32 AliveEmployees, float AvailableSeconds);
};
