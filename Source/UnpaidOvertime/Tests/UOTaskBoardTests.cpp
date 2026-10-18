// UNPAID: OVERTIME - task board (claims, completion, release) tests.

#include "UOTestFlags.h"
#include "UOTaskBoard.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOTaskBoardTests
{
	/** Two tasks: task 0 has sub-objectives 0,1 at Printer_00/01 (ink), task 1 has 2,3 at Cabinet_00 (report) */
	TArray<FUOTaskInstance> MakeBoard()
	{
		TArray<FUOTaskInstance> Tasks;

		FUOTaskInstance Printers;
		Printers.Id = 0;
		Printers.Type = EUOTaskType::PrinterMaintenance;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FUOSubObjective Sub;
			Sub.Id = Index;
			Sub.TaskId = 0;
			Sub.StationId = FName(*FString::Printf(TEXT("Printer_%02d"), Index));
			Sub.bRequiresItem = true;
			Sub.ItemType = EUOItemType::InkCartridge;
			Printers.SubObjectives.Add(Sub);
		}
		Tasks.Add(Printers);

		FUOTaskInstance Reports;
		Reports.Id = 1;
		Reports.Type = EUOTaskType::FileReports;
		for (int32 Index = 2; Index < 4; ++Index)
		{
			FUOSubObjective Sub;
			Sub.Id = Index;
			Sub.TaskId = 1;
			Sub.StationId = TEXT("Cabinet_00");
			Sub.bRequiresItem = true;
			Sub.ItemType = EUOItemType::Report;
			Reports.SubObjectives.Add(Sub);
		}
		Tasks.Add(Reports);

		return Tasks;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskBoardClaimTest, "UnpaidOvertime.Core.TaskBoard.Claim", UO_CORE_TEST_FLAGS)
