// UNPAID: OVERTIME - UEvidenceSubsystem: device logs, witnesses, pins, dark rooms and moved props.

#include "UOEvidenceSubsystem.h"
#include "UOGameState.h"
#include "UOGameMode.h"
#include "UOPlayerState.h"
#include "UOBossSystemComponent.h"
#include "UOTerminal.h"
#include "UOAIController.h"
#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Math/RandomStream.h"
#include "UnpaidOvertime.h"

namespace UOEvidencePrivate
{
	constexpr float PresenceSampleSeconds = 0.25f;
	/** Observers see in a wide cone in front of them (cos 70 deg) */
	constexpr float ViewConeCos = 0.34f;
}

UUOEvidenceSubsystem* UUOEvidenceSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UUOEvidenceSubsystem>() : nullptr;
}

bool UUOEvidenceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

const TArray<FName>& UUOEvidenceSubsystem::GetDoorRooms()
{
	static const TArray<FName> Rooms = { TEXT("Lobby"), TEXT("CopyRoom"), TEXT("Manager"), TEXT("Supply"), TEXT("Records") };
	return Rooms;
}

AUOGameState* UUOEvidenceSubsystem::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

AUOGameMode* UUOEvidenceSubsystem::GetGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
}

APawn* UUOEvidenceSubsystem::GetPawnOf(int32 EmployeeId) const
{
	const AUOGameState* GameState = GetUOGameState();
	const AUOPlayerState* Employee = GameState ? GameState->FindEmployee(EmployeeId) : nullptr;
	return Employee ? Employee->GetPawn() : nullptr;
}

FString UUOEvidenceSubsystem::NameOf(int32 EmployeeId) const
{
	const AUOGameState* GameState = GetUOGameState();
	return GameState ? GameState->GetEmployeeName(EmployeeId) : FString::FromInt(EmployeeId);
}

void UUOEvidenceSubsystem::ResetForDay(int32 InDayNumber, int32 InSeed)
{
	DayNumber = InDayNumber;
	Seed = InSeed;
	NextTraceId = 1;
	TrueActors.Reset();
	DarkRooms.Reset();
	PropMoves.Reset();
	LastRooms.Reset();
	SampleTimer = 0.f;
	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->ClearTraces();
		for (AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee)
			{
				Employee->GetMutablePinnedTraceIds().Reset();
			}
		}
	}
}

int32 UUOEvidenceSubsystem::Record(const FUOTraceRecord& Record)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return INDEX_NONE;
	}

	FUOTraceEntry Entry;
	Entry.Id = NextTraceId++;
	Entry.TimeSeconds = GameState->GetShiftElapsedSeconds();
	Entry.ClockTime = GameState->GetClockText();
	Entry.DayNumber = DayNumber;
	Entry.Device = Record.Device;
	Entry.Kind = Record.Kind;
	Entry.Room = Record.Room;
	Entry.StationId = Record.StationId;
	Entry.Subject = Record.Subject;
	Entry.ValueA = Record.ValueA;
	Entry.ValueB = Record.ValueB;
	Entry.Detail = Record.TrueActorId == INDEX_NONE ? EUOTraceDetail::Anonymous : Record.Detail;

	if (Record.ShownOverride.Num() > 0)
	{
		Entry.ShownActorIds = Record.ShownOverride;
		Entry.Detail = Record.ShownOverride.Num() == 1 ? EUOTraceDetail::Named : EUOTraceDetail::Partial;
	}
	else if (Entry.Detail == EUOTraceDetail::Named)
	{
		Entry.ShownActorIds = { Record.TrueActorId };
	}
	else if (Entry.Detail == EUOTraceDetail::Partial)
	{
		// The real badge and a decoy (another alive employee), in a seeded order
		TArray<int32> Others;
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && Employee->IsAlive() && Employee->GetEmployeeId() != Record.TrueActorId)
			{
				Others.Add(Employee->GetEmployeeId());
			}
		}
		FRandomStream Rng(Seed * 7 + Entry.Id * 131);
		Entry.ShownActorIds = { Record.TrueActorId };
		if (Others.Num() > 0)
		{
			const int32 Decoy = Others[Rng.RandRange(0, Others.Num() - 1)];
			Rng.FRand() < 0.5f ? Entry.ShownActorIds.Add(Decoy) : Entry.ShownActorIds.Insert(Decoy, 0);
		}
	}

	GameState->AddTrace(Entry);
	TrueActors.Add(Entry.Id, Record.TrueActorId);

	const bool bPresence = Entry.Kind == EUOTraceKind::DoorEntry || Entry.Kind == EUOTraceKind::CameraSighting;
	const FString Line = FUOTraceText::ToLogString(Entry, [this](int32 Id) { return NameOf(Id); });
	if (bPresence)
	{
		UE_LOG(LogUnpaidOvertime, Verbose, TEXT("Trace %s"), *Line);
	}
	else
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Trace %s (audit: true actor %s, detail %s)"), *Line,
			Record.TrueActorId == INDEX_NONE ? TEXT("none") : *NameOf(Record.TrueActorId), *StaticEnum<EUOTraceDetail>()->GetNameStringByValue(static_cast<int64>(Entry.Detail)));
	}

	NotifyWitnesses(Entry, Record);
	return Entry.Id;
}

