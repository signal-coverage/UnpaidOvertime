// UNPAID: OVERTIME - canvas HUD (no UMG assets): top bar, task list, prompts and the evaluation screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UOHUD.generated.h"

class AUOGameState;
class AUOPlayerState;
class UFont;

/**
 *  Read-only view of the game state. Draws nothing during setup (the Slate setup screen is up).
 */
UCLASS()
class AUOHUD : public AHUD
{
	GENERATED_BODY()

public:

	AUOHUD();

	virtual void DrawHUD() override;

protected:

	/** UI scale relative to a 1080p canvas */
	float UIScale = 1.f;

	void DrawTopBar(const AUOGameState& GameState);
	void DrawTaskList(const AUOGameState& GameState);
	void DrawCrosshairAndPrompt();
	void DrawCarriedItem(const AUOPlayerState* Employee);
	void DrawActivityLog(const AUOGameState& GameState);
	void DrawOvertimeTint();
	void DrawEvaluation(const AUOGameState& GameState);

	void DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale);
	void DrawStar(float CenterX, float CenterY, float Radius, bool bFilled, const FLinearColor& Color);
	void DrawStars(float X, float CenterY, int32 Stars, float Radius);
	void DrawProgressBar(float X, float Y, float Width, float Height, float Progress, const FLinearColor& FillColor);

	FString GetStationName(FName StationId) const;
	AUOPlayerState* GetLocalEmployee() const;

	UFont* GetBodyFont() const;
	UFont* GetBigFont() const;
};
