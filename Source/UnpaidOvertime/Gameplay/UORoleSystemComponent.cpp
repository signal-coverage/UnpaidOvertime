// UNPAID: OVERTIME - URoleSystem: secret roles and the killers' actions.

#include "UORoleSystemComponent.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOInteractableStation.h"
#include "UOTerminal.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskBoard.h"
#include "UOStarRating.h"
#include "UOEvidenceSubsystem.h"
#include "UOInvestigation.h"
#include "UOAudioSubsystem.h"
#include "UOAudioRules.h"
#include "UOAIDifficulty.h"
#include "UOBossQuestions.h"
#include "UOBossSystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Math/RandomStream.h"
#include "UnpaidOvertime.h"

#define LOCTEXT_NAMESPACE "UORoleSystem"

UUORoleSystemComponent::UUORoleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AUOGameMode* UUORoleSystemComponent::GetGameMode() const
{
	return Cast<AUOGameMode>(GetOwner());
}

AUOGameState* UUORoleSystemComponent::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

float UUORoleSystemComponent::Now() const
{
	const AUOGameState* GameState = GetUOGameState();
	return GameState ? GameState->GetShiftElapsedSeconds() : 0.f;
}

bool UUORoleSystemComponent::IsOvertime() const
{
	const AUOGameState* GameState = GetUOGameState();
	return GameState && GameState->GetPhase() == EUODayPhase::Overtime;
}

void UUORoleSystemComponent::AssignRoles(EUOKillerAssignment Assignment, int32 InSeed, bool bInKillerCanKill, int32 KillerCount)
{
	Seed = InSeed;
	bKillerCanKill = bInKillerCanKill;
	GoneKillers.Reset();
	States.Reset();
	KillsThisMatch.Reset();
	Active.Reset();
	KillerIds.Reset();

	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	GameState->ClearRevealedRoles();

	TArray<FUORoleCandidate> Candidates;
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		if (Employee && Employee->IsAlive())
		{
			FUORoleCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.EmployeeId = Employee->GetEmployeeId();
			Candidate.bIsHuman = !Employee->IsAIEmployee();
		}
	}
	const int32 Count = KillerCount > 0 ? FMath::Min(KillerCount, Candidates.Num()) : FUOKillerTuning::GetKillerCount(Candidates.Num());
	KillerIds = FUORoleRules::PickKillers(Candidates, Assignment, Seed, Count);

	FString Names;
	for (AUOPlayerState* Employee : GameState->GetEmployees())
	{
		if (!Employee)
		{
			continue;
		}
		const bool bKiller = KillerIds.Contains(Employee->GetEmployeeId());
		Employee->SetSecretRole(bKiller ? EUORole::Killer : EUORole::Employee);
		Employee->SetExemptFromDirectFiring(bKiller);
		Employee->GetMutableKillerView() = FUOKillerPrivateView();
		if (bKiller)
		{
			States.Add(Employee->GetEmployeeId()).ResetForDay();
			Names += FString::Printf(TEXT("%s (%s); "), *Employee->GetDisplayName(), Employee->IsAIEmployee() ? TEXT("AI") : TEXT("HUMAN"));
		}
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Roles: %d killer(s) among %d employees: %s(assignment %s, seed %d, kill %s). Killers know each other. [server audit; never shown to other employees]"),
		KillerIds.Num(), Candidates.Num(), *Names, *StaticEnum<EUOKillerAssignment>()->GetNameStringByValue(static_cast<int64>(Assignment)), Seed,
		bKillerCanKill ? TEXT("enabled") : TEXT("disabled"));
	RefreshPrivateView();
}

void UUORoleSystemComponent::StartDay(int32 InDayNumber)
{
	DayNumber = InDayNumber;
	for (TPair<int32, FUOKillerState>& Pair : States)
	{
		Pair.Value.ResetForDay();
	}
	Active.Reset();
	RefreshPrivateView();
}

EUORole UUORoleSystemComponent::GetOwnRole(const AUOPlayerState* Asker) const
{
	if (!Asker || KillerIds.Num() == 0)
	{
		return EUORole::Unknown;
	}
	return IsKillerServer(Asker->GetEmployeeId()) ? EUORole::Killer : EUORole::Employee;
}

