// UNPAID: OVERTIME - local player flow.

#include "UOPlayerController.h"
#include "UOGameUserSettings.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOGameUserSettings.h"
#include "UODevAutomation.h"
#include "SUOMainMenuWidget.h"
#include "SUOMatchSetupWidget.h"
#include "SUOSettingsWidget.h"
#include "SUOPauseMenuWidget.h"
#include "SUOEvaluationFlowWidget.h"
#include "SUOTerminalWidget.h"
#include "SUORoleRevealWidget.h"
#include "UOTerminal.h"
#include "UOPlayerState.h"
#include "UOAudioSubsystem.h"
#include "UOAudioRules.h"
#include "UOUIStyle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Layout/SBox.h"
#include "UnpaidOvertime.h"

namespace UOPlayerControllerPrivate
{
	constexpr int32 MenuZOrder = 50;
	constexpr float MenuShotSeconds = 16.f;
	constexpr float MenuCameraFov = 75.f;
}

/** Full-screen host for the current menu; routes Esc to the controller */
class SUOMenuRoot : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOMenuRoot) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, AUOPlayerController* InOwner)
	{
		Owner = InOwner;
		ChildSlot[InArgs._Content.Widget];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		AUOPlayerController* PC = Owner.Get();
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			if (PC)
			{
				PC->HandleBack();
			}
			return FReply::Handled();
		}
		return PC && PC->HandleMenuKey(InKeyEvent.GetKey()) ? FReply::Handled() : FReply::Unhandled();
	}

private:

	TWeakObjectPtr<AUOPlayerController> Owner;
};

AUOPlayerController::AUOPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AUOGameState* AUOPlayerController::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

void AUOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		UUOGameUserSettings::Get()->ApplyGameplaySettings(GetWorld());

#if !UE_BUILD_SHIPPING
		DevAutomation = FUODevAutomation::CreateFromCommandLine(this);
#endif
	}
}

void AUOPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DevAutomation.Reset();

	if (RootContainer.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(RootContainer.ToSharedRef());
		}
	}
	RootContainer.Reset();
	RootWidget.Reset();
	ScreenSlot.Reset();
	SetupWidget.Reset();
	EvaluationFlow.Reset();

	Super::EndPlay(EndPlayReason);
}

void AUOPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController())
	{
		return;
	}

	const AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	// First tick with a game state: open the front end
	if (!bStarted)
	{
		bStarted = true;
		LastPhase = GameState->GetPhase();
		if (LastPhase == EUODayPhase::PreGame)
		{
			StartMenuCamera();
			ShowMainMenu();
		}
	}

	if (GameState->GetPhase() != LastPhase)
	{
		LastPhase = GameState->GetPhase();
		HandlePhaseChanged(LastPhase);
	}

	if (LastPhase == EUODayPhase::PreGame)
	{
		TickMenuCamera(DeltaSeconds);
	}

	// A new match starts on day 1: the role card shows once (it is reset when a later day is reached)
	if (GameState->GetDayNumber() > 1)
	{
		bRoleRevealShown = false;
	}
	if (Screen == EUOMenuScreen::RoleReveal)
	{
		// Real time (the game is paused behind the card); it clocks you in by itself after a while
		RoleRevealClock += FApp::GetDeltaTime();
		if (RoleRevealClock > 20.f)
		{
			DismissRoleReveal();
		}
	}

	if (DevAutomation.IsValid())
	{
		DevAutomation->Tick(DeltaSeconds);
	}
}

void AUOPlayerController::HandlePhaseChanged(EUODayPhase NewPhase)
{
	switch (NewPhase)
	{
	case EUODayPhase::Shift:
		// The day started (or an emergency meeting ended): the pawn now owns the view
		SetScreen(EUOMenuScreen::None, nullptr);
		if (MenuCamera)
		{
			MenuCamera->Destroy();
			MenuCamera = nullptr;
		}
		if (const AUOGameState* GameState = GetUOGameState(); GameState && GameState->GetDayNumber() == 1 && !bRoleRevealShown)
		{
			ShowRoleReveal();
		}
		break;

	case EUODayPhase::Overtime:
		// Back from an emergency meeting during overtime
		if (Screen == EUOMenuScreen::Evaluation)
		{
			SetScreen(EUOMenuScreen::None, nullptr);
		}
		break;

	case EUODayPhase::Evaluation:
	case EUODayPhase::Meeting:
		SetPause(false);
		SetScreen(EUOMenuScreen::Evaluation, SAssignNew(EvaluationFlow, SUOEvaluationFlowWidget).Owner(this));
		break;

	default:
		break;
	}
}

