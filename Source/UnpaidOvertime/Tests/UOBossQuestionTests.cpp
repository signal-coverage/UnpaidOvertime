// UNPAID: OVERTIME - boss question tests: presence, fairness, options, determinism, tuning and the AI answer model.

#include "UOTestFlags.h"
#include "UOBossQuestions.h"
#include "UODayCalendar.h"
#include "UODayDifficulty.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOBossQuestionTestsPrivate
{
	const FName Lobby(TEXT("Lobby"));
	const FName Kitchen(TEXT("Kitchen"));
	const FName CopyRoom(TEXT("CopyRoom"));
	const FName Manager(TEXT("Manager"));

	void AddProps(FUOOfficeFacts& Facts, FName Room, EUOPropKind Kind, int32 Count, FName Color = NAME_None, FName Text = NAME_None, FName Shape = NAME_None)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FUOPropInstance Prop;
			Prop.Room = Room;
			Prop.Kind = Kind;
			Prop.Color = Color;
			Prop.Text = Text;
			Prop.Shape = Shape;
			Facts.Add(Prop);
		}
	}

	FUOBossEvent Event(float Time, const TCHAR* Clock, int32 Actor, const TCHAR* Name, FName Room, EUOBossEventKind Kind, EUOStationType Station, EUOItemType Item = EUOItemType::None)
	{
		FUOBossEvent Result;
		Result.Time = Time;
		Result.ClockTime = Clock;
		Result.ActorId = Actor;
		Result.ActorName = Name;
		Result.Room = Room;
		Result.Kind = Kind;
		Result.StationType = Station;
		Result.Item = Item;
		return Result;
	}

	/**
	 *  Employee 0 (the asked one) was in the lobby (0-10 s) and the break room (10-60 s).
	 *  Linda refilled coffee and picked up a report in the break room while 0 was there; Kevin worked in the copy
	 *  room and Stan picked up ink there, both unseen by 0. Employee 0 did the first completion of the day.
	 */
	FUOBossKnowledge MakeKnowledge()
	{
		FUOBossKnowledge Knowledge;
		Knowledge.CompanyName = TEXT("Acme Synergy");
		Knowledge.Employees = { { 0, TEXT("New hire") }, { 1, TEXT("Linda") }, { 2, TEXT("Kevin") }, { 3, TEXT("Stan") } };

		FUOOfficeFacts& Facts = Knowledge.Facts;
		AddProps(Facts, Lobby, EUOPropKind::Plant, 3);
		AddProps(Facts, Lobby, EUOPropKind::Sofa, 1, TEXT("Blue"));
		AddProps(Facts, Lobby, EUOPropKind::Sofa, 1, TEXT("Red"));
		AddProps(Facts, Lobby, EUOPropKind::Poster, 1, NAME_None, TEXT("SYNERGY"));
		AddProps(Facts, Lobby, EUOPropKind::Poster, 1, NAME_None, TEXT("TRUST"));
		AddProps(Facts, Lobby, EUOPropKind::Clock, 1, NAME_None, NAME_None, TEXT("Square"));
		AddProps(Facts, Kitchen, EUOPropKind::CoffeeMaker, 2, TEXT("Silver"));
		AddProps(Facts, Kitchen, EUOPropKind::Fridge, 2, TEXT("Silver"));
		AddProps(Facts, Kitchen, EUOPropKind::Microwave, 2);
		AddProps(Facts, Kitchen, EUOPropKind::VendingMachine, 1, TEXT("Black"));
		AddProps(Facts, Kitchen, EUOPropKind::Poster, 1, NAME_None, TEXT("SMILE"));
		AddProps(Facts, Kitchen, EUOPropKind::Mug, 5, TEXT("Red"));
		AddProps(Facts, Kitchen, EUOPropKind::Plant, 14);
		AddProps(Facts, CopyRoom, EUOPropKind::Printer, 5);
		AddProps(Facts, CopyRoom, EUOPropKind::Plant, 1);
		AddProps(Facts, CopyRoom, EUOPropKind::Poster, 1, NAME_None, TEXT("SMILE"));
		AddProps(Facts, Manager, EUOPropKind::Printer, 1);
		AddProps(Facts, Manager, EUOPropKind::Poster, 1, NAME_None, TEXT("OVERTIME"));

		for (float Time = 0.f; Time <= 10.f; Time += 0.5f)
		{
			Knowledge.Presence.Record(0, Lobby, Time);
			Knowledge.Presence.Record(1, Lobby, Time * 0.5f);
		}
		for (float Time = 10.5f; Time <= 60.f; Time += 0.5f)
		{
			Knowledge.Presence.Record(0, Kitchen, Time);
		}
		for (float Time = 5.5f; Time <= 100.f; Time += 0.5f)
		{
			Knowledge.Presence.Record(1, Kitchen, Time);
			Knowledge.Presence.Record(2, CopyRoom, Time);
			Knowledge.Presence.Record(3, CopyRoom, Time);
		}
		Knowledge.Presence.Record(0, NAME_None, 61.f);

		Knowledge.Events = {
			Event(20.f, TEXT("10:05"), 0, TEXT("New hire"), Kitchen, EUOBossEventKind::CompletedWork, EUOStationType::CoffeeMaker),
			Event(30.f, TEXT("10:40"), 1, TEXT("Linda"), Kitchen, EUOBossEventKind::CompletedWork, EUOStationType::CoffeeMaker),
			Event(35.f, TEXT("10:58"), 1, TEXT("Linda"), Kitchen, EUOBossEventKind::PickedUpItem, EUOStationType::InboxTray, EUOItemType::Report),
			Event(40.f, TEXT("11:15"), 2, TEXT("Kevin"), CopyRoom, EUOBossEventKind::CompletedWork, EUOStationType::Printer),
			Event(45.f, TEXT("11:30"), 2, TEXT("Kevin"), Kitchen, EUOBossEventKind::CompletedWork, EUOStationType::CoffeeMaker),
			Event(50.f, TEXT("11:52"), 3, TEXT("Stan"), CopyRoom, EUOBossEventKind::PickedUpItem, EUOStationType::SupplyCloset, EUOItemType::InkCartridge),
			Event(90.f, TEXT("16:20"), 1, TEXT("Linda"), Kitchen, EUOBossEventKind::CompletedWork, EUOStationType::CoffeeMaker)
		};
		return Knowledge;
	}

	bool OptionsAreDistinct(const FUOBossQuestion& Question)
	{
		TSet<FString> Seen;
		for (const FText& Option : Question.Options)
		{
			if (Option.IsEmpty() || Seen.Contains(Option.ToString()))
			{
				return false;
			}
			Seen.Add(Option.ToString());
		}
		return true;
	}

	FString Correct(const FUOBossQuestion& Question)
	{
		return Question.Options.IsValidIndex(Question.CorrectIndex) ? Question.Options[Question.CorrectIndex].ToString() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossPresenceTest, "UnpaidOvertime.Core.BossQuestions.PresenceLog", UO_CORE_TEST_FLAGS)
