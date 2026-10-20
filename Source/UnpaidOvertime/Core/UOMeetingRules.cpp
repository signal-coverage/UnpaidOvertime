// UNPAID: OVERTIME - the meeting rules and the AI side of the meeting.

#include "UOMeetingRules.h"
#include "UOKillerRules.h"
#include "UOBossQuestions.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "UOMeeting"

// ~ Vote

int32 FUOMeetingTally::GetVotes(int32 EmployeeId) const
{
	for (const TPair<int32, int32>& Count : Counts)
	{
		if (Count.Key == EmployeeId)
		{
			return Count.Value;
		}
	}
	return 0;
}

bool FUOMeetingVote::IsValidBallot(const TArray<int32>& AliveIds, int32 VoterId, int32 TargetId)
{
	if (!AliveIds.Contains(VoterId))
	{
		return false;
	}
	return TargetId == INDEX_NONE || (TargetId != VoterId && AliveIds.Contains(TargetId));
}

bool FUOMeetingVote::Cast(TArray<FUOMeetingBallot>& InOutBallots, const TArray<int32>& AliveIds, int32 VoterId, int32 TargetId)
{
	if (!IsValidBallot(AliveIds, VoterId, TargetId))
	{
		return false;
	}
	if (FUOMeetingBallot* Existing = InOutBallots.FindByPredicate([VoterId](const FUOMeetingBallot& Ballot) { return Ballot.VoterId == VoterId; }))
	{
		Existing->TargetId = TargetId;
		return true;
	}
	FUOMeetingBallot& Ballot = InOutBallots.AddDefaulted_GetRef();
	Ballot.VoterId = VoterId;
	Ballot.TargetId = TargetId;
	return true;
}

namespace UOMeetingPrivate
{
	/** Counts the valid, non-abstaining ballots (the last one per voter), skipping the given voters */
	TArray<TPair<int32, int32>> Count(const TArray<int32>& AliveIds, const TArray<FUOMeetingBallot>& Ballots, const TArray<int32>& SkipVoters)
	{
		TMap<int32, int32> LastTarget;
		for (const FUOMeetingBallot& Ballot : Ballots)
		{
			if (FUOMeetingVote::IsValidBallot(AliveIds, Ballot.VoterId, Ballot.TargetId))
			{
				LastTarget.Add(Ballot.VoterId, Ballot.TargetId);
			}
		}
		TArray<TPair<int32, int32>> Counts;
		for (const int32 Id : AliveIds)
		{
			Counts.Emplace(Id, 0);
		}
		for (const TPair<int32, int32>& Pair : LastTarget)
		{
			if (Pair.Value == INDEX_NONE || SkipVoters.Contains(Pair.Key))
			{
				continue;
			}
			for (TPair<int32, int32>& Count : Counts)
			{
				Count.Value += Count.Key == Pair.Value ? 1 : 0;
			}
		}
		return Counts;
	}

	TArray<int32> Leaders(const TArray<TPair<int32, int32>>& Counts)
	{
		int32 Best = 0;
		for (const TPair<int32, int32>& Count : Counts)
		{
			Best = FMath::Max(Best, Count.Value);
		}
		TArray<int32> Result;
		if (Best <= 0)
		{
			return Result;
		}
		for (const TPair<int32, int32>& Count : Counts)
		{
			if (Count.Value == Best)
			{
				Result.Add(Count.Key);
			}
		}
		return Result;
	}
}

