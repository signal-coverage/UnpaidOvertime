// UNPAID: OVERTIME - the meeting (design doc 6.3, M4.1 rework): open debate, public vote, rock-paper-scissors duel
// on a tie, saving throw (shield or boss question + probation), and the AI side of it (statements and votes).

#pragma once

#include "CoreMinimal.h"
#include "UOKillerTypes.h"
#include "UOInvestigation.h"

/** Meeting pacing and limits (seconds) */
struct FUOMeetingTuning
{
	static constexpr float DebateSeconds = 60.f;
	static constexpr float VoteSeconds = 15.f;
	/** Each employee may put up to this many of their pinned lines on the table */
	static constexpr int32 MaxPresentedEvidence = 3;
	/** Best of 3: first to 2 round wins */
	static constexpr int32 DuelWinsNeeded = 2;
	/** Safety cap on drawn rounds (the loser is then the side with fewer wins, else the second side) */
	static constexpr int32 MaxDuelRounds = 9;
	/** The human's pick timer per round (a random pick when it runs out) */
	static constexpr float DuelPickSeconds = 6.f;
	/** How long a round's reveal stays up */
	static constexpr float DuelRevealSeconds = 2.2f;
	/** Last seconds of a countdown that tick */
	static constexpr int32 TickLastSeconds = 3;
};

/** One live ballot: TargetId INDEX_NONE = abstain (the default) */
struct FUOMeetingBallot
{
	int32 VoterId = INDEX_NONE;
	int32 TargetId = INDEX_NONE;
};

/** The counted vote */
struct FUOMeetingTally
{
	/** (employee id, counted votes) for every alive employee, in alive order */
	TArray<TPair<int32, int32>> Counts;
	/** Most-voted (more than one = tie); empty when nobody got a counted vote */
	TArray<int32> Leaders;
	/** Voters whose ballot was shown but not counted (they were among the most voted) */
	TArray<int32> IgnoredVoters;

	int32 GetVotes(int32 EmployeeId) const;
	bool IsTie() const { return Leaders.Num() > 1; }
	bool HasExpulsion() const { return Leaders.Num() > 0; }
};

/**
 *  Rules (rule 6): every alive employee has one public vote for any other alive employee, or abstains (default).
 *  Votes can be reassigned until the time runs out; the last selection counts. Most votes leaves; nobody when no
 *  vote counts. The ballots of the most-voted employees are shown but do not count (computed on the raw tally,
 *  then recounted without them); when that would leave no counted vote at all, the raw tally stands.
 */
struct FUOMeetingVote
{
	static bool IsValidBallot(const TArray<int32>& AliveIds, int32 VoterId, int32 TargetId);

	/** Casts or reassigns (replaces the voter's previous ballot). False when invalid (nothing changes). */
	static bool Cast(TArray<FUOMeetingBallot>& InOutBallots, const TArray<int32>& AliveIds, int32 VoterId, int32 TargetId);

	static FUOMeetingTally Tally(const TArray<int32>& AliveIds, const TArray<FUOMeetingBallot>& Ballots);

	/** A tie of 3 or more goes to a 2-way duel: a seeded pick of two of the tied (documented reduction). Sorted. */
	static TArray<int32> ReduceTie(const TArray<int32>& Leaders, int32 Seed);
};

struct FUORockPaperScissors
{
	static bool Beats(EUORPSChoice A, EUORPSChoice B);
	/** The pick that beats Against */
	static EUORPSChoice Counter(EUORPSChoice Against);
	static EUORPSChoice FromRandom(float Random01);
	/** One employee against one killer: the killer always wins */
	static bool IsRigged(bool bAKiller, bool bBKiller) { return bAKiller != bBKiller; }
};

/** A best-of-3 rock-paper-scissors duel between two tied employees; the loser is the one expelled */
struct FUORPSDuel
{
	int32 SideA = INDEX_NONE;
	int32 SideB = INDEX_NONE;
	/** 0 = A, 1 = B: the killer side when exactly one side is a killer (rigged), INDEX_NONE for a fair duel */
	int32 RiggedFor = INDEX_NONE;
	int32 WinsA = 0;
	int32 WinsB = 0;
	int32 Rounds = 0;

