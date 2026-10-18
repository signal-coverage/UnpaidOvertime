// UNPAID: OVERTIME - pre-match configuration and its validation.

#include "UOMatchConfig.h"

#define LOCTEXT_NAMESPACE "UOMatchConfig"

namespace UOMatchConfigPrivate
{
	/** Small, deliberately conservative list. Matched as case-insensitive substrings. */
	static const TCHAR* const BannedWords[] =
	{
		TEXT("fuck"),
		TEXT("shit"),
		TEXT("cunt"),
		TEXT("bitch"),
		TEXT("whore"),
		TEXT("nazi"),
		TEXT("nigger"),
		TEXT("faggot")
	};
}

FText FUOConfigValidationResult::GetMessage() const
{
	switch (Error)
	{
	case EUOConfigError::None:
		return FText::GetEmpty();
	case EUOConfigError::CompanyNameEmpty:
		return LOCTEXT("ErrorNameEmpty", "Your company needs a name.");
	case EUOConfigError::CompanyNameTooLong:
		return FText::Format(LOCTEXT("ErrorNameTooLong", "Company name must be {0} characters or fewer."), FText::AsNumber(FUOMatchConfig::MaxCompanyNameLength));
	case EUOConfigError::CompanyNameInvalidCharacters:
		return LOCTEXT("ErrorNameInvalidChars", "Company name contains invalid characters.");
	case EUOConfigError::CompanyNameBanned:
		return LOCTEXT("ErrorNameBanned", "HR has rejected that company name.");
	case EUOConfigError::TasksPerDayOutOfRange:
		return FText::Format(LOCTEXT("ErrorTasksRange", "Tasks per day must be between {0} and {1}."), FText::AsNumber(FUOMatchConfig::MinTasksPerDay), FText::AsNumber(FUOMatchConfig::MaxTasksPerDay));
	case EUOConfigError::PlayerCountOutOfRange:
		return FText::Format(LOCTEXT("ErrorPlayersRange", "Human players must be between 1 and {0}."), FText::AsNumber(FUOMatchConfig::MaxTeamSize));
	default:
		return LOCTEXT("ErrorUnknown", "Invalid configuration.");
	}
}

FUOConfigValidationResult FUOConfigValidationResult::Ok()
{
	FUOConfigValidationResult Result;
	Result.bIsValid = true;
	Result.Error = EUOConfigError::None;
	return Result;
}

FUOConfigValidationResult FUOConfigValidationResult::Fail(EUOConfigError InError)
{
	FUOConfigValidationResult Result;
	Result.bIsValid = false;
	Result.Error = InError;
	return Result;
}

int32 FUOMatchConfig::GetAIPlayers() const
{
	return FMath::Max(0, MinTeamSize - FMath::Max(0, HumanPlayers));
}

int32 FUOMatchConfig::GetTotalEmployees() const
{
	return FMath::Max(0, HumanPlayers) + GetAIPlayers();
}

FString FUOMatchConfig::GetSanitizedCompanyName() const
{
	return CompanyName.TrimStartAndEnd();
}

FUOConfigValidationResult FUOMatchConfig::Validate() const
{
	const FUOConfigValidationResult NameResult = ValidateCompanyName(CompanyName);
	if (!NameResult.bIsValid)
	{
		return NameResult;
	}

	if (TasksPerDay < MinTasksPerDay || TasksPerDay > MaxTasksPerDay)
	{
		return FUOConfigValidationResult::Fail(EUOConfigError::TasksPerDayOutOfRange);
	}

	if (HumanPlayers < 1 || HumanPlayers > MaxTeamSize)
	{
		return FUOConfigValidationResult::Fail(EUOConfigError::PlayerCountOutOfRange);
	}

	return FUOConfigValidationResult::Ok();
}

int32 FUOMatchConfig::ResolveSeed(int32 RandomFallback) const
{
	if (Seed != 0)
	{
		return Seed;
	}
	return RandomFallback != 0 ? RandomFallback : 1;
}

FUOConfigValidationResult FUOMatchConfig::ValidateCompanyName(const FString& RawName)
{
	const FString Name = RawName.TrimStartAndEnd();

	if (Name.IsEmpty())
	{
		return FUOConfigValidationResult::Fail(EUOConfigError::CompanyNameEmpty);
	}

	for (const TCHAR Character : Name)
	{
		if (FChar::IsControl(Character))
		{
			return FUOConfigValidationResult::Fail(EUOConfigError::CompanyNameInvalidCharacters);
		}
	}

	if (Name.Len() > MaxCompanyNameLength)
	{
		return FUOConfigValidationResult::Fail(EUOConfigError::CompanyNameTooLong);
	}

	if (ContainsBannedWord(Name))
	{
		return FUOConfigValidationResult::Fail(EUOConfigError::CompanyNameBanned);
	}

	return FUOConfigValidationResult::Ok();
}

bool FUOMatchConfig::ContainsBannedWord(const FString& Name)
{
	for (const TCHAR* Word : UOMatchConfigPrivate::BannedWords)
	{
		if (Name.Contains(Word, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
