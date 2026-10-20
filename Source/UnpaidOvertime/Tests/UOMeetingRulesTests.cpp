// UNPAID: OVERTIME - meeting tests: live public vote, tally, tie reduction, rock-paper-scissors duel (fair and
// rigged), and the AI statements and votes.

#include "UOTestFlags.h"
#include "UOMeetingRules.h"
#include "UOKillerRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOMeetingRulesTestsPrivate
{
	FUOMeetingBallot Ballot(int32 Voter, int32 Target)
	{
		FUOMeetingBallot Result;
		Result.VoterId = Voter;
		Result.TargetId = Target;
		return Result;
	}

	void AddItem(FUOSuspicionResult& Result, int32 Suspect, float Weight, int32 TraceId)
	{
		FUOSuspicionItem& Item = Result.Items.AddDefaulted_GetRef();
		Item.SuspectId = Suspect;
		Item.Weight = Weight;
		Item.TraceId = TraceId;
		Item.Source = EUOEvidenceSource::Terminal;
		Item.Reason = FString::Printf(TEXT("trace #%d"), TraceId);
	}

	FUOMeetingAIContext Context(int32 Self, bool bKiller = false)
	{
		FUOMeetingAIContext Result;
		Result.SelfId = Self;
		Result.bSelfKiller = bKiller;
		Result.AliveIds = { 0, 1, 2, 3, 4, 5 };
		Result.Suspicion = 0.5f;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMeetingVoteTest, "UnpaidOvertime.Core.Meeting.Vote", UO_CORE_TEST_FLAGS)
bool FUOMeetingVoteTest::RunTest(const FString& Parameters)
{
	using namespace UOMeetingRulesTestsPrivate;

	const TArray<int32> Alive = { 0, 1, 2, 3 };
	TestTrue(TEXT("Vote for an alive coworker"), FUOMeetingVote::IsValidBallot(Alive, 0, 2));
	TestTrue(TEXT("Abstain is always valid"), FUOMeetingVote::IsValidBallot(Alive, 0, INDEX_NONE));
	TestFalse(TEXT("Never for yourself"), FUOMeetingVote::IsValidBallot(Alive, 2, 2));
	TestFalse(TEXT("Never for someone gone"), FUOMeetingVote::IsValidBallot(Alive, 0, 7));
	TestFalse(TEXT("The gone do not vote"), FUOMeetingVote::IsValidBallot(Alive, 7, 1));

	TArray<FUOMeetingBallot> Ballots;
	TestTrue(TEXT("Cast"), FUOMeetingVote::Cast(Ballots, Alive, 0, 2));
	TestTrue(TEXT("Reassign"), FUOMeetingVote::Cast(Ballots, Alive, 0, 3));
	TestEqual(TEXT("One ballot per voter"), Ballots.Num(), 1);
	TestEqual(TEXT("The last selection counts"), Ballots[0].TargetId, 3);
	TestFalse(TEXT("An invalid reassignment is rejected"), FUOMeetingVote::Cast(Ballots, Alive, 0, 0));
	TestEqual(TEXT("...and changes nothing"), Ballots[0].TargetId, 3);
	TestTrue(TEXT("Back to abstain"), FUOMeetingVote::Cast(Ballots, Alive, 0, INDEX_NONE));
	TestEqual(TEXT("Abstained"), Ballots[0].TargetId, static_cast<int32>(INDEX_NONE));

	// Nobody votes -> nobody leaves
	TestFalse(TEXT("All abstain: nobody leaves"), FUOMeetingVote::Tally(Alive, Ballots).HasExpulsion());
	TestFalse(TEXT("No ballots: nobody leaves"), FUOMeetingVote::Tally(Alive, {}).HasExpulsion());

	// Simple majority
	{
		const FUOMeetingTally Tally = FUOMeetingVote::Tally(Alive, { Ballot(0, 3), Ballot(1, 3), Ballot(2, 1), Ballot(3, 1) });
		// Raw: 3 has 2, 1 has 2 -> tied leaders 1 and 3; their ballots (3->1, 1->3) are ignored -> 3 has 1 (from 0), 1 has 1 (from 2)
		TestTrue(TEXT("Tie stays a tie after ignoring the leaders' ballots"), Tally.IsTie() && Tally.Leaders.Contains(1) && Tally.Leaders.Contains(3));
		TestTrue(TEXT("The leaders' ballots are shown but ignored"), Tally.IgnoredVoters.Contains(1) && Tally.IgnoredVoters.Contains(3));
		TestEqual(TEXT("Counted votes for 3"), Tally.GetVotes(3), 1);
	}
	{
		// 3 is the clear leader; 3's own vote against 0 does not count
		const FUOMeetingTally Tally = FUOMeetingVote::Tally(Alive, { Ballot(0, 3), Ballot(1, 3), Ballot(2, 3), Ballot(3, 0) });
		TestEqual(TEXT("One leader"), Tally.Leaders, TArray<int32>({ 3 }));
		TestEqual(TEXT("Three votes for 3"), Tally.GetVotes(3), 3);
		TestEqual(TEXT("The accused's vote is ignored"), Tally.GetVotes(0), 0);
	}
	{
		// The leaders' ballots swing it: raw 2 (from 0, 3) vs 1 (from 2); 2 voted 1, so after ignoring 2's ballot... 2 still leads
		const FUOMeetingTally Tally = FUOMeetingVote::Tally(Alive, { Ballot(0, 2), Ballot(3, 2), Ballot(2, 1) });
		TestEqual(TEXT("Most votes leaves"), Tally.Leaders, TArray<int32>({ 2 }));
		TestEqual(TEXT("1 lost the vote of the most voted"), Tally.GetVotes(1), 0);
	}
	{
		// Two employees accusing each other and nobody else: ignoring both would leave nothing, so the raw tie stands
		const FUOMeetingTally Tally = FUOMeetingVote::Tally({ 0, 1 }, { Ballot(0, 1), Ballot(1, 0) });
		TestTrue(TEXT("Mutual accusation of the last two is a tie"), Tally.IsTie() && Tally.Leaders.Num() == 2);
	}
	{
		// Ballots from the gone or for oneself never count
		const FUOMeetingTally Tally = FUOMeetingVote::Tally(Alive, { Ballot(9, 1), Ballot(2, 2), Ballot(0, 1) });
		TestEqual(TEXT("Only valid ballots count"), Tally.GetVotes(1), 1);
		TestEqual(TEXT("Leader"), Tally.Leaders, TArray<int32>({ 1 }));
	}

	// Ties of 3+ go to a seeded 2-way duel
	const TArray<int32> ThreeWay = { 4, 1, 6 };
	for (int32 Seed = 0; Seed < 50; ++Seed)
	{
		const TArray<int32> Pair = FUOMeetingVote::ReduceTie(ThreeWay, Seed);
		TestTrue(TEXT("Two of the tied"), Pair.Num() == 2 && ThreeWay.Contains(Pair[0]) && ThreeWay.Contains(Pair[1]) && Pair[0] < Pair[1]);
		TestEqual(TEXT("Seeded"), Pair, FUOMeetingVote::ReduceTie(ThreeWay, Seed));
	}
	TestEqual(TEXT("A 2-way tie is kept as is"), FUOMeetingVote::ReduceTie({ 5, 2 }, 3), TArray<int32>({ 2, 5 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUORockPaperScissorsTest, "UnpaidOvertime.Core.Meeting.RockPaperScissors", UO_CORE_TEST_FLAGS)
bool FUORockPaperScissorsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Rock beats scissors"), FUORockPaperScissors::Beats(EUORPSChoice::Rock, EUORPSChoice::Scissors));
	TestTrue(TEXT("Scissors beat paper"), FUORockPaperScissors::Beats(EUORPSChoice::Scissors, EUORPSChoice::Paper));
	TestTrue(TEXT("Paper beats rock"), FUORockPaperScissors::Beats(EUORPSChoice::Paper, EUORPSChoice::Rock));
	TestFalse(TEXT("Rock does not beat paper"), FUORockPaperScissors::Beats(EUORPSChoice::Rock, EUORPSChoice::Paper));
	TestFalse(TEXT("A draw beats nothing"), FUORockPaperScissors::Beats(EUORPSChoice::Rock, EUORPSChoice::Rock));
	for (const EUORPSChoice Pick : { EUORPSChoice::Rock, EUORPSChoice::Paper, EUORPSChoice::Scissors })
	{
		TestTrue(TEXT("The counter wins"), FUORockPaperScissors::Beats(FUORockPaperScissors::Counter(Pick), Pick));
	}
	TestNotEqual(TEXT("Random picks are real picks"), FUORockPaperScissors::FromRandom(0.5f), EUORPSChoice::None);
	TestEqual(TEXT("Random low"), FUORockPaperScissors::FromRandom(0.f), EUORPSChoice::Rock);
	TestEqual(TEXT("Random high"), FUORockPaperScissors::FromRandom(0.999f), EUORPSChoice::Scissors);

	TestFalse(TEXT("Two employees: fair"), FUORockPaperScissors::IsRigged(false, false));
	TestFalse(TEXT("Two killers: fair"), FUORockPaperScissors::IsRigged(true, true));
	TestTrue(TEXT("Employee vs killer: rigged"), FUORockPaperScissors::IsRigged(false, true));

	// A best of 3
	FUORPSDuel Duel = FUORPSDuel::Make(10, false, 20, false);
	TestEqual(TEXT("Fair duel"), Duel.RiggedFor, static_cast<int32>(INDEX_NONE));
	EUORPSChoice A = EUORPSChoice::Rock;
	EUORPSChoice B = EUORPSChoice::Scissors;
	TestEqual(TEXT("A wins round 1"), Duel.PlayRound(A, B), 0);
	A = EUORPSChoice::Paper;
	B = EUORPSChoice::Paper;
	TestEqual(TEXT("Draw"), Duel.PlayRound(A, B), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("Not over after 1 win"), Duel.IsOver());
	A = EUORPSChoice::Paper;
	B = EUORPSChoice::Scissors;
	TestEqual(TEXT("B wins round 3"), Duel.PlayRound(A, B), 1);
	A = EUORPSChoice::Scissors;
	B = EUORPSChoice::Paper;
	TestEqual(TEXT("A wins round 4"), Duel.PlayRound(A, B), 0);
	TestTrue(TEXT("First to 2 wins"), Duel.IsOver());
	TestEqual(TEXT("Loser is B"), Duel.GetLoser(), 20);
	TestEqual(TEXT("Winner is A"), Duel.GetWinner(), 10);

	// Rigged: the killer always wins, whatever anyone picks
	for (int32 Run = 0; Run < 200; ++Run)
	{
		const bool bKillerIsA = Run % 2 == 0;
		FUORPSDuel Rigged = FUORPSDuel::Make(1, bKillerIsA, 2, !bKillerIsA);
		TestEqual(TEXT("Rigged for the killer side"), Rigged.RiggedFor, bKillerIsA ? 0 : 1);
		FRandomStream Rng(Run);
		while (!Rigged.IsOver())
		{
			EUORPSChoice PickA = FUORockPaperScissors::FromRandom(Rng.FRand());
			EUORPSChoice PickB = FUORockPaperScissors::FromRandom(Rng.FRand());
			const int32 Winner = Rigged.PlayRound(PickA, PickB);
			TestEqual(TEXT("Every round goes to the killer"), Winner, bKillerIsA ? 0 : 1);
			TestTrue(TEXT("The reveal shows a winning pick"), bKillerIsA ? FUORockPaperScissors::Beats(PickA, PickB) : FUORockPaperScissors::Beats(PickB, PickA));
		}
		TestEqual(TEXT("Two rounds"), Rigged.Rounds, 2);
		TestEqual(TEXT("The employee loses"), Rigged.GetLoser(), bKillerIsA ? 2 : 1);
	}

	// Fair duels are fair
	int32 LossesA = 0;
	for (int32 Run = 0; Run < 2000; ++Run)
	{
		FUORPSDuel Fair = FUORPSDuel::Make(1, true, 2, true);
		FRandomStream Rng(Run * 7 + 1);
		while (!Fair.IsOver())
		{
			EUORPSChoice PickA = FUORockPaperScissors::FromRandom(Rng.FRand());
			EUORPSChoice PickB = FUORockPaperScissors::FromRandom(Rng.FRand());
			Fair.PlayRound(PickA, PickB);
		}
		TestTrue(TEXT("A fair duel ends with a loser"), Fair.GetLoser() == 1 || Fair.GetLoser() == 2);
		LossesA += Fair.GetLoser() == 1 ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("Two killers: about even (A lost %d of 2000)"), LossesA), LossesA > 850 && LossesA < 1150);

	// Endless draws still end
	FUORPSDuel Draws = FUORPSDuel::Make(1, false, 2, false);
	for (int32 Round = 0; Round < FUOMeetingTuning::MaxDuelRounds; ++Round)
	{
		EUORPSChoice PickA = EUORPSChoice::Rock;
		EUORPSChoice PickB = EUORPSChoice::Rock;
		Draws.PlayRound(PickA, PickB);
	}
	TestTrue(TEXT("Capped"), Draws.IsOver());
	TestEqual(TEXT("All draws: the second side loses"), Draws.GetLoser(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMeetingAITest, "UnpaidOvertime.Core.Meeting.AI", UO_CORE_TEST_FLAGS)
bool FUOMeetingAITest::RunTest(const FString& Parameters)
{
	using namespace UOMeetingRulesTestsPrivate;

	const float Threshold = FUOKillerTuning::GetVoteThreshold(0.5f);

	// Innocent with strong evidence against 3
	FUOMeetingAIContext Sure = Context(1);
	AddItem(Sure.Suspects, 3, Threshold, 10);
	AddItem(Sure.Suspects, 3, 1.f, 11);
	AddItem(Sure.Suspects, 2, 0.5f, 12);
	const FUODebateStatement Accuse = FUOMeetingAI::MakeStatement(Sure);
	TestEqual(TEXT("Accuses its top suspect"), Accuse.Kind, EUOStatementKind::Accuse);
	TestEqual(TEXT("...who is 3"), Accuse.SuspectId, 3);
	TestEqual(TEXT("Cites its pinnable evidence"), Accuse.TraceIds, TArray<int32>({ 10, 11 }));
	TestFalse(TEXT("Explains itself"), Accuse.Reason.IsEmpty());
	FString Reason;
	TestEqual(TEXT("Votes for its top suspect"), FUOMeetingAI::ChooseVote(Sure, Reason), 3);

	// Weak evidence: unsure statement, abstains
	FUOMeetingAIContext Weak = Context(1);
	AddItem(Weak.Suspects, 4, Threshold * 0.6f, 20);
	TestEqual(TEXT("Weak evidence: unsure"), FUOMeetingAI::MakeStatement(Weak).Kind, EUOStatementKind::Unsure);
	TestEqual(TEXT("Weak evidence: abstains"), FUOMeetingAI::ChooseVote(Weak, Reason), static_cast<int32>(INDEX_NONE));

	// Nothing at all
	TestEqual(TEXT("Nothing to say"), FUOMeetingAI::MakeStatement(Context(1)).Kind, EUOStatementKind::Nothing);
	TestEqual(TEXT("Nothing: abstains"), FUOMeetingAI::ChooseVote(Context(1), Reason), static_cast<int32>(INDEX_NONE));

	// Killer: frames an innocent, never its accomplice or itself
	FUOMeetingAIContext Killer = Context(2, true);
	Killer.Accomplices = { 5 };
	AddItem(Killer.Suspects, 5, Threshold * 3.f, 30);
	AddItem(Killer.Suspects, 4, Threshold * 0.8f, 31);
	const FUODebateStatement Frame = FUOMeetingAI::MakeStatement(Killer);
	TestEqual(TEXT("The killer frames an innocent"), Frame.SuspectId, 4);
	TestNotEqual(TEXT("...never the accomplice"), Frame.SuspectId, 5);
	const int32 KillerVote = FUOMeetingAI::ChooseVote(Killer, Reason);
	TestTrue(TEXT("The killer never votes for its accomplice"), KillerVote != 5 && KillerVote != 2);

	// Bandwagon: the room's leader
	FUOMeetingTally Live;
	Live.Counts = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 3 }, { 5, 0 } };
	Live.Leaders = { 4 };
	TestEqual(TEXT("The killer blends in with the room's leader"), FUOMeetingAI::Reconsider(Killer, INDEX_NONE, Live, 0.99f, Reason), 4);
	FUOMeetingTally AgainstAccomplice = Live;
	AgainstAccomplice.Counts = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 3 } };
	AgainstAccomplice.Leaders = { 5 };
	TestNotEqual(TEXT("...but never against its accomplice"), FUOMeetingAI::Reconsider(Killer, INDEX_NONE, AgainstAccomplice, 0.f, Reason), 5);

	FUOMeetingAIContext Follower = Weak;
	TestEqual(TEXT("An unsure innocent with something on the leader may follow (low roll)"), FUOMeetingAI::Reconsider(Follower, INDEX_NONE, Live, 0.f, Reason), 4);
	TestEqual(TEXT("...or not (high roll)"), FUOMeetingAI::Reconsider(Follower, INDEX_NONE, Live, 0.999f, Reason), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A sure innocent keeps its vote"), FUOMeetingAI::Reconsider(Sure, 3, Live, 0.f, Reason), 3);
	FUOMeetingAIContext Clueless = Context(1);
	TestEqual(TEXT("Nothing against the leader: no blind following"), FUOMeetingAI::Reconsider(Clueless, INDEX_NONE, Live, 0.f, Reason), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("Less suspicion, more conformity"), FUOMeetingAI::GetConformity(0.1f) > FUOMeetingAI::GetConformity(0.9f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
