// UNPAID: OVERTIME - the killers' rules: role assignment, sabotage cooldowns and heat, fake rating, kills.

#pragma once

#include "CoreMinimal.h"
#include "UOKillerTypes.h"

/**
 *  Every killer/investigation number in one place (design doc 6.5: cheap, low-trace sabotage on early days,
 *  more expensive and more visible later; overtime lowers cooldowns per doc 8).
 */
struct FUOKillerTuning
{
	// ~ Sabotage
	static constexpr float MaxHeat = 100.f;
	/** Heat cools down while the killer behaves */
	static constexpr float HeatDecayPerSecond = 1.2f;
	/** Doc 8: the killer gains advantages in overtime (shorter cooldowns, sabotage and kills alike) */
	static constexpr float OvertimeCooldownScale = 0.6f;
	/** No sabotage right at the start of the shift (everyone is still in the lobby) */
	static constexpr float SabotageGraceSeconds = 15.f;

	/** Base cooldown of a sabotage type on day 1 (seconds) */
	static float GetBaseCooldown(EUOSabotageType Type);
	/** Base heat of a sabotage type on day 1 */
	static float GetBaseHeat(EUOSabotageType Type);
	/** Seconds of holding F to perform it */
	static float GetHoldSeconds(EUOSabotageType Type);
	/** Cooldown and heat grow with the day: 0.8 on day 1, +0.1 per day, capped at 1.6 */
	static float GetDayCostScale(int32 DayNumber);
	/** Days 1-2 anonymous, 3-4 partial (two candidates), 5+ named */
	static EUOTraceDetail GetTraceDetail(int32 DayNumber);

	static constexpr float JamFixSeconds = 3.f;
	static constexpr float LockSeconds = 25.f;
	static constexpr float CameraOutageSeconds = 30.f;
	/** Fraction of the normal light intensity left in a room whose lights were cut */
	static constexpr float BlackoutLightScale = 0.04f;

	// ~ Fake rating (killers only)
	/** The human killer's private choice times out to the honest value */
	static constexpr float FakeRatingTimeLimitSeconds = 25.f;

	// ~ Kills (doc 6.4, optional rule): no daily limit, a cooldown and the no-witness rule
	/** No kill before this many shift seconds, and this long between two kills of the same killer */
	static constexpr float KillCooldownSeconds = 120.f;
	static constexpr float KillRange = 230.f;
	static constexpr float KillHoldSeconds = 1.5f;
	/** Another alive employee with line of sight within this radius is a witness */
	static constexpr float WitnessRadius = 1500.f;
	static constexpr int32 MaxWitnesses = 0;
	/** Seconds before a dismissal that count for the public "last seen near" note */
	static constexpr float LastSeenWindowSeconds = 40.f;
	static float GetKillCooldown(bool bOvertime);
	/** Every kill this match makes the next one easier to trace: the day's detail plus one step per earlier kill */
	static EUOTraceDetail GetKillTraceDetail(int32 DayNumber, int32 KillsBefore);
	/** Chance that the KillNumber-th kill of a killer (1 = first) leaves a public "last seen near" note */
	static float GetLastSeenNoteChance(int32 KillNumber);

	// ~ AI killer
	/** Chance per minute of considering a sabotage, scaled by Cunning (0.6x .. 1.4x) */
	static constexpr float AISabotagePerMinute = 1.5f;
	/** An AI killer considers a dismissal this often (per minute; the gap between two considerations is capped at 4 minutes) */
	static constexpr float AIKillPerMinute = 0.25f;
	/**
	 *  Chance it actually goes for the kill at each consideration. Rare by design: in the balance simulation one
	 *  dismissal swings a match about 40 points towards the killers (it removes an innocent and the emergency meeting
	 *  is a coin flip with a rigged tiebreak), so with kills on the AIs only strike now and then.
	 */
	static constexpr float AIKillCommitChance = 0.004f;
	/** An AI killer votes to expel a suspect from this fraction of the normal vote threshold on (it frames with little evidence) */
	static constexpr float KillerVoteThresholdFactor = 0.5f;

	// ~ Investigation
	static constexpr int32 MaxPins = 6;
	/** An AI votes to expel its top suspect when the score reaches this (lower with more Suspicion) */
	static float GetVoteThreshold(float Suspicion);
	/** AI terminal visits per minute during the shift */
	static float GetTerminalVisitsPerMinute(float Suspicion);
	static constexpr float TerminalReadSeconds = 4.f;

