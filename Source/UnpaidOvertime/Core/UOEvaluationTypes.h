// UNPAID: OVERTIME - enums and records of the end-of-day evaluation cycle (firings, boss questions, cut vote, match end).

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOEvaluationTypes.generated.h"

/** Day of the in-game week. Weeks are Monday..Friday followed by one "overtime weekend" day. */
UENUM(BlueprintType)
enum class EUOWeekday : uint8
{
	Monday,
	Tuesday,
	Wednesday,
	Thursday,
	Friday,
	OvertimeWeekend
};

/** Why the boss asks a question */
UENUM(BlueprintType)
enum class EUOQuestionPurpose : uint8
{
	/** 3 stars: a correct answer grants the promotion shield (harder tier) */
	Promotion,
	/** 1 star: a correct answer removes the employee from today's cut vote (easy tier) */
	Defense,
	/** Tied cut vote: the worst answer leaves (easy tier) */
	Tiebreak
};

/** Outcome of one boss question */
UENUM(BlueprintType)
enum class EUOAnswerResult : uint8
{
	Pending,
	Correct,
	Wrong,
	TimedOut
};

/** What the shields did when someone was about to be expelled */
UENUM(BlueprintType)
enum class EUOShieldCheck : uint8
{
	/** No shield at all */
	None,
	/** A real shield was consumed: the employee stays */
	ShieldUsed,
	/** The shield was a forgery: it vanishes and the expulsion goes on */
	Forgery
};

/** What the 0-star rule did to an employee */
UENUM(BlueprintType)
enum class EUODirectFiringOutcome : uint8
{
	/** Not at 0 stars */
	NotApplicable,
	Fired,
	/** The promotion shield was consumed instead */
	ShieldUsed,
	/** Exempt from direct firings (the killers fake their rating) */
	Exempt
};

/** How an employee left the match */
UENUM(BlueprintType)
enum class EUOExitCause : uint8
{
	None,
	DirectFiring,
	CutVote,
	/** Voted out at a meeting (the only exit that reveals the role) */
	Convicted,
	/** "Desvinculado" by the killer (optional kill) */
	Dismissed
};

/** Single-player match end */
UENUM(BlueprintType)
enum class EUOMatchResult : uint8
{
	None,
	/** The human was let go */
	GameOver,
	/** The human is the last employee standing */
	Victory
};

/** Kind of boss question */
UENUM(BlueprintType)
enum class EUOBossQuestionCategory : uint8
{
	PropCount,
	PosterInRoom,
	PropColor,
	ClockShape,
	WhichRoomHas,
	WhoDidWork,
	WhatPickedUp,
	FirstCompletionRoom,
	/** Last-resort fallback: the company name is on the sign and the HUD all day */
	CompanyName
};

/** Step of the evaluation sequence shown to the players */
UENUM(BlueprintType)
enum class EUOEvalStep : uint8
{
	None,
	Review,
	Firings,
	Promotion,
	Defense,
	CutVote,
	Tiebreak,
	Outcome,
	MatchOver,
	/** The killers privately pick the stars everyone sees (the public only sees "finalizing ratings") */
	Rating,
	/** A body was found: emergency meeting intro (mid-shift) */
	EmergencyMeeting,
	/** Meeting: 60 s open debate (statements, presented evidence) */
	Debate,
	/** Meeting: 15 s public vote, reassignable */
	MeetingVote,
	/** Meeting: a tie goes to rock-paper-scissors, best of 3 */
	Duel,
	/** Meeting: the expelled employee's shield or boss question */
	SavingThrow,
	/** Meeting: who leaves (and only then, their role) */
	Verdict
};

/** What the evaluation is waiting for from a human employee */
UENUM(BlueprintType)
enum class EUOEvalPrompt : uint8
{
	None,
	Continue,
	OfferPromotion,
	OfferDefense,
	AnswerQuestion,
	CastVote,
	MatchOverChoice,
	/** Debate: present pinned evidence, or vote to skip */
	Debate,
	/** Meeting vote: pick, reassign or abstain until the time runs out */
	MeetingVote,
	/** Duel: pick rock, paper or scissors */
	DuelPick
};

/** Kind of choice a human submits during the evaluation */
UENUM(BlueprintType)
enum class EUOEvalChoiceType : uint8
{
	Continue,
	AcceptOffer,
	DeclineOffer,
	Answer,
	Vote,
	MainMenu,
	NewHire,
	/** Killer only (private): Value = stars to show */
	FakeRating,
	/** Debate: Value = one of your pinned trace ids, put on the table for everyone */
	PresentEvidence,
	/** Debate: end the debate now */
	SkipDebate,
	/** Meeting vote: Value = employee id, INDEX_NONE to abstain */
	MeetingVote,
	/** Duel: Value = EUORPSChoice */
	DuelPick
};

/** A choice sent by the UI. The evaluation director validates it; the UI never decides. */
USTRUCT(BlueprintType)
struct FUOEvalChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Evaluation")
	EUOEvalChoiceType Type = EUOEvalChoiceType::Continue;

	/** Option index (Answer), employee id (Vote, MeetingVote), trace id (PresentEvidence), pick (DuelPick) */
	UPROPERTY(BlueprintReadWrite, Category="Evaluation")
	int32 Value = INDEX_NONE;

	static FUOEvalChoice Make(EUOEvalChoiceType InType, int32 InValue = INDEX_NONE)
	{
		FUOEvalChoice Choice;
		Choice.Type = InType;
		Choice.Value = InValue;
		return Choice;
	}
};

/** Best result per base AI difficulty (persisted by UUOSaveGame) */
USTRUCT(BlueprintType)
struct FUORecordEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Records")
	EUOAIDifficulty Difficulty = EUOAIDifficulty::Medium;

	UPROPERTY(BlueprintReadOnly, Category="Records")
	int32 BestDaysSurvived = 0;

	UPROPERTY(BlueprintReadOnly, Category="Records")
	int32 MatchesPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Records")
	int32 Victories = 0;
};

/** Pacing of the evaluation sequence (seconds). One place to tune it. */
struct FUOEvaluationTuning
{
	/** How long a resolved question (result shown) stays up before the sequence moves on */
	static constexpr float ResultHoldSeconds = 1.8f;
	/** How long an AI's question is displayed before its answer is revealed */
	static constexpr float AIQuestionDisplaySeconds = 2.2f;
	/** Pause before revealing each public vote */
	static constexpr float VoteRevealInterval = 0.9f;
	/** How long the final tally stays up before the outcome */
	static constexpr float TallyHoldSeconds = 2.2f;
	/** Pause after an AI declines or skips an offer */
	static constexpr float ShortBeatSeconds = 0.6f;
	/** Public "finalizing ratings" beat (the same whether or not anyone is choosing a fake rating) */
	static constexpr float RatingBeatSeconds = 1.2f;
	/** How long the vote result stays up before the duel / saving throw */
	static constexpr float VoteResultHoldSeconds = 2.4f;
	/** Shield used / forgery exposed beat */
	static constexpr float ShieldBeatSeconds = 2.6f;
};
