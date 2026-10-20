// UNPAID: OVERTIME - AI employee brain, M4 part: investigation (terminals, suspicion, meeting statements and votes)
// and the killers' behaviour (sabotage and the optional kill; a killer's work is real and lives in the base state machine).

#include "UOAIController.h"
#include "UOAIEmployee.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOInteractableStation.h"
#include "UOInteractionComponent.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskBoard.h"
#include "UOTerminal.h"
#include "UORoleSystemComponent.h"
#include "UOEvidenceSubsystem.h"
#include "UOKillerRules.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UnpaidOvertime.h"

namespace UOAIKillerPrivate
{
	constexpr float SabotageGiveUpSeconds = 40.f;
	constexpr float StalkGiveUpSeconds = 30.f;
	constexpr float RepathSeconds = 1.2f;

	bool IsQuiet(EUOTraceKind Kind)
	{
		return Kind == EUOTraceKind::DoorEntry || Kind == EUOTraceKind::CameraSighting || Kind == EUOTraceKind::WorkReported || Kind == EUOTraceKind::ItemSignedOut;
	}

	/** Device worth checking after a sabotage of this kind */
	bool DeviceFor(EUOTraceKind Kind, EUOTraceDevice& OutDevice)
	{
		switch (Kind)
		{
		case EUOTraceKind::PrinterJam:		OutDevice = EUOTraceDevice::DoorLog; return true;
		case EUOTraceKind::FileOverwritten:	OutDevice = EUOTraceDevice::CameraLog; return true;
		case EUOTraceKind::StationLocked:	OutDevice = EUOTraceDevice::DoorLog; return true;
		case EUOTraceKind::CameraOffline:	OutDevice = EUOTraceDevice::DoorLog; return true;
		case EUOTraceKind::PropMissing:		OutDevice = EUOTraceDevice::CameraLog; return true;
		case EUOTraceKind::BodyFound:		OutDevice = EUOTraceDevice::DoorLog; return true;
		default:							return false;
		}
	}
}

bool AUOAIController::bDevSpareHuman = false;

UUORoleSystemComponent* AUOAIController::GetRoleSystem() const
{
	const UWorld* World = GetWorld();
	const AUOGameMode* GameMode = World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
	return GameMode ? GameMode->GetRoleSystem() : nullptr;
}

float AUOAIController::ShiftNow() const
{
	const AUOGameState* GameState = GetUOGameState();
	return GameState ? GameState->GetShiftElapsedSeconds() : 0.f;
}

void AUOAIController::ScheduleNext(float& InOutTime, float PerMinute)
{
	// Exponential gaps (a Poisson process), clamped so nothing happens in a burst or never
	const float Mean = 60.f / FMath::Max(0.05f, PerMinute);
	const float Gap = -Mean * FMath::Loge(FMath::Max(0.02f, Rng.FRand()));
	InOutTime = ShiftNow() + FMath::Clamp(Gap, 8.f, 240.f);
}

void AUOAIController::ResetKnowledge()
{
	KnownTraces.Reset();
	SeenActs.Reset();
	Sightings.Reset();
	KnownSabotageTraces.Reset();
	LastReadAt.Reset();
	AcceptedAnswers.Reset();

	// Its own role (and, as a killer, its accomplices): the only secrets it may know
	const UUORoleSystemComponent* Roles = GetRoleSystem();
	bIsKiller = Roles && Roles->GetOwnRole(GetEmployee()) == EUORole::Killer;
	Accomplices = Roles ? Roles->GetAccomplicesOf(GetEmployee()) : TArray<int32>();

	const float Cunning = FMath::Clamp(Difficulty.Cunning, 0.f, 1.f);
	NextSabotageAt = 0.f;
	NextTerminalAt = 0.f;
	NextKillAt = 0.f;
	// The first sabotage comes soon after the grace period (a day can end early when the team is fast)
	NextSabotageAt = FUOKillerTuning::SabotageGraceSeconds + Rng.FRandRange(3.f, 25.f);
	ScheduleNext(NextTerminalAt, FUOKillerTuning::GetTerminalVisitsPerMinute(Difficulty.Suspicion) * (bIsKiller ? 0.5f : 1.f));
	ScheduleNext(NextKillAt, FUOKillerTuning::AIKillPerMinute);
	NextKillAt = bKillForced ? 0.f : FMath::Max(NextKillAt, FUOKillerTuning::KillCooldownSeconds);
	if (bIsKiller)
	{
		FString Names;
		for (const int32 Id : Accomplices)
		{
			Names += FString::Printf(TEXT("%d "), Id);
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s knows its own role (accomplices: %s); first sabotage window at %.0fs, terminal visit at %.0fs."), *GetEmployee()->GetDisplayName(),
			Names.IsEmpty() ? TEXT("none") : *Names, NextSabotageAt, NextTerminalAt);
	}
}

