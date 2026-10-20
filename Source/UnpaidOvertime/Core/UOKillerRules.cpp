// UNPAID: OVERTIME - the killer's rules.

#include "UOKillerRules.h"
#include "UOStarRating.h"
#include "Math/RandomStream.h"

// ~ Tuning

float FUOKillerTuning::GetBaseCooldown(EUOSabotageType Type)
{
	switch (Type)
	{
	case EUOSabotageType::JamPrinter:	return 40.f;
	case EUOSabotageType::CorruptFile:	return 60.f;
	case EUOSabotageType::LockStation:	return 45.f;
	case EUOSabotageType::CutCamera:	return 70.f;
	case EUOSabotageType::MoveObject:	return 30.f;
	default:							return 60.f;
	}
}

float FUOKillerTuning::GetBaseHeat(EUOSabotageType Type)
{
	switch (Type)
	{
	case EUOSabotageType::JamPrinter:	return 25.f;
	case EUOSabotageType::CorruptFile:	return 35.f;
	case EUOSabotageType::LockStation:	return 25.f;
	case EUOSabotageType::CutCamera:	return 30.f;
	case EUOSabotageType::MoveObject:	return 15.f;
	default:							return 30.f;
	}
}

float FUOKillerTuning::GetHoldSeconds(EUOSabotageType Type)
{
	switch (Type)
	{
	case EUOSabotageType::JamPrinter:	return 2.5f;
	case EUOSabotageType::CorruptFile:	return 3.5f;
	case EUOSabotageType::LockStation:	return 2.f;
	case EUOSabotageType::CutCamera:	return 2.5f;
	case EUOSabotageType::MoveObject:	return 2.f;
	default:							return 2.5f;
	}
}

float FUOKillerTuning::GetDayCostScale(int32 DayNumber)
{
	return FMath::Min(1.6f, 0.8f + 0.1f * (FMath::Max(1, DayNumber) - 1));
}

EUOTraceDetail FUOKillerTuning::GetTraceDetail(int32 DayNumber)
{
	if (DayNumber <= 2)
	{
		return EUOTraceDetail::Anonymous;
	}
	return DayNumber <= 4 ? EUOTraceDetail::Partial : EUOTraceDetail::Named;
}

float FUOKillerTuning::GetVoteThreshold(float Suspicion)
{
	return FMath::Lerp(4.f, 2.4f, FMath::Clamp(Suspicion, 0.f, 1.f));
}

float FUOKillerTuning::GetTerminalVisitsPerMinute(float Suspicion)
{
	return FMath::Lerp(0.4f, 1.8f, FMath::Clamp(Suspicion, 0.f, 1.f));
}

// ~ Kills and roles

int32 FUOKillerTuning::GetKillerCount(int32 TotalEmployees)
{
	return FMath::Max(1, TotalEmployees / 3);
}

float FUOKillerTuning::GetKillCooldown(bool bOvertime)
{
	return KillCooldownSeconds * (bOvertime ? OvertimeCooldownScale : 1.f);
}

EUOTraceDetail FUOKillerTuning::GetKillTraceDetail(int32 DayNumber, int32 KillsBefore)
{
	const int32 Step = static_cast<int32>(GetTraceDetail(DayNumber)) + FMath::Max(0, KillsBefore);
	return static_cast<EUOTraceDetail>(FMath::Min(Step, static_cast<int32>(EUOTraceDetail::Named)));
}

float FUOKillerTuning::GetLastSeenNoteChance(int32 KillNumber)
{
	return FMath::Clamp(0.35f * FMath::Max(1, KillNumber) + (KillNumber >= 3 ? 1.f : 0.f), 0.f, 1.f);
}

