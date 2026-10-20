// UNPAID: OVERTIME - end-of-day rules.

#include "UOEvaluationRules.h"

FUORosterEntry* FUORoster::Find(int32 EmployeeId)
{
	return Entries.FindByPredicate([EmployeeId](const FUORosterEntry& Entry) { return Entry.EmployeeId == EmployeeId; });
}

const FUORosterEntry* FUORoster::Find(int32 EmployeeId) const
{
	return Entries.FindByPredicate([EmployeeId](const FUORosterEntry& Entry) { return Entry.EmployeeId == EmployeeId; });
}

int32 FUORoster::CountAlive() const
{
	int32 Alive = 0;
	for (const FUORosterEntry& Entry : Entries)
	{
		Alive += Entry.bAlive ? 1 : 0;
	}
	return Alive;
}

TArray<int32> FUORoster::GetAliveIds() const
{
	TArray<int32> Ids;
	for (const FUORosterEntry& Entry : Entries)
	{
		if (Entry.bAlive)
		{
			Ids.Add(Entry.EmployeeId);
		}
	}
	return Ids;
}

bool FUORoster::IsAnyHumanAlive() const
{
	return Entries.ContainsByPredicate([](const FUORosterEntry& Entry) { return Entry.bIsHuman && Entry.bAlive; });
}

int32 FUORoster::GetLastStanding() const
{
	const TArray<int32> Alive = GetAliveIds();
	return Alive.Num() == 1 ? Alive[0] : INDEX_NONE;
}

bool FUORoster::Eliminate(int32 EmployeeId, EUOExitCause Cause, int32 DayNumber)
{
	FUORosterEntry* Entry = Find(EmployeeId);
	if (!Entry || !Entry->bAlive)
	{
		return false;
	}
	Entry->bAlive = false;
	Entry->ExitCause = Cause;
	Entry->ExitDay = DayNumber;
	return true;
}

TArray<FUOFiringDecision> FUORoster::ApplyDirectFirings(int32 DayNumber)
{
	TArray<FUOFiringDecision> Decisions;
	for (FUORosterEntry& Entry : Entries)
	{
		const bool bProbation = Entry.bOnProbation;
		Entry.bOnProbation = false;
		if (!Entry.bAlive)
		{
			continue;
		}

		// Judged on real stars: a killer's shown stars are fake (its real work counts like anyone's)
		const int32 Judged = Entry.RealStars != INDEX_NONE ? Entry.RealStars : Entry.Stars;
		EUODirectFiringOutcome Outcome = FUOEvaluationRules::ResolveDirectFiring(Judged, Entry.Shields, Entry.bExemptFromDirectFiring);
		const bool bFailedProbation = bProbation && FUOEvaluationRules::FailsProbation(Judged) && Outcome != EUODirectFiringOutcome::Fired
			&& Outcome != EUODirectFiringOutcome::ShieldUsed;
		if (bFailedProbation)
		{
			// The 0-star exemption does not cover a failed probation
			Outcome = Entry.Shields > 0 ? EUODirectFiringOutcome::ShieldUsed : EUODirectFiringOutcome::Fired;
		}
		if (Outcome == EUODirectFiringOutcome::NotApplicable)
		{
			continue;
		}

		FUOFiringDecision& Decision = Decisions.AddDefaulted_GetRef();
		Decision.EmployeeId = Entry.EmployeeId;
		Decision.bProbation = bFailedProbation || (bProbation && Outcome != EUODirectFiringOutcome::Exempt);
		if (Outcome == EUODirectFiringOutcome::Fired)
		{
			// A forged shield never saves anyone, but it is exposed on the way out
			Decision.bForgeryExposed = Entry.FakeShields > 0;
			Entry.FakeShields = 0;
			Entry.bAlive = false;
			Entry.ExitCause = EUOExitCause::DirectFiring;
			Entry.ExitDay = DayNumber;
		}
		else if (Outcome == EUODirectFiringOutcome::ShieldUsed)
		{
			Entry.Shields = FMath::Max(0, Entry.Shields - 1);
		}
		Decision.Outcome = Outcome;
	}
	return Decisions;
}

EUODirectFiringOutcome FUOEvaluationRules::ResolveDirectFiring(int32 Stars, int32 Shields, bool bExempt)
{
	if (Stars > 0)
	{
		return EUODirectFiringOutcome::NotApplicable;
	}
	if (bExempt)
	{
		return EUODirectFiringOutcome::Exempt;
	}
	return Shields > 0 ? EUODirectFiringOutcome::ShieldUsed : EUODirectFiringOutcome::Fired;
}

bool FUOEvaluationRules::CanAttemptPromotion(int32 Stars, int32 Shields)
{
	return Stars >= 3 && Shields < MaxShields;
}