void AUOAIController::ReleaseEverything()
{
	if (UUOInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->ClearTarget();
	}
	if (ClaimedSubObjectiveId != INDEX_NONE)
	{
		if (UUOTaskManagerComponent* TaskManager = GetTaskManager())
		{
			TaskManager->ReleaseClaim(GetEmployee(), ClaimedSubObjectiveId);
		}
	}
	ClaimedSubObjectiveId = INDEX_NONE;
	TargetStation.Reset();
	ItemSourceStation.Reset();
	TargetTerminal.Reset();
	SabotageTarget.Reset();
	KillVictim.Reset();
	Waypoints.Reset();
	StopMovement();
	NavMoveState = ENavMoveState::None;
	State = EUOAIState::Idle;
}

// ~ Activities

void AUOAIController::ChooseActivity()
{
	const float Now = ShiftNow();
	const UUORoleSystemComponent* Roles = GetRoleSystem();
	const int32 MyId = GetEmployee() ? GetEmployee()->GetEmployeeId() : INDEX_NONE;
	if (bIsKiller && Roles && Roles->IsKillerActive(MyId))
	{
		if (Roles->IsKillEnabled() && Now >= NextKillAt && TryStartKill())
		{
			return;
		}
		if (Now >= NextSabotageAt)
		{
			const bool bStarted = TryStartSabotage();
			UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s considers a sabotage at %.0fs (cunning %.2f, heat %.0f): %s."), *GetEmployee()->GetDisplayName(), Now,
				Difficulty.Cunning, Roles->GetKillerState(MyId).Heat, bStarted ? TEXT("going") : TEXT("no target available"));
			// On probation it lies low: fewer sabotages, more real work (it needs 2 real stars at the next review)
			const float LieLow = GetEmployee() && GetEmployee()->IsOnProbation() ? 0.3f : 1.f;
			ScheduleNext(NextSabotageAt, FUOKillerTuning::AISabotagePerMinute * LieLow * (0.6f + 0.8f * FMath::Clamp(Difficulty.Cunning, 0.f, 1.f)));
			if (bStarted)
			{
				return;
			}
		}
	}
	if (Now >= NextTerminalAt)
	{
		ScheduleNext(NextTerminalAt, FUOKillerTuning::GetTerminalVisitsPerMinute(Difficulty.Suspicion) * (bIsKiller ? 0.5f : 1.f));
		if (TryStartTerminalVisit())
		{
			return;
		}
	}
	EnterState(EUOAIState::Claim);
}

bool AUOAIController::TryStartTerminalVisit()
{
	const UWorld* World = GetWorld();
	const AUOGameMode* GameMode = World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
	const AUOGameState* GameState = GetUOGameState();
	const APawn* MyPawn = GetPawn();
	if (!GameMode || !GameState || !MyPawn)
	{
		return false;
	}

	// What it wants to check: who was around its latest known sabotage, else whatever it read least recently
	EUOTraceDevice Wanted = EUOTraceDevice::DoorLog;
	bool bWanted = false;
	float Latest = -1.f;
	for (const int32 Id : KnownSabotageTraces)
	{
		const FUOTraceEntry* Entry = GameState->FindTrace(Id);
		EUOTraceDevice Device;
		if (Entry && Entry->TimeSeconds > Latest && UOAIKillerPrivate::DeviceFor(Entry->Kind, Device))
		{
			Latest = Entry->TimeSeconds;
			Wanted = Device;
			bWanted = true;
		}
	}

	AUOTerminal* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AUOTerminal* Terminal : GameMode->GetTerminals())
	{
		EUOTraceDevice Device;
		if (!Terminal || !Terminal->GetDevice(Device) || Device == EUOTraceDevice::ActivityBoard)
		{
			continue;
		}
		const float* ReadAt = LastReadAt.Find(static_cast<uint8>(Device));
		float Score = FVector::Dist2D(MyPawn->GetActorLocation(), Terminal->GetInteractionLocation()) + Rng.FRandRange(0.f, 900.f);
		Score += ReadAt ? FMath::Max(0.f, 2500.f - (ShiftNow() - *ReadAt) * 20.f) : 0.f;
		Score -= bWanted && Device == Wanted ? 3000.f : 0.f;
		// The device that logged the sabotage itself is worth a look too
		if (Latest >= 0.f && !bWanted)
		{
			Score -= 500.f;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Terminal;
		}
	}
	if (!Best)
	{
		return false;
	}
	TargetTerminal = Best;
	EnterState(EUOAIState::GoToTerminal);
	MoveToPoint(Best->GetInteractionLocation());
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI investigation: %s goes to read the %s (suspicion %.2f%s)."), *GetEmployee()->GetDisplayName(),
		*Best->GetDisplayName().ToString(), Difficulty.Suspicion, bWanted ? TEXT(", following up on a sabotage it knows about") : TEXT(""));
	return true;
}

