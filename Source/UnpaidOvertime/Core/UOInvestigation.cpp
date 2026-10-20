// UNPAID: OVERTIME - the investigation: trace texts and the AI suspicion model.

#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "UOBossQuestions.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "UOInvestigation"

// ~ Trace text

FText FUOTraceText::GetDeviceName(EUOTraceDevice Device)
{
	switch (Device)
	{
	case EUOTraceDevice::ActivityBoard:	return LOCTEXT("DevActivity", "Activity board");
	case EUOTraceDevice::PrinterLog:	return LOCTEXT("DevPrinter", "Printer log");
	case EUOTraceDevice::FileHistory:	return LOCTEXT("DevFiles", "File history");
	case EUOTraceDevice::DoorLog:		return LOCTEXT("DevDoors", "Badge & door log");
	case EUOTraceDevice::CameraLog:		return LOCTEXT("DevCameras", "Camera log");
	default:							return LOCTEXT("DevInventory", "Inventory sheet");
	}
}

FText FUOTraceText::GetTerminalName(EUOTerminalKind Kind)
{
	switch (Kind)
	{
	case EUOTerminalKind::ActivityBoard:	return LOCTEXT("TermActivity", "Reception PC");
	case EUOTerminalKind::PrintServer:		return LOCTEXT("TermPrint", "Print server panel");
	case EUOTerminalKind::DeskComputer:		return LOCTEXT("TermDesk", "Desk PC");
	case EUOTerminalKind::BadgeReader:		return LOCTEXT("TermBadge", "Badge reader");
	case EUOTerminalKind::SecurityMonitor:	return LOCTEXT("TermMonitor", "Security monitor");
	case EUOTerminalKind::InventorySheet:	return LOCTEXT("TermInventory", "Inventory sheet");
	case EUOTerminalKind::SecurityCamera:	return LOCTEXT("TermCamera", "Security camera");
	default:								return LOCTEXT("TermProp", "Office plant");
	}
}

bool FUOTraceText::GetTerminalDevice(EUOTerminalKind Kind, EUOTraceDevice& OutDevice)
{
	switch (Kind)
	{
	case EUOTerminalKind::ActivityBoard:	OutDevice = EUOTraceDevice::ActivityBoard; return true;
	case EUOTerminalKind::PrintServer:		OutDevice = EUOTraceDevice::PrinterLog; return true;
	case EUOTerminalKind::DeskComputer:		OutDevice = EUOTraceDevice::FileHistory; return true;
	case EUOTerminalKind::BadgeReader:		OutDevice = EUOTraceDevice::DoorLog; return true;
	case EUOTerminalKind::SecurityMonitor:	OutDevice = EUOTraceDevice::CameraLog; return true;
	case EUOTerminalKind::InventorySheet:	OutDevice = EUOTraceDevice::Inventory; return true;
	default:								return false;
	}
}

FText FUOTraceText::GetSabotageName(EUOSabotageType Type)
{
	switch (Type)
	{
	case EUOSabotageType::JamPrinter:	return LOCTEXT("SabJam", "Jam the printer");
	case EUOSabotageType::CorruptFile:	return LOCTEXT("SabFile", "Corrupt a report");
	case EUOSabotageType::LockStation:	return LOCTEXT("SabLock", "Lock the station");
	case EUOSabotageType::CutCamera:	return LOCTEXT("SabCamera", "Cut camera and lights");
	default:							return LOCTEXT("SabMove", "Move it to another room");
	}
}

FText FUOTraceText::DescribeActor(const FUOTraceEntry& Entry, TFunctionRef<FString(int32)> NameOf)
{
	if (Entry.ShownActorIds.Num() == 1)
	{
		return FText::FromString(NameOf(Entry.ShownActorIds[0]));
	}
	if (Entry.ShownActorIds.Num() >= 2)
	{
		return FText::Format(LOCTEXT("ActorPartial", "{0} or {1} (badge unclear)"), FText::FromString(NameOf(Entry.ShownActorIds[0])), FText::FromString(NameOf(Entry.ShownActorIds[1])));
	}
	return LOCTEXT("ActorUnknown", "unknown");
}

