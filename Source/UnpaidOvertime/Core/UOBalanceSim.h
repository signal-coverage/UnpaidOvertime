// UNPAID: OVERTIME - a fast, abstract match simulation for balance tuning (rule 9 targets). It drives the real rule
// code (FUOSuspicionModel, FUOMeetingAI, FUOMeetingVote, FUORPSDuel, shields, probation, fake rating) with a simple
// observation model of the shift instead of actors: who was in which room, who read which terminal.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"

/** One simulation setup */
struct FUOBalanceSimConfig
{
	int32 Employees = 4;
	/** 0 = rule 1 (floor(employees / 3)) */
	int32 Killers = 0;
	/** The killers are AIs (the human is employee 0, passive: works, abstains, never presents evidence) */
	bool bAIKillers = true;
	EUOAIDifficulty Difficulty = EUOAIDifficulty::Medium;
	bool bKillerCanKill = false;
	int32 Matches = 1000;
	/** A match stops when the killers are gone, only killers are left, or after this many days (the game has no cap: this only bounds a stalemate) */
	int32 MaxDays = 30;
	int32 Seed = 1;

	// ~ Observation model of the shift (assumptions, documented in the report)
	/** Sabotages per killer per day (before the cunning scale) */
	float SabotagesPerDay = 3.f;
	/** Chance another employee is in the room of a sabotage */
	float PresenceChance = 0.2f;
	/** Chance an employee in the room sees the act (scaled by 1 - Cunning) */
	float SeeActChance = 0.15f;
	/** Chance an AI reads a given device log during the day (plus Suspicion * 0.5) */
	float ReadChance = 0.2f;
	/** Chance an AI knows where someone in the room was (door/camera log, own sighting) */
	float PlacementChance = 0.4f;
	/** Chance an AI could know the fact behind a killer's public answer (it visited that room) */
	float KnowFactChance = 0.55f;
	/** Botched work attempts per innocent AI per day at Precision 0 (scaled by 1 - Precision) */
	float BotchesPerDay = 6.f;
	// ~ Kills (kills on): an AI killer considers a kill every FUOKillerTuning::AIKillPerMinute on average once the first
	// cooldown has run out, commits with FUOKillerTuning::AIKillCommitChance, and goes through with it only when nobody is
	// in sight. Cross-checked with the in-game kill tour at the old tuning (45 s cooldown, commit always): the sim gives
	// about 3 kills per killer per 12 minute shift, the tour showed about the same.
	/** Shift length used for the kill timeline (Normal day) */
	float ShiftSeconds = 720.f;
};

/** A named setup with the killers' win-rate band it must land in */
struct FUOBalanceSetup
{
	FString Name;
	FUOBalanceSimConfig Config;
	float MinKillerWin = 0.35f;
	float MaxKillerWin = 0.45f;
};

/** What happened (rates over the matches) */
struct FUOBalanceSimResult
{
	int32 Matches = 0;
	/** At least one killer removed by the end of day 5 */
	int32 KillerCaughtByDay5 = 0;
	/** Every killer removed by the end of day 5 */
	int32 AllKillersCaughtByDay5 = 0;
	/** Only killers were left at some point (before all of them were removed) */
	int32 KillersWin = 0;
	/** Every killer was removed */
	int32 EmployeesWin = 0;
	int32 Meetings = 0;
	/** Dismissals by killers (kills on) */
	int32 Kills = 0;
	/** Of those, meetings right after a kill that expelled a killer / that expelled an innocent */
	int32 KillMeetingKillerOut = 0;
	int32 KillMeetingInnocentOut = 0;
	int32 MeetingsWithExpulsion = 0;
	/** Meetings that expelled an innocent */
	int32 WrongfulExpulsions = 0;
	int32 Duels = 0;
	int32 RiggedDuels = 0;
	int32 SavedByShield = 0;
	int32 SavedOnProbation = 0;
	int32 ForgeriesExposed = 0;
	float AverageDays = 0.f;
	/** How killers left: voted out at a meeting, cut, fired (failed probation) */
	int32 KillersExpelled = 0;
	int32 KillersCut = 0;
	int32 KillersFired = 0;
	/** Votes cast at meetings: for a killer / for an innocent / abstain (AIs) */
	int32 VotesForKillers = 0;
	int32 VotesForInnocents = 0;
	int32 Abstentions = 0;

	float CatchRateByDay5() const { return Matches > 0 ? static_cast<float>(KillerCaughtByDay5) / Matches : 0.f; }
	float WrongfulRate() const { return MeetingsWithExpulsion > 0 ? static_cast<float>(WrongfulExpulsions) / MeetingsWithExpulsion : 0.f; }
	float KillersWinRate() const { return Matches > 0 ? static_cast<float>(KillersWin) / Matches : 0.f; }
	FString ToString() const;
};

struct FUOBalanceSim
{
	static FUOBalanceSimResult Run(const FUOBalanceSimConfig& Config);

	/** The five product setups (4/6 employees, kills off/on, Easy AIs) with fixed seeds and the target bands */
	static TArray<FUOBalanceSetup> GetStandardSetups(int32 Matches);
};