void AUOPlayerController::EnsureRootWidget()
{
	if (RootWidget.IsValid())
	{
		return;
	}

	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("UOPlayerController: no viewport, menus not shown."));
		return;
	}

	RootWidget = SNew(SUOMenuRoot, this)
		[
			SAssignNew(ScreenSlot, SBox)
		];
	RootContainer = SNew(SWeakWidget).PossiblyNullContent(RootWidget);
	Viewport->AddViewportWidgetContent(RootContainer.ToSharedRef(), UOPlayerControllerPrivate::MenuZOrder);
}

void AUOPlayerController::SetScreen(EUOMenuScreen NewScreen, TSharedPtr<SWidget> Content)
{
	EnsureRootWidget();

	Screen = NewScreen;
	if (NewScreen != EUOMenuScreen::Setup)
	{
		SetupWidget.Reset();
	}
	if (NewScreen != EUOMenuScreen::Evaluation)
	{
		EvaluationFlow.Reset();
	}
	if (NewScreen != EUOMenuScreen::Terminal)
	{
		TerminalWidget.Reset();
	}

	if (ScreenSlot.IsValid())
	{
		ScreenSlot->SetContent(Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget);
		RootWidget->SetVisibility(NewScreen == EUOMenuScreen::None ? EVisibility::Collapsed : EVisibility::Visible);
	}
	ApplyInputMode();
}

void AUOPlayerController::ApplyInputMode()
{
	if (Screen == EUOMenuScreen::None)
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		return;
	}

	FInputModeUIOnly InputMode;
	TSharedPtr<SWidget> Focus = RootWidget;
	if (Screen == EUOMenuScreen::Setup && SetupWidget.IsValid())
	{
		Focus = SetupWidget->GetInitialFocusTarget();
	}
	InputMode.SetWidgetToFocus(Focus);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AUOPlayerController::ShowMainMenu()
{
	SetScreen(EUOMenuScreen::MainMenu,
		SNew(SUOMainMenuWidget)
		.OnPlay_Lambda([this]() { ShowSetup(); })
		.OnSettings_Lambda([this]() { OpenSettings(); })
		.OnQuit_Lambda([this]() { QuitGame(); }));
}

void AUOPlayerController::ShowSetup()
{
	TSharedPtr<SUOMatchSetupWidget> NewSetup;
	SAssignNew(NewSetup, SUOMatchSetupWidget)
		.OnCreateCompany(FOnUOCreateCompany::CreateUObject(this, &AUOPlayerController::SubmitMatchConfig))
		.OnBack_Lambda([this]() { ShowMainMenu(); });

	SetupWidget = NewSetup;
	Screen = EUOMenuScreen::Setup;
	EnsureRootWidget();
	if (ScreenSlot.IsValid())
	{
		ScreenSlot->SetContent(NewSetup.ToSharedRef());
		RootWidget->SetVisibility(EVisibility::Visible);
	}
	ApplyInputMode();
}

void AUOPlayerController::OpenSettings()
{
	if (Screen != EUOMenuScreen::Settings)
	{
		ScreenBeforeSettings = Screen;
	}
	FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.MenuOpen);
	SetScreen(EUOMenuScreen::Settings,
		SNew(SUOSettingsWidget)
		.Owner(this)
		.bInGame(ScreenBeforeSettings == EUOMenuScreen::Pause)
		.OnClose_Lambda([this]() { CloseSettings(); }));
}

void AUOPlayerController::CloseSettings()
{
	// Settings may close without Apply (Esc): drop any live volume preview
	UUOGameUserSettings::Get()->ApplyGameplaySettings(GetWorld());

	if (ScreenBeforeSettings == EUOMenuScreen::Pause)
	{
		OpenPauseMenu();
	}
	else
	{
		ShowMainMenu();
	}
}