bool FUOBossPresenceTest::RunTest(const FString& Parameters)
{
	using namespace UOBossQuestionTestsPrivate;

	FUOPresenceLog Log;
	Log.Record(7, Lobby, 0.f);
	Log.Record(7, Lobby, 4.f);
	Log.Record(7, NAME_None, 5.f);
	Log.Record(7, Kitchen, 20.f);
	Log.Record(7, Kitchen, 30.f);
	Log.Record(7, Lobby, 50.f);

	TestTrue(TEXT("In the lobby at 2 s"), Log.WasIn(7, Lobby, 2.f));
	TestFalse(TEXT("Not in the lobby at 10 s (in the corridor)"), Log.WasIn(7, Lobby, 10.f));
	TestTrue(TEXT("In the break room at 25 s"), Log.WasIn(7, Kitchen, 25.f));
	TestFalse(TEXT("Not in the break room at 45 s"), Log.WasIn(7, Kitchen, 45.f));
	TestTrue(TEXT("Back in the lobby at 50 s"), Log.WasIn(7, Lobby, 50.f));
	TestTrue(TEXT("Visited the break room"), Log.HasVisited(7, Kitchen));
	TestFalse(TEXT("Never visited the copy room"), Log.HasVisited(7, CopyRoom));
	TestFalse(TEXT("Unknown employee visited nothing"), Log.HasVisited(8, Lobby));
	TestEqual(TEXT("Visited rooms listed once each"), Log.GetVisitedRooms(7).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossStaticFairnessTest, "UnpaidOvertime.Core.BossQuestions.StaticFactsOnlyFromVisitedRooms", UO_CORE_TEST_FLAGS)
bool FUOBossStaticFairnessTest::RunTest(const FString& Parameters)
{
	using namespace UOBossQuestionTestsPrivate;

	const FUOBossKnowledge Knowledge = MakeKnowledge();
	for (const EUOQuestionPurpose Purpose : { EUOQuestionPurpose::Defense, EUOQuestionPurpose::Tiebreak })
	{
		for (const int32 Day : { 1, 8 })
		{
			const FUOBossTuning Tuning = FUOBossQuestionGenerator::GetTuning(Purpose, Day);
			const TArray<FUOBossQuestion> Candidates = FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 0, Purpose, Tuning, 11);
			TestTrue(TEXT("There are static questions for a visitor of two rooms"), Candidates.Num() >= 4);

			TSet<EUOBossQuestionCategory> Categories;
			for (const FUOBossQuestion& Question : Candidates)
			{
				Categories.Add(Question.Category);
				const FString Prompt = Question.Prompt.ToString();
				TestTrue(TEXT("Valid"), Question.bValid);
				TestTrue(*(TEXT("Static category: ") + Prompt), Question.Category <= EUOBossQuestionCategory::WhichRoomHas);
				TestFalse(*(TEXT("Never about the unvisited copy room: ") + Prompt), Prompt.Contains(TEXT("Copy room")));
				TestFalse(*(TEXT("Never about the unvisited manager's office: ") + Prompt), Prompt.Contains(TEXT("Manager")));
				TestEqual(*(TEXT("Option count follows tuning: ") + Prompt), Question.Options.Num(), Tuning.NumOptions);
				TestTrue(*(TEXT("Options distinct: ") + Prompt), OptionsAreDistinct(Question));
				TestTrue(*(TEXT("Correct index valid: ") + Prompt), Question.Options.IsValidIndex(Question.CorrectIndex));
				TestFalse(*(TEXT("Rationale logged: ") + Prompt), Question.Rationale.IsEmpty());
				if (Question.Category == EUOBossQuestionCategory::WhichRoomHas)
				{
					const FString Answer = Correct(Question);
					TestTrue(*(TEXT("Room answer is a visited room: ") + Answer), Answer == TEXT("Lobby") || Answer == TEXT("Break room"));
				}
				if (Prompt.Contains(TEXT("plants")) && Prompt.Contains(TEXT("Lobby")))
				{
					TestEqual(TEXT("Lobby plant count is the real count"), Correct(Question), FString(TEXT("3")));
				}
				if (Question.Category == EUOBossQuestionCategory::PropColor)
				{
					TestFalse(TEXT("Ambiguous colors (two sofas) are never asked"), Prompt.Contains(TEXT("sofa")));
				}
			}
			TestTrue(TEXT("Several static categories available"), Categories.Num() >= 3);
		}
	}

	// Hard-to-count props only on later days
	const TArray<FUOBossQuestion> Early = FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 0, EUOQuestionPurpose::Defense,
		FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, 1), 3);
	TestFalse(TEXT("Day 1 never asks to count mugs"), Early.ContainsByPredicate([](const FUOBossQuestion& Q) { return Q.Prompt.ToString().Contains(TEXT("How many mugs")); }));

	// Too many to count from memory (14 plants in the break room): never asked, on any day
	for (const int32 Day : { 1, 20 })
	{
		const TArray<FUOBossQuestion> All = FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 0, EUOQuestionPurpose::Defense,
			FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, Day), 3);
		TestFalse(TEXT("No count question about 14 plants"), All.ContainsByPredicate([](const FUOBossQuestion& Q)
		{
			return Q.Prompt.ToString().Contains(TEXT("How many plants")) && Q.Prompt.ToString().Contains(TEXT("Break room"));
		}));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossEventFairnessTest, "UnpaidOvertime.Core.BossQuestions.EventsOnlyWhenWitnessed", UO_CORE_TEST_FLAGS)
