// UNPAID: OVERTIME - day naming in weekly cycles ("Day 12 — Friday (Week 2)").

#pragma once

#include "CoreMinimal.h"
#include "UOEvaluationTypes.h"

/**
 *  The day counter never resets during a match. Weeks are six days long: Monday to Friday, then one
 *  "Overtime weekend" day (design doc 2.3). Day numbers below 1 behave like day 1.
 */
struct FUODayCalendar
{
	static constexpr int32 DaysPerWeek = 6;

	/** 1-based week number */
	static int32 GetWeekNumber(int32 DayNumber);

	static EUOWeekday GetWeekday(int32 DayNumber);

	static bool IsOvertimeWeekend(int32 DayNumber);

	static FText GetWeekdayName(EUOWeekday Weekday);

	/** "HARDCORE": badge text of the overtime weekend */
	static FText GetHardcoreLabel();

	/** "Day 12 — Friday (Week 2)"; the weekend reads "Day 6 — Overtime Weekend (Week 1) · HARDCORE" */
	static FText FormatDayName(int32 DayNumber);
};