TArray<int32> UUORoleSystemComponent::GetAccomplicesOf(const AUOPlayerState* Asker) const
{
	TArray<int32> Accomplices;
	if (Asker && IsKillerServer(Asker->GetEmployeeId()))
	{
		for (const int32 Id : KillerIds)
		{
			if (Id != Asker->GetEmployeeId())
			{
				Accomplices.Add(Id);
			}
		}
	}
	return Accomplices;
}

bool UUORoleSystemComponent::IsKillerActive(int32 EmployeeId) const
{
	if (!IsKillerServer(EmployeeId) || GoneKillers.Contains(EmployeeId))
	{
		return false;
	}
	const AUOGameState* GameState = GetUOGameState();
	const AUOPlayerState* Killer = GameState ? GameState->FindEmployee(EmployeeId) : nullptr;
	return Killer && Killer->IsAlive();
}

bool UUORoleSystemComponent::IsAnyKillerActive() const
{
	return KillerIds.ContainsByPredicate([this](int32 Id) { return IsKillerActive(Id); });
}

const FUOKillerState& UUORoleSystemComponent::GetKillerState(int32 EmployeeId) const
{
	static const FUOKillerState Empty;
	const FUOKillerState* Found = States.Find(EmployeeId);
	return Found ? *Found : Empty;
}

void UUORoleSystemComponent::OnEmployeeEliminated(const AUOPlayerState* Employee)
{
	if (Employee && IsKillerServer(Employee->GetEmployeeId()) && !GoneKillers.Contains(Employee->GetEmployeeId()))
	{
		GoneKillers.Add(Employee->GetEmployeeId());
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Roles: a killer (%s) left the match (%s). %d killer(s) still employed."), *Employee->GetDisplayName(),
			*StaticEnum<EUOExitCause>()->GetNameStringByValue(static_cast<int64>(Employee->GetExitCause())),
			KillerIds.FilterByPredicate([this](int32 Id) { return IsKillerActive(Id); }).Num());
	}
}

void UUORoleSystemComponent::TickShift(float DeltaSeconds)
{
	for (TPair<int32, FUOKillerState>& Pair : States)
	{
		Pair.Value.Tick(DeltaSeconds);
	}

	const float Time = Now();
	UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	AUOGameMode* GameMode = GetGameMode();
	for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
	{
		const FTimedSabotage Timed = Active[Index];
		if (Time < Timed.EndsAt)
		{
			continue;
		}
		Active.RemoveAt(Index);
		AUOInteractableStation* Target = Timed.Target.Get();
		if (!Target)
		{
			continue;
		}

		FUOTraceRecord Record;
		Record.Room = Target->GetRoomId();
		Record.StationId = Target->GetStationId();
		Record.Subject = Target->GetDisplayName().ToString();
		if (Timed.Type == EUOSabotageType::LockStation)
		{
			Record.Device = EUOTraceDevice::DoorLog;
			Record.Kind = EUOTraceKind::StationUnlocked;
		}
		else
		{
			Record.Device = EUOTraceDevice::CameraLog;
			Record.Kind = EUOTraceKind::CameraOnline;
			Record.ValueA = FMath::RoundToInt(Timed.EndsAt - Timed.StartedAt);
			if (AUOTerminal* Camera = Cast<AUOTerminal>(Target))
			{
				Camera->SetStatusLight(true);
			}
			if (Evidence)
			{
				Evidence->SetRoomDark(Target->GetRoomId(), false);
			}
			if (GameMode)
			{
				GameMode->SetRoomBlackout(Target->GetRoomId(), false);
			}
		}
		if (Evidence)
		{
			Evidence->Record(Record);
		}
	}
	RefreshPrivateView();
}