FUOMeetingTally FUOMeetingVote::Tally(const TArray<int32>& AliveIds, const TArray<FUOMeetingBallot>& Ballots)
{
	using namespace UOMeetingPrivate;

	FUOMeetingTally Tally;
	const TArray<TPair<int32, int32>> Raw = Count(AliveIds, Ballots, {});
	const TArray<int32> RawLeaders = Leaders(Raw);
	if (RawLeaders.Num() == 0)
	{
		Tally.Counts = Raw;
		return Tally;
	}

	// The most-voted employees' ballots are shown but do not count
	const TArray<TPair<int32, int32>> Counted = Count(AliveIds, Ballots, RawLeaders);
	const TArray<int32> CountedLeaders = Leaders(Counted);
	if (CountedLeaders.Num() == 0)
	{
		// Nothing would be left (e.g. the last two accusing each other): the raw tally stands
		Tally.Counts = Raw;
		Tally.Leaders = RawLeaders;
		return Tally;
	}
	Tally.Counts = Counted;
	Tally.Leaders = CountedLeaders;
	for (const FUOMeetingBallot& Ballot : Ballots)
	{
		if (RawLeaders.Contains(Ballot.VoterId) && Ballot.TargetId != INDEX_NONE)
		{
			Tally.IgnoredVoters.AddUnique(Ballot.VoterId);
		}
	}
	return Tally;
}

TArray<int32> FUOMeetingVote::ReduceTie(const TArray<int32>& Leaders, int32 Seed)
{
	TArray<int32> Pool = Leaders;
	Pool.Sort();
	if (Pool.Num() <= 2)
	{
		return Pool;
	}
	FRandomStream Rng(Seed * 31 + 7);
	while (Pool.Num() > 2)
	{
		Pool.RemoveAt(Rng.RandRange(0, Pool.Num() - 1));
	}
	return Pool;
}

// ~ Rock-paper-scissors

bool FUORockPaperScissors::Beats(EUORPSChoice A, EUORPSChoice B)
{
	return (A == EUORPSChoice::Rock && B == EUORPSChoice::Scissors)
		|| (A == EUORPSChoice::Scissors && B == EUORPSChoice::Paper)
		|| (A == EUORPSChoice::Paper && B == EUORPSChoice::Rock);
}

EUORPSChoice FUORockPaperScissors::Counter(EUORPSChoice Against)
{
	switch (Against)
	{
	case EUORPSChoice::Rock:		return EUORPSChoice::Paper;
	case EUORPSChoice::Paper:		return EUORPSChoice::Scissors;
	case EUORPSChoice::Scissors:	return EUORPSChoice::Rock;
	default:						return EUORPSChoice::Rock;
	}
}

EUORPSChoice FUORockPaperScissors::FromRandom(float Random01)
{
	const int32 Index = FMath::Clamp(FMath::FloorToInt(FMath::Clamp(Random01, 0.f, 0.9999f) * 3.f), 0, 2);
	return static_cast<EUORPSChoice>(Index + 1);
}

FUORPSDuel FUORPSDuel::Make(int32 A, bool bAKiller, int32 B, bool bBKiller)
{
	FUORPSDuel Duel;
	Duel.SideA = A;
	Duel.SideB = B;
	Duel.RiggedFor = FUORockPaperScissors::IsRigged(bAKiller, bBKiller) ? (bAKiller ? 0 : 1) : INDEX_NONE;
	return Duel;
}

int32 FUORPSDuel::PlayRound(EUORPSChoice& InOutPickA, EUORPSChoice& InOutPickB)
{
	if (IsOver())
	{
		return INDEX_NONE;
	}
	InOutPickA = InOutPickA == EUORPSChoice::None ? EUORPSChoice::Rock : InOutPickA;
	InOutPickB = InOutPickB == EUORPSChoice::None ? EUORPSChoice::Rock : InOutPickB;
	// One employee against one killer: the system reads the employee's pick and hands the killer its counter
	if (RiggedFor == 0)
	{
		InOutPickA = FUORockPaperScissors::Counter(InOutPickB);
	}
	else if (RiggedFor == 1)
	{
		InOutPickB = FUORockPaperScissors::Counter(InOutPickA);
	}
	++Rounds;
	if (FUORockPaperScissors::Beats(InOutPickA, InOutPickB))
	{
		++WinsA;
		return 0;
	}
	if (FUORockPaperScissors::Beats(InOutPickB, InOutPickA))
	{
		++WinsB;
		return 1;
	}
	return INDEX_NONE;
}

