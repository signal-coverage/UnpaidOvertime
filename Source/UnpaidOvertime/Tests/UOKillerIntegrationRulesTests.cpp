// UNPAID: OVERTIME - killer hooks in existing rules: direct-firing exemption, corrupted files, moved props in boss questions.

#include "UOTestFlags.h"
#include "UOEvaluationRules.h"
#include "UOTaskBoard.h"
#include "UOBossQuestions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOKillerExemptionTest, "UnpaidOvertime.Core.Killer.DirectFiringExemption", UO_CORE_TEST_FLAGS)
bool FUOKillerExemptionTest::RunTest(const FString& Parameters)
{
	FUORoster Roster;
	FUORosterEntry& Killer = Roster.Entries.AddDefaulted_GetRef();
	Killer.EmployeeId = 0;
	Killer.Stars = 2;
	Killer.RealStars = 0;
	Killer.bExemptFromDirectFiring = true;
	FUORosterEntry& Slacker = Roster.Entries.AddDefaulted_GetRef();
	Slacker.EmployeeId = 1;
	Slacker.Stars = 0;
	FUORosterEntry& Worker = Roster.Entries.AddDefaulted_GetRef();
	Worker.EmployeeId = 2;
	Worker.Stars = 3;

	const TArray<FUOFiringDecision> Decisions = Roster.ApplyDirectFirings(1);
	const FUOFiringDecision* KillerDecision = Decisions.FindByPredicate([](const FUOFiringDecision& D) { return D.EmployeeId == 0; });
	TestTrue(TEXT("The killer's real 0 stars are judged and exempted"), KillerDecision && KillerDecision->Outcome == EUODirectFiringOutcome::Exempt);
	TestTrue(TEXT("The killer stays"), Roster.Find(0)->bAlive);
	TestFalse(TEXT("A real 0-star employee is fired"), Roster.Find(1)->bAlive);
	TestTrue(TEXT("Others are untouched"), Roster.Find(2)->bAlive);
	TestEqual(TEXT("Two 0-star decisions"), Decisions.Num(), 2);

	FUORoster Honest;
	FUORosterEntry& Fake = Honest.Entries.AddDefaulted_GetRef();
	Fake.EmployeeId = 5;
	Fake.Stars = 0;
	Fake.RealStars = 2;
	TestEqual(TEXT("Real stars above 0 are never fired"), Honest.ApplyDirectFirings(1).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTaskBoardUncompleteTest, "UnpaidOvertime.Core.Killer.CorruptedFile", UO_CORE_TEST_FLAGS)
bool FUOTaskBoardUncompleteTest::RunTest(const FString& Parameters)
{
	TArray<FUOTaskInstance> Tasks;
	FUOTaskInstance& Task = Tasks.AddDefaulted_GetRef();
	Task.Id = 0;
	Task.Type = EUOTaskType::FileReports;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FUOSubObjective& Sub = Task.SubObjectives.AddDefaulted_GetRef();
		Sub.Id = Index;
		Sub.TaskId = 0;
	}

	TestTrue(TEXT("Complete"), FUOTaskBoard::Complete(Tasks, 0, 7, 12.f));
	TestEqual(TEXT("Credited"), FUOTaskBoard::CountCompletedBy(Tasks, 7), 1);
	TestEqual(TEXT("Uncomplete returns who lost the credit"), FUOTaskBoard::Uncomplete(Tasks, 0), 7);
	TestEqual(TEXT("Credit is lost"), FUOTaskBoard::CountCompletedBy(Tasks, 7), 0);
	const FUOSubObjective* Sub = FUOTaskBoard::FindSubObjective(Tasks, 0);
	TestTrue(TEXT("Open again and flagged"), Sub && Sub->State == EUOSubObjectiveState::Open && Sub->bSabotaged && Sub->CompletedBy == INDEX_NONE);
	TestEqual(TEXT("Open work cannot be undone"), FUOTaskBoard::Uncomplete(Tasks, 1), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("It can be redone"), FUOTaskBoard::Complete(Tasks, 0, 8, 20.f));
	TestFalse(TEXT("Redone work is no longer flagged"), FUOTaskBoard::FindSubObjective(Tasks, 0)->bSabotaged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossMovedPropTest, "UnpaidOvertime.Core.Killer.MovedPropQuestions", UO_CORE_TEST_FLAGS)
bool FUOBossMovedPropTest::RunTest(const FString& Parameters)
{
	FUOBossKnowledge Knowledge;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FUOPropInstance Plant;
		Plant.Room = TEXT("Kitchen");
		Plant.Kind = EUOPropKind::Plant;
		Knowledge.Facts.Add(Plant);
	}
	FUOPropInstance Sofa;
	Sofa.Room = TEXT("Lobby");
	Sofa.Kind = EUOPropKind::Sofa;
	Knowledge.Facts.Add(Sofa);

	FUOPropMove Move;
	Move.Time = 100.f;
	Move.Kind = EUOPropKind::Plant;
	Move.FromRoom = TEXT("Kitchen");
	Move.ToRoom = TEXT("Lobby");
	Knowledge.Moves.Add(Move);

	// Employee 1: kitchen before the move only. Employee 2: kitchen and lobby after the move.
	Knowledge.Presence.Record(1, TEXT("Kitchen"), 10.f);
	Knowledge.Presence.Record(1, TEXT("Kitchen"), 50.f);
	Knowledge.Presence.Record(1, TEXT("OpenPlan"), 60.f);
	Knowledge.Presence.Record(2, TEXT("Kitchen"), 150.f);
	Knowledge.Presence.Record(2, TEXT("Kitchen"), 160.f);
	Knowledge.Presence.Record(2, TEXT("Lobby"), 170.f);
	Knowledge.Presence.Record(2, TEXT("Lobby"), 180.f);

	TSet<FString> Tricks;
	const FUOOfficeFacts Before = FUOBossQuestionGenerator::MakeEmployeeView(Knowledge, 1, &Tricks);
	TestEqual(TEXT("Seen before the move: 3 plants in the kitchen"), Before.Count(TEXT("Kitchen"), EUOPropKind::Plant), 3);
	TestEqual(TEXT("No trick for someone who did not see the change"), Tricks.Num(), 0);

	Tricks.Reset();
	const FUOOfficeFacts After = FUOBossQuestionGenerator::MakeEmployeeView(Knowledge, 2, &Tricks);
	TestEqual(TEXT("Seen after the move: 2 plants in the kitchen"), After.Count(TEXT("Kitchen"), EUOPropKind::Plant), 2);
	TestEqual(TEXT("Seen after the move: 1 plant in the lobby"), After.Count(TEXT("Lobby"), EUOPropKind::Plant), 1);
	TestTrue(TEXT("The change is a trick for this employee"), Tricks.Contains(TEXT("Kitchen/Plant")));

	// Count questions follow each employee's view; "which room has" skips moved kinds
	const FUOBossTuning Tuning = FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, 1);
	auto FindAnswer = [&](int32 EmployeeId) -> FString
	{
		for (const FUOBossQuestion& Question : FUOBossQuestionGenerator::GenerateCandidates(Knowledge, EmployeeId, EUOQuestionPurpose::Defense, Tuning, 3))
		{
			if (Question.Category == EUOBossQuestionCategory::PropCount && Question.Prompt.ToString().Contains(TEXT("plant")) && Question.Prompt.ToString().Contains(FUORooms::GetDisplayName(TEXT("Kitchen")).ToString()))
			{
				return Question.Options[Question.CorrectIndex].ToString();
			}
		}
		return FString();
	};
	TestEqual(TEXT("Employee 1 is asked about the 3 plants they saw"), FindAnswer(1), FString(TEXT("3")));
	TestEqual(TEXT("Employee 2 is asked about the 2 plants they saw"), FindAnswer(2), FString(TEXT("2")));
	for (const FUOBossQuestion& Question : FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 2, EUOQuestionPurpose::Defense, Tuning, 3))
	{
		TestFalse(TEXT("Which-room questions never ask about a moved kind"), Question.Category == EUOBossQuestionCategory::WhichRoomHas && Question.Prompt.ToString().Contains(TEXT("plant")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
