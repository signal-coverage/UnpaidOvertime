// UNPAID: OVERTIME - URoleSystem (design doc 11): secret role assignment and server validation of the killers' actions.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UOKillerTypes.h"
#include "UOKillerRules.h"
#include "UOInteractionTypes.h"
#include "UORoleSystemComponent.generated.h"

class AUOGameMode;
class AUOGameState;
class AUOPlayerState;
class AUOInteractableStation;
class AUOTerminal;

/**
 *  Owns the only copy of "who are the killers" (plus owner-only replicated copies on their player states).
 *  floor(employees / 3) killers (at least one), seeded; killers know each other and never target each other.
 *  Nobody else can ask about another employee's role: AIs ask GetOwnRole for themselves (and a killer AI its
 *  accomplices); server rules (exemption, fake rating, rigged duel) ask IsKillerServer. Validates and applies every
 *  sabotage and kill, keeps each killer's cooldowns/heat (FUOKillerTuning), and expires locks and camera outages.
 */
UCLASS(ClassGroup=(UnpaidOvertime))
class UUORoleSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UUORoleSystemComponent();

	/** New match: KillerCount killers (0 = floor(alive / 3), at least 1), seeded */
	void AssignRoles(EUOKillerAssignment Assignment, int32 Seed, bool bInKillerCanKill, int32 KillerCount = 0);

	/** New day: each killer's daily state resets */
	void StartDay(int32 InDayNumber);

	/** During the shift: heat cools, locks and camera outages expire */
	void TickShift(float DeltaSeconds);

	// ~ Secret queries

	/** Server rules only (exemption, fake rating, rigged duel, audits). Never for AI decision making. */
	bool IsKillerServer(int32 EmployeeId) const { return EmployeeId != INDEX_NONE && KillerIds.Contains(EmployeeId); }

	/** Logs and dev automation only */
	const TArray<int32>& GetKillerIdsForAudit() const { return KillerIds; }

	/** An employee's own role (AIs use this for themselves only) */
	EUORole GetOwnRole(const AUOPlayerState* Asker) const;

	/** Rule 1: a killer knows the other killers; everyone else gets an empty list */
	TArray<int32> GetAccomplicesOf(const AUOPlayerState* Asker) const;

	/** At least one killer is still employed */
	bool IsAnyKillerActive() const;
	/** This killer is still employed */
	bool IsKillerActive(int32 EmployeeId) const;
	bool IsKillEnabled() const { return bKillerCanKill; }
	/** A killer's bookkeeping (empty default for anyone else) */
	const FUOKillerState& GetKillerState(int32 EmployeeId) const;

	/** An employee left the match: a gone killer stops sabotaging */
	void OnEmployeeEliminated(const AUOPlayerState* Employee);

	// ~ Fake rating

	FUOFakeRatingOffer MakeFakeRatingOffer(int32 KillerId) const;

	/** Shows ShownStars on the review (public numbers); the real stars stay hidden */
	void ApplyFakeRating(int32 KillerId, int32 ShownStars, const FUOFakeRatingOffer& Offer, int32 TotalSubObjectives, int32 AliveEmployees, float FairShare);

	// ~ Sabotage (F) and kill

	/** What a killer could do to this station/terminal right now (empty option for everyone else) */
	FUOInteractionOption QuerySabotage(const AUOPlayerState* Employee, const AUOInteractableStation* Target) const;

	/** Re-validates and applies. False when rejected. */
	bool PerformSabotage(AUOPlayerState* Employee, AUOInteractableStation* Target);

	FUOInteractionOption QueryKill(const AUOPlayerState* Employee, const AUOPlayerState* Victim) const;
	bool PerformKill(AUOPlayerState* Employee, AUOPlayerState* Victim);

	/** Copies each killer's private numbers to its player state (owner-only replication) */
	void RefreshPrivateView();

	/** Shift seconds now */
	float Now() const;

protected:

	TArray<int32> KillerIds;
	bool bKillerCanKill = false;
	TSet<int32> GoneKillers;
	int32 DayNumber = 1;
	int32 Seed = 1;
	TMap<int32, FUOKillerState> States;
	/** Kills this match, per killer (each one makes the next easier to trace) */
	TMap<int32, int32> KillsThisMatch;

	struct FTimedSabotage
	{
		TWeakObjectPtr<AUOInteractableStation> Target;
		EUOSabotageType Type = EUOSabotageType::LockStation;
		float EndsAt = 0.f;
		float StartedAt = 0.f;
	};
	TArray<FTimedSabotage> Active;

	AUOGameMode* GetGameMode() const;
	AUOGameState* GetUOGameState() const;
	bool IsOvertime() const;

	/** Trace detail for a killer's sabotage today (an AI killer's cunning may blur it one step) */
	EUOTraceDetail GetActDetail(const AUOPlayerState* Killer, EUOTraceDetail Base) const;

	/** Sabotage type for a target, false when the target takes none */
	bool GetSabotageFor(const AUOInteractableStation* Target, EUOSabotageType& OutType, FText& OutBlocked) const;

	/** Corrupt file: the finished sub-objective (never a killer's) whose loss hurts the best performer the most */
	int32 PickReportToCorrupt() const;

	static FText DescribeBlock(EUOKillerBlock Block, float Seconds);
};