bool FUOBossEventFairnessTest::RunTest(const FString& Parameters)
{
	using namespace UOBossQuestionTestsPrivate;

	const FUOBossKnowledge Knowledge = MakeKnowledge();
	TestTrue(TEXT("Linda's refill was witnessed by 0"), FUOBossQuestionGenerator::IsEventWitnessed(Knowledge, 0, Knowledge.Events[1]));
	TestFalse(TEXT("Kevin's copy room work was not"), FUOBossQuestionGenerator::IsEventWitnessed(Knowledge, 0, Knowledge.Events[3]));
	TestFalse(TEXT("Linda's 16:20 refill happened after 0 left"), FUOBossQuestionGenerator::IsEventWitnessed(Knowledge, 0, Knowledge.Events[6]));
	TestFalse(TEXT("Own actions are not 'witnessed' questions"), FUOBossQuestionGenerator::IsEventWitnessed(Knowledge, 0, Knowledge.Events[0]));

	const FUOBossTuning Tuning = FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Promotion, 2);
	const TArray<FUOBossQuestion> Candidates = FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 0, EUOQuestionPurpose::Promotion, Tuning, 5);
	TestTrue(TEXT("Promotion candidates exist"), Candidates.Num() >= 2);

	bool bSawWho = false;
	bool bSawPickup = false;
	for (const FUOBossQuestion& Question : Candidates)
	{
		const FString Prompt = Question.Prompt.ToString();
		const FString Answer = Correct(Question);
		TestTrue(*(TEXT("Event category: ") + Prompt), Question.Category >= EUOBossQuestionCategory::WhoDidWork && Question.Category <= EUOBossQuestionCategory::FirstCompletionRoom);
		TestTrue(*(TEXT("Options distinct: ") + Prompt), OptionsAreDistinct(Question));
		TestTrue(*(TEXT("Promotion has 3-4 options (4 when enough distinct names exist): ") + Prompt), Question.Options.Num() >= 3 && Question.Options.Num() <= 4);
		TestFalse(*(TEXT("Never about Stan's unseen pickup: ") + Prompt), Prompt.Contains(TEXT("Stan")));
		TestFalse(*(TEXT("Never answered by an unseen actor: ") + Prompt), Answer == TEXT("Stan"));
		TestFalse(*(TEXT("Never about the copy room: ") + Prompt), Prompt.Contains(TEXT("Copy room")));

		if (Question.Category == EUOBossQuestionCategory::WhoDidWork)
		{
			bSawWho = true;
			// Linda and Kevin both refilled coffee in the break room: the clock time disambiguates
			TestTrue(*(TEXT("Ambiguous event names the time: ") + Prompt), Prompt.Contains(TEXT("10:40")) || Prompt.Contains(TEXT("11:30")));
			if (Prompt.Contains(TEXT("10:40")))
			{
				TestEqual(TEXT("10:40 refill was Linda"), Answer, FString(TEXT("Linda")));
			}
			if (Prompt.Contains(TEXT("11:30")))
			{
				TestEqual(TEXT("11:30 refill was Kevin"), Answer, FString(TEXT("Kevin")));
			}
			TestFalse(TEXT("The asked employee is not an option"), Question.Options.ContainsByPredicate([](const FText& O) { return O.ToString() == TEXT("New hire"); }));
		}
		if (Question.Category == EUOBossQuestionCategory::WhatPickedUp)
		{
			bSawPickup = true;
			TestTrue(TEXT("Pickup question is about Linda"), Prompt.Contains(TEXT("Linda")));
			TestEqual(TEXT("Linda picked up a report"), Answer, FString(TEXT("Report")));
		}
		if (Question.Category == EUOBossQuestionCategory::FirstCompletionRoom)
		{
			TestEqual(TEXT("First completion was in the break room"), Answer, FString(TEXT("Break room")));
		}
	}
	TestTrue(TEXT("Who-did questions offered"), bSawWho);
	TestTrue(TEXT("Pickup questions offered"), bSawPickup);

	// Stan saw nothing in the break room: his promotion question falls back to the copy room's static facts
	const FUOBossQuestion StanQuestion = FUOBossQuestionGenerator::Generate(Knowledge, 3, EUOQuestionPurpose::Promotion, 2, 1);
	TestTrue(TEXT("Fallback is valid"), StanQuestion.bValid);
	TestTrue(TEXT("Stan witnessed Kevin's printer work, or falls back to static facts"), StanQuestion.Category != EUOBossQuestionCategory::CompanyName);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossGenerateTest, "UnpaidOvertime.Core.BossQuestions.GenerateDeterministicAndFallback", UO_CORE_TEST_FLAGS)
