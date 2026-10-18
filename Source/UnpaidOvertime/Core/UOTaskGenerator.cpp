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
	return 1.f + 0.2f * (FMath::Max(1.f, Difficulty) - 1.f);
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

#undef LOCTEXT_NAMESPACE
