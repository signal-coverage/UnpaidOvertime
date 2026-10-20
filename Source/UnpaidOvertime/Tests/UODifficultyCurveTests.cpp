// UNPAID: OVERTIME - difficulty curve across days: task difficulty and AI parameters rise, task count does not.

#include "UOTestFlags.h"
#include "UOTaskGenerator.h"
#include "UOAIDifficulty.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODifficultyCurveTasksTest, "UnpaidOvertime.Core.DifficultyCurve.TasksHarderNotMore", UO_CORE_TEST_FLAGS)
bool FUODifficultyCurveTasksTest::RunTest(const FString& Parameters)
{
	float PreviousD = FUOTaskGenerator::GetDifficultyForDay(1);
	int32 PreviousMin = 0;
	int32 PreviousMax = 0;
	FUOTaskGenerator::GetSubObjectiveRange(PreviousD, PreviousMin, PreviousMax);
	float PreviousScale = FUOTaskGenerator::GetWorkSecondsScale(PreviousD);

	for (int32 Day = 2; Day <= 30; ++Day)
	{
		const float D = FUOTaskGenerator::GetDifficultyForDay(Day);
		int32 Min = 0;
		int32 Max = 0;
		FUOTaskGenerator::GetSubObjectiveRange(D, Min, Max);
		const float Scale = FUOTaskGenerator::GetWorkSecondsScale(D);

		TestTrue(TEXT("D(day) strictly rises"), D > PreviousD);
		TestTrue(TEXT("Sub-objective minimum never drops"), Min >= PreviousMin);
		TestTrue(TEXT("Sub-objective maximum never drops"), Max >= PreviousMax);
		TestTrue(TEXT("Work time per sub-objective rises"), Scale > PreviousScale);

		// The task count is the configured one, every day
		const FUOGeneratedDay Generated = FUOTaskGenerator::Generate(100 + Day, 6, D);
		TestEqual(TEXT("Task count never changes with the day"), Generated.Tasks.Num(), 6);

		PreviousD = D;
		PreviousMin = Min;
		PreviousMax = Max;
		PreviousScale = Scale;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODifficultyCurveAITest, "UnpaidOvertime.Core.DifficultyCurve.AIEvaluationParams", UO_CORE_TEST_FLAGS)
bool FUODifficultyCurveAITest::RunTest(const FString& Parameters)
{
	for (const EUOAIDifficulty Base : { EUOAIDifficulty::Easy, EUOAIDifficulty::Medium, EUOAIDifficulty::Hard })
	{
		FUOAIDifficultyParams Previous = FUOAIDifficulty::Compute(Base, 1);
		for (int32 Day = 2; Day <= 40; ++Day)
		{
			const FUOAIDifficultyParams Current = FUOAIDifficulty::Compute(Base, Day);
			TestTrue(TEXT("Answer accuracy never drops"), Current.AnswerAccuracy >= Previous.AnswerAccuracy);
			TestTrue(TEXT("Answers never get slower"), Current.AnswerTimeFraction <= Previous.AnswerTimeFraction);
			TestTrue(TEXT("Suspicion never drops (M4 hook)"), Current.Suspicion >= Previous.Suspicion);
			TestTrue(TEXT("Votes never get noisier"), Current.VoteNoise <= Previous.VoteNoise);
			TestTrue(TEXT("Accuracy is a probability below certainty"), Current.AnswerAccuracy > 0.f && Current.AnswerAccuracy < 1.f);
			TestTrue(TEXT("Answer time fraction in (0, 1)"), Current.AnswerTimeFraction > 0.f && Current.AnswerTimeFraction < 1.f);
			TestTrue(TEXT("Vote noise stays positive"), Current.VoteNoise > 0.f);
			Previous = Current;
		}
	}

	const FUOAIDifficultyParams Easy = FUOAIDifficulty::Compute(EUOAIDifficulty::Easy, 1);
	const FUOAIDifficultyParams Hard = FUOAIDifficulty::Compute(EUOAIDifficulty::Hard, 1);
	TestTrue(TEXT("Hard AIs remember better than Easy ones"), Hard.AnswerAccuracy > Easy.AnswerAccuracy);
	TestTrue(TEXT("Hard AIs vote more rationally"), Hard.VoteNoise < Easy.VoteNoise);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