FText FUOTraceText::Describe(const FUOTraceEntry& Entry, TFunctionRef<FString(int32)> NameOf)
{
	const FText Who = DescribeActor(Entry, NameOf);
	const FText Where = FUORooms::GetDisplayName(Entry.Room);
	const FText What = FText::FromString(Entry.Subject);
	switch (Entry.Kind)
	{
	case EUOTraceKind::WorkReported:		return FText::Format(LOCTEXT("WorkReported", "{0} reported work done at {1}"), Who, What);
	case EUOTraceKind::PrintOk:				return FText::Format(LOCTEXT("PrintOk", "{0}: cartridge replaced, test page printed (1 page) — by {1}"), What, Who);
	case EUOTraceKind::PrintNoOutput:		return FText::Format(LOCTEXT("PrintNoOutput", "{0}: job sent, 0 pages printed — badge: {1}"), What, Who);
	case EUOTraceKind::PrinterJam:			return FText::Format(LOCTEXT("PrinterJam", "{0}: PAPER JAM (error E-12) — last badge: {1}"), What, Who);
	case EUOTraceKind::PrinterJamCleared:	return FText::Format(LOCTEXT("PrinterJamCleared", "{0}: jam cleared by {1}"), What, Who);
	case EUOTraceKind::FileSaved:			return FText::Format(LOCTEXT("FileSaved", "Report #{0} filed at {1} by {2} (checksum OK)"), FText::AsNumber(Entry.ValueA), What, Who);
	case EUOTraceKind::FileNoChange:		return FText::Format(LOCTEXT("FileNoChange", "{0}: file opened, no changes saved — user: {1}"), What, Who);
	case EUOTraceKind::FileOverwritten:		return FText::Format(LOCTEXT("FileOverwritten", "Report #{0} OVERWRITTEN from workstation {1} — user: {2}"), FText::AsNumber(Entry.ValueA), What, Who);
	case EUOTraceKind::DoorEntry:			return FText::Format(LOCTEXT("DoorEntry", "{0} badged into {1}"), Who, Where);
	case EUOTraceKind::StationLocked:		return FText::Format(LOCTEXT("StationLocked", "{0}: maintenance lock engaged ({1}s) — override badge: {2}"), What, FText::AsNumber(Entry.ValueA), Who);
	case EUOTraceKind::StationUnlocked:		return FText::Format(LOCTEXT("StationUnlocked", "{0}: maintenance lock released"), What);
	case EUOTraceKind::CameraSighting:		return FText::Format(LOCTEXT("CameraSighting", "CAM {0}: {1} on camera"), Where, Who);
	case EUOTraceKind::CameraOffline:		return FText::Format(LOCTEXT("CameraOffline", "CAM {0}: OFFLINE (power cut, lights out) — last badge nearby: {1}"), Where, Who);
	case EUOTraceKind::CameraOnline:		return FText::Format(LOCTEXT("CameraOnline", "CAM {0}: back online after {1}s"), Where, FText::AsNumber(Entry.ValueA));
	case EUOTraceKind::BodyFound:			return FText::Format(LOCTEXT("BodyFound", "INCIDENT in {0}: {1} found with a termination letter — last badge nearby: {2}"), Where, What, Who);
	case EUOTraceKind::ItemSignedOut:		return FText::Format(LOCTEXT("ItemSignedOut", "{0} signed out at {1} by {2}"), FText::FromString(Entry.Subject), Where, Who);
	case EUOTraceKind::CoffeeLogged:		return FText::Format(LOCTEXT("CoffeeLogged", "{0}: coffee pack used, refill logged — by {1}"), What, Who);
	case EUOTraceKind::CoffeeNoChange:		return FText::Format(LOCTEXT("CoffeeNoChange", "{0}: refill logged, pack count unchanged — by {1}"), What, Who);
	case EUOTraceKind::PropMissing:			return FText::Format(LOCTEXT("PropMissing", "Inventory mismatch: {0} missing from {1} ({2} → {3}) — marked by: {4}"), What, Where, FText::AsNumber(Entry.ValueA), FText::AsNumber(Entry.ValueB), Who);
	case EUOTraceKind::LastSeenNear:
	{
		FString Names;
		for (const int32 Id : Entry.ShownActorIds)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + NameOf(Id);
		}
		return FText::Format(LOCTEXT("LastSeenNear", "NOTE: last seen near the {0} before the incident: {1}"), Where,
			Names.IsEmpty() ? LOCTEXT("NobodyNear", "nobody") : FText::FromString(Names));
	}
	default:								return What;
	}
}

FString FUOTraceText::ToLogString(const FUOTraceEntry& Entry, TFunctionRef<FString(int32)> NameOf)
{
	return FString::Printf(TEXT("#%d [%s] %s: %s"), Entry.Id, *Entry.ClockTime, *GetDeviceName(Entry.Device).ToString(), *Describe(Entry, NameOf).ToString());
}

// ~ Suspicion

