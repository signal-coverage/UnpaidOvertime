// UNPAID: OVERTIME - AI difficulty scaling tests.

#include "UOTestFlags.h"
#include "UOAIDifficulty.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAIDifficultyByDayTest, "UnpaidOvertime.Core.AIDifficulty.MonotonicWithDay", UO_CORE_TEST_FLAGS)
bool FUOAIDifficultyByDayTest::RunTest(const FString& Parameters)
{
	for (const EUOAIDifficulty Base : { EUOAIDifficulty::Easy, EUOAIDifficulty::Medium, EUOAIDifficulty::Hard })
	{
		FUOAIDifficultyParams Previous = FUOAIDifficulty::Compute(Base, 1);
		for (int32 Day = 2; Day <= 40; ++Day)
		{
			const FUOAIDifficultyParams Current = FUOAIDifficulty::Compute(Base, Day);
			const FString Context = FString::Printf(TEXT("Base %d, day %d"), static_cast<int32>(Base), Day);
			TestTrue(*(Context + TEXT(": work speed never decreases")), Current.WorkSpeedMultiplier >= Previous.WorkSpeedMultiplier);
			TestTrue(*(Context + TEXT(": precision never decreases")), Current.Precision >= Previous.Precision);
			TestTrue(*(Context + TEXT(": think delay never increases")), Current.ThinkDelaySeconds <= Previous.ThinkDelaySeconds);
			TestTrue(*(Context + TEXT(": precision is a probability")), Current.Precision > 0.f && Current.Precision <= 1.f);
			TestTrue(*(Context + TEXT(": work speed is positive")), Current.WorkSpeedMultiplier > 0.f);
			TestTrue(*(Context + TEXT(": think delay is positive")), Current.ThinkDelaySeconds > 0.f);
			Previous = Current;
		}
	}

	const FUOAIDifficultyParams DayOne = FUOAIDifficulty::Compute(EUOAIDifficulty::Medium, 1);
	const FUOAIDifficultyParams DayFive = FUOAIDifficulty::Compute(EUOAIDifficulty::Medium, 5);
	TestTrue(TEXT("Medium AIs get faster by day 5"), DayFive.WorkSpeedMultiplier > DayOne.WorkSpeedMultiplier);
	TestTrue(TEXT("Medium AIs get more precise by day 5"), DayFive.Precision > DayOne.Precision);
	TestTrue(TEXT("Medium AIs think faster by day 5"), DayFive.ThinkDelaySeconds < DayOne.ThinkDelaySeconds);
	TestEqual(TEXT("Day numbers below 1 behave like day 1"), FUOAIDifficulty::Compute(EUOAIDifficulty::Medium, 0).Precision, DayOne.Precision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAIDifficultyByBaseTest, "UnpaidOvertime.Core.AIDifficulty.MonotonicWithBaseDifficulty", UO_CORE_TEST_FLAGS)
bool FUOAIDifficultyByBaseTest::RunTest(const FString& Parameters)
{
	for (int32 Day = 1; Day <= 3; ++Day)
	{
		const FUOAIDifficultyParams Easy = FUOAIDifficulty::Compute(EUOAIDifficulty::Easy, Day);
		const FUOAIDifficultyParams Medium = FUOAIDifficulty::Compute(EUOAIDifficulty::Medium, Day);
		const FUOAIDifficultyParams Hard = FUOAIDifficulty::Compute(EUOAIDifficulty::Hard, Day);

		TestTrue(TEXT("Easy works slower than Medium"), Easy.WorkSpeedMultiplier < Medium.WorkSpeedMultiplier);
		TestTrue(TEXT("Medium works slower than Hard"), Medium.WorkSpeedMultiplier < Hard.WorkSpeedMultiplier);
		TestTrue(TEXT("Easy is less precise than Medium"), Easy.Precision < Medium.Precision);
		TestTrue(TEXT("Medium is less precise than Hard"), Medium.Precision < Hard.Precision);
		TestTrue(TEXT("Easy thinks longer than Medium"), Easy.ThinkDelaySeconds > Medium.ThinkDelaySeconds);
		TestTrue(TEXT("Medium thinks longer than Hard"), Medium.ThinkDelaySeconds > Hard.ThinkDelaySeconds);
	}

	TestTrue(TEXT("Skill level grows with base difficulty"), FUOAIDifficulty::GetSkillLevel(EUOAIDifficulty::Hard, 1) > FUOAIDifficulty::GetSkillLevel(EUOAIDifficulty::Easy, 1));
	TestTrue(TEXT("Skill level grows with days"), FUOAIDifficulty::GetSkillLevel(EUOAIDifficulty::Easy, 9) > FUOAIDifficulty::GetSkillLevel(EUOAIDifficulty::Easy, 1));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
