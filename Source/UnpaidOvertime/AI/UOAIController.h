// UNPAID: OVERTIME - AI employee brain: a small C++ state machine over the shared task API.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Math/RandomStream.h"
#include "UOAIDifficulty.h"
#include "UOInteractionTypes.h"
#include "UOKillerTypes.h"
#include "UOInvestigation.h"
#include "UOMeetingRules.h"
#include "UOAIController.generated.h"

class AUOInteractableStation;
class AUOPlayerState;
class AUOGameState;
class UUOInteractionComponent;
class UUOTaskManagerComponent;
class UUORoleSystemComponent;
class AUOTerminal;

/** States of the AI employee */
UENUM(BlueprintType)
enum class EUOAIState : uint8
{
	Idle,
	Think,
	Claim,
	GoToItemSource,
	PickUp,
	GoToStation,
	Work,
	Complete,
	/** Investigation: walk to a terminal and read its log */
	GoToTerminal,
	ReadTerminal,
	/** Killer: walk to a sabotage target, wait for no witnesses, hold F */
	GoToSabotage,
	Sabotage,
	/** Killer (optional rule): follow a coworker until alone, then dismiss them */
	StalkVictim,
	Kill
};

/**
 *  Idle -> Think -> Claim -> (GoToItemSource -> PickUp) -> GoToStation -> Work -> Complete -> Idle.
 *  From Think it may also go read a terminal (investigation, scaled by Suspicion) or, as the killer, sabotage
 *  (scaled by Cunning) or stalk a victim (optional kill). A killer's work is real (rule 3); it knows its accomplices.
 *  Uses only information a human also sees (the task list, what it witnessed, what it read on terminals, public
 *  results) and the same interaction API. Its own role is the only secret it knows.
 *  Movement follows navmesh paths (MoveTo with path following). When no navmesh path exists it falls back to
 *  steering with AddMovementInput: straight lines, routed through a hub when blocked, plus a sidestep when stuck.
 */
UCLASS()
class AUOAIController : public AAIController
{
	GENERATED_BODY()

public:

	AUOAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** Server: tuning and navigation hub for this employee */
	void ConfigureEmployee(const FUOAIDifficultyParams& InDifficulty, const FVector& InHubLocation, int32 Seed);

	EUOAIState GetAIState() const { return State; }

	/** Server: forget yesterday's claims, targets and paths (the task list was replaced) */
	void ResetForNewDay();

	// ~ Investigation (M4): fed only by what this AI observed, read or what is public

	/** Something happened in its room (witnessed) or is public (activity board) */
	void ObserveTrace(const FUOTraceEntry& Entry, EUOEvidenceSource Source);
	/** It had line of sight to the actor of a deliberate act */
	void ObserveAct(int32 ActorId, const FUOTraceEntry& Entry);
	/** It saw a coworker in its room */
	void ObserveSighting(int32 EmployeeId, FName Room, float Time);
	/** Evidence put on the table at a meeting (public from then on) */
	void LearnPublicTrace(const FUOTraceEntry& Entry);
	/** A killer's public answer the boss accepted (bKnowsFact: by the fairness rules it could know, and recalled it) */
	void ObserveAcceptedAnswer(const FUOAcceptedAnswer& Answer);
	/** Reads a device log up to now (terminal visit) */
	void ReadDevice(EUOTraceDevice Device, const FString& TerminalName);

	FUOSuspicionResult EvaluateSuspicion() const;
	/** Cut vote: a candidate it suspects enough, INDEX_NONE to fall back to the performance vote */
	int32 ChooseSuspicionCutVote(const TArray<int32>& EligibleCandidates);
	/** Cut vote, killer: never an accomplice (false = it must abstain or pick among the others) */
	bool MayCutVoteFor(int32 CandidateId) const { return !(bIsKiller && Accomplices.Contains(CandidateId)); }

	// ~ Meeting (rule 6): same model for statements and votes as the balance simulation (FUOMeetingAI)
	FUOMeetingAIContext MakeMeetingContext() const;
	/** What it says in the debate (logged with its reasoning) */
	FUODebateStatement MakeDebateStatement();
	/** First ballot (INDEX_NONE = abstain) */
	int32 ChooseMeetingVote();
	/** Later in the vote: keep the ballot or follow the room */
	int32 ReconsiderMeetingVote(int32 CurrentVote, const FUOMeetingTally& Live);
	/** Rock-paper-scissors pick (random, seeded) */
	EUORPSChoice PickDuel();
	/** End of the day: today's suspicion carries over (halved) into tomorrow */
	void RememberDay();
	const TArray<int32>& GetAccomplices() const { return Accomplices; }

	/** Server: the day started and roles are assigned (re-reads its own role, forgets yesterday's evidence) */
	void BeginDay() { ResetKnowledge(); }

	/** Its own role (the only secret it knows) */
	bool IsKillerBrain() const { return bIsKiller; }

	/** Dev automation only: AI killers never pick the human as a dismissal target */
	static bool bDevSpareHuman;

