// UNPAID: OVERTIME - day naming in weekly cycles.

#include "UODayCalendar.h"

#define LOCTEXT_NAMESPACE "UODayCalendar"

int32 FUODayCalendar::GetWeekNumber(int32 DayNumber)
{
	return (FMath::Max(1, DayNumber) - 1) / DaysPerWeek + 1;
}

EUOWeekday FUODayCalendar::GetWeekday(int32 DayNumber)
{
	return static_cast<EUOWeekday>((FMath::Max(1, DayNumber) - 1) % DaysPerWeek);
}

bool FUODayCalendar::IsOvertimeWeekend(int32 DayNumber)
{
	return GetWeekday(DayNumber) == EUOWeekday::OvertimeWeekend;
}

FText FUODayCalendar::GetWeekdayName(EUOWeekday Weekday)
{
	switch (Weekday)
	{
	case EUOWeekday::Monday:			return LOCTEXT("Monday", "Monday");
	case EUOWeekday::Tuesday:			return LOCTEXT("Tuesday", "Tuesday");
	case EUOWeekday::Wednesday:			return LOCTEXT("Wednesday", "Wednesday");
	case EUOWeekday::Thursday:			return LOCTEXT("Thursday", "Thursday");
	case EUOWeekday::Friday:			return LOCTEXT("Friday", "Friday");
	case EUOWeekday::OvertimeWeekend:	return LOCTEXT("OvertimeWeekend", "Overtime Weekend");
	default:							return FText::GetEmpty();
	}
}

FText FUODayCalendar::GetHardcoreLabel()
{
	return LOCTEXT("Hardcore", "HARDCORE");
}

FText FUODayCalendar::FormatDayName(int32 DayNumber)
{
	const int32 Day = FMath::Max(1, DayNumber);
	const FText Name = FText::Format(LOCTEXT("DayName", "Day {0} — {1} (Week {2})"),
		FText::AsNumber(Day), GetWeekdayName(GetWeekday(Day)), FText::AsNumber(GetWeekNumber(Day)));
	// The overtime weekend is the hardcore day (UODayDifficulty)
	return IsOvertimeWeekend(Day) ? FText::Format(LOCTEXT("HardcoreDayName", "{0} · {1}"), Name, GetHardcoreLabel()) : Name;
}

#undef LOCTEXT_NAMESPACE
