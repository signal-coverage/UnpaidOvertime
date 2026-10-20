// UNPAID: OVERTIME - a terminal's device log on screen: entries (time, who, what), filter by employee, pin as evidence.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UOKillerTypes.h"

class AUOPlayerController;
class AUOTerminal;
class AUOGameState;
class SVerticalBox;
class SScrollBox;

/**
 *  Read-only view of AUOGameState::TraceLog for one device, newest first. Pins go through
 *  AUOPlayerController::TogglePin (the authority validates them). The game keeps running while you read.
 */
class SUOTerminalWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOTerminalWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AUOPlayerController>, Owner)
		SLATE_ARGUMENT(TWeakObjectPtr<AUOTerminal>, Terminal)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Dev automation: pin the newest entries matching a predicate (returns how many were pinned) */
	int32 PinWhere(TFunctionRef<bool(const FUOTraceEntry&)> Predicate, int32 MaxCount);

private:

	TWeakObjectPtr<AUOPlayerController> Owner;
	TWeakObjectPtr<AUOTerminal> Terminal;
	EUOTraceDevice Device = EUOTraceDevice::ActivityBoard;

	/** INDEX_NONE = everyone */
	int32 Filter = INDEX_NONE;

	TSharedPtr<SVerticalBox> Rows;
	TSharedPtr<SVerticalBox> FilterRow;
	FString BuiltSignature;

	const AUOGameState* GetGameState() const;
	int32 GetMyId() const;
	FString NameOf(int32 EmployeeId) const;
	TArray<const FUOTraceEntry*> GetEntries() const;
	bool IsPinned(int32 TraceId) const;
	int32 CountPins() const;

	void RebuildRows();
	void RebuildFilters();
};
