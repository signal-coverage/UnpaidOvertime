// UNPAID: OVERTIME - in-game HUD (Slate): clock, task list, crosshair, interaction prompt, carried item, overtime.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UOCoreTypes.h"

class APlayerController;
class AUOGameState;
class AUOPlayerState;
class UUOInteractionComponent;
class SVerticalBox;

/**
 *  Read-only view of the game state, polled every frame. Never decides anything.
 */
class SUOHUDWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOHUDWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:

	/** Coworker name tag projected to the screen this frame */
	struct FNameTag
	{
		FVector2D Position;
		FVector2f TextSize = FVector2f::ZeroVector;
		float Distance = 0.f;
		FString Name;
		FLinearColor Color;
		float Alpha = 1.f;
		/** Killers only: a fellow killer (rule 1) */
		bool bAccomplice = false;
	};
	TArray<FNameTag> NameTags;

	void UpdateNameTags();

	TWeakObjectPtr<APlayerController> Owner;

	TSharedPtr<SVerticalBox> TaskRows;
	TSharedPtr<SVerticalBox> FeedRows;
	FString TaskSignature;
	int32 FeedCount = -1;

	float Time = 0.f;

	const AUOGameState* GetGameState() const;
	const AUOPlayerState* GetEmployee() const;
	const UUOInteractionComponent* GetInteraction() const;

	bool IsOvertime() const;
	float GetOvertimePulse() const;

	void RebuildTasks(const AUOGameState& GameState);
	void RebuildFeed(const AUOGameState& GameState);

	TSharedRef<SWidget> MakeTopLeft();
	TSharedRef<SWidget> MakeTaskPanel();
	TSharedRef<SWidget> MakePrompt();
	TSharedRef<SWidget> MakeCarriedChip();
	TSharedRef<SWidget> MakeOvertimeBanner();
	/** The killer's F prompt (sabotage / dismissal), shown only to the killer */
	TSharedRef<SWidget> MakeSecondaryPrompt();
	/** The killer's private panel: heat, cooldowns, dismissal, accomplices */
	TSharedRef<SWidget> MakeKillerPanel();
	/** Voice chat indicator (inert in single player; the M7 hook) */
	TSharedRef<SWidget> MakeMicIndicator();
	bool IsKiller() const;
	TSharedRef<SWidget> MakeEdgeGlow(bool bHorizontalBand, bool bFromStart, float Thickness);

	FText GetStationLabel(FName StationId) const;
};