bool FUOEvaluationRules::ResolvePromotion(bool bCorrect, int32& InOutShields)
{
	InOutShields = FMath::Clamp(InOutShields, 0, MaxShields);
	if (!bCorrect || InOutShields >= MaxShields)
	{
		return false;
	}
	++InOutShields;
	return true;
}

bool FUOEvaluationRules::IsCutCandidate(int32 VisibleStars)
{
	return VisibleStars == 1;
}

EUOMatchResult FUOEvaluationRules::EvaluateMatchEnd(const FUORoster& Roster)
{
	if (!Roster.IsAnyHumanAlive())
	{
		return EUOMatchResult::GameOver;
	}
	// Only killers left (the human among them): the saboteurs take over the company together
	if (Roster.AreOnlyKillersLeft())
	{
		return EUOMatchResult::Victory;
	}
	// M5: in streak mode a victory opens a new branch instead of ending the match
	return Roster.CountAlive() == 1 ? EUOMatchResult::Victory : EUOMatchResult::None;
}

int32 FUOEvaluationRules::ComputeDaysSurvived(int32 DayNumber, EUOMatchResult Result)
{
	const int32 Day = FMath::Max(1, DayNumber);
	return Result == EUOMatchResult::GameOver ? Day - 1 : Day;
}

bool FUORecordBook::Submit(TArray<FUORecordEntry>& Records, EUOAIDifficulty Difficulty, int32 DaysSurvived, bool bVictory)
{
	FUORecordEntry* Entry = Records.FindByPredicate([Difficulty](const FUORecordEntry& Record) { return Record.Difficulty == Difficulty; });
	if (!Entry)
	{
		Entry = &Records.AddDefaulted_GetRef();
		Entry->Difficulty = Difficulty;
	}

	++Entry->MatchesPlayed;
	Entry->Victories += bVictory ? 1 : 0;
	if (DaysSurvived > Entry->BestDaysSurvived)
	{
		Entry->BestDaysSurvived = DaysSurvived;
		return true;
	}
	return false;
}

int32 FUORecordBook::GetBestDays(const TArray<FUORecordEntry>& Records, EUOAIDifficulty Difficulty)
{
	const FUORecordEntry* Entry = Records.FindByPredicate([Difficulty](const FUORecordEntry& Record) { return Record.Difficulty == Difficulty; });
	return Entry ? Entry->BestDaysSurvived : 0;
}

// ~ Shields, forgeries, probation (M4.1)

EUOShieldCheck FUORoster::Expel(int32 EmployeeId, EUOExitCause Cause, int32 DayNumber, bool& bOutExpelled)
{
	bOutExpelled = false;
	FUORosterEntry* Entry = Find(EmployeeId);
	if (!Entry || !Entry->bAlive)
	{
		return EUOShieldCheck::None;
	}
	const EUOShieldCheck Check = FUOEvaluationRules::CheckShield(Entry->Shields, Entry->FakeShields);
	if (Check == EUOShieldCheck::ShieldUsed)
	{
		return Check;
	}
	// A forgery does not protect: whatever forged shields were left go with the employee
	Entry->FakeShields = 0;
	bOutExpelled = Eliminate(EmployeeId, Cause, DayNumber);
	return Check;
}

bool FUORoster::AreOnlyKillersLeft() const
{
	bool bAnyAlive = false;
	for (const FUORosterEntry& Entry : Entries)
	{
		if (Entry.bAlive)
		{
			if (!Entry.bKiller)
			{
				return false;
			}
			bAnyAlive = true;
		}
	}
	return bAnyAlive;
}

int32 FUOEvaluationRules::GetDisplayedShields(int32 RealShields, int32 FakeShields)
{
	return FMath::Clamp(FMath::Max(0, RealShields) + FMath::Max(0, FakeShields), 0, MaxShields);
}

bool FUOEvaluationRules::GrantFakeShield(int32 RealShields, int32& InOutFakeShields)
{
	InOutFakeShields = FMath::Max(0, InOutFakeShields);
	if (GetDisplayedShields(RealShields, InOutFakeShields) >= MaxShields)
	{
		return false;
	}
	++InOutFakeShields;
	return true;
}

EUOShieldCheck FUOEvaluationRules::CheckShield(int32& InOutRealShields, int32& InOutFakeShields)
{
	if (InOutRealShields > 0)
	{
		--InOutRealShields;
		return EUOShieldCheck::ShieldUsed;
	}
	if (InOutFakeShields > 0)
	{
		--InOutFakeShields;
		return EUOShieldCheck::Forgery;
	}
	return EUOShieldCheck::None;
}

bool FUOEvaluationRules::FailsProbation(int32 RealStars)
{
	return RealStars < 2;
}
