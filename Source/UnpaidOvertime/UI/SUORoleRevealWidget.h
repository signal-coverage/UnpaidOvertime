// UNPAID: OVERTIME - the private role card at the start of a match ("You are an EMPLOYEE" / "You are the SABOTEUR").

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AUOPlayerController;

/** Shown to its owner only, from the role replicated to the owner. Enter / click clocks you in. */
class SUORoleRevealWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUORoleRevealWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AUOPlayerController>, Owner)
		SLATE_EVENT(FSimpleDelegate, OnDismiss)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	FSimpleDelegate OnDismiss;
	float Age = 0.f;
};