void UUORoleSystemComponent::RefreshPrivateView()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	for (const int32 Id : KillerIds)
	{
		AUOPlayerState* Killer = GameState->FindEmployee(Id);
		if (!Killer)
		{
			continue;
		}
		const FUOKillerState& State = GetKillerState(Id);
		FUOKillerPrivateView& View = Killer->GetMutableKillerView();
		View.Heat = State.Heat;
		View.MaxHeat = FUOKillerTuning::MaxHeat;
		View.ReadyAt.SetNum(static_cast<int32>(EUOSabotageType::Count));
		for (int32 Index = 0; Index < View.ReadyAt.Num(); ++Index)
		{
			View.ReadyAt[Index] = State.ReadyAt[Index];
		}
		View.bKillEnabled = bKillerCanKill;
		View.KillReadyAt = State.KillReadyAt;
		View.KillsToday = State.KillsToday;
		View.KillsThisMatch = KillsThisMatch.FindRef(Id);
		View.AccompliceIds = GetAccomplicesOf(Killer);
	}
}

// ~ Fake rating

FUOFakeRatingOffer UUORoleSystemComponent::MakeFakeRatingOffer(int32 KillerId) const
{
	const AUOGameState* GameState = GetUOGameState();
	const AUOPlayerState* Killer = GameState ? GameState->FindEmployee(KillerId) : nullptr;
	return FUOFakeRating::MakeOffer(Killer ? Killer->GetRealStars() : 0);
}

void UUORoleSystemComponent::ApplyFakeRating(int32 KillerId, int32 ShownStars, const FUOFakeRatingOffer& Offer, int32 TotalSubObjectives, int32 AliveEmployees, float FairShare)
{
	AUOGameState* GameState = GetUOGameState();
	AUOPlayerState* Killer = GameState ? GameState->FindEmployee(KillerId) : nullptr;
	if (!Killer || !IsKillerServer(KillerId))
	{
		return;
	}
	const int32 Shown = Offer.IsAllowed(ShownStars) ? ShownStars : Offer.DefaultStars;
	const int32 RealStars = Offer.RealStars;
	const int32 RealDone = Killer->GetCompletedCount();
	const int32 Displayed = FUOFakeRating::GetDisplayedCompleted(Shown, RealDone, TotalSubObjectives, AliveEmployees);
	const float Ratio = FUOStarRating::ComputeContributionRatio(Displayed, TotalSubObjectives, AliveEmployees);
	Killer->SetEvaluation(Shown, FairShare, FUOStarRating::GetConsequence(Shown), Ratio);
	Killer->SetShownEvaluation(Shown, Displayed, FUOStarRating::GetConsequence(Shown), Ratio);
	Killer->SetRealStars(RealStars);
	Killer->GetMutableKillerView().bFakeRatingPending = false;

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Fake rating: %s (killer) really earned %d star(s) (%d done, real work counts); shows %d star(s) and %d done%s. [server audit]"),
		*Killer->GetDisplayName(), RealStars, RealDone, Shown, Displayed,
		FUOFakeRating::IsMandatoryDefense(Shown) ? TEXT(" -> mandatory defense, answer always accepted") : (FUOFakeRating::GrantsFakeShield(Shown) ? TEXT(" -> promotion question, always accepted, fake shield") : TEXT("")));
	RefreshPrivateView();
}

// ~ Sabotage

EUOTraceDetail UUORoleSystemComponent::GetActDetail(const AUOPlayerState* Killer, EUOTraceDetail Base) const
{
	EUOTraceDetail Detail = Base;
	if (Killer && Killer->IsAIEmployee() && Detail != EUOTraceDetail::Anonymous)
	{
		// Doc 9.2 "Astucia": sabotage with fewer traces
		const AUOGameMode* GameMode = GetGameMode();
		const FUOAIDifficultyParams Params = FUOAIDifficulty::Compute(GameMode ? GameMode->GetMatchConfig().AIDifficulty : EUOAIDifficulty::Medium, DayNumber);
		const FUOKillerState& State = GetKillerState(Killer->GetEmployeeId());
		FRandomStream Rng(Seed * 31 + DayNumber * 977 + State.SabotagesToday * 13 + State.KillsToday * 7 + Killer->GetEmployeeId());
		if (Rng.FRand() < Params.Cunning * 0.5f)
		{
			Detail = static_cast<EUOTraceDetail>(static_cast<uint8>(Detail) - 1);
		}
	}
	return Detail;
}