bool FUORPSDuel::IsOver() const
{
	return WinsA >= FUOMeetingTuning::DuelWinsNeeded || WinsB >= FUOMeetingTuning::DuelWinsNeeded || Rounds >= FUOMeetingTuning::MaxDuelRounds;
}

int32 FUORPSDuel::GetLoser() const
{
	if (!IsOver())
	{
		return INDEX_NONE;
	}
	return WinsA > WinsB ? SideB : (WinsB > WinsA ? SideA : SideB);
}

int32 FUORPSDuel::GetWinner() const
{
	const int32 Loser = GetLoser();
	return Loser == INDEX_NONE ? INDEX_NONE : (Loser == SideA ? SideB : SideA);
}

// ~ AI

namespace UOMeetingPrivate
{
	/** Best suspect the AI may name: never itself, a killer never names an accomplice */
	int32 PickSuspect(const FUOMeetingAIContext& Context, float& OutScore)
	{
		TMap<int32, float> Scores;
		for (const FUOSuspicionItem& Item : Context.Suspects.Items)
		{
			Scores.FindOrAdd(Item.SuspectId) += Item.Weight;
		}
		TArray<int32> Ids;
		Scores.GetKeys(Ids);
		Ids.Sort();
		int32 Best = INDEX_NONE;
		OutScore = 0.f;
		for (const int32 Id : Ids)
		{
			const bool bAllowed = Id != Context.SelfId && Context.AliveIds.Contains(Id) && !(Context.bSelfKiller && Context.Accomplices.Contains(Id));
			if (bAllowed && Scores[Id] > OutScore)
			{
				OutScore = Scores[Id];
				Best = Id;
			}
		}
		return Best;
	}

	const FUOSuspicionItem* StrongestItem(const FUOSuspicionResult& Result, int32 SuspectId)
	{
		const FUOSuspicionItem* Best = nullptr;
		for (const FUOSuspicionItem& Item : Result.Items)
		{
			if (Item.SuspectId == SuspectId && (!Best || Item.Weight > Best->Weight))
			{
				Best = &Item;
			}
		}
		return Best;
	}
}

FUODebateStatement FUOMeetingAI::MakeStatement(const FUOMeetingAIContext& Context)
{
	using namespace UOMeetingPrivate;

	FUODebateStatement Statement;
	Statement.SpeakerId = Context.SelfId;
	const float Threshold = FUOKillerTuning::GetVoteThreshold(Context.Suspicion);
	float Score = 0.f;
	const int32 Suspect = PickSuspect(Context, Score);
	// A killer frames with less (it only needs to sound plausible)
	const float AccuseAt = Context.bSelfKiller ? Threshold * 0.5f : Threshold;
	const float UnsureAt = Context.bSelfKiller ? Threshold * 0.2f : Threshold * 0.5f;
	if (Suspect == INDEX_NONE || Score < UnsureAt)
	{
		Statement.Kind = EUOStatementKind::Nothing;
		Statement.Reason = TEXT("nothing solid");
		return Statement;
	}
	Statement.Kind = Score >= AccuseAt ? EUOStatementKind::Accuse : EUOStatementKind::Unsure;
	Statement.SuspectId = Suspect;
	Statement.Score = Score;
	Statement.TraceIds = Context.Suspects.GetEvidenceIds(Suspect, FUOMeetingTuning::MaxPresentedEvidence);
	if (const FUOSuspicionItem* Item = StrongestItem(Context.Suspects, Suspect))
	{
		Statement.Reason = Item->Reason;
		Statement.ReasonSource = Item->Source;
		Statement.Basis = Item->Basis;
	}
	return Statement;
}

