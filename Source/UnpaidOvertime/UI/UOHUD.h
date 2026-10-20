// UNPAID: OVERTIME - HUD actor hosting the Slate in-game HUD (no UMG or binary UI assets).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UOHUD.generated.h"

class SWidget;
class SUOHUDWidget;

/**
 *  Adds the Slate HUD to the local viewport and shows it only while the day runs (shift and overtime).
 *  Full-screen menus and the evaluation screen belong to AUOPlayerController.
 */
UCLASS()
class AUOHUD : public AHUD
{
	GENERATED_BODY()

public:

	AUOHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:

	TSharedPtr<SUOHUDWidget> HUDWidget;
	TSharedPtr<SWidget> HUDContainer;
};
