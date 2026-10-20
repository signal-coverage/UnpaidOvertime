// UNPAID: OVERTIME - pause menu (game paused while open).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;

class SUOPauseMenuWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOPauseMenuWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnSettings)
		SLATE_EVENT(FSimpleDelegate, OnQuitToMenu)
		SLATE_EVENT(FSimpleDelegate, OnQuitGame)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