bool FUOTaskBoardClaimTest::RunTest(const FString& Parameters)
{
	TArray<FUOTaskInstance> Tasks = UOTaskBoardTests::MakeBoard();

	TestTrue(TEXT("Open sub-objective can be claimed"), FUOTaskBoard::TryClaim(Tasks, 0, 7));
	TestEqual(TEXT("Claim state is Claimed"), FUOTaskBoard::FindSubObjective(Tasks, 0)->State, EUOSubObjectiveState::Claimed);
	TestEqual(TEXT("Claimed by employee 7"), FUOTaskBoard::FindSubObjective(Tasks, 0)->ClaimedBy, 7);
	TestTrue(TEXT("Re-claiming by the same employee is idempotent"), FUOTaskBoard::TryClaim(Tasks, 0, 7));
	TestFalse(TEXT("Another employee cannot claim it"), FUOTaskBoard::TryClaim(Tasks, 0, 8));
	TestFalse(TEXT("Unknown id cannot be claimed"), FUOTaskBoard::TryClaim(Tasks, 99, 8));

	TestTrue(TEXT("Claim holder can release"), FUOTaskBoard::ReleaseClaim(Tasks, 0, 7));
	TestEqual(TEXT("Released sub-objective is Open"), FUOTaskBoard::FindSubObjective(Tasks, 0)->State, EUOSubObjectiveState::Open);
	TestEqual(TEXT("Released sub-objective has no claimer"), FUOTaskBoard::FindSubObjective(Tasks, 0)->ClaimedBy, (int32)INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskBoardCompleteTest, "UnpaidOvertime.Core.TaskBoard.Complete", UO_CORE_TEST_FLAGS)
bool FUOTaskBoardCompleteTest::RunTest(const FString& Parameters)
{
	TArray<FUOTaskInstance> Tasks = UOTaskBoardTests::MakeBoard();

	FUOTaskBoard::TryClaim(Tasks, 1, 3);
	TestTrue(TEXT("A different employee can complete a claimed sub-objective"), FUOTaskBoard::Complete(Tasks, 1, 0, 42.f));

	const FUOSubObjective* Sub = FUOTaskBoard::FindSubObjective(Tasks, 1);
	TestEqual(TEXT("State is Done"), Sub->State, EUOSubObjectiveState::Done);
	TestEqual(TEXT("Credit goes to the final interactor"), Sub->CompletedBy, 0);
	TestEqual(TEXT("Claim is released on completion"), Sub->ClaimedBy, (int32)INDEX_NONE);
	TestEqual(TEXT("Completion time recorded"), Sub->CompletedAtTime, 42.f);
	TestFalse(TEXT("A done sub-objective cannot be completed twice"), FUOTaskBoard::Complete(Tasks, 1, 3, 50.f));
	TestFalse(TEXT("A done sub-objective cannot be claimed"), FUOTaskBoard::TryClaim(Tasks, 1, 3));

	TestEqual(TEXT("Total sub-objectives"), FUOTaskBoard::CountTotal(Tasks), 4);
	TestEqual(TEXT("Done sub-objectives"), FUOTaskBoard::CountDone(Tasks), 1);
	TestEqual(TEXT("Completed by employee 0"), FUOTaskBoard::CountCompletedBy(Tasks, 0), 1);
	TestEqual(TEXT("Completed by employee 3"), FUOTaskBoard::CountCompletedBy(Tasks, 3), 0);
	TestFalse(TEXT("Not all done yet"), FUOTaskBoard::AreAllDone(Tasks));

	FUOTaskBoard::Complete(Tasks, 0, 1, 10.f);
	FUOTaskBoard::Complete(Tasks, 2, 1, 11.f);
	FUOTaskBoard::Complete(Tasks, 3, 2, 12.f);
	TestTrue(TEXT("Everything done"), FUOTaskBoard::AreAllDone(Tasks));
	TestTrue(TEXT("Task 0 complete"), Tasks[0].IsComplete());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskBoardReleaseEmployeeTest, "UnpaidOvertime.Core.TaskBoard.ReleaseEliminatedEmployee", UO_CORE_TEST_FLAGS)
bool FUOTaskBoardReleaseEmployeeTest::RunTest(const FString& Parameters)
{
	TArray<FUOTaskInstance> Tasks = UOTaskBoardTests::MakeBoard();

	FUOTaskBoard::TryClaim(Tasks, 0, 5);
	FUOTaskBoard::TryClaim(Tasks, 2, 5);
	FUOTaskBoard::TryClaim(Tasks, 3, 6);
	FUOTaskBoard::Complete(Tasks, 1, 5, 3.f);

	TestEqual(TEXT("Two pending claims released"), FUOTaskBoard::ReleaseAllClaims(Tasks, 5), 2);
	TestEqual(TEXT("Released back to Open"), FUOTaskBoard::FindSubObjective(Tasks, 0)->State, EUOSubObjectiveState::Open);
	TestEqual(TEXT("Released back to Open"), FUOTaskBoard::FindSubObjective(Tasks, 2)->State, EUOSubObjectiveState::Open);
	TestEqual(TEXT("Other employees keep their claims"), FUOTaskBoard::FindSubObjective(Tasks, 3)->ClaimedBy, 6);
	TestEqual(TEXT("Sub-objectives are not deleted"), FUOTaskBoard::CountTotal(Tasks), 4);
	TestEqual(TEXT("Completed work keeps its credit"), FUOTaskBoard::FindSubObjective(Tasks, 1)->CompletedBy, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskBoardStationQueryTest, "UnpaidOvertime.Core.TaskBoard.StationQuery", UO_CORE_TEST_FLAGS)
bool FUOTaskBoardStationQueryTest::RunTest(const FString& Parameters)
{
	TArray<FUOTaskInstance> Tasks = UOTaskBoardTests::MakeBoard();

	FUOTaskBoard::TryClaim(Tasks, 2, 9);
	const FUOSubObjective* ForOther = FUOTaskBoard::FindBestAtStation(Tasks, TEXT("Cabinet_00"), 1);
	TestNotNull(TEXT("Station has pending work"), ForOther);
	TestEqual(TEXT("Open sub-objectives are preferred over someone else's claim"), ForOther ? ForOther->Id : INDEX_NONE, 3);

	const FUOSubObjective* ForClaimer = FUOTaskBoard::FindBestAtStation(Tasks, TEXT("Cabinet_00"), 9);
	TestEqual(TEXT("Own claim is preferred"), ForClaimer ? ForClaimer->Id : INDEX_NONE, 2);

	FUOTaskBoard::Complete(Tasks, 3, 1, 1.f);
	const FUOSubObjective* OnlyClaimed = FUOTaskBoard::FindBestAtStation(Tasks, TEXT("Cabinet_00"), 1);
	TestEqual(TEXT("Claimed work can still be done by others"), OnlyClaimed ? OnlyClaimed->Id : INDEX_NONE, 2);

	TestEqual(TEXT("Pending reports"), FUOTaskBoard::CountPendingRequiringItem(Tasks, EUOItemType::Report), 1);
	TestEqual(TEXT("Pending ink"), FUOTaskBoard::CountPendingRequiringItem(Tasks, EUOItemType::InkCartridge), 2);
	TestNull(TEXT("Unknown station has no work"), FUOTaskBoard::FindBestAtStation(Tasks, TEXT("Nowhere"), 1));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