void AUOPlayerController::OpenPauseMenu()
{
	const AUOGameState* GameState = GetUOGameState();
	if (!GameState || !GameState->IsWorkPhase())
	{
		return;
	}

	SetPause(true);
	if (Screen != EUOMenuScreen::Settings)
	{
		FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.MenuOpen);
	}
	SetScreen(EUOMenuScreen::Pause,
		SNew(SUOPauseMenuWidget)
		.Owner(this)
		.OnResume_Lambda([this]() { ClosePauseMenu(); })
		.OnSettings_Lambda([this]() { OpenSettings(); })
		.OnQuitToMenu_Lambda([this]() { QuitToMainMenu(); })
		.OnQuitGame_Lambda([this]() { QuitGame(); }));
}

void AUOPlayerController::ClosePauseMenu()
{
	FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.MenuClose);
	SetPause(false);
	SetScreen(EUOMenuScreen::None, nullptr);
}

void AUOPlayerController::HandleBack()
{
	if (Screen == EUOMenuScreen::Setup || Screen == EUOMenuScreen::Settings)
	{
		FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.Back);
	}

	switch (Screen)
	{
	case EUOMenuScreen::Setup:		ShowMainMenu(); break;
	case EUOMenuScreen::Settings:	CloseSettings(); break;
	case EUOMenuScreen::Pause:		ClosePauseMenu(); break;
	case EUOMenuScreen::Terminal:	CloseTerminal(); break;
	case EUOMenuScreen::RoleReveal:	DismissRoleReveal(); break;
	default:						break;
	}
}

FUOConfigValidationResult AUOPlayerController::SubmitMatchConfig(const FUOMatchConfig& Config)
{
	// Local authority (single player / listen server host). A remote client would route this through a server RPC.
	AUOGameMode* GameMode = GetWorld()->GetAuthGameMode<AUOGameMode>();
	const FUOConfigValidationResult Result = GameMode ? GameMode->RequestStartDay(Config) : Config.Validate();
	if (!Result.bIsValid)
	{
		FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.Error);
	}
	return Result;
}

bool AUOPlayerController::SubmitEvaluationChoice(const FUOEvalChoice& Choice)
{
	AUOGameMode* GameMode = GetWorld()->GetAuthGameMode<AUOGameMode>();
	const bool bAccepted = GameMode && GameMode->SubmitEvaluationChoice(this, Choice);
	if (!bAccepted)
	{
		UE_LOG(LogUnpaidOvertime, Verbose, TEXT("Evaluation choice %d (%d) rejected."), static_cast<int32>(Choice.Type), Choice.Value);
	}
	return bAccepted;
}

bool AUOPlayerController::HandleMenuKey(const FKey& Key)
{
	if (Screen == EUOMenuScreen::RoleReveal && (Key == EKeys::Enter || Key == EKeys::SpaceBar) && RoleRevealClock > 0.6f)
	{
		DismissRoleReveal();
		return true;
	}
	// The mute key works on every screen of a match (the meeting included)
	if (Key == EKeys::M && Screen != EUOMenuScreen::MainMenu && Screen != EUOMenuScreen::Setup && Screen != EUOMenuScreen::Settings)
	{
		ToggleMicMute();
		return true;
	}
	if (Screen == EUOMenuScreen::Terminal && (Key == EKeys::E || Key == EKeys::Tab))
	{
		CloseTerminal();
		return true;
	}
	return Screen == EUOMenuScreen::Evaluation && EvaluationFlow.IsValid() && EvaluationFlow->HandleKey(Key);
}

void AUOPlayerController::OpenTerminal(AUOTerminal* Terminal)
{
	const AUOGameState* GameState = GetUOGameState();
	if (!Terminal || !GameState || !GameState->IsWorkPhase() || Screen != EUOMenuScreen::None)
	{
		return;
	}
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
	{
		Audio->Play2D(UOSoundId::TerminalOpen);
	}
	SetScreen(EUOMenuScreen::Terminal, SAssignNew(TerminalWidget, SUOTerminalWidget).Owner(this).Terminal(Terminal));
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Terminal: the human opened %s."), *Terminal->GetDisplayName().ToString());
}

void AUOPlayerController::CloseTerminal()
{
	if (Screen == EUOMenuScreen::Terminal)
	{
		FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.MenuClose);
		SetScreen(EUOMenuScreen::None, nullptr);
	}
}

