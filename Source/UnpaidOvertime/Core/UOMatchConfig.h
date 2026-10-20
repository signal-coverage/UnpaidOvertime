// UNPAID: OVERTIME - pre-match configuration and its validation.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOKillerTypes.h"
#include "UOMatchConfig.generated.h"

/** Reason a match configuration was rejected */
UENUM(BlueprintType)
enum class EUOConfigError : uint8
{
	None,
	CompanyNameEmpty,
	CompanyNameTooLong,
	CompanyNameInvalidCharacters,
	CompanyNameBanned,
	TasksPerDayOutOfRange,
	PlayerCountOutOfRange
};

/** Result of validating a match configuration */
USTRUCT(BlueprintType)
struct FUOConfigValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Config")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category="Config")
	EUOConfigError Error = EUOConfigError::None;

	/** Human readable explanation of the error (empty when valid) */
	FText GetMessage() const;

	static FUOConfigValidationResult Ok();
	static FUOConfigValidationResult Fail(EUOConfigError InError);
};

/**
 *  Everything chosen before the match starts. Never changes during the match.
 *  There is intentionally no default company name: the match cannot start until one is typed.
 */
USTRUCT(BlueprintType)
struct FUOMatchConfig
{
	GENERATED_BODY()

	static constexpr int32 MinTasksPerDay = 4;
	static constexpr int32 MaxTasksPerDay = 8;
	static constexpr int32 DefaultTasksPerDay = 6;
	static constexpr int32 MaxCompanyNameLength = 24;
	static constexpr int32 MinTeamSize = 4;
	static constexpr int32 MaxTeamSize = 8;

	/** Company name shown on signs and the HUD. Required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	FString CompanyName;

	/** Total tasks per day, shared by every employee (not per player) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	int32 TasksPerDay = DefaultTasksPerDay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	EUODayLength DayLength = EUODayLength::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	EUOAIDifficulty AIDifficulty = EUOAIDifficulty::Medium;

	/** Human players in the match (the MVP supports exactly one) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	int32 HumanPlayers = 1;

	/** Employees in total (humans + AIs), MinTeamSize..MaxTeamSize. Killers: floor(TeamSize / 3), at least 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	int32 TeamSize = MinTeamSize;

	/** Who may be the killer. Random by default; AlwaysHuman is a debug option (hidden in Shipping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	EUOKillerAssignment KillerAssignment = EUOKillerAssignment::Random;

	/** Design doc 2.2 "El asesino puede matar": No by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	bool bKillerCanKill = false;

	/** Generation seed. 0 means "pick a random one when the day starts". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
	int32 Seed = 0;

	/** AI employees needed to fill the team up to the team size */
	int32 GetAIPlayers() const;

	/** Humans plus AIs */
	int32 GetTotalEmployees() const;

	/** Company name with surrounding whitespace removed */
	FString GetSanitizedCompanyName() const;

	/** Validates the whole configuration, returning the first error found */
	FUOConfigValidationResult Validate() const;

	/** Returns Seed, or the fallback when Seed is 0. Never returns 0. */
	int32 ResolveSeed(int32 RandomFallback) const;

	/** Validates only the company name (trimmed, 1-24 chars, no control chars, no banned words) */
	static FUOConfigValidationResult ValidateCompanyName(const FString& RawName);

	/** Case-insensitive check against a small banned-words list */
	static bool ContainsBannedWord(const FString& Name);
};
