// UNPAID: OVERTIME - server authority for the end-of-day cycle: firings, promotions, defenses, cut vote, meeting, match end.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "UOEvaluationTypes.h"
#include "UOEvaluationRules.h"
#include "UOCutVote.h"
#include "UOAIDifficulty.h"
#include "UOKillerRules.h"
#include "UOInvestigation.h"
#include "UOMeetingRules.h"
#include "UOBossQuestions.h"
#include "UOEvaluationDirector.generated.h"

class AUOGameMode;
class AUOGameState;
class AUOPlayerState;
class UUOBossSystemComponent;
class UUORoleSystemComponent;
class AUOAIController;

/**
 *  Runs the evaluation sequence after the shift (design doc 2.4, 4, 5; M4.1 rules):
 *    Rating (each killer privately picks the stars everyone sees: 1-3) -> Review -> Direct firings (+ failed
 *    probations) -> Promotions (3 stars; a killer's is mandatory and always accepted: a forged shield) -> Defenses
 *    (1 star; a killer's is mandatory and always accepted, in public) -> Cut vote (+ tiebreak; a shield absorbs it)
 *    -> Meeting (60 s debate, 15 s live vote, rock-paper-scissors on a tie, saving throw: shield or boss question +
 *    probation; the role is revealed only when someone leaves) -> Outcome -> next day, or Match over (game over,
 *    last one standing, or only killers left).
 *  It also runs emergency meetings mid-shift (a body was found): the same meeting, then the shift resumes.
 *  Every decision is made here and written to AUOGameState::Evaluation; the UI only shows it and submits choices
 *  through SubmitChoice (single player calls it directly; M7 routes the same call through a server RPC).
 *  AIs take the same steps with simple, explainable rules and never read hidden information.
 */
UCLASS(ClassGroup=(UnpaidOvertime))
class UUOEvaluationDirector : public UActorComponent
{
	GENERATED_BODY()

public:

	UUOEvaluationDirector();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts the sequence. Stars and contributions must already be on the player states. */
	void BeginEvaluation(int32 InDayNumber, int32 InSeed, EUOAIDifficulty InBaseDifficulty);

	/** A body was found mid-shift: emergency meeting (the game mode already switched the phase to Meeting) */
	void BeginEmergencyMeeting(int32 VictimId, FName Room, const FString& ClockTime, const TArray<int32>& LastSeenIds, int32 InDayNumber, int32 InSeed, EUOAIDifficulty InBaseDifficulty);

	/** Stops everything (new day / new match). Keeps the cut-vote memory unless bForgetMatch. */
	void Reset(bool bForgetMatch);

	/** A human's choice. Returns false (and changes nothing) when it is not what the evaluation waits for. */
	bool SubmitChoice(int32 EmployeeId, const FUOEvalChoice& Choice);

	bool IsRunning() const { return Stage != EStage::Idle; }

	/** Dev automation only: the hidden correct option of the active question (INDEX_NONE if none) */
	int32 GetCorrectIndexForDev() const { return PendingCorrectIndex; }

	// ~ Dev automation (non-Shipping switches); logged whenever used
	/** AI killers show this many stars (0 = their own choice) */
	int32 DevKillerStars = 0;
	/** AI killers' always-accepted answers are wrong on purpose */
	bool bDevKillerAnswersWrong = false;
	/** Meeting policy for the AIs: "" normal, "tie" (split the vote into a tie), "expel:<id>" (all vote for id), "abstain" */
	FString DevMeetingPolicy;
	/** Saving throws: "" normal, "right" (the expelled answers right), "wrong" */
	FString DevSavingAnswer;

protected:

	enum class EStage : uint8
	{
		Idle,
		WaitHuman,
		WaitTimer,
		QuestionRunning,
		/** The killers' private fake-rating choices (the public only sees "finalizing ratings") */
		WaitPrivate,
		/** A timed meeting phase (debate, vote, duel pick) */
		MeetingClock
	};

	enum class EQueue : uint8
	{
		None,
		Promotion,
		Defense,
		Tiebreak
	};

	enum class EMeetingPhase : uint8
	{
		None,
		Debate,
		Vote,
		DuelPick
	};

	/** Why a question is being asked */
	enum class EQuestionMode : uint8
	{
		Normal,
		/** A killer's mandatory defense/promotion: the answer is accepted whatever it is, in public */
		AlwaysAccepted,
		/** The meeting's saving throw (evaluated for real, for everyone) */
		SavingThrow
	};

	EStage Stage = EStage::Idle;
	float Timer = 0.f;
	TFunction<void()> AfterTimer;

	int32 DayNumber = 1;
	int32 Seed = 1;
	int32 QuestionCounter = 0;
	EUOAIDifficulty BaseDifficulty = EUOAIDifficulty::Medium;
	FRandomStream Rng;

	// Queues of the current step
	EQueue ActiveQueue = EQueue::None;
	TArray<int32> Queue;
	TSet<int32> Defended;
	TArray<FUOTiebreakAnswer> TiebreakAnswers;

	// Active question (the correct index stays here until the question resolves)
	int32 PendingCorrectIndex = INDEX_NONE;
	float QuestionDeadline = 0.f;
	bool bAIAnswerPending = false;
	int32 AIChosenIndex = INDEX_NONE;
	float AIAnswerSeconds = 0.f;
	EQuestionMode QuestionMode = EQuestionMode::Normal;
	FUOBossQuestion PendingQuestion;

