// UNPAID: OVERTIME - the investigation: trace texts and the AI suspicion model (visible evidence only, with noise).

#pragma once

#include "CoreMinimal.h"
#include "UOKillerTypes.h"

/** Readable text of a trace line. The name resolver turns employee ids into display names. */
struct FUOTraceText
{
	static FText GetDeviceName(EUOTraceDevice Device);
	static FText GetTerminalName(EUOTerminalKind Kind);
	/** Device a terminal shows (SecurityCamera / MovableProp have none: false) */
	static bool GetTerminalDevice(EUOTerminalKind Kind, EUOTraceDevice& OutDevice);
	static FText GetSabotageName(EUOSabotageType Type);

	/** "Doug", "Doug or Brenda (badge unclear)", "unknown" */
	static FText DescribeActor(const FUOTraceEntry& Entry, TFunctionRef<FString(int32)> NameOf);

	/** One line: "PAPER JAM (E-12) — Printer · Copy room — badge: unknown" */
	static FText Describe(const FUOTraceEntry& Entry, TFunctionRef<FString(int32)> NameOf);

	/** Plain text for logs */
	static FString ToLogString(const FUOTraceEntry& Entry, TFunctionRef<FString(int32)> NameOf);
};

/** A trace an AI knows, and how it learned it */
struct FUOKnownTrace
{
	FUOTraceEntry Entry;
	EUOEvidenceSource Source = EUOEvidenceSource::Public;
};

/** The AI saw this employee perform the act behind a trace (line of sight, lit room) */
struct FUOSeenAct
{
	int32 ActorId = INDEX_NONE;
	int32 TraceId = INDEX_NONE;
	float TimeSeconds = 0.f;
};

/** The AI saw this employee in a room (it was there too) */
struct FUOSighting
{
	int32 EmployeeId = INDEX_NONE;
	FName Room;
	float TimeSeconds = 0.f;
};

/**
 *  A killer's always-accepted public answer to the boss (shown 1 star: defense; shown 3: promotion), as one AI sees
 *  it. bKnowsFact: per the boss fairness rules, this AI could know the true answer (it was in that room / saw that
 *  event) and remembered it. bWrong only matters when it knows.
 */
struct FUOAcceptedAnswer
{
	int32 EmployeeId = INDEX_NONE;
	bool bKnowsFact = false;
	bool bWrong = false;
	FString Prompt;
};

/** Suspicion carried over from an earlier day (decays) */
struct FUOCarriedSuspicion
{
	int32 EmployeeId = INDEX_NONE;
	float Score = 0.f;
};

/** Everything one AI knows. Built only from what it observed, read on terminals, or what is public. */
struct FUOSuspicionInput
{
	int32 SelfId = INDEX_NONE;
	/** Who may be suspected (alive, not self) */
	TArray<int32> CandidateIds;
	TArray<FUOKnownTrace> Traces;
	TArray<FUOSeenAct> SeenActs;
	TArray<FUOSighting> Sightings;
	/** Killers' public answers the boss accepted today */
	TArray<FUOAcceptedAnswer> AcceptedAnswers;
	TArray<FUOCarriedSuspicion> CarriedOver;
	/** Doc 9.2 "Sospecha" (0..1): less of it means more noise and more false leads */
	float Suspicion = 1.f;
	/** Seeds the noise and the hunches (fixed per AI and meeting, so its view is stable) */
	int32 NoiseSeed = 0;
};

/** What kind of reason it is (statements put it in words) */
enum class EUOSuspicionBasis : uint8
{
	SeenAct,
	NamedTrace,
	PartialTrace,
	Presence,
	LastSeen,
	BotchedWork,
	AcceptedWrong,
	Anomaly,
	Hunch,
	Memory
};

/** One reason to suspect someone */
struct FUOSuspicionItem
{
	int32 SuspectId = INDEX_NONE;
	EUOSuspicionBasis Basis = EUOSuspicionBasis::NamedTrace;
	float Weight = 0.f;
	/** Trace behind it (INDEX_NONE for what cannot be pinned: accepted answers, hunches, memory) */
	int32 TraceId = INDEX_NONE;
	EUOEvidenceSource Source = EUOEvidenceSource::Public;
	FString Reason;
};

struct FUOSuspicionResult
{
	TArray<FUOSuspicionItem> Items;

	float GetScore(int32 SuspectId) const;
	/** Distinct pinnable trace ids supporting the suspect, strongest first */
	TArray<int32> GetEvidenceIds(int32 SuspectId, int32 MaxCount) const;
	/** Highest-scoring suspect (INDEX_NONE without any) and the runner-up score */
	int32 GetTopSuspect(float& OutScore, float& OutSecondScore) const;
};

/**
 *  AI suspicion (doc 9.2 "Sospecha"): a score per coworker from visible evidence only.
 *   - a sabotage trace naming someone (named: full weight, partial: half each);
 *   - an anonymous sabotage: everyone known to be in that room around that time (door/camera logs it read, or
 *     people it saw there itself) shares the presence weight; the same for a public "last seen near" note;
 *   - an act seen with its own eyes;
 *   - a killer's public answer the boss accepted, when this AI knows the answer is wrong (Public+Knowledge);
 *   - yesterday's suspicion, halved (memory).
 *  Rule 9, false leads, scaled by (1 - Suspicion): uneven weights, a gut feeling about someone, a botched work
 *  attempt read as suspicious, and an accepted-answer anomaly blamed on the wrong person. Thresholds scale with
 *  the AI's Suspicion parameter (FUOKillerTuning). Rule 3: the killers' work is real, so there is no fake-work or
 *  review-mismatch evidence.
 */
struct FUOSuspicionModel
{
	static constexpr float WeightSeenAct = 6.f;
	static constexpr float WeightNamed = 3.f;
	static constexpr float WeightPartial = 1.5f;
	static constexpr float WeightPresence = 2.f;
	/** Someone's botched work attempt ("0 pages printed"): a false lead for the gullible */
	static constexpr float WeightPrecisionFailure = 1.2f;
	/** The boss accepted an answer this AI knows is wrong */
	static constexpr float WeightAcceptedWrong = 3.f;
	/** "Something about that answer was off" without knowing the fact: blamed on someone at random */
	static constexpr float WeightMisreadAnomaly = 1.8f;
	/** A gut feeling about a random coworker */
	static constexpr float WeightHunch = 5.f;
	/** Fraction of yesterday's score that still counts today */
	static constexpr float CarryOverFactor = 0.3f;
	/** Chance of a hunch and of misreading an anomaly, at zero Suspicion (scales down to 0 at full Suspicion) */
	static constexpr float HunchChance = 0.7f;
	static constexpr float MisreadChance = 0.6f;
	/** +- this fraction of each evidence weight, at zero Suspicion */
	static constexpr float WeightNoise = 0.6f;
	/** Someone seen in the room within this many seconds of an anonymous sabotage is a suspect */
	static constexpr float PresenceWindowSeconds = 25.f;

	static bool IsSabotageTrace(EUOTraceKind Kind);
	static bool IsNoOutputTrace(EUOTraceKind Kind);

	static FUOSuspicionResult Evaluate(const FUOSuspicionInput& Input);

};
