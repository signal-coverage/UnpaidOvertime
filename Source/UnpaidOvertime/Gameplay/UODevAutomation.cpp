// UNPAID: OVERTIME - dev-only scripted runs for automated in-game verification.

#include "UODevAutomation.h"

#if !UE_BUILD_SHIPPING

#include "UOPlayerController.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOCharacter.h"
#include "UOInteractionComponent.h"
#include "UOInteractableStation.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskBoard.h"
#include "SUOMatchSetupWidget.h"
#include "UOPlayerState.h"
#include "UOEvaluationDirector.h"
#include "UODayCalendar.h"
#include "UOTerminal.h"
#include "UORoleSystemComponent.h"
#include "UOEvidenceSubsystem.h"
#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "UOAIController.h"
#include "UOBodyActor.h"
#include "SUOEvaluationFlowWidget.h"
#include "SUOTerminalWidget.h"
#include "UOMeetingRules.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UnpaidOvertime.h"

namespace UODevAutomationPrivate
{
	constexpr float EyeHeight = 64.f;
	constexpr float CapsuleHalfHeight = 88.f;
}

TSharedPtr<FUODevAutomation> FUODevAutomation::CreateFromCommandLine(AUOPlayerController* Owner)
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FString AutoStartName;
	bool bAutoStart = FParse::Value(CommandLine, TEXT("UOAutoStart="), AutoStartName);
	const bool bShots = FParse::Param(CommandLine, TEXT("UOShots"));
	const bool bEvalTour = FParse::Param(CommandLine, TEXT("UOEvalTour"));
	FString PolicyName;
	bool bPolicy = FParse::Value(CommandLine, TEXT("UOAutoPolicy="), PolicyName);
	FString M4Name;
	const bool bM4 = FParse::Value(CommandLine, TEXT("UOM4="), M4Name);
	if (bM4 && !bPolicy)
	{
		PolicyName = TEXT("win");
		bPolicy = true;
	}
	if (!bAutoStart && !bShots && !bEvalTour && !bPolicy)
	{
		return nullptr;
	}
	bAutoStart = bAutoStart || bEvalTour || bPolicy;

	TSharedPtr<FUODevAutomation> Automation = MakeShared<FUODevAutomation>();
	Automation->Owner = Owner;
	Automation->CompanyName = AutoStartName.IsEmpty() ? TEXT("Acme Synergy") : AutoStartName;
	int32 Tasks = 8;
	if (FParse::Value(CommandLine, TEXT("UOTasks="), Tasks))
	{
		Automation->TasksPerDay = Tasks;
	}
	Automation->ShotDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("UOAuto"));

	if (bShots && !bEvalTour)
	{
		IFileManager::Get().MakeDirectory(*Automation->ShotDirectory, true);
		Automation->BuildShotTour();
	}
	else
	{
		Automation->BuildAutoStart();
	}

	if (bPolicy || bEvalTour)
	{
		PolicyName = PolicyName.IsEmpty() ? FString(TEXT("random")) : PolicyName.ToLower();
		Automation->Policy = PolicyName == TEXT("win") ? EPolicy::Win : (PolicyName == TEXT("lose") ? EPolicy::Lose : EPolicy::Random);
		FParse::Value(CommandLine, TEXT("UOAutoDays="), Automation->MaxDays);
		int32 PolicySeed = 1234;
		FParse::Value(CommandLine, TEXT("UOAutoSeed="), PolicySeed);
		Automation->PolicyRng.Initialize(PolicySeed);
		Automation->bEvalShots = bEvalTour;
		Automation->EvalShotDirectory = Automation->ShotDirectory / TEXT("Eval");
		if (bEvalTour)
		{
			IFileManager::Get().MakeDirectory(*Automation->EvalShotDirectory, true);
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: scripted human, policy '%s', %d day(s)%s."), *PolicyName, Automation->MaxDays, bEvalTour ? TEXT(", evaluation screenshots") : TEXT(""));
	}
	FParse::Value(CommandLine, TEXT("UOSimMatchDays="), Automation->SimMatchDays);
	FParse::Value(CommandLine, TEXT("UOMeetingPolicy="), Automation->MeetingPolicy);
	FParse::Value(CommandLine, TEXT("UOSavingAnswer="), Automation->SavingAnswer);
	FParse::Value(CommandLine, TEXT("UOFakeStars="), Automation->FakeStars);
	if (bM4)
	{
		M4Name = M4Name.ToLower();
		Automation->M4 = M4Name == TEXT("killer") ? EM4::Killer : M4Name == TEXT("kill") ? EM4::Kill : M4Name == TEXT("fakeone") ? EM4::FakeOne
			: M4Name == TEXT("forgery") ? EM4::Forgery : M4Name == TEXT("duelrigged") ? EM4::DuelRigged : M4Name == TEXT("duelfair") ? EM4::DuelFair
			: M4Name == TEXT("takeover") ? EM4::Takeover : EM4::Employee;
		Automation->M4ShotDirectory = Automation->ShotDirectory / TEXT("M4");
		IFileManager::Get().MakeDirectory(*Automation->M4ShotDirectory, true);
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: M4 tour '%s' (screenshots to %s)."), *M4Name, *Automation->M4ShotDirectory);
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: %s for '%s' (%d tasks)."), bShots && !bEvalTour ? TEXT("screenshot tour") : TEXT("auto start"), *Automation->CompanyName, Automation->TasksPerDay);
	return Automation;
}

void FUODevAutomation::Add(const FString& Name, float Delay, TFunction<bool()> Run, float Timeout)
{
	FStep& Step = Steps.AddDefaulted_GetRef();
	Step.Name = Name;
	Step.Delay = Delay;
	Step.Run = MoveTemp(Run);
	Step.Timeout = Timeout;
}

void FUODevAutomation::AddShot(const FString& Name, float Delay)
{
	Add(TEXT("Shot ") + Name, Delay, [this, Name]()
	{
		const FString Path = ShotDirectory / (Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		UE_LOG(LogUnpaidOvertime, Log, TEXT("UOShot: %s"), *Path);
		return true;
	});
}

void FUODevAutomation::Tick(float DeltaSeconds)
{
	if (!Owner.IsValid())
	{
		return;
	}
	if (Policy != EPolicy::None && !bQuitting)
	{
		TickPolicy(FApp::GetDeltaTime());
	}
	if (!Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	// Real time, so the pause menu step keeps the script going
	StepClock += FApp::GetDeltaTime();
	FStep& Step = Steps[StepIndex];
	if (StepClock < Step.Delay)
	{
		return;
	}

	WaitClock += FApp::GetDeltaTime();
	const bool bDone = Step.Run();
	const bool bTimedOut = !bDone && Step.Timeout > 0.f && WaitClock > Step.Timeout;
	if (bDone || bTimedOut)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation step %d '%s' %s."), StepIndex, *Step.Name, bDone ? TEXT("done") : TEXT("timed out"));
		++StepIndex;
		StepClock = 0.f;
		WaitClock = 0.f;
	}
}

bool FUODevAutomation::IsPhase(uint8 Phase) const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
	return GameState && static_cast<uint8>(GameState->GetPhase()) == Phase;
}

void FUODevAutomation::BuildAutoStart()
{
	Add(TEXT("Wait for main menu"), 0.f, [this]() { return Owner->GetScreen() == EUOMenuScreen::MainMenu; }, 30.f);
	Add(TEXT("Start day"), 0.2f, [this]()
	{
		FUOMatchConfig Config;
		Config.CompanyName = CompanyName;
		Config.TasksPerDay = TasksPerDay;
		FString Difficulty;
		if (FParse::Value(FCommandLine::Get(), TEXT("UOAIDifficulty="), Difficulty))
		{
			Config.AIDifficulty = Difficulty.Equals(TEXT("Easy"), ESearchCase::IgnoreCase) ? EUOAIDifficulty::Easy
				: (Difficulty.Equals(TEXT("Hard"), ESearchCase::IgnoreCase) ? EUOAIDifficulty::Hard : EUOAIDifficulty::Medium);
		}
		FString Role;
		if (FParse::Value(FCommandLine::Get(), TEXT("UORole="), Role))
		{
			Config.KillerAssignment = Role.Equals(TEXT("Killer"), ESearchCase::IgnoreCase) ? EUOKillerAssignment::AlwaysHuman : EUOKillerAssignment::AlwaysAI;
		}
		int32 CanKill = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("UOKillerCanKill="), CanKill))
		{
			Config.bKillerCanKill = CanKill != 0;
		}
		int32 Employees = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("UOEmployees="), Employees))
		{
			Config.TeamSize = FMath::Clamp(Employees, FUOMatchConfig::MinTeamSize, FUOMatchConfig::MaxTeamSize);
		}
		// Tour defaults (an explicit switch on the command line wins)
		UUOEvaluationDirector* Director = nullptr;
		AUOGameMode* Mode = Owner->GetWorld()->GetAuthGameMode<AUOGameMode>();
		Director = Mode ? Mode->GetEvaluationDirector() : nullptr;
		auto DefaultPolicy = [this](const TCHAR* Policy) { if (MeetingPolicy.IsEmpty()) { MeetingPolicy = Policy; } };
		auto DefaultSaving = [this](const TCHAR* Answer) { if (SavingAnswer.IsEmpty()) { SavingAnswer = Answer; } };
		switch (M4)
		{
		case EM4::Employee:		Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; break;
		case EM4::Killer:		Config.KillerAssignment = EUOKillerAssignment::AlwaysHuman; break;
		case EM4::Kill:			Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; Config.bKillerCanKill = true; break;
		case EM4::FakeOne:		Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; FakeStars = FakeStars > 0 ? FakeStars : 1; break;
		case EM4::Forgery:		Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; FakeStars = FakeStars > 0 ? FakeStars : 3; DefaultPolicy(TEXT("expel:killer")); DefaultSaving(TEXT("wrong")); break;
		case EM4::DuelRigged:	Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; DefaultPolicy(TEXT("tie:human:killer")); DefaultSaving(TEXT("right")); break;
		case EM4::DuelFair:		Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; DefaultPolicy(TEXT("tie:innocent:innocent2")); DefaultSaving(TEXT("wrong")); break;
		case EM4::Takeover:		Config.KillerAssignment = EUOKillerAssignment::AlwaysHuman; FakeStars = FakeStars > 0 ? FakeStars : 2; DefaultPolicy(TEXT("expel:innocent")); DefaultSaving(TEXT("wrong")); break;
		default:				break;
		}
		int32 Killers = 0;
		if (Mode && FParse::Value(FCommandLine::Get(), TEXT("UOKillers="), Killers))
		{
			Mode->SetDevKillerCount(Killers);
		}
		if (Director)
		{
			int32 KillerWrong = 0;
			FParse::Value(FCommandLine::Get(), TEXT("UOKillerAnswersWrong="), KillerWrong);
			Director->DevKillerStars = FakeStars;
			Director->bDevKillerAnswersWrong = KillerWrong != 0 || M4 == EM4::FakeOne;
			Director->DevSavingAnswer = SavingAnswer;
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: team %d, killer stars %d, killer answers %s, meeting policy '%s', saving answers '%s' [dev overrides]."),
				Config.TeamSize, FakeStars, Director->bDevKillerAnswersWrong ? TEXT("wrong") : TEXT("as usual"), *MeetingPolicy, *SavingAnswer);
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: killer assignment %s, dismissals %s."),
			*StaticEnum<EUOKillerAssignment>()->GetNameStringByValue(static_cast<int64>(Config.KillerAssignment)), Config.bKillerCanKill ? TEXT("on") : TEXT("off"));
		const FUOConfigValidationResult Result = Owner->SubmitMatchConfig(Config);
		if (!Result.bIsValid)
		{
			UE_LOG(LogUnpaidOvertime, Error, TEXT("Dev automation: company rejected: %s"), *Result.GetMessage().ToString());
		}
		return true;
	});
}

