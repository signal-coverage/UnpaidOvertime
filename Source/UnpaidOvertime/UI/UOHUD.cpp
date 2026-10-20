// UNPAID: OVERTIME - HUD actor hosting the Slate in-game HUD.

#include "UOHUD.h"
#include "SUOHUDWidget.h"
#include "UOGameState.h"
#include "UOPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SWeakWidget.h"

namespace UOHUDHostPrivate
{
	/** Below the menus (50), above the game */
	constexpr int32 HUDZOrder = 10;
}

AUOHUD::AUOHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AUOHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!PC || !PC->IsLocalController() || !Viewport)
	{
		return;
	}

	SAssignNew(HUDWidget, SUOHUDWidget).Owner(PC);
	HUDWidget->SetVisibility(EVisibility::Collapsed);
	HUDContainer = SNew(SWeakWidget).PossiblyNullContent(HUDWidget);
	Viewport->AddViewportWidgetContent(HUDContainer.ToSharedRef(), UOHUDHostPrivate::HUDZOrder);
}

void AUOHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDContainer.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(HUDContainer.ToSharedRef());
		}
	}
	HUDContainer.Reset();
	HUDWidget.Reset();
	Super::EndPlay(EndPlayReason);
}

void AUOHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HUDWidget.IsValid())
	{
		return;
	}

	const AUOGameState* GameState = GetWorld()->GetGameState<AUOGameState>();
	// The private role card covers everything: no HUD (or name tags) bleeding through it
	const AUOPlayerController* PC = Cast<AUOPlayerController>(GetOwningPlayerController());
	const bool bShow = GameState && GameState->IsWorkPhase() && !(PC && PC->GetScreen() == EUOMenuScreen::RoleReveal);
	HUDWidget->SetVisibility(bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}