float FUOSuspicionResult::GetScore(int32 SuspectId) const
{
	float Score = 0.f;
	for (const FUOSuspicionItem& Item : Items)
	{
		Score += Item.SuspectId == SuspectId ? Item.Weight : 0.f;
	}
	return Score;
}

TArray<int32> FUOSuspicionResult::GetEvidenceIds(int32 SuspectId, int32 MaxCount) const
{
	TArray<const FUOSuspicionItem*> Sorted;
	for (const FUOSuspicionItem& Item : Items)
	{
		if (Item.SuspectId == SuspectId && Item.TraceId != INDEX_NONE)
		{
			Sorted.Add(&Item);
		}
	}
	Sorted.StableSort([](const FUOSuspicionItem& A, const FUOSuspicionItem& B) { return A.Weight > B.Weight; });

	TArray<int32> Ids;
	for (const FUOSuspicionItem* Item : Sorted)
	{
		if (Ids.Num() >= MaxCount)
		{
			break;
		}
		Ids.AddUnique(Item->TraceId);
	}
	return Ids;
}

int32 FUOSuspicionResult::GetTopSuspect(float& OutScore, float& OutSecondScore) const
{
	TMap<int32, float> Scores;
	for (const FUOSuspicionItem& Item : Items)
	{
		Scores.FindOrAdd(Item.SuspectId) += Item.Weight;
	}
	int32 Top = INDEX_NONE;
	OutScore = 0.f;
	OutSecondScore = 0.f;
	TArray<int32> Ids;
	Scores.GetKeys(Ids);
	Ids.Sort();
	for (const int32 Id : Ids)
	{
		const float Score = Scores[Id];
		if (Score > OutScore)
		{
			OutSecondScore = OutScore;
			OutScore = Score;
			Top = Id;
		}
		else if (Score > OutSecondScore)
		{
			OutSecondScore = Score;
		}
	}
	return Top;
}

bool FUOSuspicionModel::IsSabotageTrace(EUOTraceKind Kind)
{
	switch (Kind)
	{
	case EUOTraceKind::PrinterJam:
	case EUOTraceKind::FileOverwritten:
	case EUOTraceKind::StationLocked:
	case EUOTraceKind::CameraOffline:
	case EUOTraceKind::PropMissing:
	case EUOTraceKind::BodyFound:
		return true;
	default:
		return false;
	}
}

bool FUOSuspicionModel::IsNoOutputTrace(EUOTraceKind Kind)
{
	return Kind == EUOTraceKind::PrintNoOutput || Kind == EUOTraceKind::FileNoChange || Kind == EUOTraceKind::CoffeeNoChange;
}

namespace UOSuspicionPrivate
{
	const TCHAR* SourceName(EUOEvidenceSource Source)
	{
		switch (Source)
		{
		case EUOEvidenceSource::Witnessed:	return TEXT("witnessed");
		case EUOEvidenceSource::Terminal:			return TEXT("terminal");
		case EUOEvidenceSource::PublicKnowledge:	return TEXT("Public+Knowledge");
		case EUOEvidenceSource::Hunch:				return TEXT("hunch");
		case EUOEvidenceSource::Memory:				return TEXT("memory");
		default:									return TEXT("public");
		}
	}

	/** A known placement of an employee in a room at a time */
	struct FPlacement
	{
		int32 EmployeeId = INDEX_NONE;
		FName Room;
		float Time = 0.f;
	};
}

