// UNPAID: OVERTIME - day naming tests.

#include "UOTestFlags.h"
#include "UODayCalendar.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayCalendarWeekdaysTest, "UnpaidOvertime.Core.DayCalendar.Weekdays", UO_CORE_TEST_FLAGS)
bool FUODayCalendarWeekdaysTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Day 1 is Monday"), FUODayCalendar::GetWeekday(1), EUOWeekday::Monday);
	TestEqual(TEXT("Day 2 is Tuesday"), FUODayCalendar::GetWeekday(2), EUOWeekday::Tuesday);
	TestEqual(TEXT("Day 5 is Friday"), FUODayCalendar::GetWeekday(5), EUOWeekday::Friday);
	TestEqual(TEXT("Day 6 is the overtime weekend"), FUODayCalendar::GetWeekday(6), EUOWeekday::OvertimeWeekend);
	TestEqual(TEXT("Day 7 starts a new week on Monday"), FUODayCalendar::GetWeekday(7), EUOWeekday::Monday);
	TestEqual(TEXT("Day 12 is the second weekend"), FUODayCalendar::GetWeekday(12), EUOWeekday::OvertimeWeekend);
	TestEqual(TEXT("Day 14 is Tuesday"), FUODayCalendar::GetWeekday(14), EUOWeekday::Tuesday);
	TestEqual(TEXT("Day 0 behaves like day 1"), FUODayCalendar::GetWeekday(0), EUOWeekday::Monday);

	TestEqual(TEXT("Days 1-6 are week 1"), FUODayCalendar::GetWeekNumber(6), 1);
	TestEqual(TEXT("Day 7 is week 2"), FUODayCalendar::GetWeekNumber(7), 2);
	TestEqual(TEXT("Day 14 is week 3"), FUODayCalendar::GetWeekNumber(14), 3);
	TestEqual(TEXT("Negative days are week 1"), FUODayCalendar::GetWeekNumber(-4), 1);

	TestTrue(TEXT("Day 6 is a weekend"), FUODayCalendar::IsOvertimeWeekend(6));
	TestFalse(TEXT("Day 5 is not a weekend"), FUODayCalendar::IsOvertimeWeekend(5));

	// Every weekday cycles through exactly once per week
	TSet<EUOWeekday> Seen;
	for (int32 Day = 13; Day < 13 + FUODayCalendar::DaysPerWeek; ++Day)
	{
		Seen.Add(FUODayCalendar::GetWeekday(Day));
	}
	TestEqual(TEXT("A full week visits every weekday once"), Seen.Num(), FUODayCalendar::DaysPerWeek);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayCalendarNamesTest, "UnpaidOvertime.Core.DayCalendar.Names", UO_CORE_TEST_FLAGS)
bool FUODayCalendarNamesTest::RunTest(const FString& Parameters)
{
	const FString Day14 = FUODayCalendar::FormatDayName(14).ToString();
	TestTrue(TEXT("Day name has the cumulative counter"), Day14.Contains(TEXT("14")));
	TestTrue(TEXT("Day name has the weekday"), Day14.Contains(TEXT("Tuesday")));
	TestTrue(TEXT("Day name has the week"), Day14.Contains(TEXT("Week 3")));

	const FString Day6 = FUODayCalendar::FormatDayName(6).ToString();
	TestTrue(TEXT("Weekend is named 'Overtime weekend'"), Day6.Contains(TEXT("Overtime weekend")));

	for (const EUOWeekday Weekday : { EUOWeekday::Monday, EUOWeekday::Tuesday, EUOWeekday::Wednesday, EUOWeekday::Thursday, EUOWeekday::Friday, EUOWeekday::OvertimeWeekend })
	{
		TestFalse(TEXT("Every weekday has a name"), FUODayCalendar::GetWeekdayName(Weekday).IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
