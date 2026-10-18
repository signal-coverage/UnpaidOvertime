// UNPAID: OVERTIME - AI employee tuning from base difficulty and day number.

#include "UOAIDifficulty.h"

float FUOAIDifficulty::GetSkillLevel(EUOAIDifficulty Base, int32 DayNumber)
{
	const float BaseLevel = static_cast<float>(static_cast<uint8>(Base));
	return BaseLevel + 0.25f * static_cast<float>(FMath::Max(1, DayNumber) - 1);
}

FUOAIDifficultyParams FUOAIDifficulty::Compute(EUOAIDifficulty Base, int32 DayNumber)
{
	const float Skill = GetSkillLevel(Base, DayNumber);

	FUOAIDifficultyParams Params;
	Params.WorkSpeedMultiplier = FMath::Clamp(0.7f + 0.15f * Skill, 0.5f, 1.8f);
	Params.Precision = FMath::Clamp(0.7f + 0.1f * Skill, 0.05f, 0.99f);
	Params.ThinkDelaySeconds = FMath::Clamp(3.f - 0.6f * Skill, 0.4f, 5.f);
	return Params;
}