void FUODevAutomation::BuildShotTour()
{
	const uint8 Shift = static_cast<uint8>(EUODayPhase::Shift);
	const uint8 Overtime = static_cast<uint8>(EUODayPhase::Overtime);
	const uint8 Evaluation = static_cast<uint8>(EUODayPhase::Evaluation);

	// Front end
	Add(TEXT("Wait for main menu"), 0.f, [this]() { return Owner->GetScreen() == EUOMenuScreen::MainMenu; }, 30.f);
	AddShot(TEXT("01_MainMenu"), 9.f);
	Add(TEXT("Open settings"), 0.5f, [this]() { Owner->OpenSettings(); return true; });
	AddShot(TEXT("02_Settings"), 1.f);
	Add(TEXT("Open setup"), 0.5f, [this]() { Owner->ShowSetup(); return true; });
	Add(TEXT("Fill setup"), 0.5f, [this]()
	{
		if (TSharedPtr<SUOMatchSetupWidget> Setup = Owner->GetSetupWidget())
		{
			Setup->SetCompanyName(CompanyName);
			Setup->GetMutableConfig().TasksPerDay = TasksPerDay;
		}
		return true;
	});
	AddShot(TEXT("03_Setup"), 1.f);
	Add(TEXT("Create company"), 0.5f, [this]()
	{
		if (TSharedPtr<SUOMatchSetupWidget> Setup = Owner->GetSetupWidget())
		{
			Setup->SubmitForAutomation();
		}
		return true;
	});
	Add(TEXT("Wait for shift"), 0.f, [this, Shift]() { return IsPhase(Shift); }, 20.f);

	// Gameplay: coworkers leaving the lobby right after they spawn, then the open plan
	Add(TEXT("Lobby vantage"), 2.f, [this]()
	{
		return LookFrom(FVector(760.f, 230.f, UODevAutomationPrivate::CapsuleHalfHeight + 5.f), FRotator(-6.f, 118.f, 0.f));
	});
	AddShot(TEXT("04_Coworkers_Lobby"), 1.2f);
	AddWalk(TEXT("Walk on the lobby tile and jump (footstep audio)"), 2.4f, true);
	Add(TEXT("Open plan vantage"), 6.f, [this]()
	{
		AUOInteractableStation* Station = FindStation([](const AUOInteractableStation& S)
		{
			return S.GetStationType() == EUOStationType::FilingCabinet && S.GetRoomName().ToString().Contains(TEXT("Open"));
		});
		if (Station)
		{
			const FRotator Back(-6.f, Station->GetActorRotation().Yaw + 180.f + 25.f, 0.f);
			return LookFrom(Station->GetInteractionLocation() - Station->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, UODevAutomationPrivate::CapsuleHalfHeight), Back);
		}
		return true;
	});
	AddShot(TEXT("05_Gameplay_OpenPlan"), 1.5f);
	AddWalk(TEXT("Walk on the open plan carpet (footstep audio)"), 1.6f, false);

	// Interaction: focus a coffee maker (or any station) with open work, then hold
	Add(TEXT("Go to a station with work"), 0.5f, [this]()
	{
		AUOInteractableStation* Station = FindStation([this](const AUOInteractableStation& S)
		{
			return S.GetStationType() == EUOStationType::CoffeeMaker && HasOpenWork(S, false);
		});
		if (!Station)
		{
			Station = FindStation([this](const AUOInteractableStation& S) { return !S.IsItemSource() && HasOpenWork(S, false); });
		}
		return StandAtStation(Station);
	});
	AddShot(TEXT("06_Interact_Prompt"), 0.8f);
	Add(TEXT("Hold interact"), 0.1f, [this]()
	{
		if (AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn()))
		{
			Character->GetInteraction()->BeginInteract();
		}
		return true;
	});
	AddShot(TEXT("07_Interact_Holding"), 1.4f);

	// Carried item: take ink at a supply closet, then face a printer that needs it
	Add(TEXT("Go to supply closet"), 4.f, [this]()
	{
		AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn());
		if (Character)
		{
			Character->GetInteraction()->EndInteract();
		}
		return StandAtStation(FindStation([](const AUOInteractableStation& S) { return S.GetStationType() == EUOStationType::SupplyCloset; }));
	});
	Add(TEXT("Take ink"), 0.6f, [this]()
	{
		if (AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn()))
		{
			Character->GetInteraction()->BeginInteract();
			Character->GetInteraction()->EndInteract();
		}
		return true;
	});
	AddShot(TEXT("08_Carry_Item"), 0.8f);
	Add(TEXT("Go to printer"), 0.5f, [this]()
	{
		AUOInteractableStation* Printer = FindStation([this](const AUOInteractableStation& S) { return S.GetStationType() == EUOStationType::Printer && HasOpenWork(S, true); });
		if (!Printer)
		{
			Printer = FindStation([](const AUOInteractableStation& S) { return S.GetStationType() == EUOStationType::Printer; });
		}
		return StandAtStation(Printer);
	});
	AddShot(TEXT("09_Deliver_Prompt"), 0.8f);

	// Deliver the ink (cartridge click + task chime), then take another cartridge and drop it (drop sound)
	Add(TEXT("Deliver ink"), 0.2f, [this]()
	{
		if (AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn()))
		{
			Character->GetInteraction()->BeginInteract();
		}
		return true;
	});
	Add(TEXT("Wait for delivery"), 0.2f, [this]()
	{
		const AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn());
		return !Character || !Character->GetInteraction()->IsHolding();
	}, 10.f);
	Add(TEXT("Back to supply closet"), 0.8f, [this]()
	{
		if (AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn()))
		{
			Character->GetInteraction()->EndInteract();
		}
		return StandAtStation(FindStation([](const AUOInteractableStation& S) { return S.GetStationType() == EUOStationType::SupplyCloset; }));
	});
	Add(TEXT("Take and drop ink"), 0.6f, [this]()
	{
		if (AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn()))
		{
			Character->GetInteraction()->BeginInteract();
			Character->GetInteraction()->EndInteract();
			Character->GetInteraction()->DropItem();
		}
		return true;
	});

	Add(TEXT("Pause"), 0.5f, [this]() { Owner->OpenPauseMenu(); return true; });
	AddShot(TEXT("10_Pause"), 1.f);
	Add(TEXT("Resume"), 0.5f, [this]() { Owner->ClosePauseMenu(); return true; });

	// Overtime (needs a short shift: -ExecCmds="uo.ShiftSecondsOverride 70")
	Add(TEXT("Wait for overtime"), 0.f, [this, Overtime, Evaluation]() { return IsPhase(Overtime) || IsPhase(Evaluation); }, 240.f);
	Add(TEXT("Overtime vantage"), 5.f, [this]()
	{
		AUOInteractableStation* Station = FindStation([](const AUOInteractableStation& S)
		{
			return S.GetStationType() == EUOStationType::FilingCabinet && S.GetRoomName().ToString().Contains(TEXT("Open"));
		});
		if (Station)
		{
			const FRotator Back(-6.f, Station->GetActorRotation().Yaw + 180.f - 20.f, 0.f);
			return LookFrom(Station->GetInteractionLocation() - Station->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, UODevAutomationPrivate::CapsuleHalfHeight), Back);
		}
		return true;
	});
	AddShot(TEXT("11_Overtime"), 1.f);

	Add(TEXT("Wait for evaluation"), 0.f, [this, Evaluation]() { return IsPhase(Evaluation); }, 240.f);
	AddShot(TEXT("12_Evaluation"), 2.f);
	Add(TEXT("Quit"), 1.5f, [this]()
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: tour finished, quitting."));
		Owner->ConsoleCommand(TEXT("quit"));
		return true;
	});
}

