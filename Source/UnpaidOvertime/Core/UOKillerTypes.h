// UNPAID: OVERTIME - hidden role, sabotage and trace types (the killer and the investigation, design doc 6).

#pragma once

#include "CoreMinimal.h"
#include "UOKillerTypes.generated.h"

/** Secret role of an employee. Only the employee itself and the server may know it. */
UENUM(BlueprintType)
enum class EUORole : uint8
{
	/** What anyone else sees */
	Unknown,
	Employee,
	/** A saboteur ("el asesino"): floor(employees / 3) per match, at least one. Saboteurs know each other. */
	Killer
};

/** Who may be picked as the killer (match setup). Random is the design default; AlwaysHuman is a debug option. */
UENUM(BlueprintType)
enum class EUOKillerAssignment : uint8
{
	Random,
	AlwaysAI,
	AlwaysHuman
};

/** Sabotages of design doc 6.3 */
UENUM(BlueprintType)
enum class EUOSabotageType : uint8
{
	/** The printer's task cannot progress until someone clears the jam */
	JamPrinter,
	/** A finished report is overwritten: the sub-objective is undone and its credit lost */
	CorruptFile,
	/** A station is locked for a while: everyone has to wait */
	LockStation,
	/** A room's camera and lights go out for a while: a blind, dark zone */
	CutCamera,
	/** A prop changes rooms: the inventory notices, and boss questions about it become trick questions */
	MoveObject,
	Count UMETA(Hidden)
};

/** Device that keeps a log a terminal can show */
UENUM(BlueprintType)
enum class EUOTraceDevice : uint8
{
	/** Public work reports (the same feed everyone sees on the HUD) */
	ActivityBoard,
	PrinterLog,
	FileHistory,
	/** Badge readers on room doors */
	DoorLog,
	CameraLog,
	/** Supply sheet: items signed out, coffee packs, missing props */
	Inventory
};

/** What a trace entry records */
UENUM(BlueprintType)
enum class EUOTraceKind : uint8
{
	// Activity board (public)
	WorkReported,
	// Printer log
	PrintOk,
	PrintNoOutput,
	PrinterJam,
	PrinterJamCleared,
	// File history
	FileSaved,
	FileNoChange,
	FileOverwritten,
	// Door log
	DoorEntry,
	StationLocked,
	StationUnlocked,
	// Camera log
	CameraSighting,
	CameraOffline,
	CameraOnline,
	/** A dismissed employee was found (optional kill): time and room of the incident */
	BodyFound,
	// Inventory
	ItemSignedOut,
	CoffeeLogged,
	CoffeeNoChange,
	PropMissing,
	/** Public note after a dismissal: who was last seen near the room (the more kills, the likelier) */
	LastSeenNear
};

/** How much a trace says about who did it (doc 6.5: low-trace early days, more visible later) */
UENUM(BlueprintType)
enum class EUOTraceDetail : uint8
{
	/** Time and place only */
	Anonymous,
	/** Two candidates: the real one and a decoy ("badge #..: Doug or Brenda") */
	Partial,
	Named
};

/** How an AI came to know a trace (every AI suspicion item logs its source for fairness audits) */
UENUM(BlueprintType)
enum class EUOEvidenceSource : uint8
{
	/** Everyone sees it (activity board, the review, evidence presented at a meeting) */
	Public,
	/** The AI was in the room when it happened */
	Witnessed,
	/** The AI read it on a terminal */
	Terminal,
	/** Public, and the AI knew better: the boss accepted an answer it knows is wrong */
	PublicKnowledge,
	/** A gut feeling or a misread anomaly (false leads; more frequent with less Suspicion) */
	Hunch,
	/** Still suspicious from an earlier day */
	Memory
};

/** Rock-paper-scissors pick (meeting tie duel) */
UENUM(BlueprintType)
enum class EUORPSChoice : uint8
{
	None,
	Rock,
	Paper,
	Scissors
};

/** In-world consultable device (plus sabotage-only targets) */
UENUM(BlueprintType)
enum class EUOTerminalKind : uint8
{
	/** Reception PC: the activity board */
	ActivityBoard,
	/** Copy-room print server panel */
	PrintServer,
	/** Any desk PC: file history (also where files get corrupted) */
	DeskComputer,
	/** Lobby badge reader: door log */
	BadgeReader,
	/** Manager's office: camera log */
	SecurityMonitor,
	/** Supply room inventory sheet */
	InventorySheet,
	/** Ceiling camera of a room: not readable, only a sabotage target */
	SecurityCamera,
	/** A prop the killer can move (plants): not readable, only a sabotage target */
	MovableProp
};

/**
 *  One line of a device log. Public: never stores who really did it when the device could not know
 *  (the server keeps that in its own audit). ShownActorIds is empty (anonymous), one id (named) or two (partial).
 */
USTRUCT(BlueprintType)
struct FUOTraceEntry
{
	GENERATED_BODY()

	/** Unique within the day (pins and accusations reference it) */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	int32 Id = INDEX_NONE;

	/** Shift seconds */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	float TimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Trace")
	FString ClockTime;

	UPROPERTY(BlueprintReadOnly, Category="Trace")
	int32 DayNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category="Trace")
	EUOTraceDevice Device = EUOTraceDevice::ActivityBoard;

	UPROPERTY(BlueprintReadOnly, Category="Trace")
	EUOTraceKind Kind = EUOTraceKind::WorkReported;

	UPROPERTY(BlueprintReadOnly, Category="Trace")
	EUOTraceDetail Detail = EUOTraceDetail::Named;

	/** Who the device says it was (see struct comment) */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	TArray<int32> ShownActorIds;

	/** Room where it happened (UORoom id) */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	FName Room;

	/** Station involved, if any */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	FName StationId;

	/** Free label: station / workstation / camera / prop display name */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	FString Subject;

	/** Kind-specific numbers (report number, counts before/after, seconds offline...) */
	UPROPERTY(BlueprintReadOnly, Category="Trace")
	int32 ValueA = 0;

	UPROPERTY(BlueprintReadOnly, Category="Trace")
	int32 ValueB = 0;

	bool IsNamed() const { return ShownActorIds.Num() == 1; }
	bool Mentions(int32 EmployeeId) const { return ShownActorIds.Contains(EmployeeId); }
};