int32 FUOMeetingAI::ChooseVote(const FUOMeetingAIContext& Context, FString& OutReason)
{
	using namespace UOMeetingPrivate;

	const float Threshold = FUOKillerTuning::GetVoteThreshold(Context.Suspicion);
	float Score = 0.f;
	const int32 Suspect = PickSuspect(Context, Score);
	const float VoteAt = Context.bSelfKiller ? Threshold * FUOKillerTuning::KillerVoteThresholdFactor : Threshold;
	if (Suspect == INDEX_NONE || Score < VoteAt)
	{
		OutReason = FString::Printf(TEXT("abstains: best suspect %d at %.2f < %.2f (suspicion %.2f)%s"), Suspect, Score, VoteAt, Context.Suspicion,
			Context.bSelfKiller ? TEXT(" [killer]") : TEXT(""));
		return INDEX_NONE;
	}
	OutReason = FString::Printf(TEXT("votes %d: score %.2f >= %.2f (suspicion %.2f)%s"), Suspect, Score, VoteAt, Context.Suspicion,
		Context.bSelfKiller ? TEXT(" [killer, framing with visible evidence; never an accomplice]") : TEXT(""));
	return Suspect;
}

float FUOMeetingAI::GetConformity(float Suspicion)
{
	return FMath::Lerp(0.75f, 0.2f, FMath::Clamp(Suspicion, 0.f, 1.f));
}

int32 FUOMeetingAI::Reconsider(const FUOMeetingAIContext& Context, int32 CurrentVote, const FUOMeetingTally& Live, float Random01, FString& OutReason)
{
	if (Live.Leaders.Num() != 1)
	{
		OutReason = TEXT("keeps its vote: no clear leader in the room");
		return CurrentVote;
	}
	const int32 Leader = Live.Leaders[0];
	if (Leader == CurrentVote || Leader == Context.SelfId || !Context.AliveIds.Contains(Leader))
	{
		OutReason = TEXT("keeps its vote");
		return CurrentVote;
	}
	if (Context.bSelfKiller)
	{
		if (Context.Accomplices.Contains(Leader))
		{
			OutReason = TEXT("keeps its vote: the room leans on an accomplice [killer]");
			return CurrentVote;
		}
		OutReason = FString::Printf(TEXT("follows the room against %d (blending in) [killer]"), Leader);
		return Leader;
	}

	// An innocent only follows someone it had something against, and only if its own case is weak
	const float Threshold = FUOKillerTuning::GetVoteThreshold(Context.Suspicion);
	const float OnLeader = Context.Suspects.GetScore(Leader);
	const float OnCurrent = CurrentVote != INDEX_NONE ? Context.Suspects.GetScore(CurrentVote) : 0.f;
	const bool bWeakCase = CurrentVote == INDEX_NONE || OnCurrent < Threshold * 1.5f;
	if (OnLeader > 0.f && bWeakCase && Random01 < GetConformity(Context.Suspicion))
	{
		OutReason = FString::Printf(TEXT("follows the room against %d (own score %.2f, conformity %.2f)"), Leader, OnLeader, GetConformity(Context.Suspicion));
		return Leader;
	}
	OutReason = TEXT("keeps its vote");
	return CurrentVote;
}

// ~ Texts