void AUOAIController::TickTerminal(float DeltaSeconds)
{
	AUOTerminal* Terminal = TargetTerminal.Get();
	if (!Terminal)
	{
		EnterState(EUOAIState::Idle);
		return;
	}
	if (State == EUOAIState::GoToTerminal)
	{
		switch (UpdateMovement(DeltaSeconds))
		{
		case EMoveStatus::Arrived:
			EnterState(EUOAIState::ReadTerminal);
			StateTimer = FUOKillerTuning::TerminalReadSeconds;
			FaceLocation(Terminal->GetFocusLocation());
			break;
		case EMoveStatus::Failed:
			TargetTerminal.Reset();
			EnterState(EUOAIState::Idle);
			break;
		default:
			break;
		}
		return;
	}
	if (StateTimer <= 0.f)
	{
		EUOTraceDevice Device;
		if (Terminal->GetDevice(Device))
		{
			ReadDevice(Device, Terminal->GetDisplayName().ToString());
		}
		TargetTerminal.Reset();
		EnterState(EUOAIState::Idle);
	}
}

bool AUOAIController::TryStartSabotage()
{
	const UWorld* World = GetWorld();
	const AUOGameMode* GameMode = World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
	const AUOGameState* GameState = GetUOGameState();
	UUORoleSystemComponent* Roles = GetRoleSystem();
	AUOPlayerState* Me = GetEmployee();
	const APawn* MyPawn = GetPawn();
	if (!GameMode || !GameState || !Roles || !Me || !MyPawn)
	{
		return false;
	}

	// Who is doing best (public: the work reports everyone sees)
	const AUOPlayerState* Top = nullptr;
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		if (Employee && Employee->IsAlive() && Employee != Me && (!Top || Employee->GetCompletedCount() > Top->GetCompletedCount()))
		{
			Top = Employee;
		}
	}

	TArray<AUOInteractableStation*> Targets;
	for (AUOInteractableStation* Station : GameMode->GetLevelStations())
	{
		Targets.Add(Station);
	}
	for (AUOTerminal* Terminal : GameMode->GetTerminals())
	{
		Targets.Add(Terminal);
	}

	const float Cunning = FMath::Clamp(Difficulty.Cunning, 0.f, 1.f);
	AUOInteractableStation* Best = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	FUOInteractionOption BestOption;
	for (AUOInteractableStation* Target : Targets)
	{
		const FUOInteractionOption Option = Target ? Roles->QuerySabotage(Me, Target) : FUOInteractionOption();
		if (!Option.bAvailable)
		{
			continue;
		}
		float Score = 0.f;
		switch (Option.Sabotage)
		{
		case EUOSabotageType::CorruptFile:	Score = 3.f; break;
		case EUOSabotageType::JamPrinter:	Score = 2.2f + (FUOTaskBoard::CountPendingAtStation(GameState->GetTasks(), Target->GetStationId()) > 0 ? 1.f : 0.f); break;
		case EUOSabotageType::LockStation:	Score = 1.5f + (FUOTaskBoard::CountPendingAtStation(GameState->GetTasks(), Target->GetStationId()) > 0 ? 1.f : 0.f); break;
		case EUOSabotageType::CutCamera:	Score = 1.2f; break;
		default:							Score = 1.f; break;
		}
		const FVector Where = Target->GetInteractionLocation();
		Score -= FVector::Dist2D(MyPawn->GetActorLocation(), Where) / 1500.f;
		// Fewer people around the target is better (a cunning killer cares more)
		int32 Nearby = 0;
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			const APawn* OtherPawn = Employee ? Employee->GetPawn() : nullptr;
			if (Employee && Employee != Me && Employee->IsAlive() && OtherPawn && FVector::Dist2D(OtherPawn->GetActorLocation(), Where) < 1000.f)
			{
				++Nearby;
			}
		}
		Score -= Nearby * (0.5f + Cunning);
		// Hurt whoever is doing well
		const APawn* TopPawn = Top ? Top->GetPawn() : nullptr;
		if (TopPawn && FVector::Dist2D(TopPawn->GetActorLocation(), Where) < 800.f)
		{
			Score += 1.f;
		}
		Score += Rng.FRandRange(0.f, 0.8f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Target;
			BestOption = Option;
		}
	}
	if (!Best)
	{
		return false;
	}
	SabotageTarget = Best;
	ActivityTimer = UOAIKillerPrivate::SabotageGiveUpSeconds;
	WitnessWait = 0.f;
	EnterState(EUOAIState::GoToSabotage);
	MoveToPoint(Best->GetInteractionLocation());
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s heads to %s (%s, score %.2f, cunning %.2f)."), *Me->GetDisplayName(), *Best->GetDisplayName().ToString(),
		*StaticEnum<EUOSabotageType>()->GetNameStringByValue(static_cast<int64>(BestOption.Sabotage)), BestScore, Cunning);
	return true;
}

