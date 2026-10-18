// UNPAID: OVERTIME - match config validation tests.

#include "UOTestFlags.h"
#include "UOMatchConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOMatchConfigTests
{
	FUOMatchConfig MakeValidConfig()
	{
		FUOMatchConfig Config;
		Config.CompanyName = TEXT("Dunder Paper");
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigEmptyNameTest, "UnpaidOvertime.Core.MatchConfig.EmptyName", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigEmptyNameTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config;
	const FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("Default config has no company name and must be invalid"), Result.bIsValid);
	TestEqual(TEXT("Error is CompanyNameEmpty"), Result.Error, EUOConfigError::CompanyNameEmpty);
	TestFalse(TEXT("Error message is not empty"), Result.GetMessage().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigWhitespaceNameTest, "UnpaidOvertime.Core.MatchConfig.WhitespaceName", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigWhitespaceNameTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();
	Config.CompanyName = TEXT("   \t  ");
	const FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("Whitespace-only name is invalid"), Result.bIsValid);
	TestEqual(TEXT("Whitespace-only name counts as empty"), Result.Error, EUOConfigError::CompanyNameEmpty);

	Config.CompanyName = TEXT("   Acme Corp   ");
	TestTrue(TEXT("Surrounding whitespace is trimmed and accepted"), Config.Validate().bIsValid);
	TestEqual(TEXT("Sanitized name is trimmed"), Config.GetSanitizedCompanyName(), FString(TEXT("Acme Corp")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigTooLongTest, "UnpaidOvertime.Core.MatchConfig.TooLong", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigTooLongTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();

	Config.CompanyName = FString::ChrN(FUOMatchConfig::MaxCompanyNameLength, TEXT('A'));
	TestTrue(TEXT("Exactly 24 characters is valid"), Config.Validate().bIsValid);

	Config.CompanyName = FString::ChrN(FUOMatchConfig::MaxCompanyNameLength + 1, TEXT('A'));
	const FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("25 characters is invalid"), Result.bIsValid);
	TestEqual(TEXT("Error is CompanyNameTooLong"), Result.Error, EUOConfigError::CompanyNameTooLong);

	Config.CompanyName = TEXT("  ") + FString::ChrN(FUOMatchConfig::MaxCompanyNameLength, TEXT('B')) + TEXT("  ");
	TestTrue(TEXT("Length is measured after trimming"), Config.Validate().bIsValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigControlCharsTest, "UnpaidOvertime.Core.MatchConfig.ControlChars", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigControlCharsTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();

	Config.CompanyName = TEXT("Acme\nCorp");
	FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("Embedded newline is invalid"), Result.bIsValid);
	TestEqual(TEXT("Error is CompanyNameInvalidCharacters"), Result.Error, EUOConfigError::CompanyNameInvalidCharacters);

	Config.CompanyName = TEXT("Acme");
	Config.CompanyName.AppendChar(TCHAR(7));
	Config.CompanyName += TEXT("Corp");
	Result = Config.Validate();
	TestFalse(TEXT("Bell character is invalid"), Result.bIsValid);
	TestEqual(TEXT("Error is CompanyNameInvalidCharacters"), Result.Error, EUOConfigError::CompanyNameInvalidCharacters);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigBannedWordTest, "UnpaidOvertime.Core.MatchConfig.BannedWord", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigBannedWordTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();

	Config.CompanyName = TEXT("Shitty Paper Co");
	FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("Banned word (mixed case) is rejected"), Result.bIsValid);
	TestEqual(TEXT("Error is CompanyNameBanned"), Result.Error, EUOConfigError::CompanyNameBanned);

	Config.CompanyName = TEXT("NAZI Logistics");
	Result = Config.Validate();
	TestEqual(TEXT("Banned word (upper case) is rejected"), Result.Error, EUOConfigError::CompanyNameBanned);

	TestTrue(TEXT("ContainsBannedWord is case-insensitive"), FUOMatchConfig::ContainsBannedWord(TEXT("fUcK")));
	TestFalse(TEXT("Clean name has no banned word"), FUOMatchConfig::ContainsBannedWord(TEXT("Paper Pushers Ltd")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigValidTest, "UnpaidOvertime.Core.MatchConfig.Valid", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigValidTest::RunTest(const FString& Parameters)
{
	const FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();
	const FUOConfigValidationResult Result = Config.Validate();
	TestTrue(TEXT("Valid config passes"), Result.bIsValid);
	TestEqual(TEXT("Valid config has no error"), Result.Error, EUOConfigError::None);
	TestEqual(TEXT("Default tasks per day is 6"), Config.TasksPerDay, 6);
	TestEqual(TEXT("Default day length is Normal"), Config.DayLength, EUODayLength::Normal);
	TestEqual(TEXT("Default AI difficulty is Medium"), Config.AIDifficulty, EUOAIDifficulty::Medium);
	TestEqual(TEXT("One human player"), Config.HumanPlayers, 1);
	TestEqual(TEXT("AIs fill the team to 4"), Config.GetAIPlayers(), 3);
	TestEqual(TEXT("Team size is 4"), Config.GetTotalEmployees(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigTaskBoundsTest, "UnpaidOvertime.Core.MatchConfig.TaskCountBounds", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigTaskBoundsTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();

	Config.TasksPerDay = 3;
	FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("3 tasks is below the minimum"), Result.bIsValid);
	TestEqual(TEXT("Error is TasksPerDayOutOfRange"), Result.Error, EUOConfigError::TasksPerDayOutOfRange);

	Config.TasksPerDay = 4;
	TestTrue(TEXT("4 tasks is valid"), Config.Validate().bIsValid);

	Config.TasksPerDay = 8;
	TestTrue(TEXT("8 tasks is valid"), Config.Validate().bIsValid);

	Config.TasksPerDay = 9;
	Result = Config.Validate();
	TestFalse(TEXT("9 tasks is above the maximum"), Result.bIsValid);
	TestEqual(TEXT("Error is TasksPerDayOutOfRange"), Result.Error, EUOConfigError::TasksPerDayOutOfRange);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigPlayersTest, "UnpaidOvertime.Core.MatchConfig.PlayerCount", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigPlayersTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();

	Config.HumanPlayers = 0;
	const FUOConfigValidationResult Result = Config.Validate();
	TestFalse(TEXT("Zero humans is invalid"), Result.bIsValid);
	TestEqual(TEXT("Error is PlayerCountOutOfRange"), Result.Error, EUOConfigError::PlayerCountOutOfRange);

	Config.HumanPlayers = 6;
	TestEqual(TEXT("No AI needed above the minimum team size"), Config.GetAIPlayers(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOMatchConfigSeedTest, "UnpaidOvertime.Core.MatchConfig.ResolveSeed", UO_CORE_TEST_FLAGS)
bool FUOMatchConfigSeedTest::RunTest(const FString& Parameters)
{
	FUOMatchConfig Config = UOMatchConfigTests::MakeValidConfig();

	Config.Seed = 1234;
	TestEqual(TEXT("Explicit seed is kept"), Config.ResolveSeed(99), 1234);

	Config.Seed = 0;
	TestEqual(TEXT("Seed 0 uses the random fallback"), Config.ResolveSeed(99), 99);
	TestNotEqual(TEXT("Resolved seed is never 0"), Config.ResolveSeed(0), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