FText FUOMeetingText::DescribeStatement(const FUODebateStatement& Statement, TFunctionRef<FString(int32)> NameOf, int32 Seed)
{
	const int32 Pick = FMath::Abs(Seed) % 3;
	if (Statement.Kind == EUOStatementKind::Nothing || Statement.SuspectId == INDEX_NONE)
	{
		switch (Pick)
		{
		case 0:		return LOCTEXT("Nothing0", "I've got nothing solid. I was buried in reports all day.");
		case 1:		return LOCTEXT("Nothing1", "No idea. I just want to go home.");
		default:	return LOCTEXT("Nothing2", "Nothing from me. I'll vote with whoever has proof.");
		}
	}

	const FText Who = FText::FromString(NameOf(Statement.SuspectId));
	const FText Room = Statement.Room.IsNone() ? LOCTEXT("ThatRoom", "that room") : FUORooms::GetDisplayName(Statement.Room);
	FText Why;
	switch (Statement.Basis)
	{
	case EUOSuspicionBasis::SeenAct:		Why = LOCTEXT("WhySeen", "I saw them do it with my own eyes."); break;
	case EUOSuspicionBasis::NamedTrace:		Why = LOCTEXT("WhyNamed", "The logs name them. It's right there."); break;
	case EUOSuspicionBasis::PartialTrace:	Why = LOCTEXT("WhyPartial", "The badge log says it was them, or one other person."); break;
	case EUOSuspicionBasis::Presence:		Why = FText::Format(LOCTEXT("WhyPresence", "They were in the {0} right when it happened."), Room); break;
	case EUOSuspicionBasis::LastSeen:		Why = LOCTEXT("WhyLastSeen", "They were the last one seen near the body."); break;
	case EUOSuspicionBasis::BotchedWork:	Why = LOCTEXT("WhyBotched", "They keep \"failing\" at simple jobs. Nobody is that bad by accident."); break;
	case EUOSuspicionBasis::AcceptedWrong:	Why = LOCTEXT("WhyAccepted", "Their answer to the boss was wrong and it got accepted anyway. I know, I was there."); break;
	case EUOSuspicionBasis::Anomaly:		Why = LOCTEXT("WhyAnomaly", "Something about today doesn't add up, and they're in the middle of it."); break;
	case EUOSuspicionBasis::Memory:			Why = LOCTEXT("WhyMemory", "Same as yesterday. I haven't forgotten."); break;
	default:								Why = LOCTEXT("WhyHunch", "Call it a gut feeling: they've been acting odd all day."); break;
	}

	if (Statement.Kind == EUOStatementKind::Unsure)
	{
		return Pick == 0
			? FText::Format(LOCTEXT("Unsure0", "Not sure, but keep an eye on {0}. {1}"), Who, Why)
			: FText::Format(LOCTEXT("Unsure1", "Maybe {0}? {1} Could be nothing."), Who, Why);
	}
	switch (Pick)
	{
	case 0:		return FText::Format(LOCTEXT("Accuse0", "It's {0}. {1}"), Who, Why);
	case 1:		return FText::Format(LOCTEXT("Accuse1", "I'm voting {0}. {1}"), Who, Why);
	default:	return FText::Format(LOCTEXT("Accuse2", "Look at {0}. {1}"), Who, Why);
	}
}

FText FUOMeetingText::DescribeDefense(int32 SpeakerId, int32 AccuserId, TFunctionRef<FString(int32)> NameOf, int32 Seed)
{
	const FText Accuser = FText::FromString(NameOf(AccuserId));
	switch (FMath::Abs(Seed) % 4)
	{
	case 0:		return LOCTEXT("Defense0", "Me? I was working the whole time!");
	case 1:		return FText::Format(LOCTEXT("Defense1", "That's ridiculous, {0}. Read the logs again."), Accuser);
	case 2:		return LOCTEXT("Defense2", "I've been carrying this team and this is the thanks I get.");
	default:	return FText::Format(LOCTEXT("Defense3", "Funny how {0} is so sure. Why is that?"), Accuser);
	}
}

FText FUOMeetingText::GetPickName(EUORPSChoice Pick)
{
	switch (Pick)
	{
	case EUORPSChoice::Rock:		return LOCTEXT("Rock", "Rock");
	case EUORPSChoice::Paper:		return LOCTEXT("Paper", "Paper");
	case EUORPSChoice::Scissors:	return LOCTEXT("Scissors", "Scissors");
	default:						return LOCTEXT("NoPick", "?");
	}
}

#undef LOCTEXT_NAMESPACE