	/** Dev automation: the killer AI tries a dismissal as soon as the rules allow */
	void ForceKillSoon() { NextKillAt = 0.f; bKillForced = true; }

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	enum class EMoveStatus : uint8
	{
		Moving,
		Arrived,
		Failed
	};

	FUOAIDifficultyParams Difficulty;
	FVector HubLocation = FVector::ZeroVector;
	FRandomStream Rng;

	// ~ Role and activities
	bool bIsKiller = false;
	/** Killer: the fellow killers (rule 1) */
	TArray<int32> Accomplices;
	float NextSabotageAt = 0.f;
	float NextKillAt = 0.f;
	float NextTerminalAt = 0.f;
	bool bKillForced = false;
	float KillerInterruptTimer = 0.f;
	TWeakObjectPtr<AUOTerminal> TargetTerminal;
	TWeakObjectPtr<class AUOInteractableStation> SabotageTarget;
	TWeakObjectPtr<class AUOPlayerState> KillVictim;
	float ActivityTimer = 0.f;
	float WitnessWait = 0.f;
	bool bSecondaryResult = false;
	bool bHasSecondaryResult = false;
	FDelegateHandle SecondaryFinishedHandle;

	// ~ Knowledge (investigation)
	TMap<int32, EUOEvidenceSource> KnownTraces;
	TArray<FUOSeenAct> SeenActs;
	TArray<FUOSighting> Sightings;
	TSet<int32> KnownSabotageTraces;
	TMap<uint8, float> LastReadAt;
	TArray<FUOAcceptedAnswer> AcceptedAnswers;
	/** Yesterday's suspicion per coworker (decays by half each day) */
	TArray<FUOCarriedSuspicion> CarriedOver;
	/** Meetings held so far (noise seed: a stable view within a meeting, a new one at the next) */
	int32 MeetingCounter = 0;

	EUOAIState State = EUOAIState::Idle;
	float StateTimer = 0.f;

	/** Pause before retrying a botched work attempt */
	float RetryTimer = 0.f;

	int32 ClaimedSubObjectiveId = INDEX_NONE;
	TWeakObjectPtr<AUOInteractableStation> TargetStation;
	TWeakObjectPtr<AUOInteractableStation> ItemSourceStation;

	// Interaction result reported by the interaction component
	bool bHasInteractionResult = false;
	EUOInteractionResult LastInteractionResult = EUOInteractionResult::Rejected;
	FDelegateHandle InteractionFinishedHandle;

	// Navmesh movement
	enum class ENavMoveState : uint8
	{
		None,
		Moving,
		Succeeded,
		Failed
	};
	ENavMoveState NavMoveState = ENavMoveState::None;
	FVector MoveGoal = FVector::ZeroVector;
	int32 NavFallbackCount = 0;

	// Steering fallback
	TArray<FVector> Waypoints;
	float BestDistanceToWaypoint = 0.f;
	float StuckTimer = 0.f;
	int32 StuckCount = 0;
	float SidestepSign = 1.f;

	void EnterState(EUOAIState NewState);

	// ~ M4 activities (UOAIControllerKiller.cpp)
	/** From Think: terminal visit, sabotage, stalking, or ordinary work */
	void ChooseActivity();
	bool TryStartTerminalVisit();
	bool TryStartSabotage();
	bool TryStartKill();
	void TickTerminal(float DeltaSeconds);
	void TickSabotage(float DeltaSeconds);
	void TickStalk(float DeltaSeconds);
	void HandleSecondaryFinished(bool bSucceeded);
	void ScheduleNext(float& InOutTime, float PerMinute);
	UUORoleSystemComponent* GetRoleSystem() const;
	float ShiftNow() const;
	/** Forget today's knowledge and re-read its own role */
	void ResetKnowledge();
	void ReleaseEverything();

	void TickClaim();
	void TickGoTo(EUOAIState NextState);
	void TickPickUp();
	void TickWork(float DeltaSeconds);

	/** Picks the best open sub-objective and claims it through the task manager */
	bool ChooseAndClaim();

	/** Drops the claim and goes back to Idle */
	void AbandonCurrentWork();

	/** True when the claimed sub-objective was finished by someone else or the claim is gone */
	bool IsClaimStillValid() const;

	void StartInteractionWith(AUOInteractableStation* Station);
	void HandleInteractionFinished(AUOInteractableStation* Station, EUOInteractionResult Result);

	void MoveToPoint(const FVector& Target);
	void StartSteering(const FVector& Target);
	EMoveStatus UpdateMovement(float DeltaSeconds);
	EMoveStatus UpdateSteering(float DeltaSeconds);
	bool HasClearPath(const FVector& From, const FVector& To) const;
	void FaceLocation(const FVector& Location);

	AUOPlayerState* GetEmployee() const;
	AUOGameState* GetUOGameState() const;
	UUOTaskManagerComponent* GetTaskManager() const;
	UUOInteractionComponent* GetInteraction() const;
};
