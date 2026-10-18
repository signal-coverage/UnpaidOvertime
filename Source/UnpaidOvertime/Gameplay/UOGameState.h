// UNPAID: OVERTIME - replicated-ready match state (written by the server, read by HUD and clients).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "UOCoreTypes.h"
#include "UOTaskTypes.h"
#include "UOEvidenceTypes.h"
#include "UOGameState.generated.h"

class AUOPlayerState;

/**
 *  Authoritative day state. Only the game mode and the task manager write here;
 *  UI reads it and never decides anything.
 */
UCLASS()
class AUOGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	AUOGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ~ Read access

	const FString& GetCompanyName() const { return CompanyName; }
	EUODayPhase GetPhase() const { return Phase; }
	int32 GetDayNumber() const { return DayNumber; }
	float GetShiftElapsedSeconds() const { return ShiftElapsedSeconds; }
	float GetShiftLengthSeconds() const { return ShiftLengthSeconds; }
	bool IsUnpaidOvertime() const { return bUnpaidOvertime; }
	bool WasDayForceClosed() const { return bDayForceClosed; }
	float GetOvertimeSeconds() const { return FMath::Max(0.f, ShiftElapsedSeconds - ShiftLengthSeconds); }
	bool IsWorkPhase() const { return Phase == EUODayPhase::Shift || Phase == EUODayPhase::Overtime; }

	/** Fake office clock text ("09:00") */
	FString GetClockText() const;

	const TArray<FUOTaskInstance>& GetTasks() const { return Tasks; }
	const TArray<TObjectPtr<AUOPlayerState>>& GetEmployees() const { return Employees; }
	const TArray<FUOEvidenceEntry>& GetEvidenceLog() const { return EvidenceLog; }

	AUOPlayerState* FindEmployee(int32 EmployeeId) const;
	int32 CountAliveEmployees() const;

	// ~ Server-side writers

	void SetCompanyName(const FString& InCompanyName) { CompanyName = InCompanyName; }
	void SetDayNumber(int32 InDayNumber) { DayNumber = InDayNumber; }
	void SetPhase(EUODayPhase InPhase) { Phase = InPhase; }
	void SetClock(float InElapsedSeconds, float InShiftLengthSeconds);
	void SetUnpaidOvertime(bool bInUnpaid) { bUnpaidOvertime = bInUnpaid; }
	void SetDayForceClosed(bool bInForceClosed) { bDayForceClosed = bInForceClosed; }

	/** Mutable task list for the server-side task manager */
	TArray<FUOTaskInstance>& GetMutableTasks() { return Tasks; }
	void SetTasks(const TArray<FUOTaskInstance>& InTasks) { Tasks = InTasks; }

	void AddEmployee(AUOPlayerState* Employee);
	void ClearEmployees() { Employees.Reset(); }

	void AddEvidence(const FUOEvidenceEntry& Entry) { EvidenceLog.Add(Entry); }
	void ClearEvidence() { EvidenceLog.Reset(); }

protected:

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	FString CompanyName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	EUODayPhase Phase = EUODayPhase::PreGame;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	int32 DayNumber = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Clock")
	float ShiftElapsedSeconds = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Clock")
	float ShiftLengthSeconds = 0.f;

	/** Set when the day ran into overtime: the "unpaid overtime" penalty flag */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Clock")
	bool bUnpaidOvertime = false;

	/** Set when the overtime cap closed the day with work still pending */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Clock")
	bool bDayForceClosed = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Tasks")
	TArray<FUOTaskInstance> Tasks;

	/** Every employee of the day, humans and AIs */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employees")
	TArray<TObjectPtr<AUOPlayerState>> Employees;

	/** Trace of completed sub-objectives and item pickups/drops */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Evidence")
	TArray<FUOEvidenceEntry> EvidenceLog;
};