void UUOEvidenceSubsystem::NotifyWitnesses(const FUOTraceEntry& Entry, const FUOTraceRecord& Record)
{
	const AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	const APawn* ActorPawn = Record.TrueActorId != INDEX_NONE ? GetPawnOf(Record.TrueActorId) : nullptr;
	const bool bDark = !Entry.Room.IsNone() && IsRoomDark(Entry.Room);

	for (TActorIterator<AUOAIController> It(GetWorld()); It; ++It)
	{
		AUOAIController* Brain = *It;
		const AUOPlayerState* Me = Brain ? Brain->GetPlayerState<AUOPlayerState>() : nullptr;
		const APawn* MyPawn = Brain ? Brain->GetPawn() : nullptr;
		if (!Me || !Me->IsAlive() || !MyPawn)
		{
			continue;
		}

		// The activity board is public: everyone sees the work reports
		if (Entry.Device == EUOTraceDevice::ActivityBoard)
		{
			Brain->ObserveTrace(Entry, EUOEvidenceSource::Public);
			continue;
		}

		if (Entry.Room.IsNone() || GetRoomOf(MyPawn) != Entry.Room)
		{
			continue;
		}

		// In the room: it notices what happened (in the dark only the lights going out / the incident itself)
		const bool bNoticeable = !bDark || Entry.Kind == EUOTraceKind::CameraOffline || Entry.Kind == EUOTraceKind::BodyFound;
		if (!bNoticeable)
		{
			continue;
		}
		Brain->ObserveTrace(Entry, EUOEvidenceSource::Witnessed);

		if (!bDark)
		{
			// It also sees who else is in the room right now
			for (const AUOPlayerState* Other : GameState->GetEmployees())
			{
				const APawn* OtherPawn = Other ? Other->GetPawn() : nullptr;
				if (Other && Other != Me && Other->IsAlive() && OtherPawn && GetRoomOf(OtherPawn) == Entry.Room)
				{
					Brain->ObserveSighting(Other->GetEmployeeId(), Entry.Room, Entry.TimeSeconds);
				}
			}
		}

		if (Record.bAct && ActorPawn && Record.TrueActorId != Me->GetEmployeeId() && CanSee(MyPawn, ActorPawn))
		{
			Brain->ObserveAct(Record.TrueActorId, Entry);
		}
	}
}

int32 UUOEvidenceSubsystem::GetTrueActor(int32 TraceId) const
{
	const int32* Found = TrueActors.Find(TraceId);
	return Found ? *Found : INDEX_NONE;
}

FName UUOEvidenceSubsystem::GetRoomOf(const APawn* Pawn) const
{
	const AUOGameMode* GameMode = GetGameMode();
	const UUOBossSystemComponent* Boss = GameMode ? GameMode->GetBossSystem() : nullptr;
	return Boss && Pawn ? Boss->GetRoomAt(Pawn->GetActorLocation()) : NAME_None;
}

void UUOEvidenceSubsystem::SetRoomDark(FName Room, bool bDark)
{
	if (Room.IsNone())
	{
		return;
	}
	bDark ? (void)DarkRooms.Add(Room) : (void)DarkRooms.Remove(Room);
}

