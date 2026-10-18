// UNPAID: OVERTIME - AI employee brain: a small C++ state machine over the shared task API.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Math/RandomStream.h"
#include "UOAIDifficulty.h"
#include "UOInteractionTypes.h"
#include "UOAIController.generated.h"

class AUOInteractableStation;
class AUOPlayerState;
class AUOGameState;
class UUOInteractionComponent;
class UUOTaskManagerComponent;

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
	Complete
};

/**
 *  Idle -> Think -> Claim -> (GoToItemSource -> PickUp) -> GoToStation -> Work -> Complete -> Idle.
 *  Uses only information a human also sees (the task list) and the same interaction API.
 *  Movement steers with AddMovementInput (no navmesh needed): straight lines, routed through the office hub
 *  when the direct line is blocked, plus a sidestep when stuck.
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

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	enum class EMoveStatus : uint8
	{
		Moving,
		Arrived,
		Failed
	};

	FUOAIDifficultyParams Difficulty;
	FVector HubLocation = FVector::ZeroVector;
	FRandomStream Rng;

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

	// Steering
	TArray<FVector> Waypoints;
	float BestDistanceToWaypoint = 0.f;
	float StuckTimer = 0.f;
	int32 StuckCount = 0;
	float SidestepSign = 1.f;

	void EnterState(EUOAIState NewState);

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
	EMoveStatus UpdateMovement(float DeltaSeconds);
	bool HasClearPath(const FVector& From, const FVector& To) const;
	void FaceLocation(const FVector& Location);

	AUOPlayerState* GetEmployee() const;
	AUOGameState* GetUOGameState() const;
	UUOTaskManagerComponent* GetTaskManager() const;
	UUOInteractionComponent* GetInteraction() const;
};
