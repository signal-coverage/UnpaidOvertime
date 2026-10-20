// UNPAID: OVERTIME - UEvidenceSubsystem (design doc 11): device logs, who saw what, pins, dark rooms and moved props.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UOKillerTypes.h"
#include "UOBossQuestions.h"
#include "UOEvidenceSubsystem.generated.h"

class AUOGameState;
class AUOGameMode;
class AUOPlayerState;
class APawn;

/** A trace to record (server side). TrueActorId stays in the server audit; the entry shows only what Detail allows. */
struct FUOTraceRecord
{
	EUOTraceDevice Device = EUOTraceDevice::ActivityBoard;
	EUOTraceKind Kind = EUOTraceKind::WorkReported;
	int32 TrueActorId = INDEX_NONE;
	EUOTraceDetail Detail = EUOTraceDetail::Named;
	FName Room;
	FName StationId;
	FString Subject;
	int32 ValueA = 0;
	int32 ValueB = 0;
	/** A deliberate act (sabotage, kill): employees with line of sight see who did it */
	bool bAct = false;
	/** When set, the entry shows exactly these employees (public notes listing several people) */
	TArray<int32> ShownOverride;
};

/**
 *  Server-side owner of every trace (the replicated log lives in AUOGameState::TraceLog). Every entry comes from a
 *  real event. When something happens in a room, the AIs in that room learn it ("witnessed"), and those with a clear
 *  line of sight to the actor of a deliberate act remember who did it. AIs learn everything else only by reading
 *  terminals (AUOAIController) or from public information. Presence transitions feed the door and camera logs.
 */
UCLASS()
class UUOEvidenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UUOEvidenceSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Rooms whose doors have badge readers (door log) */
	static const TArray<FName>& GetDoorRooms();

	/** New day: empty logs, pins and audit; the day's seed drives decoys */
	void ResetForDay(int32 InDayNumber, int32 InSeed);

	/** Records a trace; returns its id */
	int32 Record(const FUOTraceRecord& Record);

	/** Presence sampling during the shift (door and camera logs) */
	void TickShift(float DeltaSeconds);

	/** Audit only (logs, dev automation): who really did it */
	int32 GetTrueActor(int32 TraceId) const;

	// ~ Rooms

	FName GetRoomOf(const APawn* Pawn) const;
	void SetRoomDark(FName Room, bool bDark);
	bool IsRoomDark(FName Room) const { return DarkRooms.Contains(Room); }

	/** Alive employees (other than Actor and Ignore) who can see Actor right now */
	int32 CountWitnesses(const APawn* Actor, const APawn* Ignore, TArray<int32>* OutWitnessIds = nullptr) const;
	bool CanSee(const APawn* Observer, const APawn* Target) const;

	// ~ Moved props (boss knowledge)

	void AddPropMove(const FUOPropMove& Move) { PropMoves.Add(Move); }
	const TArray<FUOPropMove>& GetPropMoves() const { return PropMoves; }

	// ~ Pins

	/** Pin or unpin a trace as evidence for an accusation (max FUOKillerTuning::MaxPins). False when rejected. */
	bool TogglePin(AUOPlayerState* Employee, int32 TraceId);

	/** "Doug from Sales" for log lines */
	FString NameOf(int32 EmployeeId) const;

protected:

	int32 NextTraceId = 1;
	int32 DayNumber = 1;
	int32 Seed = 1;
	TMap<int32, int32> TrueActors;
	TSet<FName> DarkRooms;
	TArray<FUOPropMove> PropMoves;
	TMap<int32, FName> LastRooms;
	float SampleTimer = 0.f;

	AUOGameState* GetUOGameState() const;
	AUOGameMode* GetGameMode() const;
	APawn* GetPawnOf(int32 EmployeeId) const;

	void NotifyWitnesses(const FUOTraceEntry& Entry, const FUOTraceRecord& Record);
	bool IsCameraRoomOnline(FName Room) const;
};