FUOSuspicionResult FUOSuspicionModel::Evaluate(const FUOSuspicionInput& Input)
{
	using namespace UOSuspicionPrivate;

	FUOSuspicionResult Result;
	// Less Suspicion: evidence is weighed unevenly, and hunches and misreads creep in (rule 9: false leads)
	const float Gullibility = 1.f - FMath::Clamp(Input.Suspicion, 0.f, 1.f);
	FRandomStream Rng(Input.NoiseSeed * 7907 + Input.SelfId * 131 + 17);
	auto IsCandidate = [&Input](int32 Id) { return Id != Input.SelfId && Input.CandidateIds.Contains(Id); };
	EUOSuspicionBasis Basis = EUOSuspicionBasis::NamedTrace;
	auto AddItem = [&Result, &Rng, Gullibility, &Basis](int32 Suspect, float Weight, int32 TraceId, EUOEvidenceSource Source, const FString& Reason)
	{
		const float Noise = 1.f + Gullibility * WeightNoise * (2.f * Rng.FRand() - 1.f);
		const float Final = Source == EUOEvidenceSource::Hunch ? Weight : Weight * Noise;
		if (Final <= 0.f)
		{
			return;
		}
		FUOSuspicionItem& Item = Result.Items.AddDefaulted_GetRef();
		Item.SuspectId = Suspect;
		Item.Weight = Final;
		Item.TraceId = TraceId;
		Item.Source = Source;
		Item.Reason = Reason;
		Item.Basis = Basis;
	};

	// Where people were: door and camera logs it knows, plus what it saw itself
	TArray<FPlacement> Placements;
	for (const FUOKnownTrace& Known : Input.Traces)
	{
		const FUOTraceEntry& Entry = Known.Entry;
		if ((Entry.Kind == EUOTraceKind::DoorEntry || Entry.Kind == EUOTraceKind::CameraSighting) && Entry.IsNamed())
		{
			Placements.Add({ Entry.ShownActorIds[0], Entry.Room, Entry.TimeSeconds });
		}
	}
	for (const FUOSighting& Sighting : Input.Sightings)
	{
		Placements.Add({ Sighting.EmployeeId, Sighting.Room, Sighting.TimeSeconds });
	}
	Placements.StableSort([](const FPlacement& A, const FPlacement& B) { return A.Time < B.Time; });

	// Was the employee in Room around Time? Last known placement at or before Time (within the window), or one
	// in the room shortly after.
	auto WasThere = [&Placements](int32 EmployeeId, FName Room, float Time)
	{
		const FPlacement* Last = nullptr;
		bool bRightAfter = false;
		for (const FPlacement& Placement : Placements)
		{
			if (Placement.EmployeeId != EmployeeId)
			{
				continue;
			}
			if (Placement.Time <= Time + 1.f)
			{
				Last = &Placement;
			}
			else if (Placement.Time <= Time + PresenceWindowSeconds && Placement.Room == Room)
			{
				bRightAfter = true;
			}
		}
		const bool bBefore = Last && Last->Room == Room && Time - Last->Time <= PresenceWindowSeconds * 3.f;
		return bBefore || bRightAfter;
	};

	for (const FUOKnownTrace& Known : Input.Traces)
	{
		const FUOTraceEntry& Entry = Known.Entry;
		const FString Source = SourceName(Known.Source);

		if (IsSabotageTrace(Entry.Kind))
		{
			if (Entry.ShownActorIds.Num() == 1)
			{
				Basis = EUOSuspicionBasis::NamedTrace;
				if (IsCandidate(Entry.ShownActorIds[0]))
				{
					AddItem(Entry.ShownActorIds[0], WeightNamed, Entry.Id, Known.Source,
						FString::Printf(TEXT("sabotage trace #%d (%s) names them [%s]"), Entry.Id, *StaticEnum<EUOTraceKind>()->GetNameStringByValue(static_cast<int64>(Entry.Kind)), *Source));
				}
				continue;
			}
			if (Entry.ShownActorIds.Num() >= 2)
			{
				Basis = EUOSuspicionBasis::PartialTrace;
				for (const int32 Id : Entry.ShownActorIds)
				{
					if (IsCandidate(Id))
					{
						AddItem(Id, WeightPartial, Entry.Id, Known.Source,
							FString::Printf(TEXT("sabotage trace #%d (%s) names them as one of %d candidates [%s]"), Entry.Id,
								*StaticEnum<EUOTraceKind>()->GetNameStringByValue(static_cast<int64>(Entry.Kind)), Entry.ShownActorIds.Num(), *Source));
					}
				}
				continue;
			}

			// Anonymous: everyone it knows was in that room around that time
			TArray<int32> Present;
			for (const int32 Id : Input.CandidateIds)
			{
				if (IsCandidate(Id) && WasThere(Id, Entry.Room, Entry.TimeSeconds))
				{
					Present.Add(Id);
				}
			}
			Basis = EUOSuspicionBasis::Presence;
			for (const int32 Id : Present)
			{
				AddItem(Id, WeightPresence / Present.Num(), Entry.Id, Known.Source,
					FString::Printf(TEXT("anonymous sabotage #%d in %s at %s; known to be there (1 of %d) [%s + door/camera/own sightings]"),
						Entry.Id, *Entry.Room.ToString(), *Entry.ClockTime, Present.Num(), *Source));
			}
			continue;
		}

		if (Entry.Kind == EUOTraceKind::LastSeenNear)
		{
			// Public note after a dismissal: whoever was around shares the presence weight
			TArray<int32> Named;
			for (const int32 Id : Entry.ShownActorIds)
			{
				if (IsCandidate(Id))
				{
					Named.Add(Id);
				}
			}
			Basis = EUOSuspicionBasis::LastSeen;
			for (const int32 Id : Named)
			{
				AddItem(Id, WeightPresence / Named.Num(), Entry.Id, Known.Source,
					FString::Printf(TEXT("last seen near the %s before the incident (#%d, 1 of %d) [%s]"), *Entry.Room.ToString(), Entry.Id, Named.Num(), *Source));
			}
			continue;
		}

		if (IsNoOutputTrace(Entry.Kind) && Gullibility > 0.f && Entry.ShownActorIds.Num() > 0)
		{
			// Rule 3: no fake work. A botched attempt is just that; only a gullible AI reads anything into it.
			Basis = EUOSuspicionBasis::BotchedWork;
			for (const int32 Id : Entry.ShownActorIds)
			{
				if (IsCandidate(Id))
				{
					AddItem(Id, WeightPrecisionFailure * Gullibility / Entry.ShownActorIds.Num(), Entry.Id, Known.Source,
						FString::Printf(TEXT("a botched job at %s at %s (#%d): sloppy, or up to something? [%s, false lead]"), *Entry.StationId.ToString(), *Entry.ClockTime, Entry.Id, *Source));
				}
			}
		}
	}

	Basis = EUOSuspicionBasis::SeenAct;
	for (const FUOSeenAct& Act : Input.SeenActs)
	{
		if (IsCandidate(Act.ActorId))
		{
			AddItem(Act.ActorId, WeightSeenAct, Act.TraceId, EUOEvidenceSource::Witnessed,
				FString::Printf(TEXT("saw them do it with its own eyes (trace #%d) [witnessed]"), Act.TraceId));
		}
	}

	// Rule 4: a killer's public answer that the boss accepted
	for (const FUOAcceptedAnswer& Answer : Input.AcceptedAnswers)
	{
		if (!IsCandidate(Answer.EmployeeId))
		{
			continue;
		}
		if (Answer.bKnowsFact)
		{
			if (Answer.bWrong)
			{
				Basis = EUOSuspicionBasis::AcceptedWrong;
				AddItem(Answer.EmployeeId, WeightAcceptedWrong, INDEX_NONE, EUOEvidenceSource::PublicKnowledge,
					FString::Printf(TEXT("the boss accepted their answer to \"%s\" and it knows that answer is wrong [Public+Knowledge]"), *Answer.Prompt));
			}
			continue;
		}
		// It cannot tell, but something about the office will not add up: blamed on the wrong person
		if (Answer.bWrong && Rng.FRand() < MisreadChance * Gullibility)
		{
			TArray<int32> Others;
			for (const int32 Id : Input.CandidateIds)
			{
				if (IsCandidate(Id) && Id != Answer.EmployeeId)
				{
					Others.Add(Id);
				}
			}
			if (Others.Num() > 0)
			{
				const int32 Blamed = Others[Rng.RandRange(0, Others.Num() - 1)];
				Basis = EUOSuspicionBasis::Anomaly;
				AddItem(Blamed, WeightMisreadAnomaly * (0.6f + 0.8f * Rng.FRand()), INDEX_NONE, EUOEvidenceSource::Hunch,
					FString::Printf(TEXT("an anomaly around \"%s\" it cannot place, misattributed [hunch, false lead]"), *Answer.Prompt));
			}
		}
	}

	Basis = EUOSuspicionBasis::Memory;
	for (const FUOCarriedSuspicion& Carried : Input.CarriedOver)
	{
		if (IsCandidate(Carried.EmployeeId) && Carried.Score > 0.f)
		{
			AddItem(Carried.EmployeeId, Carried.Score * CarryOverFactor, INDEX_NONE, EUOEvidenceSource::Memory,
				FString::Printf(TEXT("still suspicious from earlier days (%.2f) [memory]"), Carried.Score));
		}
	}

	// A gut feeling about someone (rule 9)
	if (Gullibility > 0.f && Rng.FRand() < HunchChance * Gullibility)
	{
		TArray<int32> Pool;
		for (const int32 Id : Input.CandidateIds)
		{
			if (IsCandidate(Id))
			{
				Pool.Add(Id);
			}
		}
		if (Pool.Num() > 0)
		{
			const int32 Target = Pool[Rng.RandRange(0, Pool.Num() - 1)];
			Basis = EUOSuspicionBasis::Hunch;
			AddItem(Target, WeightHunch * Gullibility * (0.5f + Rng.FRand()), INDEX_NONE, EUOEvidenceSource::Hunch,
				TEXT("they have been acting odd all day (gut feeling) [hunch, false lead]"));
		}
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