bool FUOBossGenerateTest::RunTest(const FString& Parameters)
{
	using namespace UOBossQuestionTestsPrivate;

	const FUOBossKnowledge Knowledge = MakeKnowledge();
	const FUOBossQuestion A = FUOBossQuestionGenerator::Generate(Knowledge, 0, EUOQuestionPurpose::Defense, 3, 42);
	const FUOBossQuestion B = FUOBossQuestionGenerator::Generate(Knowledge, 0, EUOQuestionPurpose::Defense, 3, 42);
	TestTrue(TEXT("Valid"), A.bValid);
	TestEqual(TEXT("Same seed, same prompt"), A.Prompt.ToString(), B.Prompt.ToString());
	TestEqual(TEXT("Same seed, same answer slot"), A.CorrectIndex, B.CorrectIndex);
	TestEqual(TEXT("Purpose recorded"), A.Purpose, EUOQuestionPurpose::Defense);
	TestTrue(TEXT("Time limit set"), A.TimeLimitSeconds > 0.f);

	TSet<FString> Prompts;
	TSet<int32> Slots;
	for (int32 Seed = 0; Seed < 30; ++Seed)
	{
		const FUOBossQuestion Q = FUOBossQuestionGenerator::Generate(Knowledge, 0, EUOQuestionPurpose::Defense, 3, Seed);
		Prompts.Add(Q.Prompt.ToString());
		Slots.Add(Q.CorrectIndex);
	}
	TestTrue(TEXT("Seeds vary the question"), Prompts.Num() >= 4);
	TestTrue(TEXT("The correct answer moves between slots"), Slots.Num() >= 2);

	// Someone who was never seen in any room still gets a fair question: the company name
	FUOBossKnowledge Ghost = Knowledge;
	Ghost.Presence.Reset();
	const FUOBossQuestion Fallback = FUOBossQuestionGenerator::Generate(Ghost, 0, EUOQuestionPurpose::Defense, 1, 9);
	TestTrue(TEXT("Fallback valid"), Fallback.bValid);
	TestEqual(TEXT("Fallback asks the company name"), Fallback.Category, EUOBossQuestionCategory::CompanyName);
	TestEqual(TEXT("Company name is the answer"), Correct(Fallback), FString(TEXT("Acme Synergy")));
	TestTrue(TEXT("Fallback options distinct"), OptionsAreDistinct(Fallback));

	// Promotion with nothing witnessed falls back to static facts of visited rooms
	FUOBossKnowledge Quiet = Knowledge;
	Quiet.Events.Reset();
	const FUOBossQuestion Static = FUOBossQuestionGenerator::Generate(Quiet, 0, EUOQuestionPurpose::Promotion, 2, 4);
	TestTrue(TEXT("Promotion fallback valid"), Static.bValid);
	TestTrue(TEXT("Promotion fallback uses static facts"), Static.Category <= EUOBossQuestionCategory::WhichRoomHas);
	TestEqual(TEXT("Promotion fallback keeps 4 options"), Static.Options.Num(), 4);
	TestTrue(TEXT("Rationale says it is a fallback"), Static.Rationale.Contains(TEXT("fallback")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossTuningTest, "UnpaidOvertime.Core.BossQuestions.TuningRisesWithDay", UO_CORE_TEST_FLAGS)
bool FUOBossTuningTest::RunTest(const FString& Parameters)
{
	for (const EUOQuestionPurpose Purpose : { EUOQuestionPurpose::Defense, EUOQuestionPurpose::Tiebreak, EUOQuestionPurpose::Promotion })
	{
		FUOBossTuning Previous = FUOBossQuestionGenerator::GetTuning(Purpose, 1);
		for (int32 Day = 2; Day <= 30; ++Day)
		{
			const FUOBossTuning Current = FUOBossQuestionGenerator::GetTuning(Purpose, Day);
			// The hardcore weekend is a spike above the curve (UODayDifficultyTests); the weekday curve is what never eases
			if (FUODayCalendar::IsOvertimeWeekend(Day))
			{
				TestTrue(TEXT("Weekend timer stays playable"), Current.TimeLimitSeconds >= FUODayDifficulty::GetHardcoreTuning().BossMinTimeLimitSeconds);
				continue;
			}
			TestTrue(TEXT("Options never decrease"), Current.NumOptions >= Previous.NumOptions);
			TestTrue(TEXT("Timer never grows"), Current.TimeLimitSeconds <= Previous.TimeLimitSeconds);
			TestTrue(TEXT("Hard facts stay allowed"), Current.bAllowHardFacts || !Previous.bAllowHardFacts);
			TestTrue(TEXT("Close distractors stay on"), Current.bCloseDistractors || !Previous.bCloseDistractors);
			TestTrue(TEXT("3 to 4 options"), Current.NumOptions >= 3 && Current.NumOptions <= 4);
			TestTrue(TEXT("Timer stays playable"), Current.TimeLimitSeconds >= 6.f);
			Previous = Current;
		}
	}

	const FUOBossTuning DefenseDay1 = FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, 1);
	const FUOBossTuning DefenseDay10 = FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, 10);
	TestEqual(TEXT("Day 1 defense has 3 options"), DefenseDay1.NumOptions, 3);
	TestEqual(TEXT("Day 10 defense has 4 options"), DefenseDay10.NumOptions, 4);
	TestTrue(TEXT("Day 10 defense is shorter"), DefenseDay10.TimeLimitSeconds < DefenseDay1.TimeLimitSeconds);
	TestTrue(TEXT("Day 10 allows hard facts"), DefenseDay10.bAllowHardFacts && !DefenseDay1.bAllowHardFacts);

	const FUOBossTuning PromotionDay1 = FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Promotion, 1);
	TestEqual(TEXT("Promotion always has 4 options"), PromotionDay1.NumOptions, 4);
	TestTrue(TEXT("Promotion is harder than defense on the same day"), PromotionDay1.TimeLimitSeconds < DefenseDay1.TimeLimitSeconds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBossWhoKnowsTest, "UnpaidOvertime.Core.BossQuestions.WhoKnowsTheAnswer", UO_CORE_TEST_FLAGS)
