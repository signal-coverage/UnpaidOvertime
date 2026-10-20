// UNPAID: OVERTIME - aim-and-use interaction with hold-to-interact, shared by humans and AIs.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UOInteractionTypes.h"
#include "UOInteractionComponent.generated.h"

class AUOInteractableStation;
class AUOPlayerState;
class UUOTaskManagerComponent;
class UAudioComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUOInteractionFinished, AUOInteractableStation* /*Station*/, EUOInteractionResult /*Result*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUOSecondaryFinished, bool /*bSucceeded*/);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// ~ Secondary action (F): the killer's sabotage or dismissal. Empty for everyone else.

	/** Starts the killer's hold on the focused station / the coworker in front (or explicit targets for AIs) */
	void BeginSecondary(AUOInteractableStation* ExplicitTarget = nullptr, class AUOPlayerState* ExplicitVictim = nullptr);
	void EndSecondary();
	const FUOInteractionOption& GetSecondaryOption() const { return SecondaryOption; }
	bool IsSecondaryHolding() const { return bSecondaryHolding; }
	float GetSecondaryProgress() const;
	/** Coworker the dismissal would target (humans: in front, within reach) */
	class AUOPlayerState* GetSecondaryVictim() const { return SecondaryVictim.Get(); }
	FOnUOSecondaryFinished OnSecondaryFinished;

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

	// Secondary (killer) hold
	FUOInteractionOption SecondaryOption;
	FUOInteractionOption SecondaryHoldOption;
	TWeakObjectPtr<AUOInteractableStation> SecondaryStation;
	TWeakObjectPtr<class AUOPlayerState> SecondaryVictim;
	TWeakObjectPtr<class AUOPlayerState> ExplicitVictim;
	bool bSecondaryHolding = false;
	float SecondaryElapsed = 0.f;

	void UpdateSecondary(float DeltaTime);
	void FinishSecondary();
	class AUOPlayerState* FindVictimInFront() const;
	class UUORoleSystemComponent* GetRoleSystem() const;

	/** Station loop while holding (printer whir, coffee gurgle...), 3D so coworkers are heard working */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HoldLoopAudio;

	void UpdateFocus();
	void FinishHold();
	void CancelHold(bool bNotify);

	AUOPlayerState* GetEmployee() const;

	// ~ Sounds (cosmetic; ids come from FUOAudioRules)
	void PlayHoldStartSounds(const AUOInteractableStation* Station);
	void PlayCompletionSounds(const AUOInteractableStation* Station) const;
	void PlayDenied() const;
	void StopHoldLoop();
	/** Only the local human hears "denied" feedback */
	bool IsLocalHuman() const;
	UUOTaskManagerComponent* GetTaskManager() const;
	float GetWorkSpeed() const;
};