TArray<int32> FUORoleRules::PickKillers(const TArray<FUORoleCandidate>& Candidates, EUOKillerAssignment Assignment, int32 Seed, int32 Count)
{
	TArray<int32> Killers;
	if (Candidates.Num() == 0 || Count <= 0)
	{
		return Killers;
	}

	// Scramble the seed so consecutive seeds do not walk the roster in order
	FRandomStream Rng(static_cast<int32>((static_cast<uint32>(Seed) * 2654435761u) ^ 0x5bd1e995u));
	Rng.GetUnsignedInt();

	TArray<int32> Humans;
	TArray<int32> AIs;
	TArray<int32> Everyone;
	for (const FUORoleCandidate& Candidate : Candidates)
	{
		(Candidate.bIsHuman ? Humans : AIs).Add(Candidate.EmployeeId);
		Everyone.Add(Candidate.EmployeeId);
	}

	auto TakeRandom = [&Rng, &Killers](TArray<int32>& Pool)
	{
		const int32 Index = Rng.RandRange(0, Pool.Num() - 1);
		Killers.Add(Pool[Index]);
		Pool.RemoveAt(Index);
	};

	const int32 Wanted = FMath::Min(Count, Everyone.Num());
	if (Assignment == EUOKillerAssignment::AlwaysHuman && Humans.Num() > 0)
	{
		// The (first, seeded) human is one of them; the rest uniform among everyone else
		const int32 Human = Humans[Rng.RandRange(0, Humans.Num() - 1)];
		Killers.Add(Human);
		Everyone.Remove(Human);
	}
	else if (Assignment == EUOKillerAssignment::AlwaysAI && AIs.Num() > 0)
	{
		while (Killers.Num() < Wanted && AIs.Num() > 0)
		{
			TakeRandom(AIs);
		}
		for (const int32 Id : Killers)
		{
			Everyone.Remove(Id);
		}
	}
	while (Killers.Num() < Wanted && Everyone.Num() > 0)
	{
		TakeRandom(Everyone);
	}
	Killers.Sort();
	return Killers;
}

// ~ Killer state

void FUOKillerState::ResetForDay()
{
	Heat = 0.f;
	for (float& Ready : ReadyAt)
	{
		Ready = FUOKillerTuning::SabotageGraceSeconds;
	}
	KillsToday = 0;
	KillReadyAt = FUOKillerTuning::KillCooldownSeconds;
	SabotagesToday = 0;
}

void FUOKillerState::Tick(float DeltaSeconds)
{
	Heat = FMath::Max(0.f, Heat - FUOKillerTuning::HeatDecayPerSecond * DeltaSeconds);
}

// ~ Sabotage

float FUOSabotageRules::GetCooldown(EUOSabotageType Type, int32 DayNumber, bool bOvertime)
{
	return FUOKillerTuning::GetBaseCooldown(Type) * FUOKillerTuning::GetDayCostScale(DayNumber) * (bOvertime ? FUOKillerTuning::OvertimeCooldownScale : 1.f);
}

float FUOSabotageRules::GetHeatCost(EUOSabotageType Type, int32 DayNumber)
{
	return FUOKillerTuning::GetBaseHeat(Type) * FUOKillerTuning::GetDayCostScale(DayNumber);
}

float FUOSabotageRules::GetRemainingCooldown(const FUOKillerState& State, EUOSabotageType Type, float Now)
{
	const int32 Index = static_cast<int32>(Type);
	return Index >= 0 && Index < static_cast<int32>(EUOSabotageType::Count) ? FMath::Max(0.f, State.ReadyAt[Index] - Now) : 0.f;
}

EUOKillerBlock FUOSabotageRules::CanSabotage(const FUOKillerState& State, EUOSabotageType Type, float Now, int32 DayNumber)
{
	if (Now < FUOKillerTuning::SabotageGraceSeconds)
	{
		return EUOKillerBlock::TooEarly;
	}
	if (GetRemainingCooldown(State, Type, Now) > 0.f)
	{
		return EUOKillerBlock::CoolingDown;
	}
	if (State.Heat + GetHeatCost(Type, DayNumber) > FUOKillerTuning::MaxHeat)
	{
		return EUOKillerBlock::TooMuchHeat;
	}
	return EUOKillerBlock::None;
}

