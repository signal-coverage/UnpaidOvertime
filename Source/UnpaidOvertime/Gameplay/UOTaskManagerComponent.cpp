// UNPAID: OVERTIME - server-side task authority owned by the game mode.

#include "UOTaskManagerComponent.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOInteractableStation.h"
#include "UOTaskBoard.h"
#include "UOTaskGenerator.h"
#include "UODayClock.h"
#include "UOGameMode.h"
#include "UORoleSystemComponent.h"
#include "UOEvidenceSubsystem.h"
#include "UOTerminal.h"
#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UnpaidOvertime.h"

#define LOCTEXT_NAMESPACE "UOTaskManager"

namespace UOTaskManagerPrivate
{
	FText GetWorkVerb(EUOStationType StationType)
	{
		switch (StationType)
		{
		case EUOStationType::Printer:		return LOCTEXT("VerbReplaceInk", "Replace ink cartridge");
		case EUOStationType::FilingCabinet:	return LOCTEXT("VerbFileReport", "File report");
		case EUOStationType::CoffeeMaker:	return LOCTEXT("VerbRefillCoffee", "Refill coffee");
		default:							return LOCTEXT("VerbWork", "Work");
		}
	}

	const TArray<FUOTaskInstance>& EmptyTasks()
	{
		static const TArray<FUOTaskInstance> Empty;
		return Empty;
	}
}

UUOTaskManagerComponent::UUOTaskManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUOTaskManagerComponent::StartDay(const TArray<FUOTaskInstance>& Tasks, const TArray<AUOInteractableStation*>& InStations)
{
	Stations.Reset();
	StationsById.Reset();
	ActiveWork.Reset();

	for (AUOInteractableStation* Station : InStations)
	{
		if (Station)
		{
			Stations.Add(Station);
			StationsById.Add(Station->GetStationId(), Station);
		}
	}

	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->SetTasks(Tasks);
		GameState->ClearEvidence();
	}

	// Warn loudly if a sub-objective points at a station the builder did not place
	for (const FUOTaskInstance& Task : Tasks)
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (!StationsById.Contains(Sub.StationId))
			{
				UE_LOG(LogUnpaidOvertime, Error, TEXT("Task manager: sub-objective %d references missing station %s"), Sub.Id, *Sub.StationId.ToString());
			}
		}
	}

	bAcceptingWork = true;
}

void UUOTaskManagerComponent::EndDay()
{
	bAcceptingWork = false;
	ActiveWork.Reset();
}

const TArray<FUOTaskInstance>& UUOTaskManagerComponent::GetTasks() const
{
	if (const AUOGameState* GameState = GetUOGameState())
	{
		return GameState->GetTasks();
	}
	return UOTaskManagerPrivate::EmptyTasks();
}

