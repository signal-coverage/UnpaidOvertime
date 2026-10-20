// UNPAID: OVERTIME - replicated-ready view of the end-of-day evaluation (written by the director, read by the UI).

#pragma once

#include "CoreMinimal.h"
#include "UOEvaluationTypes.h"
#include "UOKillerTypes.h"
#include "UOEvaluationState.generated.h"

/** The boss question currently on screen */
USTRUCT(BlueprintType)
struct FUOQuestionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	int32 AskedEmployeeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	EUOQuestionPurpose Purpose = EUOQuestionPurpose::Defense;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	EUOBossQuestionCategory Category = EUOBossQuestionCategory::PropCount;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	FText Prompt;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	TArray<FText> Options;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	float TimeLimitSeconds = 0.f;

	/** Server world time when the question was asked (countdowns use the replicated server time) */
	UPROPERTY(BlueprintReadOnly, Category="Boss")
	float StartServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	EUOAnswerResult Result = EUOAnswerResult::Pending;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	int32 ChosenIndex = INDEX_NONE;

	/** Only filled once the question is resolved (the answer never travels to clients before that) */
	UPROPERTY(BlueprintReadOnly, Category="Boss")
	int32 CorrectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Boss")
	float AnswerSeconds = 0.f;

	bool IsResolved() const { return Result != EUOAnswerResult::Pending; }
};

/** One line of the direct firings reveal */
USTRUCT(BlueprintType)
struct FUOFiringView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 EmployeeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	EUODirectFiringOutcome Outcome = EUODirectFiringOutcome::NotApplicable;

	/** Shields the employee shows after this decision (one fewer when a shield was used) */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 ShieldsLeft = 0;

	/** The firing (or the shield used) comes from a failed probation */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	bool bProbation = false;

	/** A forged shield was exposed on the way out ("The shield was a forgery.") */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	bool bForgeryExposed = false;
};

/** One promotion / defense / tiebreak attempt (or a declined offer) */
USTRUCT(BlueprintType)
struct FUOAttemptView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 EmployeeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	EUOQuestionPurpose Purpose = EUOQuestionPurpose::Defense;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	bool bAttempted = false;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	EUOAnswerResult Result = EUOAnswerResult::Pending;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	bool bGainedShield = false;

	/** Shields held after the attempt (promotions stack them) */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 ShieldsAfter = 0;
};

/** A public cut vote (CandidateId INDEX_NONE = abstained: a sole candidate cannot vote for themselves) */
USTRUCT(BlueprintType)
struct FUOVoteView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 VoterId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 CandidateId = INDEX_NONE;
};

/** What someone said during the open debate */
USTRUCT(BlueprintType)
struct FUODebateStatementView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 SpeakerId = INDEX_NONE;

	/** Who they point at (INDEX_NONE: nobody) */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 SuspectId = INDEX_NONE;

	/** 0 accuse, 1 unsure, 2 nothing, 3 defending themselves */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	uint8 Kind = 2;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FText Text;

	/** Lines they put on the table with it */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> TraceIds;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	float ServerTime = 0.f;
};

/** A line put on the table for everyone (public from now on) */
USTRUCT(BlueprintType)
struct FUOPresentedEvidenceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 PresenterId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FUOTraceEntry Entry;
};

/** A live public ballot (TargetId INDEX_NONE = abstains) */
USTRUCT(BlueprintType)
struct FUOMeetingBallotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 VoterId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 TargetId = INDEX_NONE;
};

/** One rock-paper-scissors round */
USTRUCT(BlueprintType)
struct FUODuelRoundView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	EUORPSChoice PickA = EUORPSChoice::None;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	EUORPSChoice PickB = EUORPSChoice::None;

	/** 0 = A, 1 = B, INDEX_NONE = draw */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 Winner = INDEX_NONE;
};

/** The tie duel (best of 3) */
USTRUCT(BlueprintType)
struct FUODuelView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bActive = false;

	/** Everyone who tied (a 3+ way tie is reduced to two by a seeded draw) */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> TiedIds;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 SideA = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 SideB = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<FUODuelRoundView> Rounds;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 WinsA = 0;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 WinsB = 0;

	/** Waiting for the picks of the next round (server time it times out) */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bPicking = false;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	float PickDeadline = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 LoserId = INDEX_NONE;
};

