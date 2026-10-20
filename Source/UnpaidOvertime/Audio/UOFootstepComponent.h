// UNPAID: OVERTIME - footsteps, jumps and landings for any character (players and AI coworkers).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UOFootstepComponent.generated.h"

/**
 *  Distance-based steps: every stride (longer when faster) it traces the floor under the owner, maps the floor
 *  material to a surface (carpet / tile) and plays that surface's footstep at the feet through the audio
 *  subsystem. Velocity drives it, so teleports never trigger a burst of steps. Also detects take-off and landing.
 */
UCLASS(ClassGroup=(UnpaidOvertime), meta=(BlueprintSpawnableComponent))
class UUOFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UUOFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Scales every sound of this owner (AI coworkers step a bit softer than the player) */
	UPROPERTY(EditAnywhere, Category="Footsteps", meta=(ClampMin="0", ClampMax="2"))
	float VolumeMultiplier = 1.f;

	/** Ground speed under which no steps are produced (cm/s) */
	UPROPERTY(EditAnywhere, Category="Footsteps")
	float MinSpeed = 30.f;

	/** Play jump/land sounds (players) */
	UPROPERTY(EditAnywhere, Category="Footsteps")
	bool bJumpAndLandSounds = true;

protected:

	float DistanceSinceStep = 0.f;
	float AirTime = 0.f;
	bool bWasOnGround = true;
	bool bFirstTick = true;

	void PlayStep(float Volume);
	FVector GetFeetLocation() const;
};
