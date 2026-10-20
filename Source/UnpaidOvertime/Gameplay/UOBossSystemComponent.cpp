// UNPAID: OVERTIME - the boss's knowledge of the office.

#include "UOBossSystemComponent.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "UnpaidOvertime.h"

namespace UOBossSystemPrivate
{
	const FString RoomTagPrefix(TEXT("UORoom."));
	const FString PropTagPrefix(TEXT("UOProp."));
	const FString ColorTagPrefix(TEXT("UOColor."));
	const FString TextTagPrefix(TEXT("UOText."));
	const FString ShapeTagPrefix(TEXT("UOShape."));
	const FName RoomVolumeTag(TEXT("UORoomVolume"));

	FName FindTagValue(const AActor* Actor, const FString& Prefix)
	{
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(Prefix))
			{
				return FName(*TagString.RightChop(Prefix.Len()));
			}
		}
		return NAME_None;
	}
}

UUOBossSystemComponent::UUOBossSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

AUOGameState* UUOBossSystemComponent::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

void UUOBossSystemComponent::ScanLevel()
{
	using namespace UOBossSystemPrivate;

	RoomVolumes.Reset();
	Facts = FUOOfficeFacts();

	TMap<FString, int32> KindCounts;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const FName Room = FindTagValue(Actor, RoomTagPrefix);
		if (Room.IsNone())
		{
			continue;
		}

		if (Actor->ActorHasTag(RoomVolumeTag))
		{
			FRoomVolume& Volume = RoomVolumes.AddDefaulted_GetRef();
			Volume.Room = Room;
			Volume.Bounds = Actor->GetComponentsBoundingBox(true);
			// Room volumes are pure data: never collide with traces or pawns
			Actor->SetActorEnableCollision(false);
			continue;
		}

		const FName KindName = FindTagValue(Actor, PropTagPrefix);
		EUOPropKind Kind = EUOPropKind::Plant;
		if (KindName.IsNone() || !FUOPropKinds::Parse(KindName.ToString(), Kind))
		{
			continue;
		}

		FUOPropInstance Prop;
		Prop.Room = Room;
		Prop.Kind = Kind;
		Prop.Color = FindTagValue(Actor, ColorTagPrefix);
		Prop.Text = FindTagValue(Actor, TextTagPrefix);
		Prop.Shape = FindTagValue(Actor, ShapeTagPrefix);
		Facts.Add(Prop);
		++KindCounts.FindOrAdd(FString::Printf(TEXT("%s/%s"), *Room.ToString(), *KindName.ToString()));
	}

	KindCounts.KeySort([](const FString& A, const FString& B) { return A < B; });
	FString Summary;
	for (const TPair<FString, int32>& Pair : KindCounts)
	{
		Summary += FString::Printf(TEXT("%s=%d "), *Pair.Key, Pair.Value);
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Boss facts: %d room volumes, %d tagged props in %d rooms: %s"),
		RoomVolumes.Num(), Facts.Props.Num(), Facts.GetRooms().Num(), *Summary);
	if (RoomVolumes.Num() == 0)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("Boss facts: no UORoomVolume actors; presence cannot be tracked, so the boss falls back to company-name questions. Rebuild the level (Scripts\\BuildArt.bat level)."));
	}
}

void UUOBossSystemComponent::RegisterStationRoom(FName StationId, FName RoomId)
{
	if (!RoomId.IsNone())
	{
		StationRooms.Add(StationId, RoomId);
	}
}

FName UUOBossSystemComponent::GetRoomAt(const FVector& Location) const
{
	for (const FRoomVolume& Volume : RoomVolumes)
	{
		if (Location.X >= Volume.Bounds.Min.X && Location.X <= Volume.Bounds.Max.X && Location.Y >= Volume.Bounds.Min.Y && Location.Y <= Volume.Bounds.Max.Y)
		{
			return Volume.Room;
		}
	}
	return NAME_None;
}

void UUOBossSystemComponent::ResetForDay()
{
	Presence.Reset();
	SampleTimer = 0.f;
}

void UUOBossSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AUOGameState* GameState = GetUOGameState();
	if (!GameState || !GameState->IsWorkPhase() || RoomVolumes.Num() == 0)
	{
		return;
	}

	SampleTimer -= DeltaTime;
	if (SampleTimer <= 0.f)
	{
		SampleTimer = PresenceSampleSeconds;
		SamplePresence();
	}
}

