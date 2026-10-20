// UNPAID: OVERTIME - deterministic, seed-driven task generator.

#include "UOTaskGenerator.h"
#include "UOMatchConfig.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "UOTaskGenerator"

namespace UOTaskGeneratorPrivate
{
	constexpr int32 NumTaskTypes = 3;

	template <typename T>
	void ShuffleWithStream(TArray<T>& Items, FRandomStream& Rng)
	{
		for (int32 Index = Items.Num() - 1; Index > 0; --Index)
		{
			const int32 SwapIndex = Rng.RandRange(0, Index);
			Items.Swap(Index, SwapIndex);
		}
	}

	FName MakeStationId(EUOStationType Type, int32 Ordinal)
	{
		const TCHAR* Prefix = TEXT("Station");
		switch (Type)
		{
		case EUOStationType::Printer:		Prefix = TEXT("Printer"); break;
		case EUOStationType::CoffeeMaker:	Prefix = TEXT("CoffeeMaker"); break;
		case EUOStationType::FilingCabinet:	Prefix = TEXT("FilingCabinet"); break;
		case EUOStationType::InboxTray:		Prefix = TEXT("InboxTray"); break;
		case EUOStationType::SupplyCloset:	Prefix = TEXT("SupplyCloset"); break;
		}
		return FName(*FString::Printf(TEXT("%s_%02d"), Prefix, Ordinal));
	}

	/** Adds a station spec and bumps the matching counter */
	FName AddStation(FUOStationRequirements& Req, EUOStationType Type, int32 ZoneIndex)
	{
		int32* Counter = nullptr;
		switch (Type)
		{
		case EUOStationType::Printer:		Counter = &Req.Printers; break;
		case EUOStationType::CoffeeMaker:	Counter = &Req.CoffeeMakers; break;
		case EUOStationType::FilingCabinet:	Counter = &Req.FilingCabinets; break;
		case EUOStationType::InboxTray:		Counter = &Req.InboxTrays; break;
		case EUOStationType::SupplyCloset:	Counter = &Req.SupplyClosets; break;
		}
		check(Counter);
		++(*Counter);

		FUOStationSpec Spec;
		Spec.Type = Type;
		Spec.ZoneIndex = ZoneIndex;
		Spec.Ordinal = *Counter;
		Spec.StationId = MakeStationId(Type, Spec.Ordinal);
		Req.Stations.Add(Spec);
		return Spec.StationId;
	}
}

float FUOTaskGenerator::GetDifficultyForDay(int32 DayNumber)
{
	return 1.f + 0.25f * static_cast<float>(FMath::Max(1, DayNumber) - 1);
}

void FUOTaskGenerator::GetSubObjectiveRange(float Difficulty, int32& OutMin, int32& OutMax)
{
	const float D = FMath::Max(1.f, Difficulty);
	OutMin = FMath::Clamp(FMath::FloorToInt(1.f + D), MinSubObjectives, MaxSubObjectives);
	OutMax = FMath::Clamp(FMath::FloorToInt(2.f + D), MinSubObjectives, MaxSubObjectives);
	OutMax = FMath::Max(OutMin, OutMax);
}

float FUOTaskGenerator::GetWorkSecondsScale(float Difficulty)
{
	// Exponential approach to the cap with the initial slope WorkSecondsScalePerDifficulty: never plateaus, never explodes
	const float Range = MaxWorkSecondsScale - 1.f;
	const float Steps = FMath::Max(1.f, Difficulty) - 1.f;
	return 1.f + Range * (1.f - FMath::Exp(-Steps * WorkSecondsScalePerDifficulty / Range));
}

float FUOTaskGenerator::GetBaseWorkSeconds(EUOTaskType TaskType)
{
	switch (TaskType)
	{
	case EUOTaskType::PrinterMaintenance:	return 2.5f;
	case EUOTaskType::CoffeeRefill:			return 4.f;
	case EUOTaskType::FileReports:			return 2.f;
	default:								return 2.f;
	}
}

EUOStationType FUOTaskGenerator::GetDestinationStationType(EUOTaskType TaskType)
{
	switch (TaskType)
	{
	case EUOTaskType::PrinterMaintenance:	return EUOStationType::Printer;
	case EUOTaskType::FileReports:			return EUOStationType::FilingCabinet;
	default:								return EUOStationType::CoffeeMaker;
	}
}