FText UUORoleSystemComponent::DescribeBlock(EUOKillerBlock Block, float Seconds)
{
	switch (Block)
	{
	case EUOKillerBlock::CoolingDown:	return FText::Format(LOCTEXT("BlockCooldown", "Cooling down — {0}s"), FText::AsNumber(FMath::CeilToInt(Seconds)));
	case EUOKillerBlock::TooMuchHeat:	return LOCTEXT("BlockHeat", "Too much heat — lie low for a while");
	case EUOKillerBlock::TooEarly:		return LOCTEXT("BlockEarly", "Too early — everyone is still settling in");
	case EUOKillerBlock::Disabled:		return LOCTEXT("BlockDisabled", "Dismissals are disabled for this match");
	case EUOKillerBlock::Accomplice:	return LOCTEXT("BlockAccomplice", "Your accomplice. Not them.");
	case EUOKillerBlock::Witnesses:		return LOCTEXT("BlockWitnesses", "Someone is watching");
	case EUOKillerBlock::OutOfRange:	return LOCTEXT("BlockRange", "Get closer");
	default:							return FText::GetEmpty();
	}
}

bool UUORoleSystemComponent::GetSabotageFor(const AUOInteractableStation* Target, EUOSabotageType& OutType, FText& OutBlocked) const
{
	if (!Target)
	{
		return false;
	}
	if (const AUOTerminal* Terminal = Cast<AUOTerminal>(Target))
	{
		switch (Terminal->GetTerminalKind())
		{
		case EUOTerminalKind::DeskComputer:
			OutType = EUOSabotageType::CorruptFile;
			if (PickReportToCorrupt() == INDEX_NONE)
			{
				OutBlocked = LOCTEXT("NoReports", "No finished work to tamper with yet");
			}
			return true;
		case EUOTerminalKind::SecurityCamera:
			OutType = EUOSabotageType::CutCamera;
			if (Terminal->IsCameraOffline(Now()))
			{
				OutBlocked = LOCTEXT("CameraOff", "Already offline");
			}
			return true;
		case EUOTerminalKind::MovableProp:
			OutType = EUOSabotageType::MoveObject;
			if (Terminal->IsPropMoved())
			{
				OutBlocked = LOCTEXT("PropMoved", "Already moved");
			}
			return true;
		default:
			return false;
		}
	}

	if (Target->GetStationType() == EUOStationType::Printer)
	{
		OutType = EUOSabotageType::JamPrinter;
		if (Target->IsJammed())
		{
			OutBlocked = LOCTEXT("AlreadyJammed", "Already jammed");
		}
		return true;
	}
	OutType = EUOSabotageType::LockStation;
	if (Target->IsLocked(Now()))
	{
		OutBlocked = LOCTEXT("AlreadyLocked", "Already locked");
	}
	return true;
}

int32 UUORoleSystemComponent::PickReportToCorrupt() const
{
	const AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	int32 BestOwnerCount = -1;
	for (const FUOTaskInstance& Task : GameState->GetTasks())
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			// Never a fellow killer's work
			if (!Sub.IsDone() || IsKillerServer(Sub.CompletedBy))
			{
				continue;
			}
			// Hurt whoever is doing best; reports first
			const int32 OwnerCount = FUOTaskBoard::CountCompletedBy(GameState->GetTasks(), Sub.CompletedBy) * 2 + (Task.Type == EUOTaskType::FileReports ? 1 : 0);
			if (OwnerCount > BestOwnerCount)
			{
				BestOwnerCount = OwnerCount;
				Best = Sub.Id;
			}
		}
	}
	return Best;
}

FUOInteractionOption UUORoleSystemComponent::QuerySabotage(const AUOPlayerState* Employee, const AUOInteractableStation* Target) const
{
	FUOInteractionOption Option;
	const AUOGameState* GameState = GetUOGameState();
	if (!Employee || !Target || !IsKillerActive(Employee->GetEmployeeId()) || !GameState || !GameState->IsWorkPhase())
	{
		return Option;
	}
	const FUOKillerState& State = GetKillerState(Employee->GetEmployeeId());

	EUOSabotageType Type = EUOSabotageType::JamPrinter;
	FText Blocked;
	if (!GetSabotageFor(Target, Type, Blocked))
	{
		return Option;
	}

	Option.Kind = EUOInteractionKind::Sabotage;
	Option.Sabotage = Type;
	Option.Prompt = FUOTraceText::GetSabotageName(Type);
	Option.HoldSeconds = FUOKillerTuning::GetHoldSeconds(Type);
	if (!Blocked.IsEmpty())
	{
		Option.BlockedReason = Blocked;
		return Option;
	}
	const EUOKillerBlock Block = FUOSabotageRules::CanSabotage(State, Type, Now(), DayNumber);
	if (Block != EUOKillerBlock::None)
	{
		Option.BlockedReason = DescribeBlock(Block, FUOSabotageRules::GetRemainingCooldown(State, Type, Now()));
		return Option;
	}
	Option.bAvailable = true;
	return Option;
}

