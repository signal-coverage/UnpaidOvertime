// UNPAID: OVERTIME - hardcore overtime weekend and the never-plateauing workload curve.

#include "UOTestFlags.h"
#include "UODayDifficulty.h"
#include "UODayCalendar.h"
#include "UODayClock.h"
#include "UOTaskGenerator.h"
#include "UOBossQuestions.h"
#include "UOMatchConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UODayDifficultyTestsPrivate
{
	constexpr int32 Tasks = FUOMatchConfig::DefaultTasksPerDay;
	constexpr int32 Employees = FUOMatchConfig::MinTeamSize;

	/** Required hold-work seconds per alive employee over the shift seconds, for a difficulty and shift scale */
	float Ratio(float Difficulty, float ShiftScale)
	{
		const float Shift = FUODayClock::GetBaseShiftSeconds(EUODayLength::Normal) * ShiftScale;
		return FUODayDifficulty::ComputeWorkloadRatio(FUODayDifficulty::GetExpectedWorkSeconds(Difficulty, Tasks), Employees, Shift);
	}

	/** The day as actually played (weekend boost included) */
	float DayRatio(int32 Day)
	{
		return Ratio(FUODayDifficulty::GetTaskDifficulty(Day), FUODayDifficulty::GetShiftScale(Day));
	}

	/** The underlying curve, without the weekend boost */
	float CurveRatio(int32 Day)
	{
		return Ratio(FUOTaskGenerator::GetDifficultyForDay(Day), FUODayDifficulty::GetCurveShiftScale(Day));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayDifficultyWeekendTest, "UnpaidOvertime.Core.DayDifficulty.HardcoreWeekend", UO_CORE_TEST_FLAGS)
bool FUODayDifficultyWeekendTest::RunTest(const FString& Parameters)
{
	using namespace UODayDifficultyTestsPrivate;

	for (int32 Week = 1; Week <= 10; ++Week)
	{
		const int32 Weekend = Week * FUODayCalendar::DaysPerWeek;
		const int32 Friday = Weekend - 1;
		const int32 Monday = Weekend + 1;

		TestTrue(TEXT("The overtime weekend is hardcore"), FUODayDifficulty::IsHardcoreDay(Weekend));
		TestFalse(TEXT("Friday is not hardcore"), FUODayDifficulty::IsHardcoreDay(Friday));
		TestFalse(TEXT("Monday is not hardcore"), FUODayDifficulty::IsHardcoreDay(Monday));

		// Tasks
		const float WeekendD = FUODayDifficulty::GetTaskDifficulty(Weekend);
		TestTrue(TEXT("Weekend tasks are harder than Friday's"), WeekendD > FUODayDifficulty::GetTaskDifficulty(Friday));
		TestTrue(TEXT("Weekend tasks are harder than next Monday's"), WeekendD > FUODayDifficulty::GetTaskDifficulty(Monday));
		TestTrue(TEXT("Weekend shift is shorter than Friday's"), FUODayDifficulty::GetShiftScale(Weekend) < FUODayDifficulty::GetShiftScale(Friday));
		TestTrue(TEXT("Weekend shift is shorter than next Monday's"), FUODayDifficulty::GetShiftScale(Weekend) < FUODayDifficulty::GetShiftScale(Monday));
		TestTrue(TEXT("Weekend workload beats Friday's"), DayRatio(Weekend) > DayRatio(Friday));
		TestTrue(TEXT("Weekend workload beats next Monday's"), DayRatio(Weekend) > DayRatio(Monday));

		// The following Monday continues from its own day number, not from the boosted weekend
		TestEqual(TEXT("Monday task difficulty is the curve value"), FUODayDifficulty::GetTaskDifficulty(Monday), FUOTaskGenerator::GetDifficultyForDay(Monday));
		TestEqual(TEXT("Monday shift scale is the curve value"), FUODayDifficulty::GetShiftScale(Monday), FUODayDifficulty::GetCurveShiftScale(Monday));
		TestEqual(TEXT("Weekday task difficulty is the curve value"), FUODayDifficulty::GetTaskDifficulty(Friday), FUOTaskGenerator::GetDifficultyForDay(Friday));

		// Boss questions: harder tier, shorter timers
		for (const EUOQuestionPurpose Purpose : { EUOQuestionPurpose::Defense, EUOQuestionPurpose::Tiebreak, EUOQuestionPurpose::Promotion })
		{
			const FUOBossTuning Sat = FUOBossQuestionGenerator::GetTuning(Purpose, Weekend);
			const FUOBossTuning Fri = FUOBossQuestionGenerator::GetTuning(Purpose, Friday);
			const FUOBossTuning Mon = FUOBossQuestionGenerator::GetTuning(Purpose, Monday);
			TestTrue(TEXT("Weekend timer is shorter than Friday's"), Sat.TimeLimitSeconds < Fri.TimeLimitSeconds);
			TestTrue(TEXT("Weekend timer is shorter than Monday's"), Sat.TimeLimitSeconds < Mon.TimeLimitSeconds);
			TestTrue(TEXT("Weekend timer stays playable"), Sat.TimeLimitSeconds >= FUODayDifficulty::GetHardcoreTuning().BossMinTimeLimitSeconds);
			TestTrue(TEXT("Weekend never has fewer options"), Sat.NumOptions >= FMath::Max(Fri.NumOptions, Mon.NumOptions));
			TestTrue(TEXT("Weekend allows hard facts if a neighbour does"), Sat.bAllowHardFacts || !(Fri.bAllowHardFacts || Mon.bAllowHardFacts));
			TestTrue(TEXT("Weekend uses close distractors if a neighbour does"), Sat.bCloseDistractors || !(Fri.bCloseDistractors || Mon.bCloseDistractors));
		}
	}

	// Week 1's weekend already plays like a much later day
	const FUOBossTuning FirstWeekend = FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, 6);
	TestEqual(TEXT("First weekend defense has 4 options"), FirstWeekend.NumOptions, 4);
	TestTrue(TEXT("First weekend allows hard facts and close distractors"), FirstWeekend.bAllowHardFacts && FirstWeekend.bCloseDistractors);

	// Tuning lives in one struct and actually boosts
	const FUOHardcoreTuning& Tuning = FUODayDifficulty::GetHardcoreTuning();
	TestTrue(TEXT("Task bonus is positive"), Tuning.TaskDifficultyBonus > 0.f);
	TestTrue(TEXT("Shift scale shortens"), Tuning.ShiftScale > 0.f && Tuning.ShiftScale < 1.f);
	TestTrue(TEXT("Boss timer scale shortens"), Tuning.BossTimeLimitScale > 0.f && Tuning.BossTimeLimitScale < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayDifficultyNameTest, "UnpaidOvertime.Core.DayDifficulty.HardcoreName", UO_CORE_TEST_FLAGS)
bool FUODayDifficultyNameTest::RunTest(const FString& Parameters)
{
	const FString Weekend = FUODayCalendar::FormatDayName(6).ToString();
	TestTrue(TEXT("Weekend name says Overtime Weekend"), Weekend.Contains(TEXT("Overtime Weekend"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("Weekend name says HARDCORE"), Weekend.Contains(TEXT("HARDCORE"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("Weekend name keeps the week"), Weekend.Contains(TEXT("Week 1")));
	TestFalse(TEXT("Friday is not HARDCORE"), FUODayCalendar::FormatDayName(5).ToString().Contains(TEXT("HARDCORE")));
	TestFalse(TEXT("Monday is not HARDCORE"), FUODayCalendar::FormatDayName(7).ToString().Contains(TEXT("HARDCORE")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayDifficultyNeverPlateausTest, "UnpaidOvertime.Core.DayDifficulty.WorkloadNeverPlateaus", UO_CORE_TEST_FLAGS)
bool FUODayDifficultyNeverPlateausTest::RunTest(const FString& Parameters)
{
	using namespace UODayDifficultyTestsPrivate;

	// Day by day, the curve never eases; over every 5-day window it strictly rises (also past the sub-objective cap)
	for (int32 Day = 2; Day <= 60; ++Day)
	{
		TestTrue(*FString::Printf(TEXT("Workload ratio never drops (day %d)"), Day), CurveRatio(Day) >= CurveRatio(Day - 1));
	}
	for (int32 Day = 1; Day + 5 <= 60; ++Day)
	{
		TestTrue(*FString::Printf(TEXT("Workload ratio strictly rises over days %d-%d"), Day, Day + 5), CurveRatio(Day + 5) > CurveRatio(Day));
	}

	// Sub-objectives cap out early; work time and the shrinking shift keep pushing afterwards
	int32 Min = 0;
	int32 Max = 0;
	FUOTaskGenerator::GetSubObjectiveRange(FUOTaskGenerator::GetDifficultyForDay(20), Min, Max);
	TestEqual(TEXT("Sub-objectives are capped by day 20"), Min, FUOTaskGenerator::MaxSubObjectives);
	TestTrue(TEXT("Workload keeps rising long after the cap"), CurveRatio(60) > CurveRatio(20) * 1.3f);
	TestTrue(TEXT("Work time still rises at day 200"), FUOTaskGenerator::GetWorkSecondsScale(FUOTaskGenerator::GetDifficultyForDay(200))
		> FUOTaskGenerator::GetWorkSecondsScale(FUOTaskGenerator::GetDifficultyForDay(195)));
	TestTrue(TEXT("Shift still shrinks at day 200"), FUODayDifficulty::GetCurveShiftScale(200) < FUODayDifficulty::GetCurveShiftScale(195));

	// Sanity limits: bounded, and the worst day stays theoretically completable by a full team
	for (int32 Day = 1; Day <= 1000; ++Day)
	{
		const float D = FUODayDifficulty::GetTaskDifficulty(Day);
		TestTrue(TEXT("Work scale is bounded"), FUOTaskGenerator::GetWorkSecondsScale(D) <= FUOTaskGenerator::MaxWorkSecondsScale);
		TestTrue(TEXT("Shift never shrinks below the floor"), FUODayDifficulty::GetCurveShiftScale(Day) >= FUODayDifficulty::MinShiftScale);

		// Worst case: most tasks, slowest task type, max sub-objectives, short shift, minimum team
		const float WorstWork = FUODayDifficulty::GetMaxWorkSeconds(D, FUOMatchConfig::MaxTasksPerDay);
		const float ShortShift = FUODayClock::GetBaseShiftSeconds(EUODayLength::Short) * FUODayDifficulty::GetShiftScale(Day);
		const float DayEnd = ShortShift * (1.f + FUODayClock::OvertimeCapFraction);
		const float WorstRatio = FUODayDifficulty::ComputeWorkloadRatio(WorstWork, FUOMatchConfig::MinTeamSize, DayEnd);
		if (WorstRatio > FUODayDifficulty::MaxHoldWorkFractionOfDay)
		{
			AddError(FString::Printf(TEXT("Day %d is not completable: hold work per employee is %.2f of the day"), Day, WorstRatio));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODayDifficultyExpectedWorkTest, "UnpaidOvertime.Core.DayDifficulty.ExpectedWorkMatchesGenerator", UO_CORE_TEST_FLAGS)
bool FUODayDifficultyExpectedWorkTest::RunTest(const FString& Parameters)
{
	using namespace UODayDifficultyTestsPrivate;

	for (const float D : { 1.f, 2.25f, 6.f, 15.f })
	{
		constexpr int32 Samples = 400;
		double Total = 0.0;
		for (int32 Seed = 1; Seed <= Samples; ++Seed)
		{
			const FUOGeneratedDay Day = FUOTaskGenerator::Generate(Seed * 13 + 1, Tasks, D);
			for (const FUOTaskInstance& Task : Day.Tasks)
			{
				for (const FUOSubObjective& Sub : Task.SubObjectives)
				{
					Total += Sub.WorkSeconds;
				}
			}
		}
		const float Average = static_cast<float>(Total / Samples);
		const float Expected = FUODayDifficulty::GetExpectedWorkSeconds(D, Tasks);
		TestTrue(*FString::Printf(TEXT("Generated work %.1fs is close to the expected %.1fs (D=%.2f)"), Average, Expected, D),
			FMath::Abs(Average - Expected) <= 0.1f * Expected);
		TestTrue(TEXT("Max work bounds the expected work"), FUODayDifficulty::GetMaxWorkSeconds(D, Tasks) >= Expected);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
