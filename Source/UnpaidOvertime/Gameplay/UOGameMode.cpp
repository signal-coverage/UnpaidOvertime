// UNPAID: OVERTIME - server authority for one working day.

#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOCharacter.h"
#include "UOHUD.h"
#include "UOOfficeBuilder.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskGenerator.h"
#include "UOTaskBoard.h"
#include "UOStarRating.h"
#include "UOAIDifficulty.h"
#include "UOAIEmployee.h"
#include "UOAIController.h"
#include "SUOMatchSetupWidget.h"
#include "Camera/CameraActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Math/RandomStream.h"
#include "Misc/DateTime.h"
#include "Widgets/SWeakWidget.h"
#include "UnpaidOvertime.h"

#define LOCTEXT_NAMESPACE "UOGameMode"

namespace UOGameModePrivate
{
	const FLinearColor HumanColor(0.3f, 0.75f, 1.f);
	const FLinearColor SunOvertimeColor(1.f, 0.32f, 0.22f);
	const FLinearColor SkyOvertimeColor(1.f, 0.4f, 0.35f);
	const FRotator SunOvertimeRotation(-7.f, 35.f, 0.f);
	constexpr float SunOvertimeScale = 0.2f;
	constexpr float SkyOvertimeScale = 0.3f;
	constexpr int32 SetupWidgetZOrder = 50;
}

AUOGameMode::AUOGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = AUOCharacter::StaticClass();
	HUDClass = AUOHUD::StaticClass();
	GameStateClass = AUOGameState::StaticClass();
	PlayerStateClass = AUOPlayerState::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();

	TaskManager = CreateDefaultSubobject<UUOTaskManagerComponent>(TEXT("TaskManager"));

	AINames = {
		LOCTEXT("AIName0", "Brenda from Accounts"),
		LOCTEXT("AIName1", "Kevin from IT"),
		LOCTEXT("AIName2", "Linda from HR"),
		LOCTEXT("AIName3", "Doug from Sales"),
		LOCTEXT("AIName4", "Patricia from Legal"),
		LOCTEXT("AIName5", "Stan from Payroll"),
		LOCTEXT("AIName6", "Rita from Reception"),
		LOCTEXT("AIName7", "Marv from Facilities")
	};

	AIColors = {
		FLinearColor(0.95f, 0.35f, 0.3f),
		FLinearColor(0.4f, 0.85f, 0.35f),
		FLinearColor(0.85f, 0.45f, 0.95f),
		FLinearColor(1.f, 0.65f, 0.15f),
		FLinearColor(0.2f, 0.85f, 0.8f),
		FLinearColor(0.95f, 0.9f, 0.3f),
		FLinearColor(0.6f, 0.6f, 1.f)
	};
}

AUOGameState* AUOGameMode::GetUOGameState() const
{
	return GetGameState<AUOGameState>();
}

void AUOGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// During setup nobody gets a pawn yet: the overview camera and setup screen are shown instead
	const AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || UOGameState->GetPhase() == EUODayPhase::PreGame)
	{
		return;
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AUOGameMode::BeginPlay()
{
	Super::BeginPlay();

	SpawnEnvironment();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OfficeBuilder = GetWorld()->SpawnActor<AUOOfficeBuilder>(AUOOfficeBuilder::StaticClass(), FTransform::Identity, SpawnParams);

	// Empty preview office behind the setup screen
	OfficeBuilder->BuildOffice(1, FUOStationRequirements(), FString());

	OverviewCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), OfficeBuilder->GetOverviewCameraTransform(), SpawnParams);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			PC->SetViewTarget(OverviewCamera);
		}
	}

	if (AUOGameState* UOGameState = GetUOGameState())
	{
		UOGameState->SetPhase(EUODayPhase::PreGame);
	}

	ShowSetupScreen();
}

void AUOGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideSetupScreen();
	Super::EndPlay(EndPlayReason);
}

