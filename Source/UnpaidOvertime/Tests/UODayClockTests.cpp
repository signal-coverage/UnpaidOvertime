// UNPAID: OVERTIME - day clock state machine tests.

#include "UOTestFlags.h"
#include "UODayClock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayClockShiftToOvertimeTest, "UnpaidOvertime.Core.DayClock.ShiftOvertimeCapEvaluation", UO_CORE_TEST_FLAGS)
bool FUODayClockShiftToOvertimeTest::RunTest(const FString& Parameters)
{
	FUODayClock Clock;
	TestEqual(TEXT("A new clock is in PreGame"), Clock.GetPhase(), EUODayPhase::PreGame);
	TestEqual(TEXT("PreGame does not advance"), Clock.Advance(10.f, false), EUODayPhase::PreGame);
	TestEqual(TEXT("PreGame keeps elapsed at zero"), Clock.GetElapsedSeconds(), 0.f);

	Clock.Start(100.f);
	TestEqual(TEXT("Start enters the shift"), Clock.GetPhase(), EUODayPhase::Shift);
	TestEqual(TEXT("Overtime cap is +50% of the shift"), Clock.GetOvertimeCapSeconds(), 50.f);
	TestEqual(TEXT("Day hard end is 150% of the shift"), Clock.GetDayEndSeconds(), 150.f);

	TestEqual(TEXT("Mid-shift stays in Shift"), Clock.Advance(50.f, false), EUODayPhase::Shift);
	TestEqual(TEXT("Remaining shift time"), Clock.GetRemainingShiftSeconds(), 50.f);
	TestEqual(TEXT("Shift timer ending with pending work enters Overtime"), Clock.Advance(50.f, false), EUODayPhase::Overtime);
	TestEqual(TEXT("Still in overtime before the cap"), Clock.Advance(49.f, false), EUODayPhase::Overtime);
	TestEqual(TEXT("Overtime elapsed"), Clock.GetOvertimeElapsedSeconds(), 49.f);
	TestFalse(TEXT("Not force-closed yet"), Clock.WasForceClosed());
	TestEqual(TEXT("Reaching the cap force-closes the day"), Clock.Advance(1.f, false), EUODayPhase::Evaluation);
	TestTrue(TEXT("Day was force-closed"), Clock.WasForceClosed());
	TestEqual(TEXT("Evaluation is terminal"), Clock.Advance(100.f, false), EUODayPhase::Evaluation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayClockEarlyCompletionTest, "UnpaidOvertime.Core.DayClock.EarlyCompletion", UO_CORE_TEST_FLAGS)
bool FUODayClockEarlyCompletionTest::RunTest(const FString& Parameters)
{
	FUODayClock Clock;
	Clock.Start(100.f);
	Clock.Advance(10.f, false);
	TestEqual(TEXT("All work done during the shift goes straight to Evaluation"), Clock.Advance(0.1f, true), EUODayPhase::Evaluation);
	TestFalse(TEXT("Early completion is not a forced close"), Clock.WasForceClosed());

	FUODayClock OvertimeClock;
	OvertimeClock.Start(100.f);
	OvertimeClock.Advance(120.f, false);
	TestEqual(TEXT("In overtime"), OvertimeClock.GetPhase(), EUODayPhase::Overtime);
	TestEqual(TEXT("Finishing during overtime goes to Evaluation"), OvertimeClock.Advance(0.1f, true), EUODayPhase::Evaluation);
	TestFalse(TEXT("Finishing in overtime is not a forced close"), OvertimeClock.WasForceClosed());

	FUODayClock BigStepClock;
	BigStepClock.Start(100.f);
	TestEqual(TEXT("A huge step passes through overtime to the cap"), BigStepClock.Advance(500.f, false), EUODayPhase::Evaluation);
	TestTrue(TEXT("Huge step force-closes"), BigStepClock.WasForceClosed());

	FUODayClock ZeroClock;
	ZeroClock.Start(0.f);
	TestTrue(TEXT("Shift length is clamped to a positive value"), ZeroClock.GetShiftLengthSeconds() > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayClockShiftLengthTest, "UnpaidOvertime.Core.DayClock.ShiftLengthAndOverride", UO_CORE_TEST_FLAGS)
bool FUODayClockShiftLengthTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Short shift"), FUODayClock::GetBaseShiftSeconds(EUODayLength::Short), 480.f);
	TestEqual(TEXT("Normal shift"), FUODayClock::GetBaseShiftSeconds(EUODayLength::Normal), 720.f);
	TestEqual(TEXT("Long shift"), FUODayClock::GetBaseShiftSeconds(EUODayLength::Long), 1080.f);

	TestEqual(TEXT("Override 0 is off"), FUODayClock::ResolveShiftSeconds(EUODayLength::Normal, 0.f), 720.f);
	TestEqual(TEXT("Negative override is off"), FUODayClock::ResolveShiftSeconds(EUODayLength::Short, -5.f), 480.f);
	TestEqual(TEXT("Positive override wins"), FUODayClock::ResolveShiftSeconds(EUODayLength::Long, 30.f), 30.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayClockTimeOfDayTest, "UnpaidOvertime.Core.DayClock.TimeOfDay", UO_CORE_TEST_FLAGS)
bool FUODayClockTimeOfDayTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Shift starts at 09:00"), FUODayClock::FormatTimeOfDay(FUODayClock::ComputeTimeOfDayMinutes(0.f, 720.f)), FString(TEXT("09:00")));
	TestEqual(TEXT("Half shift is 13:00"), FUODayClock::FormatTimeOfDay(FUODayClock::ComputeTimeOfDayMinutes(360.f, 720.f)), FString(TEXT("13:00")));
	TestEqual(TEXT("Shift ends at 17:00"), FUODayClock::FormatTimeOfDay(FUODayClock::ComputeTimeOfDayMinutes(720.f, 720.f)), FString(TEXT("17:00")));
	TestEqual(TEXT("Overtime cap is 21:00"), FUODayClock::FormatTimeOfDay(FUODayClock::ComputeTimeOfDayMinutes(1080.f, 720.f)), FString(TEXT("21:00")));
	TestEqual(TEXT("Clock is independent of shift length"), FUODayClock::FormatTimeOfDay(FUODayClock::ComputeTimeOfDayMinutes(15.f, 30.f)), FString(TEXT("13:00")));
	TestEqual(TEXT("Minutes are zero-padded"), FUODayClock::FormatTimeOfDay(9.f * 60.f + 5.f), FString(TEXT("09:05")));
	TestEqual(TEXT("Wraps past midnight"), FUODayClock::FormatTimeOfDay(25.f * 60.f), FString(TEXT("01:00")));

	FUODayClock Clock;
	Clock.Start(720.f);
	Clock.Advance(180.f, false);
	TestEqual(TEXT("Clock instance reports 11:00"), FUODayClock::FormatTimeOfDay(Clock.GetTimeOfDayMinutes()), FString(TEXT("11:00")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