void AUOAIController::TickSabotage(float DeltaSeconds)
{
	AUOInteractableStation* Target = SabotageTarget.Get();
	UUOInteractionComponent* Interaction = GetInteraction();
	ActivityTimer -= DeltaSeconds;
	if (!Target || !Interaction || ActivityTimer <= 0.f)
	{
		SabotageTarget.Reset();
		AbandonCurrentWork();
		return;
	}

	if (State == EUOAIState::GoToSabotage)
	{
		switch (UpdateMovement(DeltaSeconds))
		{
		case EMoveStatus::Arrived:
			EnterState(EUOAIState::Sabotage);
			FaceLocation(Target->GetFocusLocation());
			bHasSecondaryResult = false;
			WitnessWait = 0.f;
			break;
		case EMoveStatus::Failed:
			AbandonCurrentWork();
			break;
		default:
			break;
		}
		return;
	}

	if (bHasSecondaryResult)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s %s the sabotage at %s."), *GetEmployee()->GetDisplayName(), bSecondaryResult ? TEXT("completed") : TEXT("did not complete"),
			*Target->GetDisplayName().ToString());
		bHasSecondaryResult = false;
		SabotageTarget.Reset();
		EnterState(EUOAIState::Idle);
		return;
	}
	if (Interaction->IsSecondaryHolding())
	{
		return;
	}

	// Doc 9.2 "Astucia": wait for the witnesses to leave; a sloppy killer may go ahead anyway
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	TArray<int32> Witnesses;
	if (Evidence)
	{
		Evidence->CountWitnesses(GetPawn(), nullptr, &Witnesses);
	}
	// Accomplices watching are no problem
	Witnesses.RemoveAll([this](int32 Id) { return Accomplices.Contains(Id); });
	const int32 Count = Witnesses.Num();
	if (Count > 0)
	{
		const float Cunning = FMath::Clamp(Difficulty.Cunning, 0.f, 1.f);
		const float Patience = 2.f + 6.f * Cunning;
		WitnessWait += DeltaSeconds;
		if (WitnessWait < Patience)
		{
			return;
		}
		if (Rng.FRand() >= (1.f - Cunning) * 0.6f)
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s gives up on %s, %d witness(es) around (cunning %.2f)."), *GetEmployee()->GetDisplayName(),
				*Target->GetDisplayName().ToString(), Count, Cunning);
			SabotageTarget.Reset();
			EnterState(EUOAIState::Idle);
			return;
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s sabotages %s despite %d witness(es) (sloppy, cunning %.2f)."), *GetEmployee()->GetDisplayName(),
			*Target->GetDisplayName().ToString(), Count, Cunning);
	}
	bHasSecondaryResult = false;
	Interaction->BeginSecondary(Target, nullptr);
	if (!Interaction->IsSecondaryHolding() && !bHasSecondaryResult)
	{
		SabotageTarget.Reset();
		EnterState(EUOAIState::Idle);
	}
}

