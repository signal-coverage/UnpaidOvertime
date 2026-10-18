// UNPAID: OVERTIME - aim-and-use interaction with hold-to-interact, shared by humans and AIs.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UOInteractionTypes.h"
#include "UOInteractionComponent.generated.h"

class AUOInteractableStation;
class AUOPlayerState;
class UUOTaskManagerComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUOInteractionFinished, AUOInteractableStation* /*Station*/, EUOInteractionResult /*Result*/);

/**
 *  Humans focus stations with a line trace from the view point; AIs set an explicit target.
 *  Either way, holds progress at the employee's work speed and completions are reported to the task manager,
 *  which has the final word.
 */
UCLASS(ClassGroup=(UnpaidOvertime), meta=(BlueprintSpawnableComponent))
class UUOInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UUOInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts interacting with the focused station (humans) or with the given target (AIs) */
	void BeginInteract(AUOInteractableStation* ExplicitTarget = nullptr);

	/** Releases the hold; unfinished work is cancelled */
	void EndInteract();

	/** Drops the carried item */
	bool DropItem();

	/** Clears the explicit target (AIs) */
	void ClearTarget();

	AUOInteractableStation* GetFocusedStation() const { return FocusedStation.Get(); }
	const FUOInteractionOption& GetCurrentOption() const { return CurrentOption; }
	bool IsHolding() const { return bHolding; }

	/** 0..1 progress of the current hold */
	float GetHoldProgress() const;

	/** Fired when a press/hold finishes (successfully or not) */
	FOnUOInteractionFinished OnInteractionFinished;

	/** Max trace distance from the view point (cm) */
	UPROPERTY(EditAnywhere, Category="Interaction")
	float TraceDistance = 250.f;

	/** Humans trace from the view point; AIs use an explicit target instead */
	UPROPERTY(EditAnywhere, Category="Interaction")
	bool bUseViewTrace = true;

	/** Chance (0..1) that a completed work hold succeeds; used for AI precision */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="0", ClampMax="1"))
	float SuccessChance = 1.f;

protected:

	TWeakObjectPtr<AUOInteractableStation> FocusedStation;
	TWeakObjectPtr<AUOInteractableStation> ExplicitTargetStation;
	TWeakObjectPtr<AUOInteractableStation> HoldStation;

	FUOInteractionOption CurrentOption;
	FUOInteractionOption HoldOption;

	bool bHolding = false;
	float HoldElapsed = 0.f;

	void UpdateFocus();
	void FinishHold();
	void CancelHold(bool bNotify);

	AUOPlayerState* GetEmployee() const;
	UUOTaskManagerComponent* GetTaskManager() const;
	float GetWorkSpeed() const;
};