void AUOGameMode::SpawnEnvironment()
{
	UWorld* World = GetWorld();
	const FTransform SunTransform(SunDayRotation, FVector(0.f, 0.f, 2000.f));

	SunLight = World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	UDirectionalLightComponent* Sun = SunLight ? Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()) : nullptr;
	if (Sun)
	{
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->SetIntensity(SunBaseIntensity);
		Sun->SetAtmosphereSunLight(true);
	}
	if (SunLight)
	{
		SunLight->FinishSpawning(SunTransform);
	}

	const FTransform SkyTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 1000.f));
	SkyLight = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), SkyTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (SkyLight)
	{
		USkyLightComponent* Sky = SkyLight->GetLightComponent();
		Sky->SetMobility(EComponentMobility::Movable);
		Sky->bRealTimeCapture = true;
		Sky->SetIntensity(SkyBaseIntensity);
		SkyLight->FinishSpawning(SkyTransform);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SkyAtmosphere = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, SpawnParams);
}

void AUOGameMode::ShowSetupScreen()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!Viewport || !PC)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("UOGameMode: no local viewport, setup screen not shown."));
		return;
	}

	SAssignNew(SetupWidget, SUOMatchSetupWidget)
		.OnCreateCompany(FOnUOCreateCompany::CreateUObject(this, &AUOGameMode::RequestStartDay));

	SetupWidgetContainer = SNew(SWeakWidget).PossiblyNullContent(SetupWidget);
	Viewport->AddViewportWidgetContent(SetupWidgetContainer.ToSharedRef(), UOGameModePrivate::SetupWidgetZOrder);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SetupWidget->GetInitialFocusTarget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}

void AUOGameMode::HideSetupScreen()
{
	if (SetupWidgetContainer.IsValid())
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(SetupWidgetContainer.ToSharedRef());
		}
	}
	SetupWidgetContainer.Reset();
	SetupWidget.Reset();
}

FUOConfigValidationResult AUOGameMode::RequestStartDay(const FUOMatchConfig& Config)
{
	// Server-side validation: never trust what the UI sent
	const FUOConfigValidationResult Result = Config.Validate();
	if (!Result.bIsValid)
	{
		return Result;
	}

	const AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || UOGameState->GetPhase() != EUODayPhase::PreGame || bStartDayPending)
	{
		return Result;
	}

	MatchConfig = Config;
	MatchConfig.CompanyName = Config.GetSanitizedCompanyName();
	ActiveSeed = MatchConfig.ResolveSeed(FMath::Rand() ^ static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));

	// Start on the next tick so the setup screen is not torn down inside its own click handler
	bStartDayPending = true;
	return Result;
}

void AUOGameMode::RequestNewDay(APlayerController* Requester)
{
	const AUOGameState* UOGameState = GetUOGameState();
	if (Requester && UOGameState && UOGameState->GetPhase() == EUODayPhase::Evaluation)
	{
		Requester->RestartLevel();
	}
}

void AUOGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStartDayPending)
	{
		bStartDayPending = false;
		StartDay();
		return;
	}

	TickDay(DeltaSeconds);
}

void AUOGameMode::StartDay()
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || !OfficeBuilder)
	{
		return;
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Starting day %d for '%s' (seed %d, %d tasks, %s, AI %s)"), DayNumber, *MatchConfig.CompanyName, ActiveSeed,
		MatchConfig.TasksPerDay, *UOText::GetDayLengthName(MatchConfig.DayLength).ToString(), *UOText::GetDifficultyName(MatchConfig.AIDifficulty).ToString());

	UOGameState->SetCompanyName(MatchConfig.CompanyName);
	UOGameState->SetDayNumber(DayNumber);
	UOGameState->SetUnpaidOvertime(false);
	UOGameState->SetDayForceClosed(false);
	UOGameState->ClearEmployees();

	// Tasks and the office that fits them
	const FUOGeneratedDay Day = FUOTaskGenerator::Generate(ActiveSeed, MatchConfig.TasksPerDay, FUOTaskGenerator::GetDifficultyForDay(DayNumber));
	const TArray<AUOInteractableStation*> Stations = OfficeBuilder->BuildOffice(ActiveSeed, Day.Stations, MatchConfig.CompanyName);
	TaskManager->StartDay(Day.Tasks, Stations);

	HideSetupScreen();

	int32 NextEmployeeId = 0;
	SpawnHumanEmployees(NextEmployeeId);
	SpawnAIEmployees(MatchConfig.GetAIPlayers(), NextEmployeeId);

	DayClock.Start(FUODayClock::GetShiftSecondsWithConsoleOverride(MatchConfig.DayLength));
	UOGameState->SetClock(DayClock.GetElapsedSeconds(), DayClock.GetShiftLengthSeconds());
	UOGameState->SetPhase(EUODayPhase::Shift);
	ApplyOvertimeEffects(false);
}