bool AUOAIController::TryStartKill()
{
	const AUOGameState* GameState = GetUOGameState();
	UUORoleSystemComponent* Roles = GetRoleSystem();
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	AUOPlayerState* Me = GetEmployee();
	const APawn* MyPawn = GetPawn();
	if (!GameState || !Roles || !Evidence || !Me || !MyPawn || ShiftNow() < Roles->GetKillerState(Me->GetEmployeeId()).KillReadyAt)
	{
		return false;
	}

	// Most considerations end here: a dismissal is a rare, deliberate move for an AI (the dev switch skips this)
	if (!bKillForced && Rng.FRand() >= FUOKillerTuning::AIKillCommitChance)
	{
		ScheduleNext(NextKillAt, FUOKillerTuning::AIKillPerMinute);
		return false;
	}

	AUOPlayerState* Best = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (AUOPlayerState* Employee : GameState->GetEmployees())
	{
		const APawn* OtherPawn = Employee ? Employee->GetPawn() : nullptr;
		if (!Employee || Employee == Me || !Employee->IsAlive() || !OtherPawn || (bDevSpareHuman && !Employee->IsAIEmployee())
			|| Accomplices.Contains(Employee->GetEmployeeId()))
		{
			continue;
		}
		TArray<int32> Watching;
		Evidence->CountWitnesses(OtherPawn, MyPawn, &Watching);
		Watching.RemoveAll([this](int32 Id) { return Accomplices.Contains(Id); });
		const bool bAlone = Watching.Num() == 0;
		float Score = Employee->GetCompletedCount() * 0.5f + (bAlone ? 3.f : 0.f) - FVector::Dist2D(MyPawn->GetActorLocation(), OtherPawn->GetActorLocation()) / 1000.f;
		Score += Rng.FRandRange(0.f, 1.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Employee;
		}
	}
	ScheduleNext(NextKillAt, FUOKillerTuning::AIKillPerMinute);
	if (!Best)
	{
		return false;
	}
	KillVictim = Best;
	ActivityTimer = UOAIKillerPrivate::StalkGiveUpSeconds;
	WitnessWait = 0.f;
	EnterState(EUOAIState::StalkVictim);
	MoveToPoint(Best->GetPawn()->GetActorLocation());
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s starts following %s (score %.2f)."), *Me->GetDisplayName(), *Best->GetDisplayName(), BestScore);
	return true;
}

void AUOAIController::TickStalk(float DeltaSeconds)
{
	AUOPlayerState* Victim = KillVictim.Get();
	const APawn* VictimPawn = Victim ? Victim->GetPawn() : nullptr;
	UUOInteractionComponent* Interaction = GetInteraction();
	UUORoleSystemComponent* Roles = GetRoleSystem();
	ActivityTimer -= DeltaSeconds;
	if (!Victim || !Victim->IsAlive() || !VictimPawn || !Interaction || !Roles || ActivityTimer <= 0.f)
	{
		if (ActivityTimer <= 0.f && Victim)
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("AI killer: %s gave up following %s (never alone)."), *GetEmployee()->GetDisplayName(), *Victim->GetDisplayName());
		}
		KillVictim.Reset();
		EnterState(EUOAIState::Idle);
		return;
	}

	if (State == EUOAIState::Kill)
	{
		if (bHasSecondaryResult)
		{
			bHasSecondaryResult = false;
			if (bSecondaryResult)
			{
				KillVictim.Reset();
				EnterState(EUOAIState::Idle);
			}
			else
			{
				EnterState(EUOAIState::StalkVictim);
			}
		}
		return;
	}

	const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), VictimPawn->GetActorLocation());
	if (Distance <= FUOKillerTuning::KillRange * 0.85f)
	{
		StopMovement();
		FaceLocation(VictimPawn->GetActorLocation());
		const FUOInteractionOption Option = Roles->QueryKill(GetEmployee(), Victim);
		if (Option.bAvailable)
		{
			bHasSecondaryResult = false;
			EnterState(EUOAIState::Kill);
			Interaction->BeginSecondary(nullptr, Victim);
			return;
		}
	}
	WitnessWait -= DeltaSeconds;
	if (WitnessWait <= 0.f)
	{
		WitnessWait = UOAIKillerPrivate::RepathSeconds;
		MoveToPoint(VictimPawn->GetActorLocation());
	}
	UpdateMovement(DeltaSeconds);
}

