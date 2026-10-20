// UNPAID: OVERTIME - cut vote: candidates, public ballots, tally, tiebreak and AI voters.

#include "UOCutVote.h"
#include "UOEvaluationRules.h"
#include "Math/RandomStream.h"

namespace UOCutVotePrivate
{
	constexpr float TimeEpsilon = 1.e-3f;
}

int32 FUOVoteTally::GetVotes(int32 CandidateId) const
{
	for (const TPair<int32, int32>& Count : Counts)
	{
		if (Count.Key == CandidateId)
		{
			return Count.Value;
		}
	}
	return 0;
}

TArray<int32> FUOCutVote::GetCandidates(const FUORoster& Roster)
{
	TArray<int32> Candidates;
	for (const FUORosterEntry& Entry : Roster.Entries)
	{
		if (Entry.bAlive && FUOEvaluationRules::IsCutCandidate(Entry.Stars))
		{
			Candidates.Add(Entry.EmployeeId);
		}
	}
	return Candidates;
}

TArray<int32> FUOCutVote::GetEligibleTargets(const TArray<int32>& Candidates, int32 VoterId)
{
	TArray<int32> Targets;
	for (const int32 Candidate : Candidates)
	{
		if (Candidate != VoterId)
		{
			Targets.Add(Candidate);
		}
	}
	return Targets;
}

bool FUOCutVote::IsValidBallot(const TArray<int32>& Candidates, int32 VoterId, int32 CandidateId)
{
	return CandidateId != VoterId && Candidates.Contains(CandidateId);
}

FUOVoteTally FUOCutVote::Tally(const TArray<int32>& Candidates, const TArray<FUOBallot>& Ballots)
{
	FUOVoteTally Result;
	for (const int32 Candidate : Candidates)
	{
		Result.Counts.Emplace(Candidate, 0);
	}

	TSet<int32> Voted;
	for (const FUOBallot& Ballot : Ballots)
	{
		if (Voted.Contains(Ballot.VoterId))
		{
			continue;
		}
		Voted.Add(Ballot.VoterId);
		if (!IsValidBallot(Candidates, Ballot.VoterId, Ballot.CandidateId))
		{
			continue;
		}
		for (TPair<int32, int32>& Count : Result.Counts)
		{
			if (Count.Key == Ballot.CandidateId)
			{
				++Count.Value;
			}
		}
	}

	int32 Best = -1;
	for (const TPair<int32, int32>& Count : Result.Counts)
	{
		if (Count.Value > Best)
		{
			Best = Count.Value;
			Result.Leaders.Reset();
		}
		if (Count.Value == Best)
		{
			Result.Leaders.Add(Count.Key);
		}
	}
	return Result;
}

int32 FUOCutVote::ResolveTiebreak(const TArray<FUOTiebreakAnswer>& Answers, int32 Seed)
{
	using namespace UOCutVotePrivate;

	if (Answers.Num() == 0)
	{
		return INDEX_NONE;
	}

	const bool bAnyWrong = Answers.ContainsByPredicate([](const FUOTiebreakAnswer& Answer) { return !Answer.bCorrect; });

	// Wrong answers: the first to fail. All right: the slowest.
	float Key = bAnyWrong ? TNumericLimits<float>::Max() : -TNumericLimits<float>::Max();
	for (const FUOTiebreakAnswer& Answer : Answers)
	{
		if (bAnyWrong && !Answer.bCorrect)
		{
			Key = FMath::Min(Key, Answer.AnswerSeconds);
		}
		else if (!bAnyWrong)
		{
			Key = FMath::Max(Key, Answer.AnswerSeconds);
		}
	}

	TArray<int32> Losers;
	for (const FUOTiebreakAnswer& Answer : Answers)
	{
		if (Answer.bCorrect == !bAnyWrong && FMath::Abs(Answer.AnswerSeconds - Key) <= TimeEpsilon)
		{
			Losers.Add(Answer.EmployeeId);
		}
	}
	Losers.Sort();

	if (Losers.Num() == 1)
	{
		return Losers[0];
	}
	FRandomStream Rng(Seed);
	return Losers[Rng.RandRange(0, Losers.Num() - 1)];
}

float FUOAIVoting::ScoreCandidate(const FUOVoteCandidateInfo& Candidate, bool bHoldsGrudge, float Noise01, const FUOAIDifficultyParams& Params)
{
	const float Weakness = 1.f - FMath::Clamp(Candidate.ContributionRatio, 0.f, 1.f);
	return PerformanceWeight * Weakness + (bHoldsGrudge ? GrudgeWeight : 0.f) + FMath::Max(0.f, Params.VoteNoise) * Noise01;
}

int32 FUOAIVoting::ChooseVote(int32 VoterId, const TArray<FUOVoteCandidateInfo>& Candidates, const TArray<int32>& GrudgeIds,
	const FUOAIDifficultyParams& Params, int32 Seed)
{
	FRandomStream Rng(Seed ^ (VoterId * 7919));

	int32 Best = INDEX_NONE;
	float BestScore = -TNumericLimits<float>::Max();
	for (const FUOVoteCandidateInfo& Candidate : Candidates)
	{
		// Every candidate draws noise, eligible or not, so the stream does not depend on the voter's own candidacy
		const float Noise = Rng.FRand();
		if (Candidate.EmployeeId == VoterId)
		{
			continue;
		}
		const float Score = ScoreCandidate(Candidate, GrudgeIds.Contains(Candidate.EmployeeId), Noise, Params);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate.EmployeeId;
		}
	}
	return Best;
}