void FUOSabotageRules::Apply(FUOKillerState& State, EUOSabotageType Type, float Now, int32 DayNumber, bool bOvertime)
{
	const int32 Index = static_cast<int32>(Type);
	if (Index < 0 || Index >= static_cast<int32>(EUOSabotageType::Count))
	{
		return;
	}
	State.Heat = FMath::Min(FUOKillerTuning::MaxHeat, State.Heat + GetHeatCost(Type, DayNumber));
	State.ReadyAt[Index] = Now + GetCooldown(Type, DayNumber, bOvertime);
	++State.SabotagesToday;
}

// ~ Kill

EUOKillerBlock FUOKillRules::CanKill(bool bEnabled, const FUOKillerState& State, float Now, int32 Witnesses, float Distance, bool bVictimIsAccomplice)
{
	if (!bEnabled)
	{
		return EUOKillerBlock::Disabled;
	}
	if (bVictimIsAccomplice)
	{
		return EUOKillerBlock::Accomplice;
	}
	if (Now < State.KillReadyAt)
	{
		return EUOKillerBlock::CoolingDown;
	}
	if (Distance > FUOKillerTuning::KillRange)
	{
		return EUOKillerBlock::OutOfRange;
	}
	if (Witnesses > FUOKillerTuning::MaxWitnesses)
	{
		return EUOKillerBlock::Witnesses;
	}
	return EUOKillerBlock::None;
}

void FUOKillRules::Apply(FUOKillerState& State, float Now, bool bOvertime)
{
	++State.KillsToday;
	State.KillReadyAt = Now + FUOKillerTuning::GetKillCooldown(bOvertime);
}

// ~ Fake rating

FUOFakeRatingOffer FUOFakeRating::MakeOffer(int32 RealStars)
{
	FUOFakeRatingOffer Offer;
	Offer.RealStars = FMath::Clamp(RealStars, 0, FUOStarRating::MaxStars);
	Offer.DefaultStars = FMath::Clamp(Offer.RealStars, 1, FUOStarRating::MaxStars);
	Offer.Allowed = { 1, 2, 3 };
	return Offer;
}

int32 FUOFakeRating::GetDisplayedCompleted(int32 ShownStars, int32 RealCompleted, int32 TotalSubObjectives, int32 AliveEmployees)
{
	if (FUOStarRating::ComputeStars(RealCompleted, TotalSubObjectives, AliveEmployees) == ShownStars)
	{
		return RealCompleted;
	}
	for (int32 Count = 0; Count <= FMath::Max(TotalSubObjectives, 0); ++Count)
	{
		if (FUOStarRating::ComputeStars(Count, TotalSubObjectives, AliveEmployees) == ShownStars)
		{
			return Count;
		}
	}
	return RealCompleted;
}

int32 FUOFakeRating::ChooseForAI(const FUOFakeRatingOffer& Offer, float Cunning, float Random01)
{
	if (Offer.Allowed.Num() == 0)
	{
		return Offer.DefaultStars;
	}

	// Base taste: 2 is invisible; 3 forges a shield and 1 dodges the cut vote, but both put a public answer on show
	TArray<TPair<int32, float>> Weights;
	for (const int32 Stars : Offer.Allowed)
	{
		float Weight = Stars == 2 ? 0.6f : (Stars == 3 ? 0.22f : 0.18f);
		if (Stars == Offer.DefaultStars)
		{
			// A cunning killer shows what it really earned: nothing to explain
			Weight += 0.6f * FMath::Clamp(Cunning, 0.f, 1.f);
		}
		Weights.Emplace(Stars, Weight);
	}

	float Total = 0.f;
	for (const TPair<int32, float>& Pair : Weights)
	{
		Total += Pair.Value;
	}
	float Pick = FMath::Clamp(Random01, 0.f, 0.9999f) * Total;
	for (const TPair<int32, float>& Pair : Weights)
	{
		if (Pick < Pair.Value)
		{
			return Pair.Key;
		}
		Pick -= Pair.Value;
	}
	return Offer.DefaultStars;
}