void AUOAIController::HandleSecondaryFinished(bool bSucceeded)
{
	bHasSecondaryResult = true;
	bSecondaryResult = bSucceeded;
}

// ~ Investigation

void AUOAIController::ObserveTrace(const FUOTraceEntry& Entry, EUOEvidenceSource Source)
{
	EUOEvidenceSource* Existing = KnownTraces.Find(Entry.Id);
	if (Existing)
	{
		// Seeing it beats reading about it
		if (Source == EUOEvidenceSource::Witnessed)
		{
			*Existing = Source;
		}
		return;
	}
	KnownTraces.Add(Entry.Id, Source);
	if (FUOSuspicionModel::IsSabotageTrace(Entry.Kind))
	{
		KnownSabotageTraces.Add(Entry.Id);
		// Something happened: go and check the logs soon
		NextTerminalAt = FMath::Min(NextTerminalAt, ShiftNow() + Rng.FRandRange(3.f, 10.f));
	}
	if (Source == EUOEvidenceSource::Witnessed && Entry.IsNamed() && (Entry.Kind == EUOTraceKind::DoorEntry || Entry.Kind == EUOTraceKind::CameraSighting))
	{
		ObserveSighting(Entry.ShownActorIds[0], Entry.Room, Entry.TimeSeconds);
	}
	if (!UOAIKillerPrivate::IsQuiet(Entry.Kind))
	{
		const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
		UE_LOG(LogUnpaidOvertime, Log, TEXT("AI evidence: %s learned %s [source: %s]"), *GetEmployee()->GetDisplayName(),
			*FUOTraceText::ToLogString(Entry, [Evidence](int32 Id) { return Evidence ? Evidence->NameOf(Id) : FString::FromInt(Id); }),
			*StaticEnum<EUOEvidenceSource>()->GetNameStringByValue(static_cast<int64>(Source)));
	}
}

void AUOAIController::ObserveAct(int32 ActorId, const FUOTraceEntry& Entry)
{
	FUOSeenAct& Act = SeenActs.AddDefaulted_GetRef();
	Act.ActorId = ActorId;
	Act.TraceId = Entry.Id;
	Act.TimeSeconds = Entry.TimeSeconds;
	ObserveTrace(Entry, EUOEvidenceSource::Witnessed);
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI evidence: %s SAW %s do trace #%d (%s) [source: witnessed, line of sight]"), *GetEmployee()->GetDisplayName(),
		Evidence ? *Evidence->NameOf(ActorId) : TEXT("?"), Entry.Id, *StaticEnum<EUOTraceKind>()->GetNameStringByValue(static_cast<int64>(Entry.Kind)));
}

void AUOAIController::ObserveSighting(int32 EmployeeId, FName Room, float Time)
{
	const bool bDuplicate = Sightings.ContainsByPredicate([EmployeeId, Room, Time](const FUOSighting& S)
	{
		return S.EmployeeId == EmployeeId && S.Room == Room && FMath::Abs(S.TimeSeconds - Time) < 2.f;
	});
	if (!bDuplicate)
	{
		FUOSighting& Sighting = Sightings.AddDefaulted_GetRef();
		Sighting.EmployeeId = EmployeeId;
		Sighting.Room = Room;
		Sighting.TimeSeconds = Time;
	}
}

void AUOAIController::LearnPublicTrace(const FUOTraceEntry& Entry)
{
	if (!KnownTraces.Contains(Entry.Id))
	{
		KnownTraces.Add(Entry.Id, EUOEvidenceSource::Public);
	}
}

