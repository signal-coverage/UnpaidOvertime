// UNPAID: OVERTIME - per-day difficulty: the endless curve plus the hardcore overtime weekend.

#include "UODayDifficulty.h"
#include "UODayCalendar.h"
#include "UODayClock.h"
#include "UOTaskGenerator.h"
#include "UOMatchConfig.h"

const FUOHardcoreTuning& FUODayDifficulty::GetHardcoreTuning()
{
	static const FUOHardcoreTuning Tuning;
	return Tuning;
}

bool FUODayDifficulty::IsHardcoreDay(int32 DayNumber)
{
	return FUODayCalendar::IsOvertimeWeekend(DayNumber);
}

float FUODayDifficulty::GetTaskDifficulty(int32 DayNumber)
{
	const float Curve = FUOTaskGenerator::GetDifficultyForDay(DayNumber);
	return IsHardcoreDay(DayNumber) ? Curve + GetHardcoreTuning().TaskDifficultyBonus : Curve;
}

float FUODayDifficulty::GetCurveShiftScale(int32 DayNumber)
{
	// Exponential approach to the floor: strictly shrinking every day, never below MinShiftScale
	const float Range = 1.f - MinShiftScale;
	const float Days = static_cast<float>(FMath::Max(1, DayNumber) - 1);
	return 1.f - Range * (1.f - FMath::Exp(-Days * ShiftShrinkPerDay / Range));
}

float FUODayDifficulty::GetShiftScale(int32 DayNumber)
{
	const float Curve = GetCurveShiftScale(DayNumber);
	return IsHardcoreDay(DayNumber) ? Curve * GetHardcoreTuning().ShiftScale : Curve;
}

float FUODayDifficulty::GetShiftSeconds(EUODayLength DayLength, int32 DayNumber, float OverrideSeconds)
{
	const float Base = FUODayClock::ResolveShiftSeconds(DayLength, OverrideSeconds);
	return FMath::Max(FUODayClock::MinShiftSeconds, Base * GetShiftScale(DayNumber));
}

float FUODayDifficulty::GetExpectedWorkSeconds(float Difficulty, int32 TasksPerDay)
{
	const int32 Tasks = FMath::Clamp(TasksPerDay, FUOMatchConfig::MinTasksPerDay, FUOMatchConfig::MaxTasksPerDay);
	int32 MinSubs = 0;
	int32 MaxSubs = 0;
	FUOTaskGenerator::GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);
	const float AverageSubs = 0.5f * static_cast<float>(MinSubs + MaxSubs);

	const float AverageBase = (FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType::PrinterMaintenance)
		+ FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType::CoffeeRefill)
		+ FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType::FileReports)) / 3.f;

	return static_cast<float>(Tasks) * AverageSubs * AverageBase * FUOTaskGenerator::GetWorkSecondsScale(Difficulty);
}

float FUODayDifficulty::GetMaxWorkSeconds(float Difficulty, int32 TasksPerDay)
{
	const int32 Tasks = FMath::Clamp(TasksPerDay, FUOMatchConfig::MinTasksPerDay, FUOMatchConfig::MaxTasksPerDay);
	int32 MinSubs = 0;
	int32 MaxSubs = 0;
	FUOTaskGenerator::GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);

	const float SlowestBase = FMath::Max3(FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType::PrinterMaintenance),
		FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType::CoffeeRefill),
		FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType::FileReports));

	return static_cast<float>(Tasks * MaxSubs) * SlowestBase * FUOTaskGenerator::GetWorkSecondsScale(Difficulty);
}

float FUODayDifficulty::ComputeWorkloadRatio(float WorkSeconds, int32 AliveEmployees, float AvailableSeconds)
{
	return WorkSeconds / static_cast<float>(FMath::Max(1, AliveEmployees)) / FMath::Max(KINDA_SMALL_NUMBER, AvailableSeconds);
}
