// UNPAID: OVERTIME - killer rules tests: role assignment (several killers), sabotage curve, kills, free fake rating.

#include "UOTestFlags.h"
#include "UOKillerRules.h"
#include "UOStarRating.h"
#include "UOMatchConfig.h"
#include "UOAIDifficulty.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOKillerRulesTestsPrivate
{
	TArray<FUORoleCandidate> Team(int32 Size)
	{
		TArray<FUORoleCandidate> Candidates;
		for (int32 Id = 0; Id < Size; ++Id)
		{
			FUORoleCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.EmployeeId = Id;
			Candidate.bIsHuman = Id == 0;
		}
		return Candidates;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUORoleAssignmentTest, "UnpaidOvertime.Core.Killer.RoleAssignment", UO_CORE_TEST_FLAGS)
bool FUORoleAssignmentTest::RunTest(const FString& Parameters)
{
	using namespace UOKillerRulesTestsPrivate;

	// Rule 1: floor(employees / 3), at least one
	TestEqual(TEXT("4 employees: 1 killer"), FUOKillerTuning::GetKillerCount(4), 1);
	TestEqual(TEXT("5 employees: 1 killer"), FUOKillerTuning::GetKillerCount(5), 1);
	TestEqual(TEXT("6 employees: 2 killers"), FUOKillerTuning::GetKillerCount(6), 2);
	TestEqual(TEXT("7 employees: 2 killers"), FUOKillerTuning::GetKillerCount(7), 2);
	TestEqual(TEXT("8 employees: 2 killers"), FUOKillerTuning::GetKillerCount(8), 2);
	TestEqual(TEXT("Tiny teams still have one"), FUOKillerTuning::GetKillerCount(2), 1);

	const TArray<FUORoleCandidate> Four = Team(4);
	TestEqual(TEXT("No candidates, no killer"), FUORoleRules::PickKillers({}, EUOKillerAssignment::Random, 1, 1).Num(), 0);
	TestEqual(TEXT("Same seed, same killers"), FUORoleRules::PickKillers(Four, EUOKillerAssignment::Random, 77, 1), FUORoleRules::PickKillers(Four, EUOKillerAssignment::Random, 77, 1));

	int32 HumanKiller = 0;
	TArray<int32> PerEmployee = { 0, 0, 0, 0 };
	constexpr int32 Matches = 4000;
	for (int32 Seed = 1; Seed <= Matches; ++Seed)
	{
		const TArray<int32> Killers = FUORoleRules::PickKillers(Four, EUOKillerAssignment::Random, Seed, 1);
		if (!TestEqual(TEXT("Random picks exactly one of four"), Killers.Num(), 1) || !TestTrue(TEXT("A team member"), Killers[0] >= 0 && Killers[0] < 4))
		{
			return false;
		}
		++PerEmployee[Killers[0]];
		HumanKiller += Killers[0] == 0 ? 1 : 0;

		const TArray<int32> AIKillers = FUORoleRules::PickKillers(Four, EUOKillerAssignment::AlwaysAI, Seed, 1);
		TestTrue(TEXT("AlwaysAI never picks the human"), AIKillers.Num() == 1 && AIKillers[0] >= 1);
		TestTrue(TEXT("AlwaysHuman picks the human"), FUORoleRules::PickKillers(Four, EUOKillerAssignment::AlwaysHuman, Seed, 1).Contains(0));
	}
	const float HumanShare = static_cast<float>(HumanKiller) / Matches;
	TestTrue(FString::Printf(TEXT("With 1 human + 3 AIs the human is the killer about 25%% of the time (got %.1f%%)"), HumanShare * 100.f), HumanShare > 0.21f && HumanShare < 0.29f);
	for (int32 Id = 0; Id < 4; ++Id)
	{
		TestTrue(FString::Printf(TEXT("Employee %d is picked sometimes"), Id), PerEmployee[Id] > Matches / 6);
	}

	// Six employees: two distinct killers, uniform (the human is one of them about a third of the time)
	const TArray<FUORoleCandidate> Six = Team(6);
	int32 HumanAmongSix = 0;
	TArray<int32> PickedSix = { 0, 0, 0, 0, 0, 0 };
	for (int32 Seed = 1; Seed <= Matches; ++Seed)
	{
		const TArray<int32> Killers = FUORoleRules::PickKillers(Six, EUOKillerAssignment::Random, Seed, FUOKillerTuning::GetKillerCount(6));
		if (!TestEqual(TEXT("Two killers among six"), Killers.Num(), 2) || !TestNotEqual(TEXT("Two different killers"), Killers[0], Killers[1]))
		{
			return false;
		}
		TestTrue(TEXT("Returned sorted"), Killers[0] < Killers[1]);
		HumanAmongSix += Killers.Contains(0) ? 1 : 0;
		++PickedSix[Killers[0]];
		++PickedSix[Killers[1]];

		const TArray<int32> WithHuman = FUORoleRules::PickKillers(Six, EUOKillerAssignment::AlwaysHuman, Seed, 2);
		TestTrue(TEXT("AlwaysHuman: the human plus one more"), WithHuman.Num() == 2 && WithHuman.Contains(0));
		const TArray<int32> NoHuman = FUORoleRules::PickKillers(Six, EUOKillerAssignment::AlwaysAI, Seed, 2);
		TestTrue(TEXT("AlwaysAI: two AIs"), NoHuman.Num() == 2 && !NoHuman.Contains(0));
	}
	const float HumanSixShare = static_cast<float>(HumanAmongSix) / Matches;
	TestTrue(FString::Printf(TEXT("6 employees, 2 killers: the human is one of them about 33%% of the time (got %.1f%%)"), HumanSixShare * 100.f), HumanSixShare > 0.29f && HumanSixShare < 0.38f);
	for (int32 Id = 0; Id < 6; ++Id)
	{
		TestTrue(FString::Printf(TEXT("Six: employee %d is picked about a third of the time"), Id), PickedSix[Id] > Matches / 4 && PickedSix[Id] < Matches * 2 / 5);
	}

	// Fallbacks
	TArray<FUORoleCandidate> OnlyAIs = Four;
	OnlyAIs.RemoveAt(0);
	const TArray<int32> Fallback = FUORoleRules::PickKillers(OnlyAIs, EUOKillerAssignment::AlwaysHuman, 3, 1);
	TestTrue(TEXT("AlwaysHuman without humans falls back to anyone"), Fallback.Num() == 1 && Fallback[0] >= 1);
	TestEqual(TEXT("Never more killers than candidates"), FUORoleRules::PickKillers(OnlyAIs, EUOKillerAssignment::Random, 3, 5).Num(), 3);

	FUOMatchConfig Config;
	TestEqual(TEXT("Default killer assignment is Random"), Config.KillerAssignment, EUOKillerAssignment::Random);
	TestFalse(TEXT("Killer can kill defaults to No (doc 2.2)"), Config.bKillerCanKill);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOSabotageCurveTest, "UnpaidOvertime.Core.Killer.SabotageCurve", UO_CORE_TEST_FLAGS)
bool FUOSabotageCurveTest::RunTest(const FString& Parameters)
{
	for (int32 TypeIndex = 0; TypeIndex < static_cast<int32>(EUOSabotageType::Count); ++TypeIndex)
	{
		const EUOSabotageType Type = static_cast<EUOSabotageType>(TypeIndex);
		TestTrue(TEXT("Cooldown grows with the day (doc 6.5: cheap early)"), FUOSabotageRules::GetCooldown(Type, 6, false) > FUOSabotageRules::GetCooldown(Type, 1, false));
		TestTrue(TEXT("Heat grows with the day"), FUOSabotageRules::GetHeatCost(Type, 6) > FUOSabotageRules::GetHeatCost(Type, 1));
		TestTrue(TEXT("Overtime shortens cooldowns (doc 8)"), FUOSabotageRules::GetCooldown(Type, 3, true) < FUOSabotageRules::GetCooldown(Type, 3, false));
		TestTrue(TEXT("A sabotage takes a hold"), FUOKillerTuning::GetHoldSeconds(Type) > 0.5f);
		TestTrue(TEXT("Heat is below the cap"), FUOSabotageRules::GetHeatCost(Type, 20) < FUOKillerTuning::MaxHeat);
	}
	TestTrue(TEXT("The day scale is capped"), FMath::IsNearlyEqual(FUOKillerTuning::GetDayCostScale(40), FUOKillerTuning::GetDayCostScale(60)));

	TestEqual(TEXT("Day 1 traces are anonymous"), FUOKillerTuning::GetTraceDetail(1), EUOTraceDetail::Anonymous);
	TestEqual(TEXT("Day 2 traces are anonymous"), FUOKillerTuning::GetTraceDetail(2), EUOTraceDetail::Anonymous);
	TestEqual(TEXT("Day 3 traces are partial"), FUOKillerTuning::GetTraceDetail(3), EUOTraceDetail::Partial);
	TestEqual(TEXT("Day 5 traces are named"), FUOKillerTuning::GetTraceDetail(5), EUOTraceDetail::Named);

	FUOKillerState State;
	State.ResetForDay();
	TestEqual(TEXT("Grace period at the start of the shift"), FUOSabotageRules::CanSabotage(State, EUOSabotageType::JamPrinter, 2.f, 1), EUOKillerBlock::TooEarly);
	TestEqual(TEXT("Ready after the grace period"), FUOSabotageRules::CanSabotage(State, EUOSabotageType::JamPrinter, 30.f, 1), EUOKillerBlock::None);

	FUOSabotageRules::Apply(State, EUOSabotageType::JamPrinter, 30.f, 1, false);
	TestTrue(TEXT("Heat was spent"), State.Heat > 0.f);
	TestEqual(TEXT("Sabotages are counted"), State.SabotagesToday, 1);
	TestEqual(TEXT("The same sabotage is cooling down"), FUOSabotageRules::CanSabotage(State, EUOSabotageType::JamPrinter, 31.f, 1), EUOKillerBlock::CoolingDown);
	TestTrue(TEXT("Remaining cooldown is reported"), FUOSabotageRules::GetRemainingCooldown(State, EUOSabotageType::JamPrinter, 31.f) > 0.f);
	TestEqual(TEXT("Other sabotage types are independent"), FUOSabotageRules::CanSabotage(State, EUOSabotageType::MoveObject, 31.f, 1), EUOKillerBlock::None);
	TestEqual(TEXT("Ready again after the cooldown"), FUOSabotageRules::CanSabotage(State, EUOSabotageType::JamPrinter, 30.f + FUOSabotageRules::GetCooldown(EUOSabotageType::JamPrinter, 1, false) + 0.1f, 1), EUOKillerBlock::None);

	State.Heat = FUOKillerTuning::MaxHeat - 1.f;
	TestEqual(TEXT("Too much heat blocks"), FUOSabotageRules::CanSabotage(State, EUOSabotageType::MoveObject, 200.f, 1), EUOKillerBlock::TooMuchHeat);
	State.Tick(30.f);
	TestTrue(TEXT("Heat cools down"), State.Heat < FUOKillerTuning::MaxHeat - 30.f);
	State.Tick(1000.f);
	TestEqual(TEXT("Heat never goes negative"), State.Heat, 0.f);

	State.KillsToday = 2;
	State.ResetForDay();
	TestEqual(TEXT("Kills reset every day"), State.KillsToday, 0);
	TestEqual(TEXT("Sabotages reset every day"), State.SabotagesToday, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOKillRulesTest, "UnpaidOvertime.Core.Killer.KillRules", UO_CORE_TEST_FLAGS)
bool FUOKillRulesTest::RunTest(const FString& Parameters)
{
	FUOKillerState State;
	State.ResetForDay();
	const float Cooldown = FUOKillerTuning::KillCooldownSeconds;
	TestEqual(TEXT("Kill cooldown is 120 s"), Cooldown, 120.f);
	TestEqual(TEXT("Overtime shortens it (x0.6)"), FUOKillerTuning::GetKillCooldown(true), Cooldown * 0.6f);
	TestEqual(TEXT("Normal cooldown"), FUOKillerTuning::GetKillCooldown(false), Cooldown);

	const float Ready = FUOKillerTuning::KillCooldownSeconds + 1.f;
	TestEqual(TEXT("Disabled by config"), FUOKillRules::CanKill(false, State, Ready, 0, 100.f, false), EUOKillerBlock::Disabled);
	TestEqual(TEXT("Not right at the start of the shift"), FUOKillRules::CanKill(true, State, 5.f, 0, 100.f, false), EUOKillerBlock::CoolingDown);
	TestEqual(TEXT("Witnesses block it"), FUOKillRules::CanKill(true, State, Ready, 1, 100.f, false), EUOKillerBlock::Witnesses);
	TestEqual(TEXT("Must be close"), FUOKillRules::CanKill(true, State, Ready, 0, FUOKillerTuning::KillRange + 50.f, false), EUOKillerBlock::OutOfRange);
	TestEqual(TEXT("Killers never kill each other"), FUOKillRules::CanKill(true, State, Ready, 0, 100.f, true), EUOKillerBlock::Accomplice);
	TestEqual(TEXT("Alone and close: allowed"), FUOKillRules::CanKill(true, State, Ready, 0, 100.f, false), EUOKillerBlock::None);

	FUOKillRules::Apply(State, Ready, false);
	TestEqual(TEXT("Kills are counted"), State.KillsToday, 1);
	TestEqual(TEXT("Cooling down right after"), FUOKillRules::CanKill(true, State, Ready + 10.f, 0, 100.f, false), EUOKillerBlock::CoolingDown);
	TestEqual(TEXT("No daily limit: ready again after the cooldown"), FUOKillRules::CanKill(true, State, Ready + Cooldown + 0.5f, 0, 100.f, false), EUOKillerBlock::None);
	FUOKillRules::Apply(State, Ready + Cooldown + 1.f, true);
	TestEqual(TEXT("A second kill the same day"), State.KillsToday, 2);
	TestEqual(TEXT("Overtime cooldown after an overtime kill"), FUOKillRules::CanKill(true, State, Ready + Cooldown + 1.f + Cooldown * 0.6f + 0.5f, 0, 100.f, false), EUOKillerBlock::None);

	// Every kill is easier to trace than the last
	TestEqual(TEXT("First kill on day 1: anonymous"), FUOKillerTuning::GetKillTraceDetail(1, 0), EUOTraceDetail::Anonymous);
	TestEqual(TEXT("Second kill on day 1: partial"), FUOKillerTuning::GetKillTraceDetail(1, 1), EUOTraceDetail::Partial);
	TestEqual(TEXT("Third kill: named"), FUOKillerTuning::GetKillTraceDetail(1, 2), EUOTraceDetail::Named);
	TestEqual(TEXT("Day 3 starts partial"), FUOKillerTuning::GetKillTraceDetail(3, 0), EUOTraceDetail::Partial);
	TestEqual(TEXT("Capped at named"), FUOKillerTuning::GetKillTraceDetail(6, 4), EUOTraceDetail::Named);
	TestTrue(TEXT("The public note gets likelier with every kill"), FUOKillerTuning::GetLastSeenNoteChance(2) > FUOKillerTuning::GetLastSeenNoteChance(1));
	TestTrue(TEXT("The first kill may leave no note"), FUOKillerTuning::GetLastSeenNoteChance(1) > 0.f && FUOKillerTuning::GetLastSeenNoteChance(1) < 1.f);
	TestEqual(TEXT("By the third kill there is always a note"), FUOKillerTuning::GetLastSeenNoteChance(3), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOFakeRatingTest, "UnpaidOvertime.Core.Killer.FakeRating", UO_CORE_TEST_FLAGS)
bool FUOFakeRatingTest::RunTest(const FString& Parameters)
{
	// Rule 4: 1, 2 or 3 freely, never 0, whatever the real work
	for (int32 Real = 0; Real <= 3; ++Real)
	{
		const FUOFakeRatingOffer Offer = FUOFakeRating::MakeOffer(Real);
		TestEqual(TEXT("Always 1-3"), Offer.Allowed, TArray<int32>({ 1, 2, 3 }));
		TestFalse(TEXT("0 is never allowed"), Offer.IsAllowed(0));
		TestEqual(TEXT("Real stars are kept (hidden)"), Offer.RealStars, Real);
		TestEqual(TEXT("Default is the real value, never below 1"), Offer.DefaultStars, FMath::Clamp(Real, 1, 3));
	}
	TestTrue(TEXT("Shown 1: mandatory defense"), FUOFakeRating::IsMandatoryDefense(1));
	TestFalse(TEXT("Shown 2: nothing special"), FUOFakeRating::IsMandatoryDefense(2) || FUOFakeRating::GrantsFakeShield(2));
	TestTrue(TEXT("Shown 3: fake shield"), FUOFakeRating::GrantsFakeShield(3));

	TestEqual(TEXT("Displayed done = real when it matches"), FUOFakeRating::GetDisplayedCompleted(2, 2, 16, 4), 2);
	TestEqual(TEXT("Displayed done for a fake 3 is the smallest 3-star count"), FUOFakeRating::GetDisplayedCompleted(3, 2, 16, 4), 4);
	TestEqual(TEXT("Displayed done for a fake 1 without work is 1"), FUOFakeRating::GetDisplayedCompleted(1, 0, 16, 4), 1);

	// AI choice: always allowed; mostly 2; 1 and 3 both happen
	const FUOFakeRatingOffer Offer = FUOFakeRating::MakeOffer(2);
	TMap<int32, int32> Picks;
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		const int32 Pick = FUOFakeRating::ChooseForAI(Offer, 0.4f, (Index + 0.5f) / 1000.f);
		TestTrue(TEXT("AI picks an allowed value"), Offer.IsAllowed(Pick));
		++Picks.FindOrAdd(Pick);
	}
	TestTrue(TEXT("2 stars is the most common"), Picks.FindRef(2) > Picks.FindRef(1) && Picks.FindRef(2) > Picks.FindRef(3));
	TestTrue(TEXT("1 star happens"), Picks.FindRef(1) > 30);
	TestTrue(TEXT("3 stars happens"), Picks.FindRef(3) > 30);

	// Cunning favours the honest value
	const FUOFakeRatingOffer ThreeReal = FUOFakeRating::MakeOffer(3);
	int32 HonestSloppy = 0;
	int32 HonestCunning = 0;
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		HonestSloppy += FUOFakeRating::ChooseForAI(ThreeReal, 0.f, (Index + 0.5f) / 1000.f) == 3 ? 1 : 0;
		HonestCunning += FUOFakeRating::ChooseForAI(ThreeReal, 1.f, (Index + 0.5f) / 1000.f) == 3 ? 1 : 0;
	}
	TestTrue(TEXT("A cunning killer shows its honest value more often"), HonestCunning > HonestSloppy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAICunningTest, "UnpaidOvertime.Core.Killer.AIParams", UO_CORE_TEST_FLAGS)
bool FUOAICunningTest::RunTest(const FString& Parameters)
{
	const FUOAIDifficultyParams EasyDay1 = FUOAIDifficulty::Compute(EUOAIDifficulty::Easy, 1);
	const FUOAIDifficultyParams HardDay6 = FUOAIDifficulty::Compute(EUOAIDifficulty::Hard, 6);
	TestTrue(TEXT("Cunning grows with skill"), HardDay6.Cunning > EasyDay1.Cunning);
	TestTrue(TEXT("Reaction grows with skill"), HardDay6.Reaction > EasyDay1.Reaction);
	TestTrue(TEXT("Suspicion grows with skill"), HardDay6.Suspicion > EasyDay1.Suspicion);
	TestTrue(TEXT("Params stay in 0..1"), HardDay6.Cunning <= 1.f && HardDay6.Reaction <= 1.f && EasyDay1.Cunning >= 0.f);

	TestTrue(TEXT("More suspicion lowers the vote threshold"), FUOKillerTuning::GetVoteThreshold(0.9f) < FUOKillerTuning::GetVoteThreshold(0.1f));
	TestTrue(TEXT("Suspicious AIs read terminals more"), FUOKillerTuning::GetTerminalVisitsPerMinute(0.9f) > FUOKillerTuning::GetTerminalVisitsPerMinute(0.1f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