void AUOAIController::ReadDevice(EUOTraceDevice Device, const FString& TerminalName)
{
	const AUOGameState* GameState = GetUOGameState();
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	if (!GameState)
	{
		return;
	}
	LastReadAt.Add(static_cast<uint8>(Device), ShiftNow());
	int32 Total = 0;
	int32 New = 0;
	FString Notable;
	for (const FUOTraceEntry& Entry : GameState->GetTraceLog())
	{
		if (Entry.Device != Device)
		{
			continue;
		}
		++Total;
		if (KnownTraces.Contains(Entry.Id))
		{
			continue;
		}
		++New;
		KnownTraces.Add(Entry.Id, EUOEvidenceSource::Terminal);
		if (FUOSuspicionModel::IsSabotageTrace(Entry.Kind))
		{
			KnownSabotageTraces.Add(Entry.Id);
		}
		if (!UOAIKillerPrivate::IsQuiet(Entry.Kind) || FUOSuspicionModel::IsNoOutputTrace(Entry.Kind))
		{
			Notable += TEXT("\n    ") + FUOTraceText::ToLogString(Entry, [Evidence](int32 Id) { return Evidence ? Evidence->NameOf(Id) : FString::FromInt(Id); });
		}
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI evidence: %s read the %s at %s: %d entries, %d new [source: terminal]%s"), *GetEmployee()->GetDisplayName(),
		*FUOTraceText::GetDeviceName(Device).ToString(), *TerminalName, Total, New, *Notable);
}

FUOSuspicionResult AUOAIController::EvaluateSuspicion() const
{
	FUOSuspicionInput Input;
	const AUOGameState* GameState = GetUOGameState();
	const AUOPlayerState* Me = GetEmployee();
	if (!GameState || !Me)
	{
		return FUOSuspicionResult();
	}
	Input.SelfId = Me->GetEmployeeId();
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		if (Employee && Employee->IsAlive() && Employee != Me)
		{
			Input.CandidateIds.Add(Employee->GetEmployeeId());
		}
	}
	for (const TPair<int32, EUOEvidenceSource>& Pair : KnownTraces)
	{
		if (const FUOTraceEntry* Entry = GameState->FindTrace(Pair.Key))
		{
			FUOKnownTrace& Known = Input.Traces.AddDefaulted_GetRef();
			Known.Entry = *Entry;
			Known.Source = Pair.Value;
		}
	}
	Input.SeenActs = SeenActs;
	Input.Sightings = Sightings;
	Input.AcceptedAnswers = AcceptedAnswers;
	Input.CarriedOver = CarriedOver;
	Input.Suspicion = Difficulty.Suspicion;
	Input.NoiseSeed = MeetingCounter * 7919 + (GameState->GetDayNumber() * 131);
	return FUOSuspicionModel::Evaluate(Input);
}

int32 AUOAIController::ChooseSuspicionCutVote(const TArray<int32>& EligibleCandidates)
{
	if (bIsKiller || EligibleCandidates.Num() == 0)
	{
		// A killer votes on performance like anyone (but never for an accomplice: see MayCutVoteFor)
		return INDEX_NONE;
	}
	const FUOSuspicionResult Result = EvaluateSuspicion();
	const float Threshold = FUOKillerTuning::GetVoteThreshold(Difficulty.Suspicion);
	int32 Pick = INDEX_NONE;
	float Best = Threshold;
	for (const int32 Candidate : EligibleCandidates)
	{
		const float Score = Result.GetScore(Candidate);
		if (Score >= Best)
		{
			Best = Score;
			Pick = Candidate;
		}
	}
	if (Pick != INDEX_NONE)
	{
		const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
		UE_LOG(LogUnpaidOvertime, Log, TEXT("AI vote: %s votes to cut %s on suspicion (score %.2f >= %.2f, visible evidence only)."), *GetEmployee()->GetDisplayName(),
			Evidence ? *Evidence->NameOf(Pick) : TEXT("?"), Best, Threshold);
	}
	return Pick;
}

void AUOAIController::ObserveAcceptedAnswer(const FUOAcceptedAnswer& Answer)
{
	AcceptedAnswers.Add(Answer);
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI evidence: %s heard the boss accept %s's answer to \"%s\" [source: Public%s]"), *GetEmployee()->GetDisplayName(),
		Evidence ? *Evidence->NameOf(Answer.EmployeeId) : TEXT("?"), *Answer.Prompt,
		Answer.bKnowsFact ? (Answer.bWrong ? TEXT("+Knowledge: it knows that answer is WRONG") : TEXT("+Knowledge: the answer is right")) : TEXT(", it does not know the fact"));
}

