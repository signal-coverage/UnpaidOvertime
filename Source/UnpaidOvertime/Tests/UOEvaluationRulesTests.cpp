// UNPAID: OVERTIME - direct firing, promotion shield, roster, match end and records tests.

#include "UOTestFlags.h"
#include "UOEvaluationRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOEvaluationRulesTestsPrivate
{
	FUORosterEntry Entry(int32 Id, int32 Stars, bool bHuman = false, int32 Shields = 0, bool bExempt = false)
	{
		FUORosterEntry Result;
		Result.EmployeeId = Id;
		Result.Stars = Stars;
		Result.bIsHuman = bHuman;
		Result.Shields = Shields;
		Result.bExemptFromDirectFiring = bExempt;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODirectFiringRuleTest, "UnpaidOvertime.Core.Evaluation.DirectFiringRule", UO_CORE_TEST_FLAGS)
bool FUODirectFiringRuleTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("0 stars without shield is fired"), FUOEvaluationRules::ResolveDirectFiring(0, 0, false), EUODirectFiringOutcome::Fired);
	TestEqual(TEXT("0 stars with shield uses the shield"), FUOEvaluationRules::ResolveDirectFiring(0, 1, false), EUODirectFiringOutcome::ShieldUsed);
	TestEqual(TEXT("0 stars with a stack uses a shield"), FUOEvaluationRules::ResolveDirectFiring(0, 3, false), EUODirectFiringOutcome::ShieldUsed);
	TestEqual(TEXT("Exempt employee (M4 killer) is never fired directly"), FUOEvaluationRules::ResolveDirectFiring(0, 0, true), EUODirectFiringOutcome::Exempt);
	TestEqual(TEXT("Exemption wins over the shield (shield kept)"), FUOEvaluationRules::ResolveDirectFiring(0, 2, true), EUODirectFiringOutcome::Exempt);
	for (int32 Stars = 1; Stars <= 3; ++Stars)
	{
		TestEqual(TEXT("1-3 stars are not directly fired"), FUOEvaluationRules::ResolveDirectFiring(Stars, 0, false), EUODirectFiringOutcome::NotApplicable);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUODirectFiringRosterTest, "UnpaidOvertime.Core.Evaluation.DirectFiringRoster", UO_CORE_TEST_FLAGS)