void FUODevAutomation::AddWalk(const FString& Name, float Seconds, bool bJump)
{
	TSharedRef<float> Elapsed = MakeShared<float>(0.f);
	TSharedRef<bool> bJumped = MakeShared<bool>(false);
	Add(Name, 0.3f, [this, Seconds, bJump, Elapsed, bJumped]()
	{
		AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn());
		if (!Character)
		{
			return true;
		}
		*Elapsed += FApp::GetDeltaTime();
		Character->AddMovementInput(Character->GetActorForwardVector(), 1.f);
		if (bJump && !*bJumped && *Elapsed > Seconds * 0.45f)
		{
			*bJumped = true;
			Character->Jump();
		}
		return *Elapsed >= Seconds;
	}, Seconds + 5.f);
}

AUOInteractableStation* FUODevAutomation::FindStation(TFunctionRef<bool(const AUOInteractableStation&)> Predicate) const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameMode* GameMode = PC ? PC->GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	const UUOTaskManagerComponent* TaskManager = GameMode ? GameMode->GetTaskManager() : nullptr;
	if (!TaskManager)
	{
		return nullptr;
	}
	for (AUOInteractableStation* Station : TaskManager->GetStations())
	{
		if (Station && Predicate(*Station))
		{
			return Station;
		}
	}
	return nullptr;
}

bool FUODevAutomation::HasOpenWork(const AUOInteractableStation& Station, bool bWantItem) const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
	if (!GameState)
	{
		return false;
	}
	for (const FUOTaskInstance& Task : GameState->GetTasks())
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (Sub.StationId == Station.GetStationId() && Sub.State == EUOSubObjectiveState::Open && Sub.bRequiresItem == bWantItem)
			{
				return true;
			}
		}
	}
	return false;
}

bool FUODevAutomation::StandAtStation(AUOInteractableStation* Station, float BackOff)
{
	if (!Station)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("Dev automation: no suitable station found."));
		return true;
	}

	const FVector Stand = Station->GetInteractionLocation() - Station->GetActorForwardVector() * BackOff + FVector(0.f, 0.f, UODevAutomationPrivate::CapsuleHalfHeight + 2.f);
	const FVector Eye = Stand + FVector(0.f, 0.f, UODevAutomationPrivate::EyeHeight);
	const FRotator Look = (Station->GetFocusLocation() - Eye).Rotation();
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: standing at %s"), *Station->GetDisplayName().ToString());
	return LookFrom(Stand, FRotator(Look.Pitch, Look.Yaw, 0.f));
}

bool FUODevAutomation::LookFrom(const FVector& Location, const FRotator& Rotation)
{
	AUOPlayerController* PC = Owner.Get();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return true;
	}
	Pawn->TeleportTo(Location, FRotator(0.f, Rotation.Yaw, 0.f), false, true);
	PC->SetControlRotation(Rotation);
	return true;
}

// ~ Scripted human

void FUODevAutomation::Quit(const FString& Why)
{
	if (bQuitting)
	{
		return;
	}
	bQuitting = true;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: quitting (%s)."), *Why);
	Owner->ConsoleCommand(TEXT("quit"));
}

void FUODevAutomation::EvalShot(const FString& Name)
{
	if (!bEvalShots || ShotsTaken.Contains(Name))
	{
		return;
	}
	ShotsTaken.Add(Name);
	const FString Path = EvalShotDirectory / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOShot: %s"), *Path);
}

void FUODevAutomation::OnDayStarted(int32 Day)
{
	++DaysStarted;
	ShiftClock = 0.f;
	switch (Policy)
	{
	case EPolicy::Win:	WorkTarget = 999; break;
	case EPolicy::Lose:	WorkTarget = 0; break;
	default:
	{
		// 0 (fired), 1 (likely 1 star: defense + cut vote), 2-3 (2 stars) or everything (3 stars)
		static const int32 Targets[] = { 1, 1, 2, 3, 999, 999, 0 };
		WorkTarget = Targets[PolicyRng.RandRange(0, UE_ARRAY_COUNT(Targets) - 1)];
		if (Day == 1 && WorkTarget == 0)
		{
			WorkTarget = 1;
		}
		break;
	}
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: %s started (%d of %d days played), the scripted human will complete %s sub-objective(s)."),
		*FUODayCalendar::FormatDayName(Day).ToString(), DaysStarted, MaxDays, WorkTarget >= 999 ? TEXT("as many") : *FString::FromInt(WorkTarget));
}

void FUODevAutomation::TickPolicy(float DeltaSeconds)
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC->GetWorld()->GetGameState<AUOGameState>();
	if (!GameState || GameState->GetPhase() == EUODayPhase::PreGame)
	{
		return;
	}

	// A new day (or a new hire's day 1)
	if (GameState->IsWorkPhase() && (GameState->GetDayNumber() != LastDay || ShiftClock < 0.f))
	{
		LastDay = GameState->GetDayNumber();
		if (DaysStarted >= MaxDays)
		{
			Quit(FString::Printf(TEXT("%d day(s) played"), DaysStarted));
			return;
		}
		OnDayStarted(LastDay);
	}

	// Outside the M4 tours nobody needs a picture of the role card: clock in right away
	if (M4 == EM4::None && Owner->GetScreen() == EUOMenuScreen::RoleReveal)
	{
		Owner->DismissRoleReveal();
	}

	if (GameState->IsWorkPhase())
	{
		ShiftClock += DeltaSeconds;
		if (ShiftClock > 4.f)
		{
			EvalShot(FString::Printf(TEXT("D%d_00_HUD"), LastDay));
		}
		if (M4 != EM4::None)
		{
			TickM4(DeltaSeconds);
		}
		if (!bM4Busy)
		{
			TickWorker(DeltaSeconds);
		}
	}
	else if (GameState->GetPhase() == EUODayPhase::Evaluation || GameState->GetPhase() == EUODayPhase::Meeting)
	{
		TickEvaluation(DeltaSeconds);
	}
}

void FUODevAutomation::TickWorker(float DeltaSeconds)
{
	AUOCharacter* Character = Cast<AUOCharacter>(Owner->GetPawn());
	const AUOPlayerState* Me = Owner->GetPlayerState<AUOPlayerState>();
	if (!Character || !Me || !Me->IsAlive())
	{
		return;
	}
	UUOInteractionComponent* Interaction = Character->GetInteraction();

	if (Interaction->IsHolding())
	{
		HoldClock += DeltaSeconds;
		bWasHolding = true;
		if (HoldClock > 15.f)
		{
			Interaction->EndInteract();
		}
		return;
	}
	if (bWasHolding)
	{
		bWasHolding = false;
		Interaction->EndInteract();
		WorkCooldown = 0.6f;
	}

	if (AUOInteractableStation* Pending = PendingStation.Get())
	{
		PendingClock -= DeltaSeconds;
		if (PendingClock <= 0.f)
		{
			PendingStation.Reset();
			HoldClock = 0.f;
			Interaction->BeginInteract(Pending);
			if (!Interaction->IsHolding())
			{
				// Instant interactions (item pickups) complete on press
				Interaction->EndInteract();
			}
		}
		return;
	}

	WorkCooldown -= DeltaSeconds;
	if (WorkCooldown > 0.f || Me->GetCompletedCount() >= WorkTarget)
	{
		return;
	}
	WorkCooldown = 0.5f;

	const AUOGameState* GameState = Owner->GetWorld()->GetGameState<AUOGameState>();
	auto FindOpenSub = [GameState](TFunctionRef<bool(const FUOSubObjective&)> Predicate) -> const FUOSubObjective*
	{
		for (const FUOTaskInstance& Task : GameState->GetTasks())
		{
			for (const FUOSubObjective& Sub : Task.SubObjectives)
			{
				if (Sub.State == EUOSubObjectiveState::Open && Predicate(Sub))
				{
					return &Sub;
				}
			}
		}
		return nullptr;
	};

	AUOInteractableStation* Target = nullptr;
	if (Me->IsCarryingItem())
	{
		const EUOItemType Item = Me->GetCarriedItem();
		if (const FUOSubObjective* Sub = FindOpenSub([Item](const FUOSubObjective& S) { return S.bRequiresItem && S.ItemType == Item; }))
		{
			const FName StationId = Sub->StationId;
			Target = FindStation([StationId](const AUOInteractableStation& S) { return S.GetStationId() == StationId; });
		}
		if (!Target)
		{
			Interaction->DropItem();
			return;
		}
	}
	else if (const FUOSubObjective* Work = FindOpenSub([](const FUOSubObjective& S) { return !S.bRequiresItem; }))
	{
		const FName StationId = Work->StationId;
		Target = FindStation([StationId](const AUOInteractableStation& S) { return S.GetStationId() == StationId; });
	}
	else if (const FUOSubObjective* Delivery = FindOpenSub([](const FUOSubObjective& S) { return S.bRequiresItem; }))
	{
		const EUOItemType Item = Delivery->ItemType;
		Target = FindStation([Item](const AUOInteractableStation& S) { return S.GetProvidedItem() == Item; });
	}

	if (Target)
	{
		StandAtStation(Target);
		PendingStation = Target;
		PendingClock = 0.3f;
	}
}