void AUOAIController::RememberDay()
{
	const FUOSuspicionResult Result = EvaluateSuspicion();
	TMap<int32, float> Scores;
	for (const FUOSuspicionItem& Item : Result.Items)
	{
		// Hunches are forgotten overnight; evidence and memory stay (memory decays through CarryOverFactor)
		if (Item.Source != EUOEvidenceSource::Hunch)
		{
			Scores.FindOrAdd(Item.SuspectId) += Item.Weight;
		}
	}
	CarriedOver.Reset();
	for (const TPair<int32, float>& Pair : Scores)
	{
		FUOCarriedSuspicion& Carried = CarriedOver.AddDefaulted_GetRef();
		Carried.EmployeeId = Pair.Key;
		Carried.Score = Pair.Value;
	}
}

FUOMeetingAIContext AUOAIController::MakeMeetingContext() const
{
	FUOMeetingAIContext Context;
	const AUOPlayerState* Me = GetEmployee();
	const AUOGameState* GameState = GetUOGameState();
	if (!Me || !GameState)
	{
		return Context;
	}
	Context.SelfId = Me->GetEmployeeId();
	Context.bSelfKiller = bIsKiller;
	Context.Accomplices = Accomplices;
	Context.Suspicion = Difficulty.Suspicion;
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		if (Employee && Employee->IsAlive())
		{
			Context.AliveIds.Add(Employee->GetEmployeeId());
		}
	}
	Context.Suspects = EvaluateSuspicion();
	return Context;
}

FUODebateStatement AUOAIController::MakeDebateStatement()
{
	++MeetingCounter;
	const FUOMeetingAIContext Context = MakeMeetingContext();
	FUODebateStatement Statement = FUOMeetingAI::MakeStatement(Context);
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	if (const AUOGameState* GameState = GetUOGameState())
	{
		for (const int32 TraceId : Statement.TraceIds)
		{
			if (const FUOTraceEntry* Entry = GameState->FindTrace(TraceId))
			{
				Statement.Room = Entry->Room;
				break;
			}
		}
	}
	static const TCHAR* Kinds[] = { TEXT("ACCUSES"), TEXT("is unsure about"), TEXT("has nothing") };
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI statement: %s %s %s (score %.2f, %d trace(s) on the table)%s. Reason: %s [%s]"), *GetEmployee()->GetDisplayName(),
		Kinds[static_cast<int32>(Statement.Kind)], Statement.SuspectId != INDEX_NONE && Evidence ? *Evidence->NameOf(Statement.SuspectId) : TEXT(""), Statement.Score,
		Statement.TraceIds.Num(), bIsKiller ? TEXT(" [killer: framing with visible evidence]") : TEXT(""), *Statement.Reason,
		*StaticEnum<EUOEvidenceSource>()->GetNameStringByValue(static_cast<int64>(Statement.ReasonSource)));
	for (const FUOSuspicionItem& Item : Context.Suspects.Items)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("    %s suspects %s: +%.2f %s"), *GetEmployee()->GetDisplayName(), Evidence ? *Evidence->NameOf(Item.SuspectId) : TEXT("?"), Item.Weight, *Item.Reason);
	}
	return Statement;
}

int32 AUOAIController::ChooseMeetingVote()
{
	FString Reason;
	const int32 Vote = FUOMeetingAI::ChooseVote(MakeMeetingContext(), Reason);
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("AI vote: %s %s: %s"), *GetEmployee()->GetDisplayName(),
		Vote == INDEX_NONE ? TEXT("abstains") : *FString::Printf(TEXT("votes for %s"), Evidence ? *Evidence->NameOf(Vote) : TEXT("?")), *Reason);
	return Vote;
}

int32 AUOAIController::ReconsiderMeetingVote(int32 CurrentVote, const FUOMeetingTally& Live)
{
	FString Reason;
	const int32 Vote = FUOMeetingAI::Reconsider(MakeMeetingContext(), CurrentVote, Live, Rng.FRand(), Reason);
	if (Vote != CurrentVote)
	{
		const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
		UE_LOG(LogUnpaidOvertime, Log, TEXT("AI vote: %s switches to %s: %s"), *GetEmployee()->GetDisplayName(),
			Vote == INDEX_NONE ? TEXT("abstain") : (Evidence ? *Evidence->NameOf(Vote) : TEXT("?")), *Reason);
	}
	return Vote;
}

EUORPSChoice AUOAIController::PickDuel()
{
	return FUORockPaperScissors::FromRandom(Rng.FRand());
}