bool FUODirectFiringRosterTest::RunTest(const FString& Parameters)
{
	using namespace UOEvaluationRulesTestsPrivate;

	FUORoster Roster;
	Roster.Entries = { Entry(0, 0, true), Entry(1, 0, false, 2), Entry(2, 2), Entry(3, 0, false, 1, true), Entry(4, 0) };
	Roster.Entries[4].bAlive = false; // already gone on a previous day

	const TArray<FUOFiringDecision> Decisions = Roster.ApplyDirectFirings(3);
	TestEqual(TEXT("One decision per alive 0-star employee"), Decisions.Num(), 3);

	TestFalse(TEXT("Unshielded human is fired"), Roster.Find(0)->bAlive);
	TestEqual(TEXT("Fired human exit cause"), Roster.Find(0)->ExitCause, EUOExitCause::DirectFiring);
	TestEqual(TEXT("Fired human exit day"), Roster.Find(0)->ExitDay, 3);

	TestTrue(TEXT("Shielded employee stays"), Roster.Find(1)->bAlive);
	TestEqual(TEXT("A firing consumes exactly one shield of the stack"), Roster.Find(1)->Shields, 1);

	TestTrue(TEXT("2-star employee untouched"), Roster.Find(2)->bAlive);
	TestTrue(TEXT("Exempt employee stays"), Roster.Find(3)->bAlive);
	TestEqual(TEXT("Exemption does not consume shields"), Roster.Find(3)->Shields, 1);
	TestEqual(TEXT("Already-gone employee keeps its old exit"), Roster.Find(4)->ExitCause, EUOExitCause::None);

	TestEqual(TEXT("Alive count after firings"), Roster.CountAlive(), 3);
	TestFalse(TEXT("No human left"), Roster.IsAnyHumanAlive());

	// Each shield works once: the second one saves the next 0-star day, the third 0-star day fires
	Roster.Find(1)->Stars = 0;
	Roster.ApplyDirectFirings(4);
	TestTrue(TEXT("Second shield of the stack saves again"), Roster.Find(1)->bAlive);
	TestEqual(TEXT("Stack is empty now"), Roster.Find(1)->Shields, 0);
	Roster.ApplyDirectFirings(5);
	TestFalse(TEXT("0-star day with an empty stack fires"), Roster.Find(1)->bAlive);

	TestFalse(TEXT("Eliminating twice fails"), Roster.Eliminate(1, EUOExitCause::CutVote, 5));
	TestFalse(TEXT("Eliminating an unknown id fails"), Roster.Eliminate(99, EUOExitCause::CutVote, 5));
	Roster.Find(2)->Shields = 2;
	TestTrue(TEXT("Eliminate is raw (shields are checked by Expel)"), Roster.Eliminate(2, EUOExitCause::CutVote, 5));
	TestEqual(TEXT("Last standing is the exempt one"), Roster.GetLastStanding(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOPromotionRuleTest, "UnpaidOvertime.Core.Evaluation.PromotionShield", UO_CORE_TEST_FLAGS)
bool FUOPromotionRuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("3 stars may attempt"), FUOEvaluationRules::CanAttemptPromotion(3, 0));
	TestTrue(TEXT("3 stars holding one shield may still attempt"), FUOEvaluationRules::CanAttemptPromotion(3, 1));
	TestFalse(TEXT("3 stars at the shield cap is not offered"), FUOEvaluationRules::CanAttemptPromotion(3, 2));
	TestFalse(TEXT("2 stars may not attempt"), FUOEvaluationRules::CanAttemptPromotion(2, 0));
	TestFalse(TEXT("0 stars may not attempt"), FUOEvaluationRules::CanAttemptPromotion(0, 0));

	int32 Shields = 0;
	TestTrue(TEXT("Correct answer grants a shield"), FUOEvaluationRules::ResolvePromotion(true, Shields));
	TestEqual(TEXT("One shield held"), Shields, 1);
	TestTrue(TEXT("Shields stack"), FUOEvaluationRules::ResolvePromotion(true, Shields));
	TestEqual(TEXT("Two shields held"), Shields, 2);

	TestFalse(TEXT("Wrong answer gains nothing"), FUOEvaluationRules::ResolvePromotion(false, Shields));
	TestEqual(TEXT("Wrong answer never removes a held shield"), Shields, 2);

	// Design rule: at most 2 shields
	TestEqual(TEXT("Shield cap is 2"), FUOEvaluationRules::MaxShields, 2);
	TestFalse(TEXT("No gain beyond the cap"), FUOEvaluationRules::ResolvePromotion(true, Shields));
	TestEqual(TEXT("Stack stays at the cap"), Shields, 2);

	TestTrue(TEXT("1 star is a cut candidate"), FUOEvaluationRules::IsCutCandidate(1));
	TestFalse(TEXT("0 stars is not a cut candidate"), FUOEvaluationRules::IsCutCandidate(0));
	TestFalse(TEXT("2 stars is not a cut candidate"), FUOEvaluationRules::IsCutCandidate(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchEndTest, "UnpaidOvertime.Core.Evaluation.MatchEnd", UO_CORE_TEST_FLAGS)
bool FUOMatchEndTest::RunTest(const FString& Parameters)
{
	using namespace UOEvaluationRulesTestsPrivate;

	FUORoster Roster;
	Roster.Entries = { Entry(0, 2, true), Entry(1, 2), Entry(2, 2) };
	TestEqual(TEXT("Several employees with the human: continue"), FUOEvaluationRules::EvaluateMatchEnd(Roster), EUOMatchResult::None);

	Roster.Eliminate(1, EUOExitCause::CutVote, 2);
	Roster.Eliminate(2, EUOExitCause::DirectFiring, 3);
	TestEqual(TEXT("Human alone: victory"), FUOEvaluationRules::EvaluateMatchEnd(Roster), EUOMatchResult::Victory);

	FUORoster Lost;
	Lost.Entries = { Entry(0, 0, true), Entry(1, 2), Entry(2, 2) };
	Lost.Eliminate(0, EUOExitCause::DirectFiring, 1);
	TestEqual(TEXT("Human gone: game over even with several AIs left"), FUOEvaluationRules::EvaluateMatchEnd(Lost), EUOMatchResult::GameOver);

	// Rule 2: only killers left -> the saboteurs take over
	FUORoster Takeover;
	Takeover.Entries = { Entry(0, 2, true), Entry(1, 2), Entry(2, 2), Entry(3, 2) };
	Takeover.Find(0)->bKiller = true;
	Takeover.Find(3)->bKiller = true;
	Takeover.Eliminate(1, EUOExitCause::Convicted, 2);
	TestFalse(TEXT("A killer pair plus an employee: not over"), Takeover.AreOnlyKillersLeft());
	TestEqual(TEXT("Not over yet"), FUOEvaluationRules::EvaluateMatchEnd(Takeover), EUOMatchResult::None);
	Takeover.Eliminate(2, EUOExitCause::Dismissed, 3);
	TestTrue(TEXT("Only the two killers left"), Takeover.AreOnlyKillersLeft());
	TestEqual(TEXT("The human killer wins with its accomplice"), FUOEvaluationRules::EvaluateMatchEnd(Takeover), EUOMatchResult::Victory);

	FUORoster AIKillersWin;
	AIKillersWin.Entries = { Entry(0, 2, true), Entry(1, 2), Entry(2, 2) };
	AIKillersWin.Find(1)->bKiller = true;
	AIKillersWin.Eliminate(2, EUOExitCause::CutVote, 2);
	TestFalse(TEXT("Human employee + AI killer: not only killers"), AIKillersWin.AreOnlyKillersLeft());
	AIKillersWin.Eliminate(0, EUOExitCause::Convicted, 3);
	TestTrue(TEXT("Only the AI killer left"), AIKillersWin.AreOnlyKillersLeft());
	TestEqual(TEXT("The human employee is out: game over"), FUOEvaluationRules::EvaluateMatchEnd(AIKillersWin), EUOMatchResult::GameOver);

	FUORoster NoKillers;
	NoKillers.Entries = { Entry(0, 2, true), Entry(1, 2), Entry(2, 2) };
	NoKillers.Find(2)->bKiller = true;
	NoKillers.Eliminate(2, EUOExitCause::Convicted, 2);
	TestEqual(TEXT("The last killer removed: employees continue"), FUOEvaluationRules::EvaluateMatchEnd(NoKillers), EUOMatchResult::None);
	TestFalse(TEXT("Nobody alive is a killer"), NoKillers.AreOnlyKillersLeft());

	FUORoster Empty;
	Empty.Entries = { Entry(0, 0, true) };
	Empty.Eliminate(0, EUOExitCause::DirectFiring, 1);
	TestEqual(TEXT("Nobody left: game over"), FUOEvaluationRules::EvaluateMatchEnd(Empty), EUOMatchResult::GameOver);

	TestEqual(TEXT("Let go on day 1: 0 days survived"), FUOEvaluationRules::ComputeDaysSurvived(1, EUOMatchResult::GameOver), 0);
	TestEqual(TEXT("Let go on day 7: 6 days survived"), FUOEvaluationRules::ComputeDaysSurvived(7, EUOMatchResult::GameOver), 6);
	TestEqual(TEXT("Winning on day 7: 7 days survived"), FUOEvaluationRules::ComputeDaysSurvived(7, EUOMatchResult::Victory), 7);
	TestEqual(TEXT("Still employed after day 4: 4"), FUOEvaluationRules::ComputeDaysSurvived(4, EUOMatchResult::None), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOShieldsAnyExpulsionTest, "UnpaidOvertime.Core.Evaluation.ShieldsAndForgeries", UO_CORE_TEST_FLAGS)
bool FUOShieldsAnyExpulsionTest::RunTest(const FString& Parameters)
{
	using namespace UOEvaluationRulesTestsPrivate;

	// Displayed = real + fake, capped
	TestEqual(TEXT("Real and fake shields look the same"), FUOEvaluationRules::GetDisplayedShields(1, 1), 2);
	TestEqual(TEXT("Shown stack capped at 2"), FUOEvaluationRules::GetDisplayedShields(2, 1), 2);
	TestTrue(TEXT("Promotion offered with a shown stack below the cap"), FUOEvaluationRules::CanAttemptPromotion(3, FUOEvaluationRules::GetDisplayedShields(0, 1)));
	TestFalse(TEXT("A forged stack at the cap is full too"), FUOEvaluationRules::CanAttemptPromotion(3, FUOEvaluationRules::GetDisplayedShields(0, 2)));

	int32 Fake = 0;
	TestTrue(TEXT("A killer 3-star promotion forges a shield"), FUOEvaluationRules::GrantFakeShield(0, Fake));
	TestEqual(TEXT("One forged shield"), Fake, 1);
	TestTrue(TEXT("Forged shields stack"), FUOEvaluationRules::GrantFakeShield(0, Fake));
	TestFalse(TEXT("Not beyond the shown cap"), FUOEvaluationRules::GrantFakeShield(0, Fake));
	TestEqual(TEXT("Stays at 2"), Fake, 2);

	// Rule 5: a real shield absorbs any expulsion; a forged one never does
	int32 Real = 1;
	Fake = 1;
	TestEqual(TEXT("Real shield first"), FUOEvaluationRules::CheckShield(Real, Fake), EUOShieldCheck::ShieldUsed);
	TestTrue(TEXT("One real consumed, the forgery untouched"), Real == 0 && Fake == 1);
	TestEqual(TEXT("Then the forgery is exposed"), FUOEvaluationRules::CheckShield(Real, Fake), EUOShieldCheck::Forgery);
	TestEqual(TEXT("The forgery vanishes"), Fake, 0);
	TestEqual(TEXT("Nothing left"), FUOEvaluationRules::CheckShield(Real, Fake), EUOShieldCheck::None);

	FUORoster Roster;
	Roster.Entries = { Entry(0, 1, true, 1), Entry(1, 1), Entry(2, 1) };
	Roster.Find(2)->FakeShields = 1;
	Roster.Find(2)->bKiller = true;
	bool bExpelled = true;
	TestEqual(TEXT("Cut vote on a shielded employee: the shield goes"), Roster.Expel(0, EUOExitCause::CutVote, 2, bExpelled), EUOShieldCheck::ShieldUsed);
	TestFalse(TEXT("...and they stay"), bExpelled);
	TestTrue(TEXT("Still alive, no shield left"), Roster.Find(0)->bAlive && Roster.Find(0)->Shields == 0);
	TestEqual(TEXT("Unshielded: expelled"), Roster.Expel(1, EUOExitCause::Convicted, 2, bExpelled), EUOShieldCheck::None);
	TestTrue(TEXT("...gone"), bExpelled && !Roster.Find(1)->bAlive && Roster.Find(1)->ExitCause == EUOExitCause::Convicted);
	TestEqual(TEXT("A forged shield is exposed"), Roster.Expel(2, EUOExitCause::Convicted, 2, bExpelled), EUOShieldCheck::Forgery);
	TestTrue(TEXT("...and the killer still leaves"), bExpelled && !Roster.Find(2)->bAlive && Roster.Find(2)->FakeShields == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOProbationTest, "UnpaidOvertime.Core.Evaluation.Probation", UO_CORE_TEST_FLAGS)
bool FUOProbationTest::RunTest(const FString& Parameters)
{
	using namespace UOEvaluationRulesTestsPrivate;

	TestTrue(TEXT("1 real star fails probation"), FUOEvaluationRules::FailsProbation(1));
	TestTrue(TEXT("0 real stars fails probation"), FUOEvaluationRules::FailsProbation(0));
	TestFalse(TEXT("2 real stars passes"), FUOEvaluationRules::FailsProbation(2));
	TestFalse(TEXT("3 real stars passes"), FUOEvaluationRules::FailsProbation(3));

	FUORoster Roster;
	Roster.Entries = { Entry(0, 1, true), Entry(1, 2), Entry(2, 2), Entry(3, 1, false, 1), Entry(4, 2) };
	Roster.Find(0)->bOnProbation = true;
	Roster.Find(1)->bOnProbation = true;
	// A killer on probation: shows 2 (fake) but really earned 1 -> fired (the exemption only covers 0 stars)
	Roster.Find(2)->bOnProbation = true;
	Roster.Find(2)->bKiller = true;
	Roster.Find(2)->bExemptFromDirectFiring = true;
	Roster.Find(2)->RealStars = 1;
	Roster.Find(3)->bOnProbation = true;
	// Real 3, shows 2: passes
	Roster.Find(4)->bOnProbation = true;
	Roster.Find(4)->RealStars = 3;

	const TArray<FUOFiringDecision> Decisions = Roster.ApplyDirectFirings(4);
	auto Find = [&Decisions](int32 Id) { return Decisions.FindByPredicate([Id](const FUOFiringDecision& D) { return D.EmployeeId == Id; }); };
	TestTrue(TEXT("1 star on probation: fired"), Find(0) && Find(0)->Outcome == EUODirectFiringOutcome::Fired && Find(0)->bProbation && !Roster.Find(0)->bAlive);
	TestTrue(TEXT("2 stars on probation: kept"), !Find(1) && Roster.Find(1)->bAlive);
	TestTrue(TEXT("Killer with real 1 on probation: fired"), Find(2) && Find(2)->Outcome == EUODirectFiringOutcome::Fired && !Roster.Find(2)->bAlive);
	TestTrue(TEXT("A real shield absorbs a failed probation too"), Find(3) && Find(3)->Outcome == EUODirectFiringOutcome::ShieldUsed && Roster.Find(3)->bAlive && Roster.Find(3)->Shields == 0);
	TestTrue(TEXT("Real 3 passes"), !Find(4) && Roster.Find(4)->bAlive);
	for (const FUORosterEntry& Entry : Roster.Entries)
	{
		TestFalse(TEXT("Every probation ends with the review"), Entry.bOnProbation);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUORecordBookTest, "UnpaidOvertime.Core.Evaluation.Records", UO_CORE_TEST_FLAGS)
bool FUORecordBookTest::RunTest(const FString& Parameters)
{
	TArray<FUORecordEntry> Records;
	TestEqual(TEXT("No record yet is 0"), FUORecordBook::GetBestDays(Records, EUOAIDifficulty::Medium), 0);

	TestTrue(TEXT("First positive result is a record"), FUORecordBook::Submit(Records, EUOAIDifficulty::Medium, 3, false));
	TestFalse(TEXT("Worse result is not a record"), FUORecordBook::Submit(Records, EUOAIDifficulty::Medium, 2, false));
	TestFalse(TEXT("Equal result is not a record"), FUORecordBook::Submit(Records, EUOAIDifficulty::Medium, 3, false));
	TestTrue(TEXT("Better result is a record"), FUORecordBook::Submit(Records, EUOAIDifficulty::Medium, 5, true));
	TestEqual(TEXT("Best kept"), FUORecordBook::GetBestDays(Records, EUOAIDifficulty::Medium), 5);
	TestFalse(TEXT("0 days is never a record"), FUORecordBook::Submit(Records, EUOAIDifficulty::Hard, 0, false));
	TestEqual(TEXT("Records are per difficulty"), FUORecordBook::GetBestDays(Records, EUOAIDifficulty::Hard), 0);

	const FUORecordEntry* Medium = Records.FindByPredicate([](const FUORecordEntry& R) { return R.Difficulty == EUOAIDifficulty::Medium; });
	TestNotNull(TEXT("Medium entry exists"), Medium);
	if (Medium)
	{
		TestEqual(TEXT("Every match counted"), Medium->MatchesPlayed, 4);
		TestEqual(TEXT("Victories counted"), Medium->Victories, 1);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
