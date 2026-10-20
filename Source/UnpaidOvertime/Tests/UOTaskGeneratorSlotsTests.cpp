// UNPAID: OVERTIME - task generator tests for level-driven offices (fixed station anchors).

#include "UOTestFlags.h"
#include "UOTaskGenerator.h"
#include "UOMatchConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOTaskGeneratorSlotsTests
{
	constexpr int32 SeedsToCheck = 40;

	/** Anchors in /Game/UO/Maps/Lvl_Office */
	FUOStationSlots MakeOfficeSlots()
	{
		FUOStationSlots Slots;
		Slots.Printers = 8;
		Slots.CoffeeMakers = 6;
		Slots.FilingCabinets = 8;
		Slots.InboxTrays = 4;
		Slots.SupplyClosets = 2;
		return Slots;
	}

	FUOStationSlots MakeSlots(int32 Printers, int32 Coffee, int32 Cabinets, int32 Inbox, int32 Supply)
	{
		FUOStationSlots Slots;
		Slots.Printers = Printers;
		Slots.CoffeeMakers = Coffee;
		Slots.FilingCabinets = Cabinets;
		Slots.InboxTrays = Inbox;
		Slots.SupplyClosets = Supply;
		return Slots;
	}

	/** Distinct destination stations of a type referenced by sub-objectives */
	int32 CountDistinctStations(const FUOGeneratedDay& Day, EUOStationType Type)
	{
		TSet<FName> Used;
		for (const FUOTaskInstance& Task : Day.Tasks)
		{
			if (FUOTaskGenerator::GetDestinationStationType(Task.Type) != Type)
			{
				continue;
			}
			for (const FUOSubObjective& Sub : Task.SubObjectives)
			{
				Used.Add(Sub.StationId);
			}
		}
		return Used.Num();
	}

	int32 CountSubObjectives(const FUOGeneratedDay& Day, EUOTaskType Type)
	{
		int32 Count = 0;
		for (const FUOTaskInstance& Task : Day.Tasks)
		{
			Count += Task.Type == Type ? Task.SubObjectives.Num() : 0;
		}
		return Count;
	}

	int32 CountTasks(const FUOGeneratedDay& Day, EUOTaskType Type)
	{
		return Day.Tasks.FilterByPredicate([Type](const FUOTaskInstance& Task) { return Task.Type == Type; }).Num();
	}

	/** Every sub-objective points at a listed station of the right type whose ordinal is a real anchor */
	bool AllStationsAreRealAnchors(FAutomationTestBase& Test, const FUOGeneratedDay& Day, const FUOStationSlots& Slots, const FString& Context)
	{
		for (const FUOTaskInstance& Task : Day.Tasks)
		{
			for (const FUOSubObjective& Sub : Task.SubObjectives)
			{
				const FUOStationSpec* Spec = Day.Stations.FindStation(Sub.StationId);
				if (!Test.TestNotNull(*(Context + TEXT(": sub-objective station is listed")), Spec))
				{
					return false;
				}
				const EUOStationType Expected = FUOTaskGenerator::GetDestinationStationType(Task.Type);
				if (!Test.TestEqual(*(Context + TEXT(": station type matches the task")), Spec->Type, Expected)
					|| !Test.TestTrue(*(Context + TEXT(": station ordinal is an existing anchor")), Spec->Ordinal >= 1 && Spec->Ordinal <= Slots.Get(Expected))
					|| !Test.TestEqual(*(Context + TEXT(": station id matches type and ordinal")), Spec->StationId, FUOTaskGenerator::MakeStationId(Spec->Type, Spec->Ordinal)))
				{
					return false;
				}
				if (Sub.bRequiresItem)
				{
					const EUOStationType Source = FUOTaskGenerator::GetItemSourceStationType(Sub.ItemType);
					if (!Test.TestTrue(*(Context + TEXT(": item source is listed")), Day.Stations.GetCountForType(Source) > 0))
					{
						return false;
					}
				}
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorSlotsCapsTest, "UnpaidOvertime.Core.TaskGenerator.Slots.RespectsStationCaps", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorSlotsCapsTest::RunTest(const FString& Parameters)
{
	using namespace UOTaskGeneratorSlotsTests;

	const FUOStationSlots Slots = MakeOfficeSlots();
	const float Difficulties[] = { 1.f, 1.5f, 2.f, 3.f, 10.f };
	for (const float Difficulty : Difficulties)
	{
		int32 MinSubs = 0;
		int32 MaxSubs = 0;
		FUOTaskGenerator::GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);

		for (int32 TaskCount = FUOMatchConfig::MinTasksPerDay; TaskCount <= FUOMatchConfig::MaxTasksPerDay; ++TaskCount)
		{
			for (int32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
			{
				const FUOGeneratedDay Day = FUOTaskGenerator::GenerateForSlots(Seed, TaskCount, Difficulty, Slots);
				const FString Context = FString::Printf(TEXT("Seed %d, %d tasks, difficulty %.1f"), Seed, TaskCount, Difficulty);

				if (!TestEqual(*(Context + TEXT(": exact task count")), Day.Tasks.Num(), TaskCount))
				{
					return false;
				}

				// Each printer / coffee sub-objective has its own machine; each report task its own cabinet
				const int32 PrinterSubs = CountSubObjectives(Day, EUOTaskType::PrinterMaintenance);
				const int32 CoffeeSubs = CountSubObjectives(Day, EUOTaskType::CoffeeRefill);
				const int32 ReportTasks = CountTasks(Day, EUOTaskType::FileReports);
				if (!TestTrue(*(Context + TEXT(": printer sub-objectives fit the printers")), PrinterSubs <= Slots.Printers)
					|| !TestTrue(*(Context + TEXT(": coffee sub-objectives fit the coffee makers")), CoffeeSubs <= Slots.CoffeeMakers)
					|| !TestTrue(*(Context + TEXT(": report tasks fit the filing cabinets")), ReportTasks <= Slots.FilingCabinets))
				{
					return false;
				}
				TestEqual(*(Context + TEXT(": printer sub-objectives never share a printer")), CountDistinctStations(Day, EUOStationType::Printer), PrinterSubs);
				TestEqual(*(Context + TEXT(": coffee sub-objectives never share a machine")), CountDistinctStations(Day, EUOStationType::CoffeeMaker), CoffeeSubs);
				TestEqual(*(Context + TEXT(": report tasks never share a cabinet")), CountDistinctStations(Day, EUOStationType::FilingCabinet), ReportTasks);

				// The office is big enough, so difficulty is not compromised
				for (const FUOTaskInstance& Task : Day.Tasks)
				{
					const int32 Count = Task.SubObjectives.Num();
					if (!TestTrue(*FString::Printf(TEXT("%s: %d sub-objectives within [%d, %d]"), *Context, Count, MinSubs, MaxSubs), Count >= MinSubs && Count <= MaxSubs))
					{
						return false;
					}
				}

				for (const EUOTaskType Type : { EUOTaskType::PrinterMaintenance, EUOTaskType::CoffeeRefill, EUOTaskType::FileReports })
				{
					if (!TestTrue(*(Context + TEXT(": every task type is present")), CountTasks(Day, Type) > 0))
					{
						return false;
					}
				}

				if (!AllStationsAreRealAnchors(*this, Day, Slots, Context))
				{
					return false;
				}

				TSet<FName> StationIds;
				for (const FUOStationSpec& Spec : Day.Stations.Stations)
				{
					bool bAlreadyInSet = false;
					StationIds.Add(Spec.StationId, &bAlreadyInSet);
					TestFalse(*(Context + TEXT(": listed station ids are unique")), bAlreadyInSet);
				}
				TestEqual(*(Context + TEXT(": every inbox tray is listed as a source")), Day.Stations.InboxTrays, Slots.InboxTrays);
				TestEqual(*(Context + TEXT(": every supply closet is listed as a source")), Day.Stations.SupplyClosets, Slots.SupplyClosets);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorSlotsTightTest, "UnpaidOvertime.Core.TaskGenerator.Slots.TightOfficeShrinksSubObjectives", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorSlotsTightTest::RunTest(const FString& Parameters)
{
	using namespace UOTaskGeneratorSlotsTests;

	// 3 + 3 + 3 tasks of capacity with one machine each: 8 tasks only fit with fewer sub-objectives
	const FUOStationSlots Slots = MakeSlots(3, 3, 3, 1, 1);
	for (int32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
	{
		const FUOGeneratedDay Day = FUOTaskGenerator::GenerateForSlots(Seed, FUOMatchConfig::MaxTasksPerDay, 3.f, Slots);
		const FString Context = FString::Printf(TEXT("Seed %d"), Seed);
		if (!TestEqual(*(Context + TEXT(": exact task count")), Day.Tasks.Num(), FUOMatchConfig::MaxTasksPerDay))
		{
			return false;
		}
		TestTrue(*(Context + TEXT(": printers capped")), CountSubObjectives(Day, EUOTaskType::PrinterMaintenance) <= Slots.Printers);
		TestTrue(*(Context + TEXT(": coffee capped")), CountSubObjectives(Day, EUOTaskType::CoffeeRefill) <= Slots.CoffeeMakers);
		TestTrue(*(Context + TEXT(": cabinets capped")), CountTasks(Day, EUOTaskType::FileReports) <= Slots.FilingCabinets);
		for (const FUOTaskInstance& Task : Day.Tasks)
		{
			TestTrue(*(Context + TEXT(": every task keeps at least one sub-objective")), Task.SubObjectives.Num() >= 1);
		}
		if (!AllStationsAreRealAnchors(*this, Day, Slots, Context))
		{
			return false;
		}
	}

	// Not enough machines at all: still exactly TasksPerDay tasks, stations get reused but always exist
	const FUOStationSlots Tiny = MakeSlots(1, 1, 1, 1, 1);
	for (int32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
	{
		const FUOGeneratedDay Day = FUOTaskGenerator::GenerateForSlots(Seed, FUOMatchConfig::MaxTasksPerDay, 1.f, Tiny);
		const FString Context = FString::Printf(TEXT("Tiny office, seed %d"), Seed);
		if (!TestEqual(*(Context + TEXT(": exact task count")), Day.Tasks.Num(), FUOMatchConfig::MaxTasksPerDay)
			|| !AllStationsAreRealAnchors(*this, Day, Tiny, Context))
		{
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorSlotsMissingTypeTest, "UnpaidOvertime.Core.TaskGenerator.Slots.MissingStationTypesAreSkipped", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorSlotsMissingTypeTest::RunTest(const FString& Parameters)
{
	using namespace UOTaskGeneratorSlotsTests;

	for (int32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
	{
		// No coffee makers
		const FUOGeneratedDay NoCoffee = FUOTaskGenerator::GenerateForSlots(Seed, 6, 1.f, MakeSlots(8, 0, 8, 4, 2));
		TestEqual(TEXT("No coffee: exact task count"), NoCoffee.Tasks.Num(), 6);
		TestEqual(TEXT("No coffee: no coffee tasks"), CountTasks(NoCoffee, EUOTaskType::CoffeeRefill), 0);

		// Printers but no ink source: printers cannot be serviced
		const FUOGeneratedDay NoInk = FUOTaskGenerator::GenerateForSlots(Seed, 6, 1.f, MakeSlots(8, 6, 8, 4, 0));
		TestEqual(TEXT("No supply closet: exact task count"), NoInk.Tasks.Num(), 6);
		TestEqual(TEXT("No supply closet: no printer tasks"), CountTasks(NoInk, EUOTaskType::PrinterMaintenance), 0);

		// Cabinets but no inbox tray: no reports to file
		const FUOGeneratedDay NoInbox = FUOTaskGenerator::GenerateForSlots(Seed, 6, 1.f, MakeSlots(8, 6, 8, 0, 2));
		TestEqual(TEXT("No inbox tray: exact task count"), NoInbox.Tasks.Num(), 6);
		TestEqual(TEXT("No inbox tray: no report tasks"), CountTasks(NoInbox, EUOTaskType::FileReports), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskGeneratorSlotsDeterminismTest, "UnpaidOvertime.Core.TaskGenerator.Slots.DeterministicStationChoice", UO_CORE_TEST_FLAGS)
bool FUOTaskGeneratorSlotsDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace UOTaskGeneratorSlotsTests;

	const FUOStationSlots Slots = MakeOfficeSlots();
	TSet<FName> FirstPrinterBySeed;
	for (int32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
	{
		const FUOGeneratedDay A = FUOTaskGenerator::GenerateForSlots(Seed, 6, 1.f, Slots);
		const FUOGeneratedDay B = FUOTaskGenerator::GenerateForSlots(Seed, 6, 1.f, Slots);
		if (!TestEqual(TEXT("Same seed, same task count"), A.Tasks.Num(), B.Tasks.Num()))
		{
			return false;
		}
		for (int32 TaskIndex = 0; TaskIndex < A.Tasks.Num(); ++TaskIndex)
		{
			const FUOTaskInstance& TaskA = A.Tasks[TaskIndex];
			const FUOTaskInstance& TaskB = B.Tasks[TaskIndex];
			if (!TestEqual(TEXT("Same seed, same task types"), TaskA.Type, TaskB.Type)
				|| !TestEqual(TEXT("Same seed, same sub-objective count"), TaskA.SubObjectives.Num(), TaskB.SubObjectives.Num()))
			{
				return false;
			}
			for (int32 SubIndex = 0; SubIndex < TaskA.SubObjectives.Num(); ++SubIndex)
			{
				TestEqual(TEXT("Same seed, same stations"), TaskA.SubObjectives[SubIndex].StationId, TaskB.SubObjectives[SubIndex].StationId);
			}
			if (TaskA.Type == EUOTaskType::PrinterMaintenance && TaskA.SubObjectives.Num() > 0)
			{
				FirstPrinterBySeed.Add(TaskA.SubObjectives[0].StationId);
			}
		}
	}

	// The seed spreads work over the office instead of always picking "Printer 1"
	TestTrue(TEXT("Different seeds use different printers"), FirstPrinterBySeed.Num() >= 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
