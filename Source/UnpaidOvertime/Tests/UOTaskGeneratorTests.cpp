// UNPAID: OVERTIME - task generator tests.

#include "UOTestFlags.h"
#include "UOTaskGenerator.h"
#include "UOMatchConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOTaskGeneratorTests
{
	constexpr int32 SeedsToCheck = 40;

	bool AreDaysIdentical(const FUOGeneratedDay& A, const FUOGeneratedDay& B)
	{
		if (A.Tasks.Num() != B.Tasks.Num() || A.Stations.Stations.Num() != B.Stations.Stations.Num())
		{
			return false;
		}

		for (int32 TaskIndex = 0; TaskIndex < A.Tasks.Num(); ++TaskIndex)
		{
			const FUOTaskInstance& TaskA = A.Tasks[TaskIndex];
			const FUOTaskInstance& TaskB = B.Tasks[TaskIndex];
			if (TaskA.Type != TaskB.Type || TaskA.ZoneIndex != TaskB.ZoneIndex || !TaskA.DisplayName.EqualTo(TaskB.DisplayName)
				|| TaskA.SubObjectives.Num() != TaskB.SubObjectives.Num())
			{
				return false;
			}

			for (int32 SubIndex = 0; SubIndex < TaskA.SubObjectives.Num(); ++SubIndex)
			{
				const FUOSubObjective& SubA = TaskA.SubObjectives[SubIndex];
				const FUOSubObjective& SubB = TaskB.SubObjectives[SubIndex];
				if (SubA.Id != SubB.Id || SubA.StationId != SubB.StationId || SubA.ItemType != SubB.ItemType
					|| !FMath::IsNearlyEqual(SubA.WorkSeconds, SubB.WorkSeconds))
				{
					return false;
				}
			}
		}

		for (int32 StationIndex = 0; StationIndex < A.Stations.Stations.Num(); ++StationIndex)
		{
			if (A.Stations.Stations[StationIndex].StationId != B.Stations.Stations[StationIndex].StationId)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorTaskCountTest, "UnpaidOvertime.Core.TaskGenerator.ExactTaskCount", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorTaskCountTest::RunTest(const FString& Parameters)
{
	const float Difficulty = FUOTaskGenerator::GetDifficultyForDay(1);
	for (int32 TaskCount = FUOMatchConfig::MinTasksPerDay; TaskCount <= FUOMatchConfig::MaxTasksPerDay; ++TaskCount)
	{
		for (int32 Seed = 1; Seed <= UOTaskGeneratorTests::SeedsToCheck; ++Seed)
		{
			const FUOGeneratedDay Day = FUOTaskGenerator::Generate(Seed, TaskCount, Difficulty);
			if (!TestEqual(*FString::Printf(TEXT("Seed %d produces %d tasks"), Seed, TaskCount), Day.Tasks.Num(), TaskCount))
			{
				return false;
			}
		}
	}

	TestEqual(TEXT("Task count below range is clamped up"), FUOTaskGenerator::Generate(1, 2, Difficulty).Tasks.Num(), FUOMatchConfig::MinTasksPerDay);
	TestEqual(TEXT("Task count above range is clamped down"), FUOTaskGenerator::Generate(1, 12, Difficulty).Tasks.Num(), FUOMatchConfig::MaxTasksPerDay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorSubObjectiveBoundsTest, "UnpaidOvertime.Core.TaskGenerator.SubObjectiveBounds", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorSubObjectiveBoundsTest::RunTest(const FString& Parameters)
{
	int32 DayOneMin = 0;
	int32 DayOneMax = 0;
	FUOTaskGenerator::GetSubObjectiveRange(FUOTaskGenerator::GetDifficultyForDay(1), DayOneMin, DayOneMax);
	TestEqual(TEXT("Day 1 minimum sub-objectives"), DayOneMin, 2);
	TestEqual(TEXT("Day 1 maximum sub-objectives"), DayOneMax, 3);

	const float Difficulties[] = { 1.f, 1.5f, 2.f, 3.f, 10.f };
	for (const float Difficulty : Difficulties)
	{
		int32 MinSubs = 0;
		int32 MaxSubs = 0;
		FUOTaskGenerator::GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);
		TestTrue(TEXT("Range stays within 2..4"), MinSubs >= FUOTaskGenerator::MinSubObjectives && MaxSubs <= FUOTaskGenerator::MaxSubObjectives && MinSubs <= MaxSubs);

		for (int32 Seed = 1; Seed <= UOTaskGeneratorTests::SeedsToCheck; ++Seed)
		{
			const FUOGeneratedDay Day = FUOTaskGenerator::Generate(Seed, FUOMatchConfig::MaxTasksPerDay, Difficulty);
			for (const FUOTaskInstance& Task : Day.Tasks)
			{
				const int32 Count = Task.SubObjectives.Num();
				if (!TestTrue(*FString::Printf(TEXT("Seed %d difficulty %.1f: %d sub-objectives within [%d, %d]"), Seed, Difficulty, Count, MinSubs, MaxSubs), Count >= MinSubs && Count <= MaxSubs))
				{
					return false;
				}
			}
		}
	}

	int32 HardMin = 0;
	int32 HardMax = 0;
	FUOTaskGenerator::GetSubObjectiveRange(10.f, HardMin, HardMax);
	TestTrue(TEXT("Higher difficulty never lowers the range"), HardMin >= DayOneMin && HardMax >= DayOneMax);
	TestTrue(TEXT("Work time grows with difficulty"), FUOTaskGenerator::GetWorkSecondsScale(3.f) > FUOTaskGenerator::GetWorkSecondsScale(1.f));
	TestTrue(TEXT("Difficulty grows with days"), FUOTaskGenerator::GetDifficultyForDay(5) > FUOTaskGenerator::GetDifficultyForDay(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorDeterminismTest, "UnpaidOvertime.Core.TaskGenerator.DeterministicBySeed", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorDeterminismTest::RunTest(const FString& Parameters)
{
	const float Difficulty = FUOTaskGenerator::GetDifficultyForDay(1);
	for (int32 Seed = 1; Seed <= UOTaskGeneratorTests::SeedsToCheck; ++Seed)
	{
		const FUOGeneratedDay First = FUOTaskGenerator::Generate(Seed, 6, Difficulty);
		const FUOGeneratedDay Second = FUOTaskGenerator::Generate(Seed, 6, Difficulty);
		if (!TestTrue(*FString::Printf(TEXT("Seed %d generates the same day twice"), Seed), UOTaskGeneratorTests::AreDaysIdentical(First, Second)))
		{
			return false;
		}
	}

	const FUOGeneratedDay Reference = FUOTaskGenerator::Generate(1, 6, Difficulty);
	bool bAnyDifferent = false;
	for (int32 Seed = 2; Seed <= UOTaskGeneratorTests::SeedsToCheck && !bAnyDifferent; ++Seed)
	{
		bAnyDifferent = !UOTaskGeneratorTests::AreDaysIdentical(Reference, FUOTaskGenerator::Generate(Seed, 6, Difficulty));
	}
	TestTrue(TEXT("Different seeds produce different days"), bAnyDifferent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorPlaceableTest, "UnpaidOvertime.Core.TaskGenerator.AllTypesPlaceable", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorPlaceableTest::RunTest(const FString& Parameters)
{
	const float Difficulties[] = { FUOTaskGenerator::GetDifficultyForDay(1), 10.f };
	for (const float Difficulty : Difficulties)
	{
		for (int32 TaskCount = FUOMatchConfig::MinTasksPerDay; TaskCount <= FUOMatchConfig::MaxTasksPerDay; ++TaskCount)
		{
			for (int32 Seed = 1; Seed <= UOTaskGeneratorTests::SeedsToCheck; ++Seed)
			{
				const FUOGeneratedDay Day = FUOTaskGenerator::Generate(Seed, TaskCount, Difficulty);
				const FString Context = FString::Printf(TEXT("Seed %d, %d tasks, difficulty %.1f"), Seed, TaskCount, Difficulty);

				// Every task type shows up at least once
				for (const EUOTaskType Type : { EUOTaskType::PrinterMaintenance, EUOTaskType::CoffeeRefill, EUOTaskType::FileReports })
				{
					const bool bHasType = Day.Tasks.ContainsByPredicate([Type](const FUOTaskInstance& Task) { return Task.Type == Type; });
					if (!TestTrue(*(Context + TEXT(": every task type is present")), bHasType))
					{
						return false;
					}
				}

				// The office has room for every station
				if (!TestTrue(*(Context + TEXT(": stations fit the office wall slots")), Day.Stations.GetWallStationCount() <= FUOTaskGenerator::MaxWallStationSlots))
				{
					return false;
				}

				// Every sub-objective targets an existing station of the right type, and needs a reachable item source
				for (const FUOTaskInstance& Task : Day.Tasks)
				{
					for (const FUOSubObjective& Sub : Task.SubObjectives)
					{
						const FUOStationSpec* Station = Day.Stations.FindStation(Sub.StationId);
						if (!TestNotNull(*(Context + TEXT(": sub-objective station exists")), Station))
						{
							return false;
						}
						TestEqual(*(Context + TEXT(": station type matches task type")), Station->Type, FUOTaskGenerator::GetDestinationStationType(Task.Type));

						if (Sub.bRequiresItem)
						{
							const EUOStationType SourceType = FUOTaskGenerator::GetItemSourceStationType(Sub.ItemType);
							TestTrue(*(Context + TEXT(": item source station exists")), Day.Stations.GetCountForType(SourceType) > 0);
						}
						TestTrue(*(Context + TEXT(": work time is positive")), Sub.WorkSeconds > 0.f);
					}
				}

				// Unique ids
				TSet<FName> StationIds;
				for (const FUOStationSpec& Spec : Day.Stations.Stations)
				{
					bool bAlreadyInSet = false;
					StationIds.Add(Spec.StationId, &bAlreadyInSet);
					TestFalse(*(Context + TEXT(": station ids are unique")), bAlreadyInSet);
				}

				TSet<int32> SubIds;
				for (const FUOTaskInstance& Task : Day.Tasks)
				{
					for (const FUOSubObjective& Sub : Task.SubObjectives)
					{
						bool bAlreadyInSet = false;
						SubIds.Add(Sub.Id, &bAlreadyInSet);
						TestFalse(*(Context + TEXT(": sub-objective ids are unique")), bAlreadyInSet);
					}
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorRequirementsTest, "UnpaidOvertime.Core.TaskGenerator.StationRequirementsMatch", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorRequirementsTest::RunTest(const FString& Parameters)
{
	const float Difficulty = FUOTaskGenerator::GetDifficultyForDay(1);
	for (int32 Seed = 1; Seed <= UOTaskGeneratorTests::SeedsToCheck; ++Seed)
	{
		const FUOGeneratedDay Day = FUOTaskGenerator::Generate(Seed, 7, Difficulty);

		int32 PrinterSubs = 0;
		int32 CoffeeSubs = 0;
		int32 ReportTasks = 0;
		int32 PrinterTasks = 0;
		for (const FUOTaskInstance& Task : Day.Tasks)
		{
			switch (Task.Type)
			{
			case EUOTaskType::PrinterMaintenance:	PrinterSubs += Task.SubObjectives.Num(); ++PrinterTasks; break;
			case EUOTaskType::CoffeeRefill:			CoffeeSubs += Task.SubObjectives.Num(); break;
			case EUOTaskType::FileReports:			++ReportTasks; break;
			}
		}

		const FUOStationRequirements& Req = Day.Stations;
		TestEqual(TEXT("One printer per printer sub-objective"), Req.Printers, PrinterSubs);
		TestEqual(TEXT("One coffee maker per coffee sub-objective"), Req.CoffeeMakers, CoffeeSubs);
		TestEqual(TEXT("One filing cabinet per report task"), Req.FilingCabinets, ReportTasks);
		TestEqual(TEXT("One inbox tray per report task"), Req.InboxTrays, ReportTasks);
		TestEqual(TEXT("One supply closet when printers need ink"), Req.SupplyClosets, PrinterTasks > 0 ? 1 : 0);

		int32 SpecsByType[5] = { 0, 0, 0, 0, 0 };
		for (const FUOStationSpec& Spec : Req.Stations)
		{
			++SpecsByType[static_cast<int32>(Spec.Type)];
		}
		for (int32 TypeIndex = 0; TypeIndex < 5; ++TypeIndex)
		{
			TestEqual(TEXT("Station list matches the counters"), SpecsByType[TypeIndex], Req.GetCountForType(static_cast<EUOStationType>(TypeIndex)));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorZoneNamesTest, "UnpaidOvertime.Core.TaskGenerator.RepeatedTypesUseDistinctZones", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorZoneNamesTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = 1; Seed <= UOTaskGeneratorTests::SeedsToCheck; ++Seed)
	{
		const FUOGeneratedDay Day = FUOTaskGenerator::Generate(Seed, FUOMatchConfig::MaxTasksPerDay, 1.f);
		TSet<FString> Names;
		for (const FUOTaskInstance& Task : Day.Tasks)
		{
			bool bAlreadyInSet = false;
			Names.Add(Task.DisplayName.ToString(), &bAlreadyInSet);
			if (!TestFalse(*FString::Printf(TEXT("Seed %d: task names are unique (%s)"), Seed, *Task.DisplayName.ToString()), bAlreadyInSet))
			{
				return false;
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
