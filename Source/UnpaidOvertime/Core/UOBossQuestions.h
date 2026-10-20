// UNPAID: OVERTIME - the boss's questions: generated from the real office and the day's events, always fair.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"
#include "UOEvaluationTypes.h"
#include "UOAIDifficulty.h"

/** Kinds of props the level tags with UOProp.<Kind> (counted at runtime, so facts never drift from the level) */
enum class EUOPropKind : uint8
{
	Plant,
	Printer,
	CoffeeMaker,
	Fridge,
	Microwave,
	FilingCabinet,
	Chair,
	Poster,
	Clock,
	Mug,
	RecycleBin,
	WaterCooler,
	VendingMachine,
	Sofa,
	Whiteboard,
	Count
};

/** Attribute a prop can carry (UOColor.<X> / UOText.<X> / UOShape.<X> tags) */
enum class EUOPropAttribute : uint8
{
	Color,
	Text,
	Shape
};

/** Room ids (UORoom.<Id> tags) and their display names */
struct FUORooms
{
	/** "Kitchen" -> "Break room"; unknown ids are shown as they are */
	static FText GetDisplayName(FName RoomId);

	/** Every room id the level script tags */
	static const TArray<FName>& GetKnownRooms();
};

struct FUOPropKinds
{
	/** "Plant" -> Plant (case-insensitive). False for unknown names. */
	static bool Parse(const FString& Name, EUOPropKind& OutKind);
	static FText GetSingularName(EUOPropKind Kind);
	static FText GetPluralName(EUOPropKind Kind);

	/** Kinds the boss asks to count */
	static bool IsCountable(EUOPropKind Kind);

	/** Countable kinds that are hard to count (many small things); only asked on later days */
	static bool IsHardToCount(EUOPropKind Kind);

	/** Kinds the boss asks the color of */
	static bool HasAskableColor(EUOPropKind Kind);
};

/** One tagged prop in a room */
struct FUOPropInstance
{
	FName Room;
	EUOPropKind Kind = EUOPropKind::Plant;
	FName Color;
	FName Text;
	FName Shape;
};

/** Static facts of the office, built from the level's tagged actors */
struct FUOOfficeFacts
{
	TArray<FUOPropInstance> Props;

	void Add(const FUOPropInstance& Prop) { Props.Add(Prop); }

	int32 Count(FName Room, EUOPropKind Kind) const;

	/** Distinct non-empty values of an attribute for a kind in a room, sorted */
	TArray<FName> GetValues(FName Room, EUOPropKind Kind, EUOPropAttribute Attribute) const;

	/** Rooms containing at least one prop of the kind, sorted */
	TArray<FName> GetRoomsWith(EUOPropKind Kind) const;

	/** Every room with at least one prop, sorted */
	TArray<FName> GetRooms() const;
};

/** One stay of an employee in a room, in shift seconds */
struct FUOPresenceInterval
{
	FName Room;
	float EnterTime = 0.f;
	float ExitTime = 0.f;
};

/** Who was in which room and when (sampled during the shift) */
struct FUOPresenceLog
{
	/** Slack around an interval when checking presence (covers the sampling period) */
	static constexpr float ToleranceSeconds = 1.f;

	TMap<int32, TArray<FUOPresenceInterval>> ByEmployee;

	void Reset() { ByEmployee.Reset(); }

	/** Sample: the employee is in Room (NAME_None = outside every room) at Time */
	void Record(int32 EmployeeId, FName Room, float Time);

	bool WasIn(int32 EmployeeId, FName Room, float Time) const;
	bool HasVisited(int32 EmployeeId, FName Room) const;
	TArray<FName> GetVisitedRooms(int32 EmployeeId) const;
};

enum class EUOBossEventKind : uint8
{
	CompletedWork,
	PickedUpItem
};

/** Something an employee did in a room (from the evidence log) */
struct FUOBossEvent
{
	float Time = 0.f;
	FString ClockTime;
	int32 ActorId = INDEX_NONE;
	FString ActorName;
	FName Room;
	EUOBossEventKind Kind = EUOBossEventKind::CompletedWork;
	EUOStationType StationType = EUOStationType::Printer;
	EUOItemType Item = EUOItemType::None;
};

/** A prop the killer moved to another room during the shift ("move objects" sabotage) */
struct FUOPropMove
{
	float Time = 0.f;
	EUOPropKind Kind = EUOPropKind::Plant;
	FName FromRoom;
	FName ToRoom;
};

/** Everything the generator may draw from */
struct FUOBossKnowledge
{
	/** Facts as the level was built this morning (moves are applied per employee, see MakeEmployeeView) */
	FUOOfficeFacts Facts;
	/** Props moved today, in time order */
	TArray<FUOPropMove> Moves;
	FUOPresenceLog Presence;
	TArray<FUOBossEvent> Events;
	/** (employee id, display name) of the day's employees */
	TArray<TPair<int32, FString>> Employees;
	FString CompanyName;
};

