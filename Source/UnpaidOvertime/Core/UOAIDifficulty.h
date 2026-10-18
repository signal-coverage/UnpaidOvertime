// UNPAID: OVERTIME - AI employee tuning from base difficulty and day number.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOAIDifficulty.generated.h"

/** Rule parameters for an AI employee (no learning, just tuned numbers) */
USTRUCT(BlueprintType)
struct FUOAIDifficultyParams
{
	GENERATED_BODY()

	/** Multiplies work progress per second (also scales walking pace) */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float WorkSpeedMultiplier = 1.f;

	/** Chance (0..1) that a work attempt succeeds; failures are retried */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float Precision = 0.8f;

	/** Average pause before picking the next sub-objective */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float ThinkDelaySeconds = 2.f;
};

struct FUOAIDifficulty
{
	/** Abstract skill: base level (0/1/2) plus a quarter point per day after the first */
	static float GetSkillLevel(EUOAIDifficulty Base, int32 DayNumber);

	/** Pure function: parameters for a base difficulty on a given day */
	static FUOAIDifficultyParams Compute(EUOAIDifficulty Base, int32 DayNumber);
};
