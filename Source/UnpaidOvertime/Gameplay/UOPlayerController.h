// UNPAID: OVERTIME - local player flow: menus, pause, settings, evaluation and the menu camera.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UOCoreTypes.h"
#include "UOMatchConfig.h"
#include "UOGameMode.h"
#include "UOEvaluationTypes.h"
#include "UOPlayerController.generated.h"

class ACameraActor;
class SWidget;
class SBox;
class SUOMatchSetupWidget;
class SUOEvaluationFlowWidget;
class SUOTerminalWidget;
class AUOTerminal;
class FUODevAutomation;

/** Full-screen UI currently shown by the player controller */
UENUM()
enum class EUOMenuScreen : uint8
{
	None,
	MainMenu,
	Setup,
	Settings,
	Pause,
	Evaluation,
	/** A terminal's log (the game keeps running) */
	Terminal,
	/** Day 1: the private role card */
	RoleReveal
};

/**
 *  Owns every full-screen Slate screen and the input mode that goes with it. The flow is:
 *  Main menu (camera fly-through) -> Setup -> day (HUD) -> Esc pause menu / Settings -> Evaluation sequence
 *  (review, firings, boss questions, cut vote, outcome) -> next day (HUD) ... -> match over -> new hire / main menu. The UI only collects input: the game mode validates and decides everything.
 */
UCLASS()
class AUOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AUOPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	EUOMenuScreen GetScreen() const { return Screen; }

	void ShowMainMenu();
	void ShowSetup();
	void OpenSettings();
	void CloseSettings();
	void OpenPauseMenu();
	void ClosePauseMenu();

	/** Esc / Back from the current screen */
	void HandleBack();

	/** Sends the setup to the authority; returns its validation */
	FUOConfigValidationResult SubmitMatchConfig(const FUOMatchConfig& Config);

	/**
	 *  The only way the evaluation UI talks to the rules: forwards the local player's choice to the authority
	 *  (single player: direct call; M7: a server RPC goes here). Returns false when rejected.
	 */
	bool SubmitEvaluationChoice(const FUOEvalChoice& Choice);

	/** Keys other than Esc pressed on a full-screen menu; true when the screen used it */
	bool HandleMenuKey(const FKey& Key);

	// ~ Investigation (M4)

	/** Opens a terminal's log on screen (game keeps running) */
	void OpenTerminal(AUOTerminal* Terminal);
	void CloseTerminal();

	/** Pins/unpins a trace as evidence (forwarded to the authority) */
	bool TogglePin(int32 TraceId);

	/** Day 1: who you are (private) */
	void ShowRoleReveal();
	void DismissRoleReveal();

	// ~ Voice chat (M7 hook): inert in single player, logged

	/** The mute key (M): mutes / unmutes the microphone. M7: route to the online voice interface here. */
	void ToggleMicMute();
	/** The effective state the HUD shows: the Settings mode, muted by the key */
	bool IsMicMuted() const;

	void QuitToMainMenu();
	void QuitGame();

	/** Setup screen while it is open (dev automation fills it) */
	TSharedPtr<SUOMatchSetupWidget> GetSetupWidget() const { return SetupWidget; }

	/** Dev automation: the open evaluation flow / terminal panel */
	TSharedPtr<SUOEvaluationFlowWidget> GetEvaluationFlow() const { return EvaluationFlow; }
	TSharedPtr<SUOTerminalWidget> GetTerminalWidget() const { return TerminalWidget; }

protected:

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MenuCamera;

	EUOMenuScreen Screen = EUOMenuScreen::None;

	/** Screen to return to when Settings closes */
	EUOMenuScreen ScreenBeforeSettings = EUOMenuScreen::MainMenu;

	TSharedPtr<SWidget> RootWidget;
	TSharedPtr<SWidget> RootContainer;
	TSharedPtr<SBox> ScreenSlot;
	TSharedPtr<SUOMatchSetupWidget> SetupWidget;
	TSharedPtr<SUOEvaluationFlowWidget> EvaluationFlow;
	TSharedPtr<SUOTerminalWidget> TerminalWidget;

	/** The role card was shown for the current match */
	bool bRoleRevealShown = false;
	/** Muted with the M key (on top of the Settings voice mode) */
	bool bMicMutedByKey = false;
	float RoleRevealClock = 0.f;

	TArray<FUOMenuCameraShot> MenuShots;
	int32 MenuShotIndex = 0;
	float MenuShotTime = 0.f;

	EUODayPhase LastPhase = EUODayPhase::PreGame;
	bool bStarted = false;

	/** Dev-only scripted run (-UOAutoStart / -UOShots); never created in Shipping */
	TSharedPtr<FUODevAutomation> DevAutomation;

	void EnsureRootWidget();
	void SetScreen(EUOMenuScreen NewScreen, TSharedPtr<SWidget> Content);
	void ApplyInputMode();
	void HandlePhaseChanged(EUODayPhase NewPhase);

	void StartMenuCamera();
	void TickMenuCamera(float DeltaSeconds);

	class AUOGameState* GetUOGameState() const;
};