EUOStationType FUOTaskGenerator::GetItemSourceStationType(EUOItemType ItemType)
{
	return ItemType == EUOItemType::Report ? EUOStationType::InboxTray : EUOStationType::SupplyCloset;
}

FText FUOTaskGenerator::GetZoneName(int32 ZoneIndex)
{
	switch (((ZoneIndex % NumZones) + NumZones) % NumZones)
	{
	case 0:		return LOCTEXT("ZoneNorth", "North wing");
	case 1:		return LOCTEXT("ZoneSouth", "South wing");
	case 2:		return LOCTEXT("ZoneEast", "East wing");
	case 3:		return LOCTEXT("ZoneWest", "West wing");
	case 4:		return LOCTEXT("ZoneReception", "Reception");
	case 5:		return LOCTEXT("ZoneBreakRoom", "Break room");
	case 6:		return LOCTEXT("ZoneArchive", "Archive");
	default:	return LOCTEXT("ZoneExecutive", "Executive suite");
	}
}

FUOGeneratedDay FUOTaskGenerator::Generate(int32 Seed, int32 TasksPerDay, float Difficulty)
{
	using namespace UOTaskGeneratorPrivate;

	FRandomStream Rng(Seed);
	FUOGeneratedDay Day;

	const int32 TaskCount = FMath::Clamp(TasksPerDay, FUOMatchConfig::MinTasksPerDay, FUOMatchConfig::MaxTasksPerDay);

	// Every type at least once, the rest random, then shuffle the order
	TArray<EUOTaskType> Types = { EUOTaskType::PrinterMaintenance, EUOTaskType::CoffeeRefill, EUOTaskType::FileReports };
	while (Types.Num() < TaskCount)
	{
		Types.Add(static_cast<EUOTaskType>(Rng.RandRange(0, NumTaskTypes - 1)));
	}
	ShuffleWithStream(Types, Rng);

	// Repeated types walk through zones from a random offset, so their suffixes never collide
	int32 ZoneOffsets[NumTaskTypes];
	int32 Occurrences[NumTaskTypes];
	for (int32 TypeIndex = 0; TypeIndex < NumTaskTypes; ++TypeIndex)
	{
		ZoneOffsets[TypeIndex] = Rng.RandRange(0, NumZones - 1);
		Occurrences[TypeIndex] = 0;
	}

	int32 MinSubs = MinSubObjectives;
	int32 MaxSubs = MaxSubObjectives;
	GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);
	const float WorkScale = GetWorkSecondsScale(Difficulty);

	int32 NextSubObjectiveId = 0;
	bool bNeedsSupplyCloset = false;

	for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
	{
		const EUOTaskType Type = Types[TaskIndex];
		const int32 TypeIndex = static_cast<int32>(Type);
		const int32 ZoneIndex = (ZoneOffsets[TypeIndex] + Occurrences[TypeIndex]++) % NumZones;

		FUOTaskInstance& Task = Day.Tasks.AddDefaulted_GetRef();
		Task.Id = TaskIndex;
		Task.Type = Type;
		Task.ZoneIndex = ZoneIndex;
		Task.DisplayName = FText::Format(LOCTEXT("TaskNameFormat", "{0} — {1}"), UOText::GetTaskTypeName(Type), GetZoneName(ZoneIndex));

		const int32 SubCount = Rng.RandRange(MinSubs, MaxSubs);
		const float WorkSeconds = FMath::RoundToFloat(GetBaseWorkSeconds(Type) * WorkScale * 10.f) / 10.f;

		// Report tasks share one filing cabinet (destination) and one inbox tray (source)
		FName SharedCabinet = NAME_None;
		if (Type == EUOTaskType::FileReports)
		{
			SharedCabinet = AddStation(Day.Stations, EUOStationType::FilingCabinet, ZoneIndex);
			AddStation(Day.Stations, EUOStationType::InboxTray, ZoneIndex);
		}

		for (int32 SubIndex = 0; SubIndex < SubCount; ++SubIndex)
		{
			FUOSubObjective& Sub = Task.SubObjectives.AddDefaulted_GetRef();
			Sub.Id = NextSubObjectiveId++;
			Sub.TaskId = Task.Id;
			Sub.WorkSeconds = WorkSeconds;

			switch (Type)
			{
			case EUOTaskType::PrinterMaintenance:
				Sub.StationId = AddStation(Day.Stations, EUOStationType::Printer, ZoneIndex);
				Sub.bRequiresItem = true;
				Sub.ItemType = EUOItemType::InkCartridge;
				bNeedsSupplyCloset = true;
				break;

			case EUOTaskType::CoffeeRefill:
				Sub.StationId = AddStation(Day.Stations, EUOStationType::CoffeeMaker, ZoneIndex);
				break;

			case EUOTaskType::FileReports:
				Sub.StationId = SharedCabinet;
				Sub.bRequiresItem = true;
				Sub.ItemType = EUOItemType::Report;
				break;
			}
		}
	}

	if (bNeedsSupplyCloset)
	{
		AddStation(Day.Stations, EUOStationType::SupplyCloset, INDEX_NONE);
	}

	return Day;
}

