// UNPAID: OVERTIME - shift clock state machine: PreGame -> Shift -> Overtime -> Evaluation.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"

/**
 *  Pure day clock. Shift time runs from 09:00 to 17:00 on the fake office clock.
 *  When the shift ends with work pending the day enters Overtime, capped at +50% of the shift;
 *  hitting the cap force-closes the day. Finishing all work at any point ends the day.
 */
struct FUODayClock
{
	static constexpr float OvertimeCapFraction = 0.5f;
	static constexpr float ShiftStartMinutes = 9.f * 60.f;
	static constexpr float ShiftEndMinutes = 17.f * 60.f;
	static constexpr float MinShiftSeconds = 1.f;

	/** Shift length for a day length setting */
	static float GetBaseShiftSeconds(EUODayLength DayLength);

	/** Override (> 0) replaces the base shift length; 0 or less means "off" */
	static float ResolveShiftSeconds(EUODayLength DayLength, float OverrideSeconds);

	/** Shift length taking the uo.ShiftSecondsOverride console variable into account */
	static float GetShiftSecondsWithConsoleOverride(EUODayLength DayLength);

	/** Raw uo.ShiftSecondsOverride value (0 or less = off) */
	static float GetConsoleShiftSecondsOverride();

	/** Minutes since midnight on the fake office clock */
	static float ComputeTimeOfDayMinutes(float ElapsedSeconds, float ShiftLengthSeconds);

	/** "HH:MM", 24h, wrapping past midnight */
	static FString FormatTimeOfDay(float MinutesSinceMidnight);

	/** Starts the shift */
	void Start(float ShiftLengthSeconds);

	/** Advances the clock and returns the resulting phase */
	EUODayPhase Advance(float DeltaSeconds, bool bAllWorkDone);

	EUODayPhase GetPhase() const { return Phase; }
	float GetElapsedSeconds() const { return ElapsedSeconds; }
	float GetShiftLengthSeconds() const { return ShiftLengthSeconds; }
	float GetOvertimeCapSeconds() const { return ShiftLengthSeconds * OvertimeCapFraction; }
	float GetDayEndSeconds() const { return ShiftLengthSeconds + GetOvertimeCapSeconds(); }
	float GetRemainingShiftSeconds() const { return FMath::Max(0.f, ShiftLengthSeconds - ElapsedSeconds); }
	float GetOvertimeElapsedSeconds() const { return FMath::Max(0.f, ElapsedSeconds - ShiftLengthSeconds); }
	float GetTimeOfDayMinutes() const { return ComputeTimeOfDayMinutes(ElapsedSeconds, ShiftLengthSeconds); }

	/** True when the overtime cap closed the day with work still pending */
	bool WasForceClosed() const { return bForceClosed; }

private:

	EUODayPhase Phase = EUODayPhase::PreGame;
	float ElapsedSeconds = 0.f;
	float ShiftLengthSeconds = 0.f;
	bool bForceClosed = false;
};
