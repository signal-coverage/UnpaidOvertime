// UNPAID: OVERTIME - title screen over the camera fly-through.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SUOMainMenuWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOMainMenuWidget) {}
		SLATE_EVENT(FSimpleDelegate, OnPlay)
		SLATE_EVENT(FSimpleDelegate, OnSettings)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	FSimpleDelegate OnPlay;
	FSimpleDelegate OnSettings;
	FSimpleDelegate OnQuit;
};