bool UUORoleSystemComponent::PerformSabotage(AUOPlayerState* Employee, AUOInteractableStation* Target)
{
	const FUOInteractionOption Option = QuerySabotage(Employee, Target);
	AUOGameMode* GameMode = GetGameMode();
	AUOGameState* GameState = GetUOGameState();
	UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	if (!Option.bAvailable || !GameMode || !GameState || !Evidence)
	{
		return false;
	}
	const APawn* Pawn = Employee->GetPawn();
	if (!Pawn || FVector::Dist2D(Pawn->GetActorLocation(), Target->GetInteractionLocation()) > 450.f)
	{
		return false;
	}

	const float Time = Now();
	const EUOSabotageType Type = Option.Sabotage;
	TArray<int32> Witnesses;
	Evidence->CountWitnesses(Pawn, nullptr, &Witnesses);

	FUOTraceRecord Record;
	Record.TrueActorId = Employee->GetEmployeeId();
	Record.Detail = GetActDetail(Employee, FUOKillerTuning::GetTraceDetail(DayNumber));
	Record.Room = Target->GetRoomId();
	Record.StationId = Target->GetStationId();
	Record.Subject = Target->GetDisplayName().ToString();
	Record.bAct = true;

	FName Sound = NAME_None;
	FString Effect;
	switch (Type)
	{
	case EUOSabotageType::JamPrinter:
		Record.Device = EUOTraceDevice::PrinterLog;
		Record.Kind = EUOTraceKind::PrinterJam;
		Evidence->Record(Record);
		Target->SetJammed(true);
		Sound = UOSoundId::PrinterJam;
		Effect = TEXT("printer jammed until someone clears it");
		break;

	case EUOSabotageType::CorruptFile:
	{
		const int32 SubId = PickReportToCorrupt();
		const int32 LostBy = FUOTaskBoard::Uncomplete(GameState->GetMutableTasks(), SubId);
		if (LostBy == INDEX_NONE)
		{
			return false;
		}
		if (AUOPlayerState* Owner = GameState->FindEmployee(LostBy))
		{
			Owner->SetCompletedCount(FUOTaskBoard::CountCompletedBy(GameState->GetTasks(), LostBy));
		}
		if (const FUOSubObjective* Sub = FUOTaskBoard::FindSubObjective(GameState->GetTasks(), SubId))
		{
			if (AUOInteractableStation* Station = GameMode->GetTaskManager()->FindStation(Sub->StationId))
			{
				Station->SetWorkDone(false);
			}
		}
		Record.Device = EUOTraceDevice::FileHistory;
		Record.Kind = EUOTraceKind::FileOverwritten;
		Record.ValueA = 100 + SubId;
		Evidence->Record(Record);
		Sound = UOSoundId::FileCorrupt;
		Effect = FString::Printf(TEXT("sub-objective %d undone, %s loses the credit"), SubId, *Evidence->NameOf(LostBy));
		break;
	}

	case EUOSabotageType::LockStation:
		Record.Device = EUOTraceDevice::DoorLog;
		Record.Kind = EUOTraceKind::StationLocked;
		Record.ValueA = FMath::RoundToInt(FUOKillerTuning::LockSeconds);
		Evidence->Record(Record);
		Target->SetLockedUntil(Time + FUOKillerTuning::LockSeconds);
		Active.Add({ Target, Type, Time + FUOKillerTuning::LockSeconds, Time });
		Sound = UOSoundId::LockClunk;
		Effect = FString::Printf(TEXT("locked for %.0fs"), FUOKillerTuning::LockSeconds);
		break;

	case EUOSabotageType::CutCamera:
		Record.Device = EUOTraceDevice::CameraLog;
		Record.Kind = EUOTraceKind::CameraOffline;
		// Witnesses see the act before the lights go out
		Evidence->Record(Record);
		if (AUOTerminal* Camera = Cast<AUOTerminal>(Target))
		{
			Camera->SetOfflineUntil(Time + FUOKillerTuning::CameraOutageSeconds);
		}
		Evidence->SetRoomDark(Target->GetRoomId(), true);
		GameMode->SetRoomBlackout(Target->GetRoomId(), true);
		Active.Add({ Target, Type, Time + FUOKillerTuning::CameraOutageSeconds, Time });
		Sound = UOSoundId::PowerCut;
		Effect = FString::Printf(TEXT("%s camera offline and lights out for %.0fs"), *Target->GetRoomId().ToString(), FUOKillerTuning::CameraOutageSeconds);
		break;

	case EUOSabotageType::MoveObject:
	{
		AUOTerminal* Prop = Cast<AUOTerminal>(Target);
		FTransform Destination;
		FName ToRoom;
		if (!Prop || !GameMode->FindPropDestination(Prop, Destination, ToRoom))
		{
			return false;
		}
		const FName FromRoom = Prop->GetRoomId();
		EUOPropKind Kind = EUOPropKind::Plant;
		int32 Before = 0;
		// Count from the boss facts (as the level was built) plus today's earlier moves
		if (const UUOBossSystemComponent* BossSystem = GameMode->GetBossSystem())
		{
			Before = BossSystem->GetFacts().Count(FromRoom, Kind);
			for (const FUOPropMove& Earlier : Evidence->GetPropMoves())
			{
				Before += (Earlier.ToRoom == FromRoom && Earlier.Kind == Kind) ? 1 : 0;
				Before -= (Earlier.FromRoom == FromRoom && Earlier.Kind == Kind) ? 1 : 0;
			}
		}
		Record.Device = EUOTraceDevice::Inventory;
		Record.Kind = EUOTraceKind::PropMissing;
		Record.Subject = LOCTEXT("OnePlant", "1 plant").ToString();
		Record.ValueA = FMath::Max(1, Before);
		Record.ValueB = FMath::Max(0, Before - 1);
		Evidence->Record(Record);
		Prop->MovePropTo(Destination);
		FUOPropMove Move;
		Move.Time = Time;
		Move.Kind = Kind;
		Move.FromRoom = FromRoom;
		Move.ToRoom = ToRoom;
		Evidence->AddPropMove(Move);
		Sound = UOSoundId::PropScrape;
		Effect = FString::Printf(TEXT("plant moved %s -> %s (boss questions about it become trick questions)"), *FromRoom.ToString(), *ToRoom.ToString());
		break;
	}

	default:
		return false;
	}

	FUOKillerState& State = States.FindOrAdd(Employee->GetEmployeeId());
	FUOSabotageRules::Apply(State, Type, Time, DayNumber, IsOvertime());
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
	{
		Audio->PlayAtLocation(Sound, Target->GetFocusLocation());
	}

	FString WitnessNames;
	for (const int32 Id : Witnesses)
	{
		WitnessNames += Evidence->NameOf(Id) + TEXT("; ");
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Sabotage: %s (killer, %s) -> %s at %s in %s: %s. Heat %.0f/%.0f, cooldown %.0fs%s, trace detail %s, witnesses: %s"),
		*Employee->GetDisplayName(), Employee->IsAIEmployee() ? TEXT("AI") : TEXT("human"), *StaticEnum<EUOSabotageType>()->GetNameStringByValue(static_cast<int64>(Type)),
		*Target->GetDisplayName().ToString(), *Target->GetRoomId().ToString(), *Effect, State.Heat, FUOKillerTuning::MaxHeat,
		FUOSabotageRules::GetCooldown(Type, DayNumber, IsOvertime()), IsOvertime() ? TEXT(" (overtime)") : TEXT(""),
		*StaticEnum<EUOTraceDetail>()->GetNameStringByValue(static_cast<int64>(Record.Detail)), WitnessNames.IsEmpty() ? TEXT("none") : *WitnessNames);
	RefreshPrivateView();
	return true;
}

