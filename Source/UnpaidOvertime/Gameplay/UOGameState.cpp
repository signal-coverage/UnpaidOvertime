// UNPAID: OVERTIME - replicated-ready match state.

#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UODayClock.h"
#include "Net/UnrealNetwork.h"

AUOGameState::AUOGameState()
{
}

void AUOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUOGameState, CompanyName);
	DOREPLIFETIME(AUOGameState, Phase);
	DOREPLIFETIME(AUOGameState, DayNumber);
	DOREPLIFETIME(AUOGameState, ShiftElapsedSeconds);
	DOREPLIFETIME(AUOGameState, ShiftLengthSeconds);
	DOREPLIFETIME(AUOGameState, bUnpaidOvertime);
	DOREPLIFETIME(AUOGameState, bDayForceClosed);
	DOREPLIFETIME(AUOGameState, Tasks);
	DOREPLIFETIME(AUOGameState, Employees);
	DOREPLIFETIME(AUOGameState, EvidenceLog);
	DOREPLIFETIME(AUOGameState, Evaluation);
	DOREPLIFETIME(AUOGameState, TraceLog);
	DOREPLIFETIME(AUOGameState, RevealedKillers);
	DOREPLIFETIME(AUOGameState, RevealedEmployees);
}

FString AUOGameState::GetClockText() const
{
	return FUODayClock::FormatTimeOfDay(FUODayClock::ComputeTimeOfDayMinutes(ShiftElapsedSeconds, ShiftLengthSeconds));
}

AUOPlayerState* AUOGameState::FindEmployee(int32 EmployeeId) const
{
	for (AUOPlayerState* Employee : Employees)
	{
		if (Employee && Employee->GetEmployeeId() == EmployeeId)
		{
			return Employee;
		}
	}
	return nullptr;
}

FString AUOGameState::GetEmployeeName(int32 EmployeeId) const
{
	const AUOPlayerState* Employee = FindEmployee(EmployeeId);
	return Employee ? Employee->GetDisplayName() : FString();
}

int32 AUOGameState::CountAliveEmployees() const
{
	int32 Alive = 0;
	for (const AUOPlayerState* Employee : Employees)
	{
		Alive += (Employee && Employee->IsAlive()) ? 1 : 0;
	}
	return Alive;
}

void AUOGameState::SetClock(float InElapsedSeconds, float InShiftLengthSeconds)
{
	ShiftElapsedSeconds = InElapsedSeconds;
	ShiftLengthSeconds = InShiftLengthSeconds;
}

void AUOGameState::AddEmployee(AUOPlayerState* Employee)
{
	if (Employee)
	{
		Employees.AddUnique(Employee);
	}
}

bool AUOGameState::IsRoleRevealed(int32 EmployeeId, bool& bOutKiller) const
{
	bOutKiller = RevealedKillers.Contains(EmployeeId);
	return bOutKiller || RevealedEmployees.Contains(EmployeeId);
}

void AUOGameState::RevealRole(int32 EmployeeId, bool bKiller)
{
	(bKiller ? RevealedKillers : RevealedEmployees).AddUnique(EmployeeId);
}