void AUOGameMode::SpawnHumanEmployees(int32& InOutNextEmployeeId)
{
	AUOGameState* UOGameState = GetUOGameState();
	const FTransform StartTransform = OfficeBuilder->GetPlayerStartTransform();
	int32 HumanIndex = 0;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AUOPlayerState* Employee = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
		if (!Employee)
		{
			continue;
		}

		const FText Name = HumanIndex == 0
			? LOCTEXT("HumanName", "New hire")
			: FText::Format(LOCTEXT("HumanNameN", "New hire #{0}"), FText::AsNumber(HumanIndex + 1));
		Employee->InitEmployee(InOutNextEmployeeId++, Name.ToString(), false, UOGameModePrivate::HumanColor, 1.f);
		UOGameState->AddEmployee(Employee);

		if (APawn* OldPawn = PC->GetPawn())
		{
			PC->UnPossess();
			OldPawn->Destroy();
		}

		// Spread extra humans out a little
		FTransform SpawnTransform = StartTransform;
		SpawnTransform.AddToTranslation(FVector(120.f * HumanIndex, 0.f, 0.f));
		RestartPlayerAtTransform(PC, SpawnTransform);

		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		++HumanIndex;
	}
}

void AUOGameMode::SpawnAIEmployees(int32 Count, int32& InOutNextEmployeeId)
{
	AUOGameState* UOGameState = GetUOGameState();
	UWorld* World = GetWorld();

	// Seeded pick of names and colors
	FRandomStream Rng(ActiveSeed * 7 + 3);
	TArray<int32> NameOrder;
	for (int32 Index = 0; Index < AINames.Num(); ++Index)
	{
		NameOrder.Add(Index);
	}
	for (int32 Index = NameOrder.Num() - 1; Index > 0; --Index)
	{
		NameOrder.Swap(Index, Rng.RandRange(0, Index));
	}

	const FUOAIDifficultyParams Difficulty = FUOAIDifficulty::Compute(MatchConfig.AIDifficulty, DayNumber);
	const TArray<FVector> SpawnLocations = OfficeBuilder->GetAISpawnLocations();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Location = SpawnLocations.Num() > 0 ? SpawnLocations[Index % SpawnLocations.Num()] : FVector(0.f, 0.f, 100.f);
		AUOAIEmployee* AIPawn = World->SpawnActor<AUOAIEmployee>(AUOAIEmployee::StaticClass(), Location, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), SpawnParams);
		if (!AIPawn)
		{
			UE_LOG(LogUnpaidOvertime, Error, TEXT("UOGameMode: failed to spawn AI employee %d"), Index);
			continue;
		}

		if (!AIPawn->GetController())
		{
			AIPawn->SpawnDefaultController();
		}

		AUOAIController* Brain = Cast<AUOAIController>(AIPawn->GetController());
		AUOPlayerState* Employee = AIPawn->GetPlayerState<AUOPlayerState>();
		if (!Brain || !Employee)
		{
			UE_LOG(LogUnpaidOvertime, Error, TEXT("UOGameMode: AI employee %d has no controller or player state"), Index);
			AIPawn->Destroy();
			continue;
		}

		const FString Name = AINames.Num() > 0 ? AINames[NameOrder[Index % NameOrder.Num()]].ToString() : FString::Printf(TEXT("Coworker %d"), Index + 1);
		const FLinearColor Color = AIColors.Num() > 0 ? AIColors[Index % AIColors.Num()] : FLinearColor::White;

		Employee->InitEmployee(InOutNextEmployeeId++, Name, true, Color, Difficulty.WorkSpeedMultiplier);
		AIPawn->SetEmployeeLook(Name, Color);
		Brain->ConfigureEmployee(Difficulty, OfficeBuilder->GetHubLocation(), ActiveSeed + 101 * (Index + 1));

		UOGameState->AddEmployee(Employee);
		AIEmployees.Add(AIPawn);
	}
}