void UUOBossSystemComponent::SamplePresence()
{
	const AUOGameState* GameState = GetUOGameState();
	const float Time = GameState->GetShiftElapsedSeconds();
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		const APawn* Pawn = Employee ? Employee->GetPawn() : nullptr;
		if (Employee && Employee->IsAlive() && Pawn)
		{
			Presence.Record(Employee->GetEmployeeId(), GetRoomAt(Pawn->GetActorLocation()), Time);
		}
	}
}

FUOBossKnowledge UUOBossSystemComponent::BuildKnowledge() const
{
	FUOBossKnowledge Knowledge;
	Knowledge.Facts = Facts;
	Knowledge.Presence = Presence;

	const AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return Knowledge;
	}

	Knowledge.CompanyName = GameState->GetCompanyName();
	for (const AUOPlayerState* Employee : GameState->GetEmployees())
	{
		// Everyone who worked today (fired employees from earlier days are not plausible answers)
		if (Employee && (Employee->IsAlive() || Employee->HasEvaluation()))
		{
			Knowledge.Employees.Emplace(Employee->GetEmployeeId(), Employee->GetDisplayName());
		}
	}

	for (const FUOEvidenceEntry& Entry : GameState->GetEvidenceLog())
	{
		if (Entry.Action == EUOEvidenceAction::DroppedItem)
		{
			continue;
		}
		const FName* Room = StationRooms.Find(Entry.StationId);
		if (!Room)
		{
			continue;
		}

		FUOBossEvent& Event = Knowledge.Events.AddDefaulted_GetRef();
		Event.Time = Entry.TimeSeconds;
		Event.ClockTime = Entry.ClockTime;
		Event.ActorId = Entry.EmployeeId;
		Event.ActorName = Entry.EmployeeName;
		Event.Room = *Room;
		Event.Kind = Entry.Action == EUOEvidenceAction::PickedUpItem ? EUOBossEventKind::PickedUpItem : EUOBossEventKind::CompletedWork;
		Event.Item = Entry.Item;
		// Station type from the id prefix ("CoffeeMaker_02")
		const FString Id = Entry.StationId.ToString();
		Event.StationType = Id.StartsWith(TEXT("CoffeeMaker")) ? EUOStationType::CoffeeMaker
			: Id.StartsWith(TEXT("FilingCabinet")) ? EUOStationType::FilingCabinet
			: Id.StartsWith(TEXT("InboxTray")) ? EUOStationType::InboxTray
			: Id.StartsWith(TEXT("SupplyCloset")) ? EUOStationType::SupplyCloset
			: EUOStationType::Printer;
	}
	return Knowledge;
}

FUOBossQuestion UUOBossSystemComponent::GenerateQuestion(int32 AskedEmployeeId, EUOQuestionPurpose Purpose, int32 DayNumber, int32 Seed) const
{
	const FUOBossKnowledge Knowledge = BuildKnowledge();
	const FUOBossQuestion Question = FUOBossQuestionGenerator::Generate(Knowledge, AskedEmployeeId, Purpose, DayNumber, Seed);

	FString Visited;
	for (const FName& Room : Presence.GetVisitedRooms(AskedEmployeeId))
	{
		Visited += Room.ToString() + TEXT(" ");
	}
	const TCHAR* PurposeName = Purpose == EUOQuestionPurpose::Promotion ? TEXT("promotion") : (Purpose == EUOQuestionPurpose::Defense ? TEXT("defense") : TEXT("tiebreak"));
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Boss question (%s, day %d) for employee %d [visited: %s| %d events today]: \"%s\" -> \"%s\" (%d options, %.0fs). Why fair: %s"),
		PurposeName, DayNumber, AskedEmployeeId, *Visited, Knowledge.Events.Num(), *Question.Prompt.ToString(),
		Question.Options.IsValidIndex(Question.CorrectIndex) ? *Question.Options[Question.CorrectIndex].ToString() : TEXT("?"),
		Question.Options.Num(), Question.TimeLimitSeconds, *Question.Rationale);
	return Question;
}