FUOInteractionOption UUOTaskManagerComponent::QueryInteraction(const AUOPlayerState* Employee, const AUOInteractableStation* Station) const
{
	FUOInteractionOption Option;
	if (!bAcceptingWork || !Employee || !Station || !Employee->IsAlive())
	{
		return Option;
	}

	const EUOItemType Carried = Employee->GetCarriedItem();

	// Terminals: read the log (cameras and props have nothing to read)
	if (const AUOTerminal* Terminal = Cast<AUOTerminal>(Station))
	{
		EUOTraceDevice Device;
		if (Terminal->GetDevice(Device))
		{
			Option.Kind = EUOInteractionKind::ReadTerminal;
			Option.Prompt = FText::Format(LOCTEXT("PromptRead", "Read the {0}"), FUOTraceText::GetDeviceName(Device).ToLower());
			Option.bAvailable = true;
		}
		else if (Terminal->GetTerminalKind() == EUOTerminalKind::SecurityCamera)
		{
			const AUOGameState* GameState = GetUOGameState();
			Option.BlockedReason = Terminal->IsCameraOffline(GameState ? GameState->GetShiftElapsedSeconds() : 0.f)
				? LOCTEXT("CameraOffline", "Camera offline") : LOCTEXT("CameraOnline", "Recording");
		}
		else
		{
			Option.BlockedReason = LOCTEXT("JustAPlant", "Just a plant. Probably.");
		}
		return Option;
	}

	// Sabotaged machines: anyone can clear a jam; a lock just has to be waited out
	if (Station->IsJammed())
	{
		Option.Kind = EUOInteractionKind::FixJam;
		Option.Prompt = LOCTEXT("PromptFixJam", "Clear the paper jam");
		Option.HoldSeconds = FUOKillerTuning::JamFixSeconds;
		Option.bAvailable = true;
		return Option;
	}
	{
		const AUOGameState* GameState = GetUOGameState();
		if (GameState && Station->IsLocked(GameState->GetShiftElapsedSeconds()))
		{
			Option.BlockedReason = FText::Format(LOCTEXT("BlockedLocked", "Locked for maintenance — {0}s"),
				FText::AsNumber(FMath::CeilToInt(Station->GetLockedUntil() - GameState->GetShiftElapsedSeconds())));
			return Option;
		}
	}

	// Item sources hand out one item at a time while it is still needed
	if (Station->IsItemSource())
	{
		const EUOItemType Item = Station->GetProvidedItem();
		Option.Kind = EUOInteractionKind::PickUpItem;
		Option.Item = Item;
		Option.HoldSeconds = 0.f;
		Option.Prompt = FText::Format(LOCTEXT("PromptTakeItem", "Take {0}"), UOText::GetItemName(Item).ToLower());

		if (Carried != EUOItemType::None)
		{
			Option.BlockedReason = FText::Format(LOCTEXT("BlockedHandsFull", "Hands full - press Q to drop the {0}"), UOText::GetItemName(Carried).ToLower());
		}
		else if (CountUnclaimedItemDemand(Item) <= 0)
		{
			Option.BlockedReason = FText::Format(LOCTEXT("BlockedNoDemand", "No {0} needed right now"), UOText::GetItemName(Item).ToLower());
		}
		else
		{
			Option.bAvailable = true;
		}
		return Option;
	}

	const FUOSubObjective* Sub = FUOTaskBoard::FindBestAtStation(GetTasks(), Station->GetStationId(), Employee->GetEmployeeId());
	if (!Sub)
	{
		Option.BlockedReason = LOCTEXT("BlockedNothingToDo", "Nothing to do here");
		return Option;
	}

	Option.SubObjectiveId = Sub->Id;
	Option.HoldSeconds = Sub->WorkSeconds;
	Option.Prompt = UOTaskManagerPrivate::GetWorkVerb(Station->GetStationType());

	if (Sub->bRequiresItem)
	{
		Option.Kind = EUOInteractionKind::DeliverItem;
		Option.Item = Sub->ItemType;
		if (Carried != Sub->ItemType)
		{
			const EUOStationType SourceType = FUOTaskGenerator::GetItemSourceStationType(Sub->ItemType);
			Option.BlockedReason = FText::Format(LOCTEXT("BlockedNeedsItem", "Needs: {0} (get it from the {1})"),
				UOText::GetItemName(Sub->ItemType).ToLower(), UOText::GetStationTypeName(SourceType).ToLower());
			return Option;
		}
	}
	else
	{
		Option.Kind = EUOInteractionKind::Work;
	}

	// Rule 3: the killers' work is real and counts like anyone's
	Option.bAvailable = true;
	return Option;
}

bool UUOTaskManagerComponent::BeginWork(AUOPlayerState* Employee, AUOInteractableStation* Station)
{
	const FUOInteractionOption Option = QueryInteraction(Employee, Station);
	if (!Option.bAvailable || !IsWithinReach(Employee, Station))
	{
		return false;
	}

	CancelWork(Employee);

	// Claim open work for the duration of the hold so nobody else grabs it mid-way
	int32 ClaimedId = INDEX_NONE;
	AUOGameState* GameState = GetUOGameState();
	const FUOSubObjective* Sub = GameState ? FUOTaskBoard::FindSubObjective(GameState->GetTasks(), Option.SubObjectiveId) : nullptr;
	if (Sub && Sub->State == EUOSubObjectiveState::Open
		&& FUOTaskBoard::TryClaim(GameState->GetMutableTasks(), Option.SubObjectiveId, Employee->GetEmployeeId()))
	{
		ClaimedId = Option.SubObjectiveId;
	}

	FWorkInProgress& Work = ActiveWork.FindOrAdd(Employee->GetEmployeeId());
	Work.Station = Station;
	Work.StartTimeSeconds = GetWorld()->GetTimeSeconds();
	Work.ClaimedSubObjectiveId = ClaimedId;
	return true;
}