/** Difficulty knobs for one question */
struct FUOBossTuning
{
	int32 NumOptions = 3;
	float TimeLimitSeconds = 15.f;
	/** Hard-to-count props are allowed */
	bool bAllowHardFacts = false;
	/** Count distractors sit right next to the answer */
	bool bCloseDistractors = false;
	/** Event questions name the clock time even when not needed to disambiguate */
	bool bTimeQualifiedEvents = false;
};

/** A generated question */
struct FUOBossQuestion
{
	bool bValid = false;
	EUOBossQuestionCategory Category = EUOBossQuestionCategory::PropCount;
	EUOQuestionPurpose Purpose = EUOQuestionPurpose::Defense;
	FText Prompt;
	TArray<FText> Options;
	int32 CorrectIndex = INDEX_NONE;
	float TimeLimitSeconds = 0.f;
	/** Why this question is fair for the asked employee (logged) */
	FString Rationale;
	/** Room the fact or event is about (NAME_None: common knowledge, the company name) */
	FName SubjectRoom;
	/** Event questions: when it happened (shift seconds); static facts: -1 */
	float SubjectTime = -1.f;
};

/**
 *  Fairness rule (design doc 5): a question never depends on something the asked employee could not have seen.
 *   - Static facts are only asked about rooms the employee entered at least once.
 *   - Event questions are only asked about events that happened in a room while the employee was in it
 *     (and never about the employee's own actions).
 *  Defense and tiebreak questions use static facts; promotion questions use witnessed events (falling back to
 *  hard static facts when nothing was witnessed). Deterministic for a given seed.
 */
struct FUOBossQuestionGenerator
{
	static constexpr int32 MinOptions = 3;
	static constexpr int32 MaxOptions = 4;

	/** Counts above this are never asked: nobody can count that many from memory (hard days allow a bit more) */
	static constexpr int32 MaxAskedCount = 6;
	static constexpr int32 MaxAskedCountHard = 10;

	/** Options, timer and category hardness by purpose and day */
	static FUOBossTuning GetTuning(EUOQuestionPurpose Purpose, int32 DayNumber);

	/** Picks one fair question. Always valid (falls back to the company name). */
	static FUOBossQuestion Generate(const FUOBossKnowledge& Knowledge, int32 AskedEmployeeId, EUOQuestionPurpose Purpose, int32 DayNumber, int32 Seed);

	/** Every fair question of the purpose's tier (without fallbacks), for inspection and tests */
	static TArray<FUOBossQuestion> GenerateCandidates(const FUOBossKnowledge& Knowledge, int32 AskedEmployeeId, EUOQuestionPurpose Purpose,
		const FUOBossTuning& Tuning, int32 Seed);

	/**
	 *  The office as this employee last saw it: a move is applied to a room only if the employee was in that room after
	 *  the move (so trick questions are answered from what the employee actually saw). OutTrickNotes lists the
	 *  "<Room>/<Kind>" pairs the employee saw changed.
	 */
	static FUOOfficeFacts MakeEmployeeView(const FUOBossKnowledge& Knowledge, int32 EmployeeId, TSet<FString>* OutTrickNotes = nullptr);

	/** True when the employee was in the event's room when it happened (and did not do it themselves) */
	static bool IsEventWitnessed(const FUOBossKnowledge& Knowledge, int32 EmployeeId, const FUOBossEvent& Event);

	/**
	 *  Could this employee know the true answer (by the same fairness rules)? Static facts: they entered the room;
	 *  events: they were in the room then; the company name: everyone. Used by AIs judging a public answer.
	 */
	static bool DoesEmployeeKnow(const FUOBossKnowledge& Knowledge, const FUOBossQuestion& Question, int32 EmployeeId);

	/** Display text of an attribute value ("Silver", "“SYNERGY”", "Square") */
	static FText GetValueText(EUOPropAttribute Attribute, FName Value);
};

/** How an AI answers: it only gets questions about what it saw; accuracy models its memory */
struct FUOAIAnswerModel
{
	static float GetCorrectChance(const FUOAIDifficultyParams& Params, EUOQuestionPurpose Purpose);

	/** Seconds the AI takes, for a uniform random value in 0..1. Always below the time limit. */
	static float GetAnswerSeconds(const FUOAIDifficultyParams& Params, float TimeLimitSeconds, float Random01);

	/** Option the AI picks: the correct one if bCorrect, else a wrong one chosen with Random01 */
	static int32 PickOption(bool bCorrect, int32 CorrectIndex, int32 NumOptions, float Random01);
};