void FUODevAutomation::TickEvaluation(float DeltaSeconds)
{
	AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC->GetWorld()->GetGameState<AUOGameState>();
	const AUOPlayerState* Me = PC->GetPlayerState<AUOPlayerState>();
	const AUOGameMode* GameMode = PC->GetWorld()->GetAuthGameMode<AUOGameMode>();
	if (!GameState || !Me || !GameMode)
	{
		return;
	}

	const FUOEvaluationState& State = GameState->GetEvaluation();
	const int32 Day = GameState->GetDayNumber();
	const FString D = FString::Printf(TEXT("D%d_"), Day);
	const bool bMine = State.Prompt != EUOEvalPrompt::None && (State.Prompt == EUOEvalPrompt::MatchOverChoice || State.PromptEmployeeId == Me->GetEmployeeId());

	// M4 screens that need no input
	if (M4 != EM4::None)
	{
		if (State.Step == EUOEvalStep::Review)
		{
			M4Shot(FString::Printf(TEXT("M4_D%d_Review"), Day));
		}
		// A killer's always-accepted public answer (AI killers: resolved card)
		if ((State.Step == EUOEvalStep::Defense || State.Step == EUOEvalStep::Promotion) && State.Question.bActive && State.Question.IsResolved() && IsKillerForDev(State.Question.AskedEmployeeId))
		{
			M4Shot(State.Step == EUOEvalStep::Defense ? TEXT("M41_FakeOne_AcceptedAnswer") : TEXT("M41_FakeShield_AcceptedPromotion"));
		}
		if (State.Step == EUOEvalStep::Outcome)
		{
			M4Shot(FString::Printf(TEXT("M4_D%d_Outcome"), Day));
		}
		if (State.Step == EUOEvalStep::MatchOver)
		{
			M4Shot(State.bKillersTakeOver ? TEXT("M41_Takeover") : TEXT("M41_MatchOver"));
		}
		TickMeeting(DeltaSeconds);
		// The killer's private fake rating
		if (State.Step == EUOEvalStep::Rating && Me->GetKillerView().bFakeRatingPending)
		{
			PromptClock += DeltaSeconds;
			if (PromptClock > 1.2f)
			{
				M4Shot(TEXT("M4_12_FakeRating"));
			}
			if (PromptClock > 2.2f)
			{
				const TArray<int32>& Allowed = Me->GetKillerView().FakeAllowed;
				const int32 Pick = FakeStars > 0 && Allowed.Contains(FakeStars) ? FakeStars : (Allowed.Contains(3) ? 3 : Me->GetKillerView().FakeDefaultStars);
				FUOEvalChoice Choice = FUOEvalChoice::Make(EUOEvalChoiceType::FakeRating, Pick);
				const bool bAccepted = PC->SubmitEvaluationChoice(Choice);
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: fake rating %d star(s) %s."), Pick, bAccepted ? TEXT("accepted") : TEXT("REJECTED"));
				PromptClock = 0.f;
			}
			return;
		}
	}

	// Screens that need no input
	if (State.Question.bActive && State.Question.AskedEmployeeId != Me->GetEmployeeId())
	{
		EvalShot(D + (State.Question.IsResolved() ? TEXT("05_AIAnswered") : TEXT("05_AIQuestion")));
	}
	if (State.Question.bActive && State.Question.AskedEmployeeId == Me->GetEmployeeId() && State.Question.IsResolved())
	{
		EvalShot(D + TEXT("04_BossAnswer"));
	}
	if (State.Step == EUOEvalStep::CutVote && State.Votes.Num() >= 2 && State.PendingVotes > 0)
	{
		EvalShot(D + TEXT("06_CutVote_Revealing"));
	}
	if ((State.Step == EUOEvalStep::CutVote || State.Step == EUOEvalStep::Tiebreak) && State.bTallyFinal)
	{
		EvalShot(D + TEXT("06_CutVote_Tally"));
	}
	if (State.Step == EUOEvalStep::Tiebreak && State.Question.bActive)
	{
		EvalShot(D + TEXT("06_Tiebreak"));
	}

	if (!bMine)
	{
		ActedRevision = INDEX_NONE;
		PromptClock = 0.f;
		return;
	}

	// A new prompt: wait a moment (for the screenshot and to look human), then act once
	if (ActedRevision == State.Revision)
	{
		return;
	}
	PromptClock += DeltaSeconds;
	const float Delay = State.Prompt == EUOEvalPrompt::AnswerQuestion ? 2.4f : 1.6f;
	if (PromptClock < Delay * 0.5f)
	{
		return;
	}

	switch (State.Prompt)
	{
	case EUOEvalPrompt::Continue:
		EvalShot(D + (State.Step == EUOEvalStep::Review ? TEXT("01_Review") : (State.Step == EUOEvalStep::Firings ? TEXT("02_Firings") : TEXT("07_Outcome"))));
		if (State.Step == EUOEvalStep::EmergencyMeeting)
		{
			M4Shot(TEXT("M4_10_EmergencyMeeting"));
		}
		if (State.Step == EUOEvalStep::Verdict)
		{
			M4Shot(State.Meeting.bProbation ? TEXT("M41_Verdict_Probation") : (State.Meeting.ExpelledId != INDEX_NONE ? TEXT("M41_Verdict_RoleReveal") : TEXT("M41_Verdict")));
		}
		break;
	case EUOEvalPrompt::OfferPromotion:		EvalShot(D + TEXT("03_PromotionOffer")); break;
	case EUOEvalPrompt::OfferDefense:		EvalShot(D + TEXT("03_DefenseOffer")); break;
	case EUOEvalPrompt::AnswerQuestion:		EvalShot(D + (State.Step == EUOEvalStep::Tiebreak ? TEXT("06_TiebreakMine") : TEXT("04_BossQuestion"))); break;
	case EUOEvalPrompt::CastVote:			EvalShot(D + TEXT("06_CutVote_MyVote")); break;
	case EUOEvalPrompt::MatchOverChoice:	EvalShot(State.MatchResult == EUOMatchResult::Victory ? FString::Printf(TEXT("Victory_Day%d"), Day) : FString::Printf(TEXT("GameOver_Day%d"), Day)); break;
	default: break;
	}
	if (PromptClock < Delay)
	{
		return;
	}

	ActedRevision = State.Revision;
	PromptClock = 0.f;

	// Balance runs: end the match once the killer is gone (or after N days) and hire again
	if (SimMatchDays > 0 && State.Prompt == EUOEvalPrompt::Continue && State.Step == EUOEvalStep::Outcome)
	{
		AUOGameMode* Mode = PC->GetWorld()->GetAuthGameMode<AUOGameMode>();
		const UUORoleSystemComponent* Roles = Mode ? Mode->GetRoleSystem() : nullptr;
		const bool bKillerGone = Roles && !Roles->IsAnyKillerActive();
		if (Mode && (bKillerGone || Day >= SimMatchDays) && !State.bOutcomeEndsMatch)
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Sim: match result after day %d: %s (%d alive)."), Day,
				bKillerGone ? TEXT("KILLERS REMOVED") : TEXT("KILLERS SURVIVED"), GameState->CountAliveEmployees());
			if (DaysStarted >= MaxDays)
			{
				Quit(TEXT("sim days played"));
				return;
			}
			ShiftClock = -1.f;
			Mode->RequestNewHire();
			return;
		}
	}

	FUOEvalChoice Choice;
	switch (State.Prompt)
	{
	case EUOEvalPrompt::Continue:
		Choice = FUOEvalChoice::Make(EUOEvalChoiceType::Continue);
		break;
	case EUOEvalPrompt::OfferPromotion:
	case EUOEvalPrompt::OfferDefense:
		Choice = FUOEvalChoice::Make(Policy == EPolicy::Lose || (Policy == EPolicy::Random && PolicyRng.FRand() < 0.15f)
			// The rigged-duel tour needs the human without a shield (so the saving throw goes to the boss question)
			|| (M4 == EM4::DuelRigged && State.Prompt == EUOEvalPrompt::OfferPromotion) ? EUOEvalChoiceType::DeclineOffer : EUOEvalChoiceType::AcceptOffer);
		break;
	case EUOEvalPrompt::AnswerQuestion:
	{
		const int32 Correct = GameMode->GetEvaluationDirector()->GetCorrectIndexForDev();
		const int32 Count = State.Question.Options.Num();
		bool bRight = Policy == EPolicy::Win || (Policy == EPolicy::Random && PolicyRng.FRand() < 0.6f);
		if (State.Step == EUOEvalStep::SavingThrow && !SavingAnswer.IsEmpty())
		{
			bRight = SavingAnswer == TEXT("right");
		}
		// The scripted human killer answers wrong on purpose: the boss accepts it anyway (rule 4)
		if (IsKillerForDev(Me->GetEmployeeId()) && State.Step != EUOEvalStep::SavingThrow && State.Step != EUOEvalStep::Tiebreak && M4 != EM4::None)
		{
			bRight = false;
		}
		const int32 Wrong = Count > 1 ? (Correct + 1 + PolicyRng.RandRange(0, Count - 2)) % Count : Correct;
		Choice = FUOEvalChoice::Make(EUOEvalChoiceType::Answer, bRight ? Correct : Wrong);
		break;
	}
	case EUOEvalPrompt::CastVote:
	{
		int32 Pick = INDEX_NONE;
		float Lowest = TNumericLimits<float>::Max();
		TArray<int32> Targets;
		for (const int32 Candidate : State.Candidates)
		{
			const AUOPlayerState* Employee = GameState->FindEmployee(Candidate);
			if (Candidate == Me->GetEmployeeId() || !Employee)
			{
				continue;
			}
			Targets.Add(Candidate);
			if (Employee->GetContributionRatio() < Lowest)
			{
				Lowest = Employee->GetContributionRatio();
				Pick = Candidate;
			}
		}
		if (Policy != EPolicy::Win && Targets.Num() > 0)
		{
			Pick = Targets[PolicyRng.RandRange(0, Targets.Num() - 1)];
		}
		Choice = FUOEvalChoice::Make(EUOEvalChoiceType::Vote, Pick);
		break;
	}
	case EUOEvalPrompt::MatchOverChoice:
		if (SimMatchDays > 0)
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Sim: match result after day %d: MATCH OVER (%s)."), Day,
				State.MatchResult == EUOMatchResult::Victory ? TEXT("human last standing") : TEXT("human out"));
		}
		if (DaysStarted >= MaxDays)
		{
			Quit(TEXT("match over"));
			return;
		}
		// Keep playing: a new hire starts at day 1 again
		ShiftClock = -1.f;
		Choice = FUOEvalChoice::Make(EUOEvalChoiceType::NewHire);
		break;
	default:
		return;
	}

	const bool bAccepted = PC->SubmitEvaluationChoice(Choice);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: evaluation choice %s (%d) %s."), *StaticEnum<EUOEvalChoiceType>()->GetNameStringByValue(static_cast<int64>(Choice.Type)),
		Choice.Value, bAccepted ? TEXT("accepted") : TEXT("REJECTED"));
}