void UUOTaskManagerComponent::CancelWork(AUOPlayerState* Employee)
{
	if (!Employee)
	{
		return;
	}

	FWorkInProgress Work;
	if (ActiveWork.RemoveAndCopyValue(Employee->GetEmployeeId(), Work) && Work.ClaimedSubObjectiveId != INDEX_NONE)
	{
		if (AUOGameState* GameState = GetUOGameState())
		{
			FUOTaskBoard::ReleaseClaim(GameState->GetMutableTasks(), Work.ClaimedSubObjectiveId, Employee->GetEmployeeId());
		}
	}
}

bool UUOTaskManagerComponent::CompleteInteraction(AUOPlayerState* Employee, AUOInteractableStation* Station)
{
	const FUOInteractionOption Option = QueryInteraction(Employee, Station);
	if (!Option.bAvailable || !IsWithinReach(Employee, Station))
	{
		CancelWork(Employee);
		return false;
	}

	// Server-side work time validation: the hold must really have lasted long enough
	if (Option.HoldSeconds > 0.f)
	{
		const FWorkInProgress* Work = ActiveWork.Find(Employee->GetEmployeeId());
		const double Elapsed = Work ? GetWorld()->GetTimeSeconds() - Work->StartTimeSeconds : 0.0;
		const double Required = Option.HoldSeconds / FMath::Max(0.1f, Employee->GetWorkSpeedMultiplier()) * WorkTimeTolerance;
		if (!Work || Work->Station.Get() != Station || Elapsed < Required)
		{
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("Task manager: rejected early completion by %s at %s (%.2fs of %.2fs)"),
				*Employee->GetDisplayName(), *Station->GetStationId().ToString(), Elapsed, Required);
			CancelWork(Employee);
			return false;
		}
	}
	CancelWork(Employee);

	switch (Option.Kind)
	{
	case EUOInteractionKind::PickUpItem:
	{
		Employee->SetCarriedItem(Option.Item);
		LogEvidence(Employee, Station->GetStationId(), EUOEvidenceAction::PickedUpItem, Option.Item, INDEX_NONE);
		if (UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this))
		{
			FUOTraceRecord Record;
			Record.Device = EUOTraceDevice::Inventory;
			Record.Kind = EUOTraceKind::ItemSignedOut;
			Record.TrueActorId = Employee->GetEmployeeId();
			Record.Room = Station->GetRoomId();
			Record.StationId = Station->GetStationId();
			Record.Subject = UOText::GetItemName(Option.Item).ToString();
			Evidence->Record(Record);
		}
		return true;
	}

	case EUOInteractionKind::DeliverItem:
		Employee->SetCarriedItem(EUOItemType::None);
		CompleteSubObjective(Employee, Station, Option.SubObjectiveId);
		return true;

	case EUOInteractionKind::Work:
		CompleteSubObjective(Employee, Station, Option.SubObjectiveId);
		return true;

	case EUOInteractionKind::FixJam:
		Station->SetJammed(false);
		if (UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this))
		{
			FUOTraceRecord Record;
			Record.Device = EUOTraceDevice::PrinterLog;
			Record.Kind = EUOTraceKind::PrinterJamCleared;
			Record.TrueActorId = Employee->GetEmployeeId();
			Record.Room = Station->GetRoomId();
			Record.StationId = Station->GetStationId();
			Record.Subject = Station->GetDisplayName().ToString();
			Evidence->Record(Record);
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Jam fixed: %s cleared %s."), *Employee->GetDisplayName(), *Station->GetDisplayName().ToString());
		return true;

	case EUOInteractionKind::ReadTerminal:
		// Reading is local (UI for humans, a timed read for AIs); nothing changes on the server
		return true;

	default:
		return false;
	}
}

bool UUOTaskManagerComponent::DropCarriedItem(AUOPlayerState* Employee)
{
	if (!Employee || !Employee->IsCarryingItem())
	{
		return false;
	}

	const EUOItemType Dropped = Employee->GetCarriedItem();
	Employee->SetCarriedItem(EUOItemType::None);
	LogEvidence(Employee, NAME_None, EUOEvidenceAction::DroppedItem, Dropped, INDEX_NONE);
	return true;
}

