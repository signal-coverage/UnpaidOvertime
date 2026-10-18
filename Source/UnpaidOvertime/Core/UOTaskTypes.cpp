// UNPAID: OVERTIME - task model helpers.

#include "UOTaskTypes.h"

int32 FUOTaskInstance::CountDone() const
{
	int32 Done = 0;
	for (const FUOSubObjective& Sub : SubObjectives)
	{
		Done += Sub.IsDone() ? 1 : 0;
	}
	return Done;
}

bool FUOTaskInstance::IsComplete() const
{
	return CountDone() == SubObjectives.Num();
}

int32 FUOStationRequirements::GetCountForType(EUOStationType Type) const
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

const FUOStationSpec* FUOStationRequirements::FindStation(FName StationId) const
{
	return Stations.FindByPredicate([StationId](const FUOStationSpec& Spec) { return Spec.StationId == StationId; });
}