// ~ M4 tours

void FUODevAutomation::M4Shot(const FString& Name)
{
	if (M4 == EM4::None || ShotsTaken.Contains(Name))
	{
		return;
	}
	ShotsTaken.Add(Name);
	const FString Path = M4ShotDirectory / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOShot: %s"), *Path);
}

TArray<int32> FUODevAutomation::GetKillerIdsForDev() const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameMode* GameMode = PC ? PC->GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	return GameMode ? GameMode->GetRoleSystem()->GetKillerIdsForAudit() : TArray<int32>();
}

bool FUODevAutomation::IsKillerForDev(int32 EmployeeId) const
{
	return GetKillerIdsForDev().Contains(EmployeeId);
}

int32 FUODevAutomation::GetKillerIdForDev() const
{
	// The first killer still employed (dev audit peek)
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
	for (const int32 Id : GetKillerIdsForDev())
	{
		const AUOPlayerState* Killer = GameState ? GameState->FindEmployee(Id) : nullptr;
		if (Killer && Killer->IsAlive())
		{
			return Id;
		}
	}
	return INDEX_NONE;
}

int32 FUODevAutomation::ResolvePolicyId(const FString& Token) const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	if (Token.IsNumeric())
	{
		return FCString::Atoi(*Token);
	}
	if (Token == TEXT("human"))
	{
		return Me ? Me->GetEmployeeId() : INDEX_NONE;
	}
	if (Token == TEXT("killer"))
	{
		// An AI killer if there is one
		for (const int32 Id : GetKillerIdsForDev())
		{
			const AUOPlayerState* Killer = GameState ? GameState->FindEmployee(Id) : nullptr;
			if (Killer && Killer->IsAlive() && Killer->IsAIEmployee())
			{
				return Id;
			}
		}
		return GetKillerIdForDev();
	}
	if (Token.StartsWith(TEXT("innocent")) && GameState)
	{
		const int32 Skip = Token == TEXT("innocent2") ? 1 : 0;
		int32 Seen = 0;
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && Employee->IsAlive() && Employee->IsAIEmployee() && !IsKillerForDev(Employee->GetEmployeeId()) && Seen++ == Skip)
			{
				return Employee->GetEmployeeId();
			}
		}
	}
	return INDEX_NONE;
}

void FUODevAutomation::ApplyMeetingPolicy()
{
	AUOPlayerController* PC = Owner.Get();
	AUOGameMode* GameMode = PC ? PC->GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	UUOEvaluationDirector* Director = GameMode ? GameMode->GetEvaluationDirector() : nullptr;
	HumanTieTarget = INDEX_NONE;
	if (!Director || MeetingPolicy.IsEmpty())
	{
		return;
	}
	TArray<FString> Parts;
	MeetingPolicy.ParseIntoArray(Parts, TEXT(":"));
	FString Resolved = MeetingPolicy;
	if (Parts.Num() == 3 && Parts[0] == TEXT("tie"))
	{
		const int32 A = ResolvePolicyId(Parts[1]);
		const int32 B = ResolvePolicyId(Parts[2]);
		Resolved = FString::Printf(TEXT("tie:%d:%d"), A, B);
		HumanTieTarget = A;
	}
	else if (Parts.Num() == 2 && Parts[0] == TEXT("expel"))
	{
		Resolved = FString::Printf(TEXT("expel:%d"), ResolvePolicyId(Parts[1]));
	}
	Director->DevMeetingPolicy = Resolved;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: meeting policy '%s' -> '%s' [dev override, logged]."), *MeetingPolicy, *Resolved);
}