bool UUOTaskManagerComponent::TryClaim(AUOPlayerState* Employee, int32 SubObjectiveId)
{
	AUOGameState* GameState = GetUOGameState();
	if (!bAcceptingWork || !GameState || !Employee || !Employee->IsAlive())
	{
		return false;
	}
	return FUOTaskBoard::TryClaim(GameState->GetMutableTasks(), SubObjectiveId, Employee->GetEmployeeId());
}

bool UUOTaskManagerComponent::ReleaseClaim(AUOPlayerState* Employee, int32 SubObjectiveId)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState || !Employee)
	{
		return false;
	}
	return FUOTaskBoard::ReleaseClaim(GameState->GetMutableTasks(), SubObjectiveId, Employee->GetEmployeeId());
}

void UUOTaskManagerComponent::ReleaseEmployee(AUOPlayerState* Employee)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState || !Employee)
	{
		return;
	}

	FUOTaskBoard::ReleaseAllClaims(GameState->GetMutableTasks(), Employee->GetEmployeeId());
	DropCarriedItem(Employee);
	CancelWork(Employee);
}

bool UUOTaskManagerComponent::AreAllSubObjectivesDone() const
{
	const TArray<FUOTaskInstance>& Tasks = GetTasks();
	return Tasks.Num() > 0 && FUOTaskBoard::AreAllDone(Tasks);
}

AUOInteractableStation* UUOTaskManagerComponent::FindStation(FName StationId) const
{
	const TObjectPtr<AUOInteractableStation>* Found = StationsById.Find(StationId);
	return Found ? Found->Get() : nullptr;
}

AUOInteractableStation* UUOTaskManagerComponent::FindNearestItemSource(EUOItemType Item, const FVector& FromLocation) const
{
	AUOInteractableStation* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (AUOInteractableStation* Station : Stations)
	{
		if (Station && Station->GetProvidedItem() == Item)
		{
			const double DistSq = FVector::DistSquared2D(FromLocation, Station->GetInteractionLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Station;
			}
		}
	}
	return Best;
}

int32 UUOTaskManagerComponent::CountUnclaimedItemDemand(EUOItemType Item) const
{
	const AUOGameState* GameState = GetUOGameState();
	if (!GameState || Item == EUOItemType::None)
	{
		return 0;
	}

	int32 Demand = FUOTaskBoard::CountPendingRequiringItem(GameState->GetTasks(), Item);
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		if (Employee && Employee->IsAlive() && Employee->GetCarriedItem() == Item)
		{
			--Demand;
		}
	}
	return Demand;
}

AUOGameState* UUOTaskManagerComponent::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

bool UUOTaskManagerComponent::IsWithinReach(const AUOPlayerState* Employee, const AUOInteractableStation* Station) const
{
	const APawn* Pawn = Employee ? Employee->GetPawn() : nullptr;
	if (!Pawn || !Station)
	{
		return false;
	}
	return FVector::Dist2D(Pawn->GetActorLocation(), Station->GetActorLocation()) <= MaxInteractionDistance;
}

void UUOTaskManagerComponent::CompleteSubObjective(AUOPlayerState* Employee, AUOInteractableStation* Station, int32 SubObjectiveId)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	TArray<FUOTaskInstance>& Tasks = GameState->GetMutableTasks();
	if (FUOTaskBoard::Complete(Tasks, SubObjectiveId, Employee->GetEmployeeId(), GameState->GetShiftElapsedSeconds()))
	{
		Employee->SetCompletedCount(FUOTaskBoard::CountCompletedBy(Tasks, Employee->GetEmployeeId()));
		LogEvidence(Employee, Station->GetStationId(), EUOEvidenceAction::CompletedSubObjective, EUOItemType::None, SubObjectiveId);
		RecordWorkTraces(Employee, Station, SubObjectiveId);
		RefreshStationVisual(Station->GetStationId());
	}
}

