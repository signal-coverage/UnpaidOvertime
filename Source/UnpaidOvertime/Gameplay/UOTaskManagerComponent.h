// UNPAID: OVERTIME - server-side task authority owned by the game mode.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UOTaskTypes.h"
#include "UOInteractionTypes.h"
#include "UOEvidenceTypes.h"
#include "UOTaskManagerComponent.generated.h"

class AUOGameState;
class AUOPlayerState;
class AUOInteractableStation;

/**
 *  Owns the rules of the day's tasks: what each station offers, claims, server-side validation
 *  of completions (reach and work time) and the evidence trail. Humans and AIs use the same API.
 */
UCLASS(ClassGroup=(UnpaidOvertime))
class UUOTaskManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Max distance from the employee's pawn to the station for an interaction to be accepted */
	static constexpr float MaxInteractionDistance = 400.f;

	/** Fraction of the required work time that must really have elapsed (absorbs frame jitter) */
	static constexpr float WorkTimeTolerance = 0.9f;

	UUOTaskManagerComponent();

	/** Loads the day's tasks into the game state and indexes the placed stations */
	void StartDay(const TArray<FUOTaskInstance>& Tasks, const TArray<AUOInteractableStation*>& InStations);

	/** Stops accepting work (evaluation) */
	void EndDay();

	bool IsAcceptingWork() const { return bAcceptingWork; }

	/** What the employee could do at the station right now */
	FUOInteractionOption QueryInteraction(const AUOPlayerState* Employee, const AUOInteractableStation* Station) const;

	/** Records the start of a hold, used to validate the work time on completion */
	bool BeginWork(AUOPlayerState* Employee, AUOInteractableStation* Station);

	/** Forgets a hold in progress */
	void CancelWork(AUOPlayerState* Employee);

	/** Validates and applies an interaction. Returns false when rejected. */
	bool CompleteInteraction(AUOPlayerState* Employee, AUOInteractableStation* Station);

	/** Drops (returns to supply) the carried item */
	bool DropCarriedItem(AUOPlayerState* Employee);

	/** AI coordination: claim a sub-objective so other AIs skip it */
	bool TryClaim(AUOPlayerState* Employee, int32 SubObjectiveId);
	bool ReleaseClaim(AUOPlayerState* Employee, int32 SubObjectiveId);

	/** Employee left the game: pending claims go back to Open (never deleted) */
	void ReleaseEmployee(AUOPlayerState* Employee);

	bool AreAllSubObjectivesDone() const;

	AUOInteractableStation* FindStation(FName StationId) const;

	/** Closest station handing out the item, nullptr if none */
	AUOInteractableStation* FindNearestItemSource(EUOItemType Item, const FVector& FromLocation) const;

	const TArray<TObjectPtr<AUOInteractableStation>>& GetStations() const { return Stations; }

	/** Items of this type still needed that nobody is carrying yet */
	int32 CountUnclaimedItemDemand(EUOItemType Item) const;

	/** Read-only view of the authoritative task list */
	const TArray<FUOTaskInstance>& GetTasks() const;

protected:

	UPROPERTY()
	TArray<TObjectPtr<AUOInteractableStation>> Stations;

	UPROPERTY()
	TMap<FName, TObjectPtr<AUOInteractableStation>> StationsById;

	struct FWorkInProgress
	{
		TWeakObjectPtr<AUOInteractableStation> Station;
		double StartTimeSeconds = 0.0;
		/** Sub-objective claimed when the hold started (released if the hold is cancelled) */
		int32 ClaimedSubObjectiveId = INDEX_NONE;
	};

	/** Holds in progress, by employee id */
	TMap<int32, FWorkInProgress> ActiveWork;

	bool bAcceptingWork = false;

	AUOGameState* GetUOGameState() const;

	bool IsWithinReach(const AUOPlayerState* Employee, const AUOInteractableStation* Station) const;

	void CompleteSubObjective(AUOPlayerState* Employee, AUOInteractableStation* Station, int32 SubObjectiveId);

	void LogEvidence(const AUOPlayerState* Employee, FName StationId, EUOEvidenceAction Action, EUOItemType Item, int32 SubObjectiveId);

	void RefreshStationVisual(FName StationId);
};