void FUODevAutomation::TickMeeting(float DeltaSeconds)
{
	AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC->GetWorld()->GetGameState<AUOGameState>();
	const AUOPlayerState* Me = PC->GetPlayerState<AUOPlayerState>();
	if (!GameState || !Me)
	{
		return;
	}
	const FUOEvaluationState& State = GameState->GetEvaluation();
	const FUOMeetingView& Meeting = State.Meeting;
	const bool bMeetingStep = State.Step == EUOEvalStep::Debate || State.Step == EUOEvalStep::MeetingVote || State.Step == EUOEvalStep::Duel
		|| State.Step == EUOEvalStep::SavingThrow || State.Step == EUOEvalStep::Verdict;
	if (!bMeetingStep)
	{
		bMeetingPolicyApplied = false;
		MeetingPresented = 0;
		MeetingVoteStep = 0;
		return;
	}
	if (!bMeetingPolicyApplied)
	{
		bMeetingPolicyApplied = true;
		MeetingClock = 0.f;
		ApplyMeetingPolicy();
	}
	MeetingClock += DeltaSeconds;
	const int32 MyId = Me->GetEmployeeId();

	switch (State.Step)
	{
	case EUOEvalStep::Debate:
	{
		if (Meeting.Statements.Num() >= 2 && MeetingClock > 12.f)
		{
			M4Shot(TEXT("M41_Debate"));
		}
		// Put the pinned lines on the table, one every couple of seconds
		const TArray<int32>& Pins = Me->GetPinnedTraceIds();
		if (State.Prompt == EUOEvalPrompt::Debate && MeetingClock > 3.f + 2.f * MeetingPresented && MeetingPresented < FMath::Min(Pins.Num(), FUOMeetingTuning::MaxPresentedEvidence))
		{
			const int32 TraceId = Pins[MeetingPresented++];
			const bool bOk = PC->SubmitEvaluationChoice(FUOEvalChoice::Make(EUOEvalChoiceType::PresentEvidence, TraceId));
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: the scripted human presents trace #%d %s."), TraceId, bOk ? TEXT("(accepted)") : TEXT("(REJECTED)"));
		}
		if (MeetingPresented > 0 && MeetingClock > 3.f + 2.f * MeetingPresented)
		{
			M4Shot(TEXT("M41_Debate_Presented"));
		}
		// Skip once the room has spoken (the remaining statements are posted at once)
		if (State.Prompt == EUOEvalPrompt::Debate && MeetingClock > 22.f && Meeting.Statements.Num() >= 2 && ShotsTaken.Contains(TEXT("M41_Debate")))
		{
			PC->SubmitEvaluationChoice(FUOEvalChoice::Make(EUOEvalChoiceType::SkipDebate));
		}
		break;
	}
	case EUOEvalStep::MeetingVote:
	{
		if (Meeting.bTallyFinal)
		{
			M4Shot(TEXT("M41_Vote_Final"));
			break;
		}
		if (State.Prompt != EUOEvalPrompt::MeetingVote)
		{
			break;
		}
		const float VoteTime = GameState->GetServerWorldTimeSeconds() - Meeting.PhaseStartServerTime;
		TArray<int32> Others;
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && Employee->IsAlive() && Employee->GetEmployeeId() != MyId)
			{
				Others.Add(Employee->GetEmployeeId());
			}
		}
		const int32 Killer = GetKillerIdForDev();
		const bool bHumanKiller = IsKillerForDev(MyId);
		// The final pick: the policy's, else (an employee) the killer by the dev audit peek, else (a killer) abstain
		int32 Final = bHumanKiller ? INDEX_NONE : Killer;
		if (MeetingPolicy.StartsWith(TEXT("tie")))
		{
			// The tied pair accuse each other, everyone else abstains (see UUOEvaluationDirector::GetDevBallot)
			const TArray<FString> Parts = [this]() { TArray<FString> P; MeetingPolicy.ParseIntoArray(P, TEXT(":")); return P; }();
			const int32 A = Parts.Num() == 3 ? ResolvePolicyId(Parts[1]) : INDEX_NONE;
			const int32 B = Parts.Num() == 3 ? ResolvePolicyId(Parts[2]) : INDEX_NONE;
			Final = MyId == A ? B : (MyId == B ? A : INDEX_NONE);
		}
		else if (MeetingPolicy.StartsWith(TEXT("expel")))
		{
			const TArray<FString> Parts = [this]() { TArray<FString> P; MeetingPolicy.ParseIntoArray(P, TEXT(":")); return P; }();
			Final = Parts.Num() == 2 ? ResolvePolicyId(Parts[1]) : INDEX_NONE;
			Final = Final == MyId ? INDEX_NONE : Final;
		}
		else if (MeetingPolicy == TEXT("abstain"))
		{
			Final = INDEX_NONE;
		}
		// Show a reassignment: first someone else, then the final pick
		const int32 Decoy = Others.ContainsByPredicate([Final](int32 Id) { return Id != Final; }) ? *Others.FindByPredicate([Final](int32 Id) { return Id != Final; }) : INDEX_NONE;
		if (MeetingVoteStep == 0 && VoteTime > 2.f)
		{
			MeetingVoteStep = 1;
			PC->SubmitEvaluationChoice(FUOEvalChoice::Make(EUOEvalChoiceType::MeetingVote, Decoy));
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: the scripted human votes for %s first."), *GameState->GetEmployeeName(Decoy));
		}
		else if (MeetingVoteStep == 1 && VoteTime > 4.f)
		{
			M4Shot(TEXT("M41_Vote_First"));
			MeetingVoteStep = 2;
		}
		else if (MeetingVoteStep == 2 && VoteTime > 6.5f)
		{
			MeetingVoteStep = 3;
			PC->SubmitEvaluationChoice(FUOEvalChoice::Make(EUOEvalChoiceType::MeetingVote, Final));
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: the scripted human reassigns the vote to %s."), Final == INDEX_NONE ? TEXT("abstain") : *GameState->GetEmployeeName(Final));
		}
		else if (MeetingVoteStep == 3 && VoteTime > 8.f)
		{
			M4Shot(TEXT("M41_Vote_Reassigned"));
			MeetingVoteStep = 4;
		}
		else if (MeetingVoteStep == 4 && VoteTime > 13.5f)
		{
			// Rebalance a tie at the last moment (the AIs reconsider around 10-13.5 s)
			if (MeetingPolicy.StartsWith(TEXT("tie")))
			{
				PC->SubmitEvaluationChoice(FUOEvalChoice::Make(EUOEvalChoiceType::MeetingVote, Final));
			}
			MeetingVoteStep = 5;
		}
		break;
	}
	case EUOEvalStep::Duel:
	{
		const int32 Rounds = Meeting.Duel.Rounds.Num();
		if (Rounds >= 1 && !Meeting.Duel.bPicking)
		{
			M4Shot(Meeting.Duel.LoserId != INDEX_NONE ? TEXT("M41_Duel_Final") : TEXT("M41_Duel_Round1"));
		}
		if (State.Prompt == EUOEvalPrompt::DuelPick && Meeting.Duel.bPicking)
		{
			if (MeetingClock > 0.8f)
			{
				// Not on the first frames of the rebuilt screen
				M4Shot(TEXT("M41_Duel_Pick"));
			}
			if (MeetingClock > 1.6f)
			{
				const int32 Pick = 1 + PolicyRng.RandRange(0, 2);
				PC->SubmitEvaluationChoice(FUOEvalChoice::Make(EUOEvalChoiceType::DuelPick, Pick));
				MeetingClock = 0.f;
			}
		}
		break;
	}
	case EUOEvalStep::SavingThrow:
		if (Meeting.ShieldCheck == EUOShieldCheck::Forgery)
		{
			M4Shot(TEXT("M41_Forgery"));
		}
		if (State.Question.bActive)
		{
			M4Shot(State.Question.IsResolved() ? TEXT("M41_SavingThrow_Answered") : TEXT("M41_SavingThrow_Question"));
		}
		break;
	default:
		break;
	}
}

int32 FUODevAutomation::CountSabotageTraces() const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOGameState* GameState = PC ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
	int32 Count = 0;
	if (GameState)
	{
		for (const FUOTraceEntry& Entry : GameState->GetTraceLog())
		{
			Count += FUOSuspicionModel::IsSabotageTrace(Entry.Kind) ? 1 : 0;
		}
	}
	return Count;
}

int32 FUODevAutomation::M4PinTrueEvidence(bool bAllowPresence)
{
	AUOPlayerController* PC = Owner.Get();
	TSharedPtr<SUOTerminalWidget> Terminal = PC ? PC->GetTerminalWidget() : nullptr;
	const UUOEvidenceSubsystem* Evidence = PC ? UUOEvidenceSubsystem::Get(PC) : nullptr;
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	if (!Terminal.IsValid() || !Evidence || !Me)
	{
		return 0;
	}
	const int32 Killer = GetKillerIdForDev();
	const int32 Room = FMath::Max(0, FUOMeetingTuning::MaxPresentedEvidence - Me->GetPinnedTraceIds().Num());
	if (Room == 0)
	{
		return 0;
	}
	// Dev only: the audit says who really did it, so the tour pins true evidence
	return Terminal->PinWhere([Evidence, Killer, bAllowPresence](const FUOTraceEntry& Entry)
	{
		const bool bAct = FUOSuspicionModel::IsSabotageTrace(Entry.Kind);
		if (bAct)
		{
			return Evidence->GetTrueActor(Entry.Id) == Killer;
		}
		return bAllowPresence && (Entry.Kind == EUOTraceKind::DoorEntry || Entry.Kind == EUOTraceKind::CameraSighting) && Entry.Mentions(Killer);
	}, Room);
}

bool FUODevAutomation::M4StandForSabotage(EUOSabotageType Type)
{
	AUOPlayerController* PC = Owner.Get();
	AUOGameMode* GameMode = PC ? PC->GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	if (!GameMode || !Me)
	{
		return false;
	}
	const UUORoleSystemComponent* Roles = GameMode->GetRoleSystem();
	TArray<AUOInteractableStation*> Targets;
	for (AUOInteractableStation* Station : GameMode->GetLevelStations())
	{
		Targets.Add(Station);
	}
	for (AUOTerminal* Terminal : GameMode->GetTerminals())
	{
		Targets.Add(Terminal);
	}
	for (AUOInteractableStation* Target : Targets)
	{
		const FUOInteractionOption Option = Target ? Roles->QuerySabotage(Me, Target) : FUOInteractionOption();
		if (Option.Kind == EUOInteractionKind::Sabotage && Option.Sabotage == Type && Option.bAvailable)
		{
			StandAtStation(Target);
			M4Target = Target;
			return true;
		}
	}
	return false;
}

