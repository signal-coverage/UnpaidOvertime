// UNPAID: OVERTIME - end-of-day performance review panel (reads the final game state).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class AUOPlayerState;

class SUOEvaluationWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOEvaluationWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
		SLATE_EVENT(FSimpleDelegate, OnContinue)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	FSimpleDelegate OnContinue;
	/** Killers only: the fellow killers (marked on the review) */
	TArray<int32> Accomplices;

	TSharedRef<SWidget> MakeEmployeeRow(const AUOPlayerState& Employee, bool bIsMe, float ScaleMax) const;
};