// ~ Kill

FUOInteractionOption UUORoleSystemComponent::QueryKill(const AUOPlayerState* Employee, const AUOPlayerState* Victim) const
{
	FUOInteractionOption Option;
	const AUOGameState* GameState = GetUOGameState();
	if (!Employee || !Victim || Employee == Victim || !Victim->IsAlive() || !IsKillerActive(Employee->GetEmployeeId())
		|| !bKillerCanKill || !GameState || !GameState->IsWorkPhase())
	{
		return Option;
	}

	const APawn* Pawn = Employee->GetPawn();
	const APawn* VictimPawn = Victim->GetPawn();
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	if (!Pawn || !VictimPawn || !Evidence)
	{
		return Option;
	}

	const FUOKillerState& State = GetKillerState(Employee->GetEmployeeId());
	const bool bAccomplice = IsKillerServer(Victim->GetEmployeeId());
	Option.Kind = EUOInteractionKind::Kill;
	Option.VictimId = Victim->GetEmployeeId();
	Option.HoldSeconds = FUOKillerTuning::KillHoldSeconds;
	Option.Prompt = FText::Format(LOCTEXT("KillPrompt", "Dismiss {0}"), FText::FromString(Victim->GetDisplayName()));
	// Fellow killers watching do not count as witnesses
	TArray<int32> WitnessIds;
	Evidence->CountWitnesses(Pawn, VictimPawn, &WitnessIds);
	WitnessIds.RemoveAll([this](int32 Id) { return IsKillerServer(Id); });
	const int32 Witnesses = bAccomplice ? 0 : WitnessIds.Num();
	const float Distance = FVector::Dist(Pawn->GetActorLocation(), VictimPawn->GetActorLocation());
	const EUOKillerBlock Block = FUOKillRules::CanKill(bKillerCanKill, State, Now(), Witnesses, Distance, bAccomplice);
	if (Block != EUOKillerBlock::None)
	{
		Option.BlockedReason = DescribeBlock(Block, State.KillReadyAt - Now());
		return Option;
	}
	Option.bAvailable = true;
	return Option;
}

