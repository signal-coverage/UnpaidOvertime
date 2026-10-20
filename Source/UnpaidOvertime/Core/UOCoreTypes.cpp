// UNPAID: OVERTIME - shared enum display names.

#include "UOCoreTypes.h"

#define LOCTEXT_NAMESPACE "UOCoreTypes"

FText UOText::GetItemName(EUOItemType Item)
{
	switch (Item)
	{
	case EUOItemType::InkCartridge:	return LOCTEXT("ItemInkCartridge", "Ink cartridge");
	case EUOItemType::Report:		return LOCTEXT("ItemReport", "Report");
	default:						return LOCTEXT("ItemNone", "Nothing");
	}
}

FText UOText::GetStationTypeName(EUOStationType StationType)
{
	switch (StationType)
	{
	case EUOStationType::Printer:		return LOCTEXT("StationPrinter", "Printer");
	case EUOStationType::CoffeeMaker:	return LOCTEXT("StationCoffeeMaker", "Coffee maker");
	case EUOStationType::FilingCabinet:	return LOCTEXT("StationFilingCabinet", "Filing cabinet");
	case EUOStationType::InboxTray:		return LOCTEXT("StationInboxTray", "Inbox tray");
	case EUOStationType::SupplyCloset:	return LOCTEXT("StationSupplyCloset", "Supply closet");
	default:							return FText::GetEmpty();
	}
}

FText UOText::GetTaskTypeName(EUOTaskType TaskType)
{
	switch (TaskType)
	{
	case EUOTaskType::PrinterMaintenance:	return LOCTEXT("TaskPrinterMaintenance", "Printer maintenance");
	case EUOTaskType::CoffeeRefill:			return LOCTEXT("TaskCoffeeRefill", "Coffee refill");
	case EUOTaskType::FileReports:			return LOCTEXT("TaskFileReports", "File reports");
	default:								return FText::GetEmpty();
	}
}

FText UOText::GetDayLengthName(EUODayLength DayLength)
{
	switch (DayLength)
	{
	case EUODayLength::Short:	return LOCTEXT("DayShort", "Short");
	case EUODayLength::Long:	return LOCTEXT("DayLong", "Long");
	default:					return LOCTEXT("DayNormal", "Normal");
	}
}

FText UOText::GetDifficultyName(EUOAIDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EUOAIDifficulty::Easy:	return LOCTEXT("DifficultyEasy", "Easy");
	case EUOAIDifficulty::Hard:	return LOCTEXT("DifficultyHard", "Hard");
	default:					return LOCTEXT("DifficultyMedium", "Medium");
	}
}

FText UOText::GetPhaseName(EUODayPhase Phase)
{
	switch (Phase)
	{
	case EUODayPhase::PreGame:		return LOCTEXT("PhasePreGame", "SETUP");
	case EUODayPhase::Shift:		return LOCTEXT("PhaseShift", "SHIFT");
	case EUODayPhase::Overtime:		return LOCTEXT("PhaseOvertime", "OVERTIME");
	case EUODayPhase::Evaluation:	return LOCTEXT("PhaseEvaluation", "EVALUATION");
	case EUODayPhase::Meeting:		return LOCTEXT("PhaseMeeting", "EMERGENCY MEETING");
	default:						return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