bool FUOBossWhoKnowsTest::RunTest(const FString& Parameters)
{
	using namespace UOBossQuestionTestsPrivate;

	const FUOBossKnowledge Knowledge = MakeKnowledge();
	int32 KitchenFacts = 0;
	for (const FUOBossQuestion& Question : FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 0, EUOQuestionPurpose::Defense, FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Defense, 1), 5))
	{
		TestTrue(TEXT("The asked employee always could know"), FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, Question, 0));
		TestFalse(TEXT("Static facts name their room"), Question.SubjectRoom.IsNone());
		if (Question.SubjectRoom == Kitchen)
		{
			++KitchenFacts;
			TestTrue(TEXT("Linda was in the kitchen too"), FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, Question, 1));
			TestFalse(TEXT("Kevin never entered the kitchen"), FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, Question, 2));
		}
	}
	TestTrue(TEXT("Some kitchen facts were asked"), KitchenFacts > 0);

	int32 Events = 0;
	for (const FUOBossQuestion& Question : FUOBossQuestionGenerator::GenerateCandidates(Knowledge, 0, EUOQuestionPurpose::Promotion, FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose::Promotion, 1), 5))
	{
		if (Question.SubjectTime < 0.f)
		{
			continue;
		}
		++Events;
		TestTrue(TEXT("Events: the asked employee saw it"), FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, Question, 0));
		TestFalse(TEXT("Stan was in the copy room the whole time"), Question.SubjectRoom == Kitchen && FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, Question, 3));
	}
	TestTrue(TEXT("Some event questions"), Events > 0);

	FUOBossQuestion Company;
	Company.Category = EUOBossQuestionCategory::CompanyName;
	TestTrue(TEXT("Everyone knows the company name"), FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, Company, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAIAnswerModelTest, "UnpaidOvertime.Core.BossQuestions.AIAnswerModel", UO_CORE_TEST_FLAGS)