	// Cut vote: ballots decided before the reveal, revealed one by one
	TArray<FUOBallot> HiddenBallots;
	TArray<FUOBallot> AllBallots;
	/** Grudge memory: candidate id -> voters who voted for them in the previous cut vote (public information) */
	TMap<int32, TArray<int32>> LastVotersAgainst;

	// Fake rating (private to the killers)
	TMap<int32, FUOFakeRatingOffer> PendingOffers;
	TSet<int32> PendingHumanRatings;
	float RatingDeadline = 0.f;

	// Meeting
	bool bEmergency = false;
	bool bNoCutCandidates = false;
	EMeetingPhase MeetingPhase = EMeetingPhase::None;
	float PhaseEndsAt = 0.f;
	TArray<FUOMeetingBallot> MeetingBallots;
	/** AI statement schedule (server world time, speaker) */
	TArray<TPair<float, int32>> StatementSchedule;
	/** AI replies to being named (server world time, speaker, accuser) */
	struct FReply { float At = 0.f; int32 Speaker = INDEX_NONE; int32 Accuser = INDEX_NONE; };
	TArray<FReply> Replies;
	/** AI ballots: first vote and reconsider times */
	struct FAIBallotPlan { int32 Voter = INDEX_NONE; float FirstAt = 0.f; float ReconsiderAt = 0.f; bool bCast = false; bool bReconsidered = false; };
	TArray<FAIBallotPlan> BallotPlans;
	/** Evidence the human already put on the table this meeting */
	int32 HumanPresented = 0;
	FUORPSDuel Duel;
	EUORPSChoice HumanDuelPick = EUORPSChoice::None;
	/** Match memory: the human voted for a killer who was then expelled (doc 7.1 recognition) */
	bool bHumanConvictedKiller = false;

	AUOGameMode* GetGameMode() const;
	AUOGameState* GetUOGameState() const;
	UUOBossSystemComponent* GetBossSystem() const;
	AUOPlayerState* FindEmployee(int32 EmployeeId) const;
	int32 GetHumanId() const;
	float Now() const;
	TArray<int32> GetAliveIds() const;
	FString NameOf(int32 EmployeeId) const;

	FUORoster BuildRoster() const;
	/** Writes alive / shields / probation / exit back to the player states and notifies the game mode of new exits */
	void ApplyRoster(const FUORoster& Roster);

	void Touch();
	void SetStep(EUOEvalStep Step);
	void WaitForHuman(EUOEvalPrompt Prompt, int32 EmployeeId);
	void WaitFor(float Seconds, TFunction<void()> Then);

	// ~ Sequence
	void StartFirings();
	void AfterFirings();
	void StartPromotions();
	void StartDefenses();
	void ProcessQueue();
	void HandleOffer(int32 EmployeeId, bool bAccepted);
	void AskQuestion(int32 EmployeeId, EUOQuestionPurpose Purpose, EQuestionMode Mode = EQuestionMode::Normal);
	void ResolveQuestion(EUOAnswerResult Result, int32 ChosenIndex, float AnswerSeconds);
	/** A killer's always-accepted public answer: every AI judges it with what it could know */
	void BroadcastAcceptedAnswer(int32 EmployeeId, bool bReallyCorrect);
	void StartCutVote();
	void RevealNextVote();
	void FinishVote();
	void StartTiebreak();
	void CutEmployee(int32 EmployeeId, int32 Votes);
	void ShowOutcome(bool bNoCandidates);
	/** True when the match ended (and the match-over screen is up) */
	bool CheckMatchEnd();

	bool ShouldAIAttempt(int32 EmployeeId, EUOQuestionPurpose Purpose) const;
	FUOAIDifficultyParams GetAIParams() const;

	// ~ Killers: fake rating
	UUORoleSystemComponent* GetRoleSystem() const;
	AUOAIController* FindBrain(int32 EmployeeId) const;
	bool IsKiller(int32 EmployeeId) const;
	void StartRating();
	void FinishRating(bool bTimedOut);
	void ShowReview();

	// ~ Meeting (rule 6)
	/** After the cut vote (or none): the meeting, unless the match is over */
	void AfterCut(bool bNoCandidates);
	void StartMeeting();
	void TickMeeting();
	void PostStatement(int32 SpeakerId, const FUODebateStatement& Statement);
	void PresentEvidence(int32 PresenterId, int32 TraceId);
	void EndDebate();
	void StartMeetingVote();
	bool CastMeetingBallot(int32 VoterId, int32 TargetId);
	FUOMeetingTally TallyMeeting() const;
	void EndMeetingVote();
	void StartDuel(const TArray<int32>& Tied);
	void StartDuelRound();
	void ResolveDuelRound();
	void StartSavingThrow(int32 CandidateId);
	void AfterSavingQuestion(bool bCorrect);
	void ExpelCandidate(int32 CandidateId);
	void ShowVerdict();
	void EndMeeting();
	/** Dev meeting policy override for an AI ballot (INDEX_NONE = none) */
	bool GetDevBallot(int32 VoterId, int32& OutTarget) const;
};