	static FUORPSDuel Make(int32 A, bool bAKiller, int32 B, bool bBKiller);

	/**
	 *  Plays one round. A rigged duel reads the employee's pick first and gives the killer its counter (the picks
	 *  are updated in place, so the reveal shows what "happened"). Returns 0 (A won), 1 (B won) or INDEX_NONE (draw).
	 */
	int32 PlayRound(EUORPSChoice& InOutPickA, EUORPSChoice& InOutPickB);

	bool IsOver() const;
	/** When over: the side that lost (INDEX_NONE before) */
	int32 GetLoser() const;
	int32 GetWinner() const;
};

/** What the AI says during the debate */
enum class EUOStatementKind : uint8
{
	/** "I think it was X" (with evidence) */
	Accuse,
	/** "X has been odd, not sure" */
	Unsure,
	/** "Nothing solid from me" */
	Nothing
};

struct FUODebateStatement
{
	int32 SpeakerId = INDEX_NONE;
	EUOStatementKind Kind = EUOStatementKind::Nothing;
	int32 SuspectId = INDEX_NONE;
	float Score = 0.f;
	/** Pinnable lines it puts on the table (up to FUOMeetingTuning::MaxPresentedEvidence) */
	TArray<int32> TraceIds;
	/** Strongest reason behind it (log) */
	FString Reason;
	EUOEvidenceSource ReasonSource = EUOEvidenceSource::Public;
	EUOSuspicionBasis Basis = EUOSuspicionBasis::Hunch;
	/** Room of the strongest piece of evidence, when it has one */
	FName Room;
};

/** Statement texts (the debate feed). Variety is picked by Seed; the same seed says the same thing. */
struct FUOMeetingText
{
	static FText DescribeStatement(const FUODebateStatement& Statement, TFunctionRef<FString(int32)> NameOf, int32 Seed);
	/** A short reply from someone who was just named */
	static FText DescribeDefense(int32 SpeakerId, int32 AccuserId, TFunctionRef<FString(int32)> NameOf, int32 Seed);
	static FText GetPickName(EUORPSChoice Pick);
};

/** Everything one AI uses for its meeting decisions (its own view only) */
struct FUOMeetingAIContext
{
	int32 SelfId = INDEX_NONE;
	bool bSelfKiller = false;
	/** Killers only: the fellow killers (never voted for, never accused) */
	TArray<int32> Accomplices;
	TArray<int32> AliveIds;
	/** Doc 9.2 "Sospecha" (0..1) */
	float Suspicion = 0.4f;
	FUOSuspicionResult Suspects;
};

/**
 *  AI meeting behaviour, shared by the game's AI controllers and the balance simulation:
 *   - innocent: names its top suspect (accuse at the vote threshold, unsure from half of it), votes for it at the
 *     threshold, else abstains; may follow the room's clear leader if it had something against them too;
 *   - killer: frames the most suspicious non-accomplice by its own visible evidence, votes for them, and blends in
 *     with the room's leader when that is not an accomplice. Never names or votes for itself or an accomplice.
 */
struct FUOMeetingAI
{
	static FUODebateStatement MakeStatement(const FUOMeetingAIContext& Context);

	/** First ballot: a target or INDEX_NONE (abstain) */
	static int32 ChooseVote(const FUOMeetingAIContext& Context, FString& OutReason);

	/** Later in the vote: keep CurrentVote or follow the live leader (bandwagon). Random01 rolls conformity. */
	static int32 Reconsider(const FUOMeetingAIContext& Context, int32 CurrentVote, const FUOMeetingTally& Live, float Random01, FString& OutReason);

	/** Chance of following the crowd (less Suspicion, more conformity) */
	static float GetConformity(float Suspicion);
};