bool FUOAIAnswerModelTest::RunTest(const FString& Parameters)
{
	const FUOAIDifficultyParams Easy = FUOAIDifficulty::Compute(EUOAIDifficulty::Easy, 1);
	const FUOAIDifficultyParams Hard = FUOAIDifficulty::Compute(EUOAIDifficulty::Hard, 10);

	for (const FUOAIDifficultyParams& Params : { Easy, Hard })
	{
		for (const EUOQuestionPurpose Purpose : { EUOQuestionPurpose::Defense, EUOQuestionPurpose::Promotion })
		{
			const float Chance = FUOAIAnswerModel::GetCorrectChance(Params, Purpose);
			TestTrue(TEXT("Chance is a probability"), Chance > 0.f && Chance < 1.f);
		}
		TestTrue(TEXT("Promotion is harder for the AI"), FUOAIAnswerModel::GetCorrectChance(Params, EUOQuestionPurpose::Promotion) < FUOAIAnswerModel::GetCorrectChance(Params, EUOQuestionPurpose::Defense));
		for (const float Random : { 0.f, 0.5f, 1.f })
		{
			const float Seconds = FUOAIAnswerModel::GetAnswerSeconds(Params, 12.f, Random);
			TestTrue(TEXT("Answers within the time limit"), Seconds > 0.f && Seconds < 12.f);
		}
	}
	TestTrue(TEXT("Skilled AIs remember better"), FUOAIAnswerModel::GetCorrectChance(Hard, EUOQuestionPurpose::Defense) > FUOAIAnswerModel::GetCorrectChance(Easy, EUOQuestionPurpose::Defense));
	TestTrue(TEXT("Skilled AIs answer faster"), FUOAIAnswerModel::GetAnswerSeconds(Hard, 12.f, 0.5f) < FUOAIAnswerModel::GetAnswerSeconds(Easy, 12.f, 0.5f));

	TestEqual(TEXT("Correct pick"), FUOAIAnswerModel::PickOption(true, 2, 4, 0.3f), 2);
	for (const float Random : { 0.f, 0.3f, 0.6f, 0.999f, 1.f })
	{
		const int32 Wrong = FUOAIAnswerModel::PickOption(false, 2, 4, Random);
		TestTrue(TEXT("Wrong pick is a different valid option"), Wrong != 2 && Wrong >= 0 && Wrong < 4);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