int32 FUOStationSlots::Get(EUOStationType Type) const
{
	switch (Type)
	{
	case EUOStationType::Printer:		return Printers;
	case EUOStationType::CoffeeMaker:	return CoffeeMakers;
	case EUOStationType::FilingCabinet:	return FilingCabinets;
	case EUOStationType::InboxTray:		return InboxTrays;
	case EUOStationType::SupplyCloset:	return SupplyClosets;
	default:							return 0;
	}
}

void FUOStationSlots::Add(EUOStationType Type, int32 Count)
{
	switch (Type)
	{
	case EUOStationType::Printer:		Printers += Count; break;
	case EUOStationType::CoffeeMaker:	CoffeeMakers += Count; break;
	case EUOStationType::FilingCabinet:	FilingCabinets += Count; break;
	case EUOStationType::InboxTray:		InboxTrays += Count; break;
	case EUOStationType::SupplyCloset:	SupplyClosets += Count; break;
	}
}

FName FUOTaskGenerator::MakeStationId(EUOStationType Type, int32 Ordinal)
{
	return UOTaskGeneratorPrivate::MakeStationId(Type, Ordinal);
}

namespace UOTaskGeneratorPrivate
{
	/** Lists an anchor once (stations are shared only in offices that are too small) */
	FName ListAnchor(FUOStationRequirements& Req, EUOStationType Type, int32 Ordinal, int32 ZoneIndex)
	{
		const FName Id = MakeStationId(Type, Ordinal);
		if (!Req.FindStation(Id))
		{
			switch (Type)
			{
			case EUOStationType::Printer:		++Req.Printers; break;
			case EUOStationType::CoffeeMaker:	++Req.CoffeeMakers; break;
			case EUOStationType::FilingCabinet:	++Req.FilingCabinets; break;
			case EUOStationType::InboxTray:		++Req.InboxTrays; break;
			case EUOStationType::SupplyCloset:	++Req.SupplyClosets; break;
			}

			FUOStationSpec Spec;
			Spec.Type = Type;
			Spec.ZoneIndex = ZoneIndex;
			Spec.Ordinal = Ordinal;
			Spec.StationId = Id;
			Req.Stations.Add(Spec);
		}
		return Id;
	}

	/** Seed-shuffled anchor ordinals of one type, handed out in order (wrapping only when exhausted) */
	struct FAnchorDeck
	{
		TArray<int32> Ordinals;
		int32 Next = 0;

		void Init(int32 Count, FRandomStream& Rng)
		{
			Ordinals.Reset();
			for (int32 Ordinal = 1; Ordinal <= Count; ++Ordinal)
			{
				Ordinals.Add(Ordinal);
			}
			ShuffleWithStream(Ordinals, Rng);
			Next = 0;
		}

		int32 Draw()
		{
			check(Ordinals.Num() > 0);
			return Ordinals[Next++ % Ordinals.Num()];
		}
	};
}