void UUOTaskManagerComponent::RecordFailedAttempt(AUOPlayerState* Employee, AUOInteractableStation* Station)
{
	if (!Employee || !Station || Station->IsTerminal())
	{
		return;
	}
	UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	const AUOGameMode* GameMode = Cast<AUOGameMode>(GetOwner());
	if (!Evidence || !GameMode)
	{
		return;
	}
	FUOTraceRecord Record;
	Record.TrueActorId = Employee->GetEmployeeId();
	Record.Detail = FUOKillerTuning::GetTraceDetail(GameMode->GetDayNumber());
	Record.Room = Station->GetRoomId();
	Record.StationId = Station->GetStationId();
	Record.Subject = Station->GetDisplayName().ToString();
	switch (Station->GetStationType())
	{
	case EUOStationType::Printer:		Record.Device = EUOTraceDevice::PrinterLog; Record.Kind = EUOTraceKind::PrintNoOutput; break;
	case EUOStationType::FilingCabinet:	Record.Device = EUOTraceDevice::FileHistory; Record.Kind = EUOTraceKind::FileNoChange; break;
	case EUOStationType::CoffeeMaker:	Record.Device = EUOTraceDevice::Inventory; Record.Kind = EUOTraceKind::CoffeeNoChange; break;
	default:							return;
	}
	Evidence->Record(Record);
}

void UUOTaskManagerComponent::RecordWorkTraces(const AUOPlayerState* Employee, const AUOInteractableStation* Station, int32 SubObjectiveId)
{
	UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	const AUOGameMode* GameMode = Cast<AUOGameMode>(GetOwner());
	if (!Evidence || !Employee || !Station || !GameMode)
	{
		return;
	}

	// The public report
	FUOTraceRecord Report;
	Report.Device = EUOTraceDevice::ActivityBoard;
	Report.Kind = EUOTraceKind::WorkReported;
	Report.TrueActorId = Employee->GetEmployeeId();
	Report.Room = Station->GetRoomId();
	Report.StationId = Station->GetStationId();
	Report.Subject = Station->GetDisplayName().ToString();
	Evidence->Record(Report);

	// The device's own line: real work leaves output
	FUOTraceRecord Device = Report;
	Device.Detail = EUOTraceDetail::Named;
	switch (Station->GetStationType())
	{
	case EUOStationType::Printer:
		Device.Device = EUOTraceDevice::PrinterLog;
		Device.Kind = EUOTraceKind::PrintOk;
		break;
	case EUOStationType::FilingCabinet:
		Device.Device = EUOTraceDevice::FileHistory;
		Device.Kind = EUOTraceKind::FileSaved;
		Device.ValueA = 100 + SubObjectiveId;
		break;
	case EUOStationType::CoffeeMaker:
		Device.Device = EUOTraceDevice::Inventory;
		Device.Kind = EUOTraceKind::CoffeeLogged;
		break;
	default:
		return;
	}
	Evidence->Record(Device);
}

void UUOTaskManagerComponent::LogEvidence(const AUOPlayerState* Employee, FName StationId, EUOEvidenceAction Action, EUOItemType Item, int32 SubObjectiveId)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState || !Employee)
	{
		return;
	}

	FUOEvidenceEntry Entry;
	Entry.TimeSeconds = GameState->GetShiftElapsedSeconds();
	Entry.ClockTime = GameState->GetClockText();
	Entry.EmployeeId = Employee->GetEmployeeId();
	Entry.EmployeeName = Employee->GetDisplayName();
	Entry.StationId = StationId;
	Entry.Action = Action;
	Entry.Item = Item;
	Entry.SubObjectiveId = SubObjectiveId;
	GameState->AddEvidence(Entry);

	const TCHAR* ActionName = Action == EUOEvidenceAction::CompletedSubObjective ? TEXT("completed sub-objective")
		: (Action == EUOEvidenceAction::PickedUpItem ? TEXT("picked up item") : TEXT("dropped item"));
	UE_LOG(LogUnpaidOvertime, Log, TEXT("[%s] %s %s %d at %s (item %d)"),
		*Entry.ClockTime, *Entry.EmployeeName, ActionName, SubObjectiveId, *StationId.ToString(), static_cast<int32>(Item));
}

void UUOTaskManagerComponent::RefreshStationVisual(FName StationId)
{
	if (AUOInteractableStation* Station = FindStation(StationId))
	{
		Station->SetWorkDone(FUOTaskBoard::CountPendingAtStation(GetTasks(), StationId) == 0);
	}
}

#undef LOCTEXT_NAMESPACE
