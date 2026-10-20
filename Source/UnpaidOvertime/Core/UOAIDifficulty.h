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

	/** Chance (0..1) of remembering correctly what it saw when the boss asks (only witnessed facts are asked) */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float AnswerAccuracy = 0.7f;

	/** Fraction of the question time limit the AI takes to answer (lower = faster) */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float AnswerTimeFraction = 0.6f;

	/** Doc 9.2 "Sospecha": how well it spots inconsistencies in visible traces (accusations, accusation votes). 0..1 */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float Suspicion = 0.4f;

	/** Doc 9.2 "Astucia" (as the killer): fewer witnesses, fewer retouches, better-timed sabotage. 0..1 */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float Cunning = 0.4f;

	/** Doc 9.2 "Reacción": how fast it responds to an accusation (vote reveal pace). 0..1 */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float Reaction = 0.4f;

	/** Weight of the random part of a vote (lower = more rational voter) */
	UPROPERTY(BlueprintReadOnly, Category="AI")
	float VoteNoise = 0.5f;
};

struct FUOAIDifficulty
{
	/** Abstract skill: base level (0/1/2) plus a quarter point per day after the first */
	static float GetSkillLevel(EUOAIDifficulty Base, int32 DayNumber);

	/** Pure function: parameters for a base difficulty on a given day */
	static FUOAIDifficultyParams Compute(EUOAIDifficulty Base, int32 DayNumber);
};