bool UUOEvidenceSubsystem::CanSee(const APawn* Observer, const APawn* Target) const
{
	using namespace UOEvidencePrivate;

	if (!Observer || !Target || Observer == Target)
	{
		return false;
	}
	const FVector Eye = Observer->GetPawnViewLocation();
	const FVector Point = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector ToTarget = Point - Eye;
	const float Distance = ToTarget.Size();
	if (Distance > FUOKillerTuning::WitnessRadius)
	{
		return false;
	}
	// A dark room hides everything in it from everyone
	const FName ObserverRoom = GetRoomOf(Observer);
	const FName TargetRoom = GetRoomOf(Target);
	if ((!ObserverRoom.IsNone() && IsRoomDark(ObserverRoom)) || (!TargetRoom.IsNone() && IsRoomDark(TargetRoom)))
	{
		return false;
	}
	const FVector Facing = Observer->GetViewRotation().Vector().GetSafeNormal2D();
	if (Distance > 120.f && FVector::DotProduct(Facing, ToTarget.GetSafeNormal2D()) < ViewConeCos)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(UOWitnessTrace), false, Observer);
	Params.AddIgnoredActor(Target);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	return !GetWorld()->LineTraceTestByObjectType(Eye, Point, Objects, Params);
}

int32 UUOEvidenceSubsystem::CountWitnesses(const APawn* Actor, const APawn* Ignore, TArray<int32>* OutWitnessIds) const
{
	const AUOGameState* GameState = GetUOGameState();
	if (!GameState || !Actor)
	{
		return 0;
	}
	int32 Count = 0;
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		const APawn* Pawn = Employee ? Employee->GetPawn() : nullptr;
		if (!Employee || !Employee->IsAlive() || !Pawn || Pawn == Actor || Pawn == Ignore)
		{
			continue;
		}
		if (CanSee(Pawn, Actor))
		{
			++Count;
			if (OutWitnessIds)
			{
				OutWitnessIds->Add(Employee->GetEmployeeId());
			}
		}
	}
	return Count;
}

bool UUOEvidenceSubsystem::IsCameraRoomOnline(FName Room) const
{
	const AUOGameMode* GameMode = GetGameMode();
	const AUOGameState* GameState = GetUOGameState();
	if (!GameMode || !GameState)
	{
		return false;
	}
	bool bHasCamera = false;
	for (const AUOTerminal* Terminal : GameMode->GetTerminals())
	{
		if (Terminal && Terminal->GetTerminalKind() == EUOTerminalKind::SecurityCamera && Terminal->GetRoomId() == Room)
		{
			bHasCamera = true;
			if (Terminal->IsCameraOffline(GameState->GetShiftElapsedSeconds()))
			{
				return false;
			}
		}
	}
	return bHasCamera;
}

void UUOEvidenceSubsystem::TickShift(float DeltaSeconds)
{
	SampleTimer -= DeltaSeconds;
	if (SampleTimer > 0.f)
	{
		return;
	}
	SampleTimer = UOEvidencePrivate::PresenceSampleSeconds;

	const AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		const APawn* Pawn = Employee ? Employee->GetPawn() : nullptr;
		if (!Employee || !Employee->IsAlive() || !Pawn)
		{
			continue;
		}
		const FName Room = GetRoomOf(Pawn);
		FName& Last = LastRooms.FindOrAdd(Employee->GetEmployeeId());
		if (Room == Last || Room.IsNone())
		{
			if (!Room.IsNone())
			{
				Last = Room;
			}
			continue;
		}
		Last = Room;

		FUOTraceRecord Presence;
		Presence.TrueActorId = Employee->GetEmployeeId();
		Presence.Room = Room;
		Presence.Detail = EUOTraceDetail::Named;
		if (GetDoorRooms().Contains(Room))
		{
			Presence.Device = EUOTraceDevice::DoorLog;
			Presence.Kind = EUOTraceKind::DoorEntry;
			Record(Presence);
		}
		if (IsCameraRoomOnline(Room) && !IsRoomDark(Room))
		{
			Presence.Device = EUOTraceDevice::CameraLog;
			Presence.Kind = EUOTraceKind::CameraSighting;
			Record(Presence);
		}
	}
}

bool UUOEvidenceSubsystem::TogglePin(AUOPlayerState* Employee, int32 TraceId)
{
	const AUOGameState* GameState = GetUOGameState();
	if (!Employee || !Employee->IsAlive() || !GameState || !GameState->FindTrace(TraceId))
	{
		return false;
	}
	TArray<int32>& Pins = Employee->GetMutablePinnedTraceIds();
	if (Pins.Remove(TraceId) > 0)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Evidence: %s unpinned trace #%d (%d pinned)."), *Employee->GetDisplayName(), TraceId, Pins.Num());
		return true;
	}
	if (Pins.Num() >= FUOKillerTuning::MaxPins)
	{
		return false;
	}
	Pins.Add(TraceId);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Evidence: %s pinned trace #%d (%d pinned)."), *Employee->GetDisplayName(), TraceId, Pins.Num());
	return true;
}