bool UUORoleSystemComponent::PerformKill(AUOPlayerState* Employee, AUOPlayerState* Victim)
{
	const FUOInteractionOption Option = QueryKill(Employee, Victim);
	AUOGameMode* GameMode = GetGameMode();
	if (!Option.bAvailable || !GameMode)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Kill: rejected (%s)."), *Option.BlockedReason.ToString());
		return false;
	}
	const int32 KillerId = Employee->GetEmployeeId();
	int32& Kills = KillsThisMatch.FindOrAdd(KillerId);
	const EUOTraceDetail Detail = FUOKillerTuning::GetKillTraceDetail(DayNumber, Kills);
	++Kills;
	FUOKillRules::Apply(States.FindOrAdd(KillerId), Now(), IsOvertime());

	// Every kill makes the next one likelier to leave a public "last seen near" note
	FRandomStream Rng(Seed * 53 + DayNumber * 389 + KillerId * 17 + Kills * 7);
	const float NoteChance = FUOKillerTuning::GetLastSeenNoteChance(Kills);
	const bool bNote = Rng.FRand() < NoteChance;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Kill: %s (killer, %s) dismissed %s with no witnesses (kill #%d this match, %d today; trace detail %s; last-seen note %s at %.0f%%; next kill in %.0fs%s)."),
		*Employee->GetDisplayName(), Employee->IsAIEmployee() ? TEXT("AI") : TEXT("human"), *Victim->GetDisplayName(), Kills, GetKillerState(KillerId).KillsToday,
		*StaticEnum<EUOTraceDetail>()->GetNameStringByValue(static_cast<int64>(Detail)), bNote ? TEXT("YES") : TEXT("no"), NoteChance * 100.f,
		FUOKillerTuning::GetKillCooldown(IsOvertime()), IsOvertime() ? TEXT(", overtime") : TEXT(""));
	GameMode->HandleDismissal(Employee, Victim, Detail, bNote);
	RefreshPrivateView();
	return true;
}

#undef LOCTEXT_NAMESPACE