void FUODevAutomation::TickM4(float DeltaSeconds)
{
	M4Clock += DeltaSeconds;
	switch (M4)
	{
	case EM4::Employee:	TickM4Employee(DeltaSeconds); break;
	case EM4::Killer:	TickM4Killer(DeltaSeconds); break;
	case EM4::Kill:		TickM4Kill(DeltaSeconds); break;
	default:
	{
		// Meeting tours: the role card, then plain work (the meeting is driven by TickMeeting)
		AUOPlayerController* PC = Owner.Get();
		if (M4Step == 0)
		{
			bM4Busy = true;
			if (PC->GetScreen() == EUOMenuScreen::RoleReveal)
			{
				if (M4Clock > 1.5f)
				{
					M4Shot(IsKillerForDev(PC->GetPlayerState<AUOPlayerState>() ? PC->GetPlayerState<AUOPlayerState>()->GetEmployeeId() : INDEX_NONE) ? TEXT("M41_RoleReveal_Killer") : TEXT("M41_RoleReveal_Employee"));
				}
				if (M4Clock > 2.5f)
				{
					PC->DismissRoleReveal();
					M4Step = 1;
					M4Clock = 0.f;
				}
			}
			else if (M4Clock > 3.f)
			{
				M4Step = 1;
				M4Clock = 0.f;
			}
			return;
		}
		bM4Busy = false;
		if (ShiftClock > 6.f && ShiftClock < 7.f)
		{
			M4Shot(FString::Printf(TEXT("M41_HUD_Day%d"), LastDay));
		}
		break;
	}
	}
}

void FUODevAutomation::TickM4Employee(float DeltaSeconds)
{
	AUOPlayerController* PC = Owner.Get();
	AUOGameMode* GameMode = PC->GetWorld()->GetAuthGameMode<AUOGameMode>();
	AUOCharacter* Character = Cast<AUOCharacter>(PC->GetPawn());
	const AUOPlayerState* Me = PC->GetPlayerState<AUOPlayerState>();
	if (!GameMode || !Character || !Me)
	{
		return;
	}
	auto Next = [this](int32 Step) { M4Step = Step; M4Clock = 0.f; };

	switch (M4Step)
	{
	case 0:
		bM4Busy = true;
		if (PC->GetScreen() == EUOMenuScreen::RoleReveal)
		{
			if (M4Clock > 1.5f)
			{
				M4Shot(TEXT("M4_01_RoleReveal_Employee"));
			}
			if (M4Clock > 2.5f)
			{
				PC->DismissRoleReveal();
				bM4Busy = false;
				Next(1);
			}
		}
		else if (M4Clock > 3.f)
		{
			bM4Busy = false;
			Next(1);
		}
		return;

	case 1:
	{
		// Work until the AI killer jams a printer (or it is time to read the logs)
		bM4Busy = false;
		AUOInteractableStation* Jammed = FindStation([](const AUOInteractableStation& S) { return S.IsJammed(); });
		if (Jammed && !Character->GetInteraction()->IsHolding())
		{
			bM4Busy = true;
			Character->GetInteraction()->EndInteract();
			PendingStation.Reset();
			StandAtStation(Jammed);
			M4Target = Jammed;
			Next(2);
		}
		else if (!bM4ToursDone && ShiftClock > 75.f && CountSabotageTraces() > 0)
		{
			Next(4);
		}
		return;
	}

	case 2:
		bM4Busy = true;
		if (M4Clock > 0.8f)
		{
			M4Shot(TEXT("M4_04_PrinterJammed"));
		}
		if (M4Clock > 1.4f)
		{
			Character->GetInteraction()->BeginInteract(M4Target.Get());
			Next(3);
		}
		return;

	case 3:
		bM4Busy = true;
		if (M4Clock > 1.2f)
		{
			M4Shot(TEXT("M4_05_PrinterFix"));
		}
		if (M4Clock > 0.3f && !Character->GetInteraction()->IsHolding())
		{
			Character->GetInteraction()->EndInteract();
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: the scripted human cleared the jam at %s."), M4Target.IsValid() ? *M4Target->GetDisplayName().ToString() : TEXT("?"));
			Next(CountSabotageTraces() > 0 ? 4 : 1);
		}
		return;

	case 4:
	{
		// Terminal tour: walk up, read, pin what really points at the killer
		bM4Busy = true;
		if (M4TerminalOrder.Num() == 0)
		{
			M4TerminalOrder = { static_cast<uint8>(EUOTerminalKind::PrintServer), static_cast<uint8>(EUOTerminalKind::BadgeReader), static_cast<uint8>(EUOTerminalKind::SecurityMonitor),
				static_cast<uint8>(EUOTerminalKind::InventorySheet), static_cast<uint8>(EUOTerminalKind::DeskComputer), static_cast<uint8>(EUOTerminalKind::ActivityBoard) };
			M4Index = 0;
		}
		if (!M4TerminalOrder.IsValidIndex(M4Index))
		{
			M4TerminalOrder.Reset();
			bM4ToursDone = Me->GetPinnedTraceIds().Num() >= FUOMeetingTuning::MaxPresentedEvidence;
			if (!bM4ToursDone && !bM4SecondPass)
			{
				// Not enough yet: a second pass also pins the killer's badge/camera lines
				bM4SecondPass = true;
				M4Clock = 0.f;
				return;
			}
			M4SabotagesSeenAtTour = CountSabotageTraces();
			bM4Busy = false;
			Next(5);
			return;
		}
		const EUOTerminalKind Kind = static_cast<EUOTerminalKind>(M4TerminalOrder[M4Index]);
		AUOTerminal* Best = nullptr;
		for (AUOTerminal* Terminal : GameMode->GetTerminals())
		{
			if (Terminal && Terminal->GetTerminalKind() == Kind)
			{
				Best = Terminal;
				break;
			}
		}
		if (!Best)
		{
			++M4Index;
			return;
		}
		if (M4Clock < 0.05f)
		{
			Character->GetInteraction()->EndInteract();
			StandAtStation(Best);
		}
		if (M4Clock > 0.5f && PC->GetScreen() == EUOMenuScreen::None)
		{
			PC->OpenTerminal(Best);
		}
		if (M4Clock > 1.5f && PC->GetScreen() == EUOMenuScreen::Terminal)
		{
			M4Shot(FString::Printf(TEXT("M4_06_Terminal_%s"), *StaticEnum<EUOTerminalKind>()->GetNameStringByValue(static_cast<int64>(Kind))));
		}
		if (M4Clock > 1.9f && M4Clock < 2.0f + DeltaSeconds && PC->GetScreen() == EUOMenuScreen::Terminal)
		{
			// First tour: incidents and fake work only; later tours also presence lines of the same person
			const int32 Pinned = M4PinTrueEvidence(M4SabotagesSeenAtTour > 0 || bM4SecondPass);
			if (Pinned > 0)
			{
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: pinned %d entr(ies) at %s [dev audit peek picks true evidence]."), Pinned, *Best->GetDisplayName().ToString());
			}
		}
		if (M4Clock > 2.6f && Me->GetPinnedTraceIds().Num() > 0)
		{
			M4Shot(TEXT("M4_07_PinEvidence"));
		}
		if (M4Clock > 3.2f)
		{
			PC->CloseTerminal();
			++M4Index;
			M4Clock = 0.f;
		}
		return;
	}

	default:
		// Back to work; read the logs again if a new sabotage shows up and the case is not ready yet
		bM4Busy = false;
		if (!bM4ToursDone && CountSabotageTraces() > M4SabotagesSeenAtTour && ShiftClock > 30.f)
		{
			Next(4);
		}
		else if (AUOInteractableStation* Jammed = FindStation([](const AUOInteractableStation& S) { return S.IsJammed(); }); Jammed && !Character->GetInteraction()->IsHolding())
		{
			bM4Busy = true;
			PendingStation.Reset();
			StandAtStation(Jammed);
			M4Target = Jammed;
			Next(2);
		}
		return;
	}
}