FUOGeneratedDay FUOTaskGenerator::GenerateForSlots(int32 Seed, int32 TasksPerDay, float Difficulty, const FUOStationSlots& Slots)
{
	using namespace UOTaskGeneratorPrivate;

	const int32 TaskCount = FMath::Clamp(TasksPerDay, FUOMatchConfig::MinTasksPerDay, FUOMatchConfig::MaxTasksPerDay);

	int32 MinSubs = MinSubObjectives;
	int32 MaxSubs = MaxSubObjectives;
	GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);
	const float WorkScale = GetWorkSecondsScale(Difficulty);

	// A type is usable when its destination exists and, if it needs an item, the item source exists too
	const EUOTaskType AllTypes[NumTaskTypes] = { EUOTaskType::PrinterMaintenance, EUOTaskType::CoffeeRefill, EUOTaskType::FileReports };
	bool bUsable[NumTaskTypes];
	int32 Capacity[NumTaskTypes];	// destination stations not reserved yet
	int32 TaskCost[NumTaskTypes];	// stations one task needs at the difficulty's minimum size
	bool bAnyUsable = false;
	for (int32 TypeIndex = 0; TypeIndex < NumTaskTypes; ++TypeIndex)
	{
		const EUOTaskType Type = AllTypes[TypeIndex];
		Capacity[TypeIndex] = Slots.Get(GetDestinationStationType(Type));
		TaskCost[TypeIndex] = Type == EUOTaskType::FileReports ? 1 : MinSubs;
		bUsable[TypeIndex] = Capacity[TypeIndex] > 0;
		if (Type == EUOTaskType::PrinterMaintenance)
		{
			bUsable[TypeIndex] &= Slots.SupplyClosets > 0;
		}
		else if (Type == EUOTaskType::FileReports)
		{
			bUsable[TypeIndex] &= Slots.InboxTrays > 0;
		}
		bAnyUsable |= bUsable[TypeIndex];
	}

	// No usable anchor at all: the procedural generator builds its own stations
	if (!bAnyUsable)
	{
		return Generate(Seed, TasksPerDay, Difficulty);
	}

	FRandomStream Rng(Seed);
	FUOGeneratedDay Day;

	// Picks a type that still fits a full task, else one that fits a smaller task, else any usable type (sharing)
	auto PickType = [&]() -> int32
	{
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			TArray<int32, TInlineAllocator<NumTaskTypes>> Candidates;
			for (int32 TypeIndex = 0; TypeIndex < NumTaskTypes; ++TypeIndex)
			{
				const bool bFits = Tier == 0 ? Capacity[TypeIndex] >= TaskCost[TypeIndex] : (Tier == 1 ? Capacity[TypeIndex] >= 1 : true);
				if (bUsable[TypeIndex] && bFits)
				{
					Candidates.Add(TypeIndex);
				}
			}
			if (Candidates.Num() > 0)
			{
				return Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
			}
		}
		return INDEX_NONE;
	};

	// A full-size task reserves its minimum; when that no longer fits, a one-sub-objective task reserves one station
	auto Reserve = [&](int32 TypeIndex)
	{
		const int32 Amount = Capacity[TypeIndex] >= TaskCost[TypeIndex] ? TaskCost[TypeIndex] : 1;
		Capacity[TypeIndex] = FMath::Max(0, Capacity[TypeIndex] - Amount);
	};

	// Every usable type at least once, the rest random among what still fits, then shuffle the order
	TArray<EUOTaskType> Types;
	for (int32 TypeIndex = 0; TypeIndex < NumTaskTypes && Types.Num() < TaskCount; ++TypeIndex)
	{
		if (bUsable[TypeIndex])
		{
			Types.Add(AllTypes[TypeIndex]);
			Reserve(TypeIndex);
		}
	}
	while (Types.Num() < TaskCount)
	{
		const int32 TypeIndex = PickType();
		Types.Add(AllTypes[TypeIndex]);
		Reserve(TypeIndex);
	}
	ShuffleWithStream(Types, Rng);

	// Sub-objective counts: random within the difficulty range, then trimmed to the machines of each type
	TArray<int32> SubCounts;
	for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
	{
		SubCounts.Add(Rng.RandRange(MinSubs, MaxSubs));
	}
	for (const EUOTaskType Type : { EUOTaskType::PrinterMaintenance, EUOTaskType::CoffeeRefill })
	{
		const int32 Budget = Slots.Get(GetDestinationStationType(Type));
		int32 Used = 0;
		for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
		{
			Used += Types[TaskIndex] == Type ? SubCounts[TaskIndex] : 0;
		}

		// Trim the biggest task first (ties: the earliest), never below one sub-objective
		while (Used > Budget)
		{
			int32 Biggest = INDEX_NONE;
			for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
			{
				if (Types[TaskIndex] == Type && SubCounts[TaskIndex] > 1 && (Biggest == INDEX_NONE || SubCounts[TaskIndex] > SubCounts[Biggest]))
				{
					Biggest = TaskIndex;
				}
			}
			if (Biggest == INDEX_NONE)
			{
				break;
			}
			--SubCounts[Biggest];
			--Used;
		}
	}

	// Seeded order in which each type's anchors are used
	FAnchorDeck Printers;
	FAnchorDeck CoffeeMakers;
	FAnchorDeck Cabinets;
	Printers.Init(Slots.Printers, Rng);
	CoffeeMakers.Init(Slots.CoffeeMakers, Rng);
	Cabinets.Init(Slots.FilingCabinets, Rng);

	// Repeated types walk through zones from a random offset, so their names never collide
	int32 ZoneOffsets[NumTaskTypes];
	int32 Occurrences[NumTaskTypes];
	for (int32 TypeIndex = 0; TypeIndex < NumTaskTypes; ++TypeIndex)
	{
		ZoneOffsets[TypeIndex] = Rng.RandRange(0, NumZones - 1);
		Occurrences[TypeIndex] = 0;
	}

	int32 NextSubObjectiveId = 0;
	for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
	{
		const EUOTaskType Type = Types[TaskIndex];
		const int32 TypeIndex = static_cast<int32>(Type);
		const int32 ZoneIndex = (ZoneOffsets[TypeIndex] + Occurrences[TypeIndex]++) % NumZones;

		FUOTaskInstance& Task = Day.Tasks.AddDefaulted_GetRef();
		Task.Id = TaskIndex;
		Task.Type = Type;
		Task.ZoneIndex = ZoneIndex;
		Task.DisplayName = FText::Format(LOCTEXT("TaskNameFormat", "{0} — {1}"), UOText::GetTaskTypeName(Type), GetZoneName(ZoneIndex));

		const float WorkSeconds = FMath::RoundToFloat(GetBaseWorkSeconds(Type) * WorkScale * 10.f) / 10.f;
		const FName SharedCabinet = Type == EUOTaskType::FileReports
			? ListAnchor(Day.Stations, EUOStationType::FilingCabinet, Cabinets.Draw(), ZoneIndex)
			: NAME_None;

		for (int32 SubIndex = 0; SubIndex < SubCounts[TaskIndex]; ++SubIndex)
		{
			FUOSubObjective& Sub = Task.SubObjectives.AddDefaulted_GetRef();
			Sub.Id = NextSubObjectiveId++;
			Sub.TaskId = Task.Id;
			Sub.WorkSeconds = WorkSeconds;

			switch (Type)
			{
			case EUOTaskType::PrinterMaintenance:
				Sub.StationId = ListAnchor(Day.Stations, EUOStationType::Printer, Printers.Draw(), ZoneIndex);
				Sub.bRequiresItem = true;
				Sub.ItemType = EUOItemType::InkCartridge;
				break;

			case EUOTaskType::CoffeeRefill:
				Sub.StationId = ListAnchor(Day.Stations, EUOStationType::CoffeeMaker, CoffeeMakers.Draw(), ZoneIndex);
				break;

			case EUOTaskType::FileReports:
				Sub.StationId = SharedCabinet;
				Sub.bRequiresItem = true;
				Sub.ItemType = EUOItemType::Report;
				break;
			}
		}
	}

	// Every item source anchor works as a source
	for (int32 Ordinal = 1; Ordinal <= Slots.InboxTrays; ++Ordinal)
	{
		ListAnchor(Day.Stations, EUOStationType::InboxTray, Ordinal, INDEX_NONE);
	}
	for (int32 Ordinal = 1; Ordinal <= Slots.SupplyClosets; ++Ordinal)
	{
		ListAnchor(Day.Stations, EUOStationType::SupplyCloset, Ordinal, INDEX_NONE);
	}

	return Day;
}

#undef LOCTEXT_NAMESPACE
