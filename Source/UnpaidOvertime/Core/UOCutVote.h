// UNPAID: OVERTIME - cut vote (votación de recorte): candidates, public ballots, tally, tiebreak and AI voters.

#pragma once

#include "CoreMinimal.h"
#include "UOAIDifficulty.h"

struct FUORoster;

/** Public information about a candidate (what everyone saw on the performance review) */
struct FUOVoteCandidateInfo
{
	int32 EmployeeId = INDEX_NONE;
	int32 Stars = 1;
	float ContributionRatio = 0.f;
};

/** One public vote */
struct FUOBallot
{
	int32 VoterId = INDEX_NONE;
	int32 CandidateId = INDEX_NONE;
};

/** Result of counting the ballots */
struct FUOVoteTally
{
	/** (candidate id, votes) in candidate order, including candidates with no votes */
	TArray<TPair<int32, int32>> Counts;

	/** Candidates with the most votes (more than one = tie) */
	TArray<int32> Leaders;

	bool IsTie() const { return Leaders.Num() > 1; }
	int32 GetVotes(int32 CandidateId) const;
};

/** A tied candidate's tiebreak answer. Timeouts are wrong answers given at the time limit. */
struct FUOTiebreakAnswer
{
	int32 EmployeeId = INDEX_NONE;
	bool bCorrect = false;
	float AnswerSeconds = 0.f;
};

/**
 *  Rules (design doc 4.3):
 *   - candidates are the alive employees with exactly 1 visible star (minus successful defenses);
 *   - every alive employee votes for one candidate, publicly; nobody may vote for themselves
 *     (a sole candidate abstains);
 *   - most votes is cut; a tie goes to the tiebreak question;
 *   - at most one cut per day.
 */
struct FUOCutVote
{
	/** Alive employees showing exactly 1 star, in roster order */
	static TArray<int32> GetCandidates(const FUORoster& Roster);

	/** Candidates this voter may vote for (everyone but themselves) */
	static TArray<int32> GetEligibleTargets(const TArray<int32>& Candidates, int32 VoterId);

	static bool IsValidBallot(const TArray<int32>& Candidates, int32 VoterId, int32 CandidateId);

	/** Counts valid ballots (one per voter; later duplicates and invalid ballots are ignored) */
	static FUOVoteTally Tally(const TArray<int32>& Candidates, const TArray<FUOBallot>& Ballots);

	/**
	 *  Who leaves after a tiebreak: wrong answers lose before right ones; among wrong answers the first to fail
	 *  (fastest wrong answer) leaves; if everyone was right, the slowest leaves. An exact tie is a seeded pick.
	 *  Returns INDEX_NONE for an empty input.
	 */
	static int32 ResolveTiebreak(const TArray<FUOTiebreakAnswer>& Answers, int32 Seed);
};

/**
 *  AI voters use only visible information: the stars and the contribution shown on the review, plus a mild
 *  grudge against whoever voted for them in the previous cut vote, plus seeded noise that shrinks with skill.
 *  M4 hook: FUOAIDifficultyParams::Suspicion will weigh trace inconsistencies (killer hunting).
 */
struct FUOAIVoting
{
	static constexpr float PerformanceWeight = 1.f;
	static constexpr float GrudgeWeight = 0.35f;

	/** Higher = more likely to receive this voter's vote */
	static float ScoreCandidate(const FUOVoteCandidateInfo& Candidate, bool bHoldsGrudge, float Noise01, const FUOAIDifficultyParams& Params);

	/** The voter's pick among eligible candidates, INDEX_NONE if none (sole candidate voting) */
	static int32 ChooseVote(int32 VoterId, const TArray<FUOVoteCandidateInfo>& Candidates, const TArray<int32>& GrudgeIds,
		const FUOAIDifficultyParams& Params, int32 Seed);
};