void FUODevAutomation::TickM4Killer(float DeltaSeconds)
{
	AUOPlayerController* PC = Owner.Get();
	AUOCharacter* Character = Cast<AUOCharacter>(PC->GetPawn());
	if (!Character)
	{
		return;
	}
	UUOInteractionComponent* Interaction = Character->GetInteraction();
	auto Next = [this](int32 Step) { M4Step = Step; M4Clock = 0.f; };
	static const EUOSabotageType Plan[] = { EUOSabotageType::JamPrinter, EUOSabotageType::LockStation, EUOSabotageType::MoveObject, EUOSabotageType::CutCamera, EUOSabotageType::CorruptFile };

	// Every day: the HUD (shield chip with "(fake)", mic indicator) looking at an accomplice (red name tag), if any
	const FString HudShot = FString::Printf(TEXT("M41_HUD_Killer_Day%d"), LastDay);
	if (PC->GetScreen() == EUOMenuScreen::None && ShiftClock > 3.f && !ShotsTaken.Contains(HudShot) && !Interaction->IsHolding())
	{
		const AUOPlayerState* Me = PC->GetPlayerState<AUOPlayerState>();
		const AUOGameState* GameState = PC->GetWorld()->GetGameState<AUOGameState>();
		const APawn* Accomplice = nullptr;
		for (const int32 Id : Me ? Me->GetKillerView().AccompliceIds : TArray<int32>())
		{
			const AUOPlayerState* Other = GameState ? GameState->FindEmployee(Id) : nullptr;
			Accomplice = Other && Other->IsAlive() ? Other->GetPawn() : Accomplice;
		}
		if (Accomplice && M4Clock < 100.f)
		{
			const FVector Target = Accomplice->GetActorLocation();
			const FVector Stand = Target - Accomplice->GetActorForwardVector() * -320.f;
			const FRotator Look = (Target + FVector(0.f, 0.f, 40.f) - (Stand + FVector(0.f, 0.f, UODevAutomationPrivate::EyeHeight))).Rotation();
			LookFrom(FVector(Stand.X, Stand.Y, Target.Z), FRotator(Look.Pitch, Look.Yaw, 0.f));
		}
		if (ShiftClock <= 3.6f)
		{
			bM4Busy = true;
			return;
		}
		M4Shot(HudShot);
		bM4Busy = false;
	}

	switch (M4Step)
	{
	case 0:
		bM4Busy = true;
		if (PC->GetScreen() == EUOMenuScreen::RoleReveal)
		{
			if (M4Clock > 1.5f)
			{
				M4Shot(TEXT("M4_01_RoleReveal_Killer"));
			}
			if (M4Clock > 2.5f)
			{
				PC->DismissRoleReveal();
				Next(1);
			}
		}
		else if (M4Clock > 3.f)
		{
			Next(1);
		}
		return;

	case 1:
		// Real work first (it counts, rule 3), then the sabotage plan
		bM4Busy = false;
		if (ShiftClock > 6.f && Interaction->IsHolding())
		{
			if (M4Clock > 0.8f)
			{
				M4Shot(TEXT("M4_08_KillerRealWork"));
			}
		}
		else
		{
			M4Clock = 0.f;
		}
		if (ShiftClock > FUOKillerTuning::SabotageGraceSeconds + 3.f && !Interaction->IsHolding())
		{
			M4Index = 0;
			Next(2);
		}
		return;

	case 2:
		// Walk to a target for the next planned sabotage (retry while it is not available yet)
		bM4Busy = true;
		if (!UE_ARRAY_COUNT(Plan) || M4Index >= UE_ARRAY_COUNT(Plan))
		{
			Next(9);
			return;
		}
		PendingStation.Reset();
		if (M4StandForSabotage(Plan[M4Index]))
		{
			Next(3);
		}
		else if (M4Clock > 40.f)
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: no target for %s, skipped."), *StaticEnum<EUOSabotageType>()->GetNameStringByValue(static_cast<int64>(Plan[M4Index])));
			++M4Index;
			M4Clock = 0.f;
		}
		else
		{
			bM4Busy = false;
		}
		return;

	case 3:
		bM4Busy = true;
		if (M4Clock > 0.8f && M4Index == 0)
		{
			M4Shot(TEXT("M4_02_SabotagePrompt"));
		}
		if (M4Clock > 1.2f)
		{
			const AUOInteractableStation* Focused = Interaction->GetFocusedStation();
			{
				FVector Eye;
				FRotator View;
				Character->GetActorEyesViewPoint(Eye, View);
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(UODevProbe), false, Character);
				const bool bHit = PC->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 250.f, ECC_Visibility, Params);
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: view probe from %s dir %s hits %s at %.0f cm (target %s at %s)."), *Eye.ToCompactString(), *View.ToCompactString(),
					bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nothing"), bHit ? Hit.Distance : -1.f,
					M4Target.IsValid() ? *M4Target->GetName() : TEXT("?"), M4Target.IsValid() ? *M4Target->GetFocusLocation().ToCompactString() : TEXT("?"));
			}
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: killer focuses %s, F option: %s%s."), Focused ? *Focused->GetDisplayName().ToString() : TEXT("nothing"),
				*Interaction->GetSecondaryOption().Prompt.ToString(), Interaction->GetSecondaryOption().bAvailable ? TEXT(" (available)") : *(TEXT(" blocked: ") + Interaction->GetSecondaryOption().BlockedReason.ToString()));
			Interaction->BeginSecondary();
			Next(4);
		}
		return;

	case 4:
		bM4Busy = true;
		if (M4Clock > 0.2f && !Interaction->IsSecondaryHolding())
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: the scripted killer finished a %s hold."), *StaticEnum<EUOSabotageType>()->GetNameStringByValue(static_cast<int64>(Plan[M4Index])));
			Next(5);
		}
		return;

	case 5:
		bM4Busy = true;
		if (M4Clock > 0.7f)
		{
			if (Plan[M4Index] == EUOSabotageType::JamPrinter)
			{
				M4Shot(TEXT("M4_03_PrinterJammed_ByKiller"));
			}
			else if (Plan[M4Index] == EUOSabotageType::CutCamera)
			{
				M4Shot(TEXT("M4_09_CameraCut_Dark"));
			}
			else if (Plan[M4Index] == EUOSabotageType::LockStation)
			{
				M4Shot(TEXT("M4_09b_StationLocked"));
			}
		}
		if (M4Clock > 1.5f)
		{
			++M4Index;
			// Real work between sabotages, so heat cools and the work counts
			bM4Busy = false;
			Next(6);
		}
		return;

	case 6:
		bM4Busy = false;
		if (M4Clock > 9.f && !Interaction->IsHolding())
		{
			Next(2);
		}
		return;

	default:
		bM4Busy = false;
		return;
	}
}

void FUODevAutomation::TickM4Kill(float DeltaSeconds)
{
	AUOPlayerController* PC = Owner.Get();
	AUOGameMode* GameMode = PC->GetWorld()->GetAuthGameMode<AUOGameMode>();
	auto Next = [this](int32 Step) { M4Step = Step; M4Clock = 0.f; };

	switch (M4Step)
	{
	case 0:
		bM4Busy = true;
		if (PC->GetScreen() == EUOMenuScreen::RoleReveal && M4Clock > 1.5f)
		{
			M4Shot(TEXT("M4_01_RoleReveal_Employee"));
			PC->DismissRoleReveal();
		}
		if (M4Clock > 2.5f)
		{
			AUOAIController::bDevSpareHuman = true;
			for (TActorIterator<AUOAIController> It(PC->GetWorld()); It; ++It)
			{
				It->ForceKillSoon();
			}
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Dev automation: AI killers try a dismissal as soon as the rules allow (never the human)."));
			bM4Busy = false;
			Next(1);
		}
		return;

	case 1:
		bM4Busy = false;
		if (GameMode && GameMode->GetBodies().Num() > 0)
		{
			Next(2);
		}
		return;

	case 2:
	{
		// Back from the emergency meeting: go and look at the body
		bM4Busy = true;
		const AUOBodyActor* Body = GameMode && GameMode->GetBodies().Num() > 0 ? GameMode->GetBodies()[0].Get() : nullptr;
		if (!Body)
		{
			Next(9);
			return;
		}
		if (M4Clock < 0.05f)
		{
			if (AUOCharacter* Character = Cast<AUOCharacter>(PC->GetPawn()))
			{
				Character->GetInteraction()->EndInteract();
			}
			PendingStation.Reset();
			const FVector Target = Body->GetActorLocation();
			// A vantage point with a clear view of the body (no wall in between)
			FVector Stand = Target + FVector(170.f, 110.f, 0.f);
			const FVector2D Offsets[] = { FVector2D(170.f, 110.f), FVector2D(-170.f, 110.f), FVector2D(170.f, -110.f), FVector2D(-170.f, -110.f),
				FVector2D(230.f, 0.f), FVector2D(-230.f, 0.f), FVector2D(0.f, 230.f), FVector2D(0.f, -230.f), FVector2D(120.f, 60.f), FVector2D(-120.f, -60.f) };
			FCollisionQueryParams Probe(SCENE_QUERY_STAT(UODevBodyVantage), false, PC->GetPawn());
			Probe.AddIgnoredActor(Body);
			for (const FVector2D& Offset : Offsets)
			{
				const FVector Candidate = Target + FVector(Offset.X, Offset.Y, UODevAutomationPrivate::CapsuleHalfHeight + 5.f + UODevAutomationPrivate::EyeHeight);
				if (!PC->GetWorld()->LineTraceTestByChannel(Candidate, Target + FVector(0.f, 0.f, 20.f), ECC_Visibility, Probe)
					&& !PC->GetWorld()->LineTraceTestByChannel(Target + FVector(0.f, 0.f, 120.f), Candidate, ECC_Visibility, Probe))
				{
					Stand = Target + FVector(Offset.X, Offset.Y, 0.f);
					break;
				}
			}
			Stand.Z = Target.Z + UODevAutomationPrivate::CapsuleHalfHeight + 5.f;
			const FVector Eye = Stand + FVector(0.f, 0.f, UODevAutomationPrivate::EyeHeight);
			const FRotator Look = (Target + FVector(30.f, 20.f, 10.f) - Eye).Rotation();
			LookFrom(Stand, FRotator(Look.Pitch, Look.Yaw, 0.f));
		}
		if (M4Clock > 1.2f)
		{
			M4Shot(TEXT("M4_11_Body_Letter"));
		}
		if (M4Clock > 2.f)
		{
			Next(9);
		}
		return;
	}

	default:
		bM4Busy = false;
		return;
	}
}

#else

TSharedPtr<FUODevAutomation> FUODevAutomation::CreateFromCommandLine(AUOPlayerController* Owner)
{
	return nullptr;
}

void FUODevAutomation::Tick(float DeltaSeconds)
{
}

#endif // !UE_BUILD_SHIPPING