void AUOGameMode::TickDay(float DeltaSeconds)
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || !UOGameState->IsWorkPhase())
	{
		return;
	}

	const EUODayPhase Before = DayClock.GetPhase();
	const EUODayPhase After = DayClock.Advance(DeltaSeconds, TaskManager->AreAllSubObjectivesDone());
	UOGameState->SetClock(DayClock.GetElapsedSeconds(), DayClock.GetShiftLengthSeconds());

	if (Before == After)
	{
		return;
	}

	UOGameState->SetPhase(After);

	if (After == EUODayPhase::Overtime)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Shift over with work pending: OVERTIME (unpaid)."));
		UOGameState->SetUnpaidOvertime(true);
		ApplyOvertimeEffects(true);
	}
	else if (After == EUODayPhase::Evaluation)
	{
		FinishDay();
	}
}

void AUOGameMode::ApplyOvertimeEffects(bool bOvertime)
{
	using namespace UOGameModePrivate;

	if (UDirectionalLightComponent* Sun = SunLight ? Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()) : nullptr)
	{
		Sun->SetIntensity(SunBaseIntensity * (bOvertime ? SunOvertimeScale : 1.f));
		Sun->SetLightColor(bOvertime ? SunOvertimeColor : FLinearColor::White);
		SunLight->SetActorRotation(bOvertime ? SunOvertimeRotation : SunDayRotation);
	}

	if (SkyLight)
	{
		USkyLightComponent* Sky = SkyLight->GetLightComponent();
		Sky->SetIntensity(SkyBaseIntensity * (bOvertime ? SkyOvertimeScale : 1.f));
		Sky->SetLightColor(bOvertime ? SkyOvertimeColor : FLinearColor::White);
	}

	if (OfficeBuilder)
	{
		OfficeBuilder->SetOvertimeMood(bOvertime);
	}
}

void AUOGameMode::FinishDay()
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState)
	{
		return;
	}

	TaskManager->EndDay();
	UOGameState->SetDayForceClosed(DayClock.WasForceClosed());

	const TArray<FUOTaskInstance>& Tasks = UOGameState->GetTasks();
	const int32 Total = FUOTaskBoard::CountTotal(Tasks);
	const int32 Alive = UOGameState->CountAliveEmployees();
	const float FairShare = FUOStarRating::ComputeFairShare(Total, Alive);

	for (AUOPlayerState* Employee : UOGameState->GetEmployees())
	{
		if (!Employee || !Employee->IsAlive())
		{
			continue;
		}

		const int32 Completed = FUOTaskBoard::CountCompletedBy(Tasks, Employee->GetEmployeeId());
		const int32 Stars = FUOStarRating::ComputeStars(Completed, Total, Alive);
		Employee->SetCompletedCount(Completed);
		Employee->SetEvaluation(Stars, FairShare, FUOStarRating::GetConsequence(Stars));

		UE_LOG(LogUnpaidOvertime, Log, TEXT("Evaluation: %s completed %d (fair share %.2f) -> %d stars"), *Employee->GetDisplayName(), Completed, FairShare, Stars);
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Day %d closed at %s (%s). Press R to start a new day."), DayNumber, *UOGameState->GetClockText(),
		DayClock.WasForceClosed() ? TEXT("force-closed") : TEXT("all work done"));
}

#undef LOCTEXT_NAMESPACE
