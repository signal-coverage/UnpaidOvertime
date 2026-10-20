// UNPAID: OVERTIME - shift clock state machine.

#include "UODayClock.h"
#include "HAL/IConsoleManager.h"

namespace UODayClockPrivate
{
	static TAutoConsoleVariable<float> CVarShiftSecondsOverride(
		TEXT("uo.ShiftSecondsOverride"),
		0.f,
		TEXT("Overrides the base shift length in seconds for quick testing (0 = use the configured day length; the per-day shift scale still applies). Applies to the next day started."),
		ECVF_Default);
}

float FUODayClock::GetBaseShiftSeconds(EUODayLength DayLength)
{
	switch (DayLength)
	{
	case EUODayLength::Short:	return 480.f;
	case EUODayLength::Long:	return 1080.f;
	default:					return 720.f;
	}
}

float FUODayClock::ResolveShiftSeconds(EUODayLength DayLength, float OverrideSeconds)
{
	return OverrideSeconds > 0.f ? OverrideSeconds : GetBaseShiftSeconds(DayLength);
}

float FUODayClock::GetShiftSecondsWithConsoleOverride(EUODayLength DayLength)
{
	return ResolveShiftSeconds(DayLength, UODayClockPrivate::CVarShiftSecondsOverride.GetValueOnGameThread());
}

float FUODayClock::GetConsoleShiftSecondsOverride()
{
	return UODayClockPrivate::CVarShiftSecondsOverride.GetValueOnGameThread();
}

float FUODayClock::ComputeTimeOfDayMinutes(float ElapsedSeconds, float ShiftLengthSeconds)
{
	const float SafeShift = FMath::Max(MinShiftSeconds, ShiftLengthSeconds);
	const float ShiftMinutes = ShiftEndMinutes - ShiftStartMinutes;
	return ShiftStartMinutes + (FMath::Max(0.f, ElapsedSeconds) / SafeShift) * ShiftMinutes;
}

FString FUODayClock::FormatTimeOfDay(float MinutesSinceMidnight)
{
	const int32 TotalMinutes = FMath::FloorToInt(FMath::Max(0.f, MinutesSinceMidnight) + 1.e-3f);
	const int32 Hours = (TotalMinutes / 60) % 24;
	const int32 Minutes = TotalMinutes % 60;
	return FString::Printf(TEXT("%02d:%02d"), Hours, Minutes);
}

void FUODayClock::Start(float InShiftLengthSeconds)
{
	ShiftLengthSeconds = FMath::Max(MinShiftSeconds, InShiftLengthSeconds);
	ElapsedSeconds = 0.f;
	bForceClosed = false;
	Phase = EUODayPhase::Shift;
}

EUODayPhase FUODayClock::Advance(float DeltaSeconds, bool bAllWorkDone)
{
	if (Phase != EUODayPhase::Shift && Phase != EUODayPhase::Overtime)
	{
		return Phase;
	}

	if (bAllWorkDone)
	{
		Phase = EUODayPhase::Evaluation;
		return Phase;
	}

	ElapsedSeconds += FMath::Max(0.f, DeltaSeconds);

	if (Phase == EUODayPhase::Shift && ElapsedSeconds >= ShiftLengthSeconds)
	{
		Phase = EUODayPhase::Overtime;
	}

	if (Phase == EUODayPhase::Overtime && ElapsedSeconds >= GetDayEndSeconds())
	{
		ElapsedSeconds = GetDayEndSeconds();
		bForceClosed = true;
		Phase = EUODayPhase::Evaluation;
	}

	return Phase;
}