bool AUOPlayerController::TogglePin(int32 TraceId)
{
	AUOGameMode* GameMode = GetWorld()->GetAuthGameMode<AUOGameMode>();
	const bool bAccepted = GameMode && GameMode->SubmitPinToggle(this, TraceId);
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
	{
		Audio->Play2D(bAccepted ? UOSoundId::TerminalBeep : UOSoundId::InteractDenied);
	}
	return bAccepted;
}

void AUOPlayerController::ShowRoleReveal()
{
	const AUOPlayerState* Me = GetPlayerState<AUOPlayerState>();
	if (!Me || Me->GetKnownRole() == EUORole::Unknown)
	{
		return;
	}
	bRoleRevealShown = true;
	RoleRevealClock = 0.f;
	SetPause(true);
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
	{
		Audio->Play2D(Me->GetKnownRole() == EUORole::Killer ? UOSoundId::RoleKiller : UOSoundId::RoleEmployee);
	}
	SetScreen(EUOMenuScreen::RoleReveal, SNew(SUORoleRevealWidget).Owner(this).OnDismiss_Lambda([this]() { DismissRoleReveal(); }));
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Role reveal shown to the human (private): %s."), Me->GetKnownRole() == EUORole::Killer ? TEXT("SABOTEUR") : TEXT("EMPLOYEE"));
}

void AUOPlayerController::DismissRoleReveal()
{
	if (Screen == EUOMenuScreen::RoleReveal)
	{
		SetPause(false);
		SetScreen(EUOMenuScreen::None, nullptr);
	}
}

void AUOPlayerController::QuitToMainMenu()
{
	if (AUOGameMode* GameMode = GetWorld()->GetAuthGameMode<AUOGameMode>())
	{
		GameMode->RequestReturnToMenu(this);
	}
}

void AUOPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AUOPlayerController::StartMenuCamera()
{
	const AUOGameMode* GameMode = GetWorld()->GetAuthGameMode<AUOGameMode>();
	MenuShots = GameMode ? GameMode->GetMenuCameraShots() : TArray<FUOMenuCameraShot>();
	if (MenuShots.Num() == 0)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MenuCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), MenuShots[0].From, SpawnParams);
	if (MenuCamera)
	{
		MenuCamera->GetCameraComponent()->SetFieldOfView(UOPlayerControllerPrivate::MenuCameraFov);
		MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		SetViewTarget(MenuCamera);
	}
	MenuShotIndex = 0;
	MenuShotTime = 0.f;
}

void AUOPlayerController::TickMenuCamera(float DeltaSeconds)
{
	if (!MenuCamera || MenuShots.Num() == 0)
	{
		return;
	}

	MenuShotTime += DeltaSeconds;
	if (MenuShotTime >= UOPlayerControllerPrivate::MenuShotSeconds)
	{
		MenuShotTime = 0.f;
		MenuShotIndex = (MenuShotIndex + 1) % MenuShots.Num();
	}

	// Slow ease-in/out drift between the two keys of the current shot
	const FUOMenuCameraShot& Shot = MenuShots[MenuShotIndex];
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, MenuShotTime / UOPlayerControllerPrivate::MenuShotSeconds, 1.6f);
	const FVector Location = FMath::Lerp(Shot.From.GetLocation(), Shot.To.GetLocation(), Alpha);
	const FQuat Rotation = FQuat::Slerp(Shot.From.GetRotation(), Shot.To.GetRotation(), Alpha);
	MenuCamera->SetActorLocationAndRotation(Location, Rotation);

	if (GetViewTarget() != MenuCamera)
	{
		SetViewTarget(MenuCamera);
	}
}

void AUOPlayerController::ToggleMicMute()
{
	bMicMutedByKey = !bMicMutedByKey;
	const EUOVoiceMode Mode = UUOGameUserSettings::Get()->GetVoiceMode();
	// M7 hook: forward to the online voice interface (StartNetworkedVoice / StopNetworkedVoice) here
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Voice chat: microphone %s (mode %s). Single player: nothing is transmitted (M7 hook)."),
		IsMicMuted() ? TEXT("MUTED") : TEXT("live"), *StaticEnum<EUOVoiceMode>()->GetNameStringByValue(static_cast<int64>(Mode)));
}

bool AUOPlayerController::IsMicMuted() const
{
	return bMicMutedByKey || UUOGameUserSettings::Get()->GetVoiceMode() == EUOVoiceMode::Muted;
}