	/** floor(employees / 3), at least 1: 4-5 employees -> 1 killer, 6-8 -> 2 */
	static int32 GetKillerCount(int32 TotalEmployees);

};

/** A possible killer */
struct FUORoleCandidate
{
	int32 EmployeeId = INDEX_NONE;
	bool bIsHuman = false;
};

struct FUORoleRules
{
	/**
	 *  Count killers, seeded and uniform among all candidates (the human may be one). AlwaysHuman puts the human
	 *  among them (the rest are uniform among the others); AlwaysAI keeps humans out (falling back to anyone when
	 *  there are not enough). Returned sorted. Empty only for an empty roster.
	 */
	static TArray<int32> PickKillers(const TArray<FUORoleCandidate>& Candidates, EUOKillerAssignment Assignment, int32 Seed, int32 Count);
};

/** Why a killer action is not possible right now */
enum class EUOKillerBlock : uint8
{
	None,
	CoolingDown,
	TooMuchHeat,
	TooEarly,
	Disabled,
	Witnesses,
	OutOfRange,
	/** Killers never target each other */
	Accomplice
};

/** One killer's bookkeeping (server only) */
struct FUOKillerState
{
	float Heat = 0.f;
	/** Shift time each sabotage type is ready again */
	float ReadyAt[static_cast<int32>(EUOSabotageType::Count)] = {};
	int32 KillsToday = 0;
	float KillReadyAt = 0.f;
	int32 SabotagesToday = 0;

	/** New day: heat, cooldowns and daily counters reset */
	void ResetForDay();
	void Tick(float DeltaSeconds);
};

struct FUOSabotageRules
{
	static float GetCooldown(EUOSabotageType Type, int32 DayNumber, bool bOvertime);
	static float GetHeatCost(EUOSabotageType Type, int32 DayNumber);
	static float GetRemainingCooldown(const FUOKillerState& State, EUOSabotageType Type, float Now);
	static EUOKillerBlock CanSabotage(const FUOKillerState& State, EUOSabotageType Type, float Now, int32 DayNumber);
	/** Spends heat and starts the cooldown */
	static void Apply(FUOKillerState& State, EUOSabotageType Type, float Now, int32 DayNumber, bool bOvertime);
};

struct FUOKillRules
{
	static EUOKillerBlock CanKill(bool bEnabled, const FUOKillerState& State, float Now, int32 Witnesses, float Distance, bool bVictimIsAccomplice);
	/** Counts the kill and starts the cooldown (shorter in overtime) */
	static void Apply(FUOKillerState& State, float Now, bool bOvertime);
};

/** What a killer may show at the review: 1, 2 or 3 stars, freely (never 0) */
struct FUOFakeRatingOffer
{
	/** Stars its real (counted) work earned; hidden from everyone else */
	int32 RealStars = 0;
	/** Shown when the choice times out: the real value, but never below 1 */
	int32 DefaultStars = 1;
	TArray<int32> Allowed;

	bool IsAllowed(int32 Stars) const { return Allowed.Contains(Stars); }
};

struct FUOFakeRating
{
	static FUOFakeRatingOffer MakeOffer(int32 RealStars);

	/** "Done" shown on the review: the real count when it matches the shown stars, else the smallest count that would */
	static int32 GetDisplayedCompleted(int32 ShownStars, int32 RealCompleted, int32 TotalSubObjectives, int32 AliveEmployees);

	/** Shown 1 star: the killer must take the defense question (its answer is always accepted, in public) */
	static bool IsMandatoryDefense(int32 ShownStars) { return ShownStars == 1; }
	/** Shown 3 stars: the promotion question (always accepted, in public) grants a fake shield */
	static bool GrantsFakeShield(int32 ShownStars) { return ShownStars >= 3; }

	/**
	 *  AI killer: mostly 2 (nothing special), sometimes 3 (a fake shield, but a public answer) or 1 (never a cut
	 *  candidate, but a public answer that is often visibly wrong). Cunning favours the honest value. Always allowed.
	 */
	static int32 ChooseForAI(const FUOFakeRatingOffer& Offer, float Cunning, float Random01);
};