/** The meeting: debate, vote, duel, saving throw, verdict */
USTRUCT(BlueprintType)
struct FUOMeetingView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bActive = false;

	/** Countdown of the current phase (debate 60 s, vote 15 s), on the replicated server clock */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	float PhaseStartServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	float PhaseSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<FUODebateStatementView> Statements;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<FUOPresentedEvidenceView> Presented;

	/** Live and public */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<FUOMeetingBallotView> Ballots;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bTallyFinal = false;

	/** Final tally: (employee, counted votes) */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> TallyIds;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> TallyVotes;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> Leaders;

	/** Shown but not counted: they were among the most voted */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> IgnoredVoters;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FUODuelView Duel;

	/** Who is about to leave (after the vote or the duel) */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 CandidateId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	EUOShieldCheck ShieldCheck = EUOShieldCheck::None;

	/** The boss question of the saving throw was asked */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bQuestionAsked = false;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	EUOAnswerResult SavingAnswer = EUOAnswerResult::Pending;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bResolved = false;

	/** Saved (shield or question): nobody leaves this meeting */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bSaved = false;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bProbation = false;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 ExpelledId = INDEX_NONE;

	/** Only meaningful once someone was expelled: their role is revealed */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bExpelledWasKiller = false;

	/** "Nobody received a vote." and similar */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FText Note;
};

/** Everything the evaluation screens show. Revision bumps on every change. */
USTRUCT(BlueprintType)
struct FUOEvaluationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	EUOEvalStep Step = EUOEvalStep::None;

	/** What the evaluation waits for, and from whom */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	EUOEvalPrompt Prompt = EUOEvalPrompt::None;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 PromptEmployeeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	TArray<FUOFiringView> Firings;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	TArray<FUOAttemptView> Attempts;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	FUOQuestionView Question;

	/** Cut vote candidates (after defenses) */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	TArray<int32> Candidates;

	/** Public votes revealed so far */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	TArray<FUOVoteView> Votes;

	/** Votes still to be revealed after the ones in Votes */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 PendingVotes = 0;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	bool bTallyFinal = false;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	TArray<int32> TiedCandidates;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 CutEmployeeId = INDEX_NONE;

	/** The cut vote's most voted and what their shields did (ShieldUsed: nobody is cut today) */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	int32 CutTargetId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	EUOShieldCheck CutShieldCheck = EUOShieldCheck::None;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	FText OutcomeHeadline;

	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	FText OutcomeLine;

	/** Outcome screen: true when continuing ends the match instead of starting the next day */
	UPROPERTY(BlueprintReadOnly, Category="Evaluation")
	bool bOutcomeEndsMatch = false;

	UPROPERTY(BlueprintReadOnly, Category="Match")
	EUOMatchResult MatchResult = EUOMatchResult::None;

	UPROPERTY(BlueprintReadOnly, Category="Match")
	EUOExitCause HumanExitCause = EUOExitCause::None;

	UPROPERTY(BlueprintReadOnly, Category="Match")
	int32 DaysSurvived = 0;

	UPROPERTY(BlueprintReadOnly, Category="Match")
	int32 BestDaysSurvived = 0;

	UPROPERTY(BlueprintReadOnly, Category="Match")
	bool bNewRecord = false;

	// ~ Meetings (M4)

	/** True for an emergency meeting called mid-shift */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	bool bEmergency = false;

	/** Emergency meeting: who was dismissed and where the body is */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	int32 VictimId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FName VictimRoom;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FString VictimClockTime;

	/** Emergency meeting: the public "last seen near" note (empty when the kill left none) */
	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	TArray<int32> LastSeenIds;

	UPROPERTY(BlueprintReadOnly, Category="Meeting")
	FUOMeetingView Meeting;

	/** Match over: the human won as a killer (alone or with accomplices) */
	UPROPERTY(BlueprintReadOnly, Category="Match")
	bool bKillerVictory = false;

	/** Match over: only killers were left ("The saboteurs take over the company") */
	UPROPERTY(BlueprintReadOnly, Category="Match")
	bool bKillersTakeOver = false;

	/** Match over: the human voted out a killer at some meeting (doc 7.1 extra recognition) */
	UPROPERTY(BlueprintReadOnly, Category="Match")
	bool bHumanConvictedKiller = false;
};
