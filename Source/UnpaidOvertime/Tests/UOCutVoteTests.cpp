// UNPAID: OVERTIME - cut vote tests: candidates, ballots, tally, tiebreak and AI voters.

#include "UOTestFlags.h"
#include "UOCutVote.h"
#include "UOEvaluationRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOCutVoteTestsPrivate
{
	FUORosterEntry Entry(int32 Id, int32 Stars, bool bAlive = true)
	{
		FUORosterEntry Result;
		Result.EmployeeId = Id;
		Result.Stars = Stars;
		Result.bAlive = bAlive;
		return Result;
	}

	FUOBallot Ballot(int32 Voter, int32 Candidate)
	{
		FUOBallot Result;
		Result.VoterId = Voter;
		Result.CandidateId = Candidate;
		return Result;
	}

	FUOTiebreakAnswer Answer(int32 Id, bool bCorrect, float Seconds)
	{
		FUOTiebreakAnswer Result;
		Result.EmployeeId = Id;
		Result.bCorrect = bCorrect;
		Result.AnswerSeconds = Seconds;
		return Result;
	}

	FUOVoteCandidateInfo Candidate(int32 Id, float Ratio)
	{
		FUOVoteCandidateInfo Result;
		Result.EmployeeId = Id;
		Result.Stars = 1;
		Result.ContributionRatio = Ratio;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOCutVoteCandidatesTest, "UnpaidOvertime.Core.CutVote.Candidates", UO_CORE_TEST_FLAGS)
bool FUOCutVoteCandidatesTest::RunTest(const FString& Parameters)
{
	using namespace UOCutVoteTestsPrivate;

	FUORoster Roster;
	Roster.Entries = { Entry(0, 1), Entry(1, 2), Entry(2, 1), Entry(3, 0), Entry(4, 3), Entry(5, 1, false) };
	const TArray<int32> Candidates = FUOCutVote::GetCandidates(Roster);
	TestEqual(TEXT("Only alive 1-star employees are candidates"), Candidates, TArray<int32>({ 0, 2 }));

	TestEqual(TEXT("A candidate cannot vote for themselves"), FUOCutVote::GetEligibleTargets(Candidates, 0), TArray<int32>({ 2 }));
	TestEqual(TEXT("A non-candidate may vote for any candidate"), FUOCutVote::GetEligibleTargets(Candidates, 4), TArray<int32>({ 0, 2 }));
	TestEqual(TEXT("A sole candidate has nobody to vote for"), FUOCutVote::GetEligibleTargets(TArray<int32>({ 7 }), 7).Num(), 0);

	TestTrue(TEXT("Valid ballot"), FUOCutVote::IsValidBallot(Candidates, 4, 2));
	TestFalse(TEXT("Self vote is invalid"), FUOCutVote::IsValidBallot(Candidates, 2, 2));
	TestFalse(TEXT("Vote for a non-candidate is invalid"), FUOCutVote::IsValidBallot(Candidates, 4, 1));

	FUORoster NoOne;
	NoOne.Entries = { Entry(0, 2), Entry(1, 3) };
	TestEqual(TEXT("Nobody with 1 star: no candidates (no cut today)"), FUOCutVote::GetCandidates(NoOne).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOCutVoteTallyTest, "UnpaidOvertime.Core.CutVote.Tally", UO_CORE_TEST_FLAGS)
bool FUOCutVoteTallyTest::RunTest(const FString& Parameters)
{
	using namespace UOCutVoteTestsPrivate;

	const TArray<int32> Candidates = { 0, 2 };
	const FUOVoteTally Clear = FUOCutVote::Tally(Candidates, { Ballot(0, 2), Ballot(1, 2), Ballot(2, 0), Ballot(3, 2) });
	TestEqual(TEXT("Candidate 2 has 3 votes"), Clear.GetVotes(2), 3);
	TestEqual(TEXT("Candidate 0 has 1 vote"), Clear.GetVotes(0), 1);
	TestFalse(TEXT("Clear winner is not a tie"), Clear.IsTie());
	TestEqual(TEXT("Most voted leads"), Clear.Leaders, TArray<int32>({ 2 }));

	const FUOVoteTally Tie = FUOCutVote::Tally(Candidates, { Ballot(0, 2), Ballot(2, 0) });
	TestTrue(TEXT("1-1 is a tie"), Tie.IsTie());
	TestEqual(TEXT("Both tied candidates lead"), Tie.Leaders.Num(), 2);

	const FUOVoteTally Cleaned = FUOCutVote::Tally(Candidates, { Ballot(1, 0), Ballot(1, 2), Ballot(2, 2), Ballot(3, 9) });
	TestEqual(TEXT("Second ballot by the same voter ignored"), Cleaned.GetVotes(2), 0);
	TestEqual(TEXT("First ballot counts"), Cleaned.GetVotes(0), 1);
	TestEqual(TEXT("Leader after ignoring self vote and unknown candidate"), Cleaned.Leaders, TArray<int32>({ 0 }));

	const FUOVoteTally Empty = FUOCutVote::Tally(Candidates, {});
	TestEqual(TEXT("Every candidate listed with zero votes"), Empty.Counts.Num(), 2);
	TestTrue(TEXT("No votes at all: everyone tied"), Empty.IsTie());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOCutVoteTiebreakTest, "UnpaidOvertime.Core.CutVote.Tiebreak", UO_CORE_TEST_FLAGS)
bool FUOCutVoteTiebreakTest::RunTest(const FString& Parameters)
{
	using namespace UOCutVoteTestsPrivate;

	TestEqual(TEXT("The only wrong answer leaves"), FUOCutVote::ResolveTiebreak({ Answer(1, true, 9.f), Answer(2, false, 3.f), Answer(3, true, 2.f) }, 7), 2);
	TestEqual(TEXT("Among wrong answers, the first to fail leaves"), FUOCutVote::ResolveTiebreak({ Answer(1, false, 6.f), Answer(2, false, 2.5f) }, 7), 2);
	TestEqual(TEXT("A timeout fails last"), FUOCutVote::ResolveTiebreak({ Answer(1, false, 15.f), Answer(2, false, 4.f) }, 7), 2);
	TestEqual(TEXT("Everyone right: the slowest leaves"), FUOCutVote::ResolveTiebreak({ Answer(1, true, 3.f), Answer(2, true, 8.f), Answer(3, true, 5.f) }, 7), 2);
	TestEqual(TEXT("Empty input"), FUOCutVote::ResolveTiebreak({}, 7), static_cast<int32>(INDEX_NONE));

	// Exact tie: seeded, deterministic, and always one of the tied
	const TArray<FUOTiebreakAnswer> Exact = { Answer(4, true, 5.f), Answer(9, true, 5.f) };
	const int32 First = FUOCutVote::ResolveTiebreak(Exact, 1234);
	TestTrue(TEXT("Exact tie picks one of the tied"), First == 4 || First == 9);
	TestEqual(TEXT("Exact tie is deterministic"), FUOCutVote::ResolveTiebreak(Exact, 1234), First);
	TestEqual(TEXT("Input order does not matter"), FUOCutVote::ResolveTiebreak({ Exact[1], Exact[0] }, 1234), First);

	TSet<int32> Picks;
	for (int32 Seed = 0; Seed < 40; ++Seed)
	{
		Picks.Add(FUOCutVote::ResolveTiebreak(Exact, Seed));
	}
	TestEqual(TEXT("Different seeds can pick either"), Picks.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAIVotingTest, "UnpaidOvertime.Core.CutVote.AIVoters", UO_CORE_TEST_FLAGS)
bool FUOAIVotingTest::RunTest(const FString& Parameters)
{
	using namespace UOCutVoteTestsPrivate;

	FUOAIDifficultyParams Rational;
	Rational.VoteNoise = 0.f;

	const TArray<FUOVoteCandidateInfo> Candidates = { Candidate(1, 0.5f), Candidate(2, 0.1f), Candidate(3, 0.3f) };
	TestEqual(TEXT("Without noise the weakest contributor gets the vote"), FUOAIVoting::ChooseVote(5, Candidates, {}, Rational, 1), 2);
	TestEqual(TEXT("Never votes for themselves"), FUOAIVoting::ChooseVote(2, Candidates, {}, Rational, 1), 3);
	TestEqual(TEXT("A grudge tips a close call"), FUOAIVoting::ChooseVote(5, Candidates, { 3 }, Rational, 1), 3);
	TestEqual(TEXT("A grudge does not override a big gap"), FUOAIVoting::ChooseVote(5, Candidates, { 1 }, Rational, 1), 2);
	TestEqual(TEXT("Sole candidate voting: abstains"), FUOAIVoting::ChooseVote(1, { Candidate(1, 0.2f) }, {}, Rational, 1), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("No candidates: abstains"), FUOAIVoting::ChooseVote(1, {}, {}, Rational, 1), static_cast<int32>(INDEX_NONE));

	FUOAIDifficultyParams Noisy;
	Noisy.VoteNoise = 2.f;
	const int32 Pick = FUOAIVoting::ChooseVote(5, Candidates, {}, Noisy, 99);
	TestEqual(TEXT("Noisy voting is deterministic per seed"), FUOAIVoting::ChooseVote(5, Candidates, {}, Noisy, 99), Pick);
	TSet<int32> Picks;
	for (int32 Seed = 0; Seed < 60; ++Seed)
	{
		const int32 Vote = FUOAIVoting::ChooseVote(5, Candidates, {}, Noisy, Seed);
		TestTrue(TEXT("Noisy vote is always a candidate"), Vote == 1 || Vote == 2 || Vote == 3);
		Picks.Add(Vote);
	}
	TestTrue(TEXT("Noise spreads votes"), Picks.Num() >= 2);

	const float Weak = FUOAIVoting::ScoreCandidate(Candidate(1, 0.1f), false, 0.f, Rational);
	const float Strong = FUOAIVoting::ScoreCandidate(Candidate(1, 0.45f), false, 0.f, Rational);
	TestTrue(TEXT("Lower contribution scores higher"), Weak > Strong);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
