// UNPAID: OVERTIME - end-of-day rules: roster bookkeeping, direct firings, promotion shields, match end, records.

#pragma once

#include "CoreMinimal.h"
#include "UOEvaluationTypes.h"

/** One employee as the evaluation rules see it (built from the player states, written back after) */
struct FUORosterEntry
{
	int32 EmployeeId = INDEX_NONE;
	bool bIsHuman = false;
	bool bAlive = true;
	/** Real promotion shields held (they stack): each one cancels one expulsion (firing, cut vote or meeting) */
	int32 Shields = 0;
	/** Forged shields (a killer's fake 3-star promotion): shown like real ones, never protect */
	int32 FakeShields = 0;
	/** Never fired directly for 0 stars (the killers show a fake rating and are exempt) */
	bool bExemptFromDirectFiring = false;
	/** Server only: a saboteur */
	bool bKiller = false;
	/** Saved at a meeting by a boss question: needs 2+ real stars at the next review or is fired then */
	bool bOnProbation = false;
	/** Stars everyone sees on the review (the killer's are fake) */
	int32 Stars = 0;
	/** Real stars when they differ from the shown ones (the killer: always 0), INDEX_NONE = same as Stars */
	int32 RealStars = INDEX_NONE;
	/** Completed / fair share, as shown on the review */
	float ContributionRatio = 0.f;
	EUOExitCause ExitCause = EUOExitCause::None;
	/** Day the employee left, 0 while still employed */
	int32 ExitDay = 0;
};

/** What happened to one 0-star (or failed-probation) employee */
struct FUOFiringDecision
{
	int32 EmployeeId = INDEX_NONE;
	EUODirectFiringOutcome Outcome = EUODirectFiringOutcome::NotApplicable;
	/** The firing comes from a failed probation (fewer than 2 real stars) */
	bool bProbation = false;
	/** A forged shield was exposed on the way out */
	bool bForgeryExposed = false;
};

/** The match roster. Fired employees stay listed (not alive) for the rest of the match. */
struct FUORoster
{
	TArray<FUORosterEntry> Entries;

	FUORosterEntry* Find(int32 EmployeeId);
	const FUORosterEntry* Find(int32 EmployeeId) const;

	int32 CountAlive() const;
	TArray<int32> GetAliveIds() const;

	/** True while at least one human is still employed */
	bool IsAnyHumanAlive() const;

	/** The only employee left, INDEX_NONE unless exactly one is alive */
	int32 GetLastStanding() const;

	/** Marks an alive employee as gone (raw: shields are not checked). Returns false if unknown or already gone. */
	bool Eliminate(int32 EmployeeId, EUOExitCause Cause, int32 DayNumber);

	/**
	 *  An expulsion (direct firing, cut vote, meeting): a real shield is consumed instead and the employee stays;
	 *  a forged shield vanishes (EUOShieldCheck::Forgery) and the expulsion goes on. Returns what the shields did;
	 *  OutExpelled tells whether the employee left.
	 */
	EUOShieldCheck Expel(int32 EmployeeId, EUOExitCause Cause, int32 DayNumber, bool& bOutExpelled);

	/** Alive employees that are all killers (and at least one is alive) */
	bool AreOnlyKillersLeft() const;

	/**
	 *  Applies the 0-star rule (on real stars) to every alive employee: fires them, consumes exactly one of their
	 *  real shields, or (a killer) records an exemption that the evaluation keeps out of the public firing list.
	 *  Probation: anyone on probation with fewer than 2 real stars is fired the same way (killers included: the
	 *  exemption only covers the 0-star rule). Every probation ends with this review.
	 */
	TArray<FUOFiringDecision> ApplyDirectFirings(int32 DayNumber);
};

struct FUOEvaluationRules
{
	/** Design rule: shields stack up to 2. A correct answer at the cap gains nothing. */
	static constexpr int32 MaxShields = 2;

	/** 0 stars: fired, unless exempt (shields untouched) or holding shields (exactly one consumed) */
	static EUODirectFiringOutcome ResolveDirectFiring(int32 Stars, int32 Shields, bool bExempt);

	/** 3 stars may attempt a promotion unless the shown shield stack is already full (nothing to gain) */
	static bool CanAttemptPromotion(int32 Stars, int32 DisplayedShields);

	/** Correct answer adds one shield (up to MaxShields). Wrong loses nothing. Returns true if a shield was gained. */
	static bool ResolvePromotion(bool bCorrect, int32& InOutShields);

	/** What everyone sees: real plus forged shields, capped at MaxShields */
	static int32 GetDisplayedShields(int32 RealShields, int32 FakeShields);

	/** A killer's always-accepted 3-star promotion: one forged shield, unless the shown stack is full */
	static bool GrantFakeShield(int32 RealShields, int32& InOutFakeShields);

	/** An expulsion meets the shields: a real one is consumed (saved), else a forged one vanishes (not saved) */
	static EUOShieldCheck CheckShield(int32& InOutRealShields, int32& InOutFakeShields);

	/** Probation: fewer than 2 real stars at the next review is a firing */
	static bool FailsProbation(int32 RealStars);

	/** Only employees showing exactly 1 star can be voted out */
	static bool IsCutCandidate(int32 VisibleStars);

	/**
	 *  Single player: the human gone ends the match (game over); the human alone wins; only killers left ends the
	 *  match too (the saboteurs take over: a victory when the human is one of them).
	 */
	static EUOMatchResult EvaluateMatchEnd(const FUORoster& Roster);

	/**
	 *  Days survived = evaluations passed. Let go at the end of day N: N - 1. Winning on day N: N.
	 *  Still playing after day N: N.
	 */
	static int32 ComputeDaysSurvived(int32 DayNumber, EUOMatchResult Result);
};

/** Best-days records per base AI difficulty */
struct FUORecordBook
{
	/** Counts the match and keeps the best. Returns true when DaysSurvived beats the previous best. */
	static bool Submit(TArray<FUORecordEntry>& Records, EUOAIDifficulty Difficulty, int32 DaysSurvived, bool bVictory);

	static int32 GetBestDays(const TArray<FUORecordEntry>& Records, EUOAIDifficulty Difficulty);
};
