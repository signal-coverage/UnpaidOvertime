// UNPAID: OVERTIME - aim-and-use interaction with hold-to-interact.

#include "UOInteractionComponent.h"
#include "UOInteractableStation.h"
#include "UOPlayerState.h"
#include "UOTaskManagerComponent.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UORoleSystemComponent.h"
#include "UOKillerRules.h"
#include "EngineUtils.h"
#include "UOAudioSubsystem.h"
#include "UOAudioRules.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "UnpaidOvertime.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UUOInteractionComponent::UUOInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UUOInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFocus();
	UpdateSecondary(DeltaTime);

	if (!bHolding)
	{
		return;
	}

	// The hold breaks if the target changed or the offer changed (someone else finished it, item lost...)
	if (FocusedStation.Get() != HoldStation.Get() || !CurrentOption.bAvailable || !CurrentOption.IsSameAction(HoldOption))
	{
		CancelHold(true);
		return;
	}

	HoldElapsed += DeltaTime * GetWorkSpeed();
	if (HoldElapsed >= HoldOption.HoldSeconds)
	{
		FinishHold();
	}
}

void UUOInteractionComponent::UpdateFocus()
{
	AUOInteractableStation* NewFocus = nullptr;

	if (bUseViewTrace)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && GetWorld())
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);

			FCollisionQueryParams Params(SCENE_QUERY_STAT(UOInteractionTrace), false, Pawn);
			FHitResult Hit;
			const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;
			if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
			{
				NewFocus = Cast<AUOInteractableStation>(Hit.GetActor());
			}
		}
	}
	else
	{
		NewFocus = ExplicitTargetStation.Get();
	}

	FocusedStation = NewFocus;

	const UUOTaskManagerComponent* TaskManager = GetTaskManager();
	CurrentOption = (TaskManager && NewFocus) ? TaskManager->QueryInteraction(GetEmployee(), NewFocus) : FUOInteractionOption();
}

void UUOInteractionComponent::BeginInteract(AUOInteractableStation* ExplicitTarget)
{
	if (bHolding)
	{
		return;
	}

	if (!bUseViewTrace && ExplicitTarget)
	{
		ExplicitTargetStation = ExplicitTarget;
	}

	UpdateFocus();

	AUOInteractableStation* Station = FocusedStation.Get();
	UUOTaskManagerComponent* TaskManager = GetTaskManager();
	AUOPlayerState* Employee = GetEmployee();
	if (!Station || !TaskManager || !Employee || !CurrentOption.bAvailable)
	{
		if (Station)
		{
			OnInteractionFinished.Broadcast(Station, EUOInteractionResult::Rejected);
			PlayDenied();
		}
		return;
	}

	HoldStation = Station;
	HoldOption = CurrentOption;
	HoldElapsed = 0.f;

	if (HoldOption.HoldSeconds <= 0.f)
	{
		FinishHold();
		return;
	}

	if (!TaskManager->BeginWork(Employee, Station))
	{
		OnInteractionFinished.Broadcast(Station, EUOInteractionResult::Rejected);
		PlayDenied();
		return;
	}
	bHolding = true;
	PlayHoldStartSounds(Station);
}

void UUOInteractionComponent::EndInteract()
{
	if (bHolding)
	{
		CancelHold(false);
	}
}

bool UUOInteractionComponent::DropItem()
{
	if (bHolding)
	{
		CancelHold(false);
	}

	UUOTaskManagerComponent* TaskManager = GetTaskManager();
	const bool bDropped = TaskManager && TaskManager->DropCarriedItem(GetEmployee());
	if (bDropped && GetOwner())
	{
		if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
		{
			Audio->PlayAtLocation(UOSoundId::ItemDrop, GetOwner()->GetActorLocation() - FVector(0.f, 0.f, 60.f));
		}
	}
	return bDropped;
}

void UUOInteractionComponent::ClearTarget()
{
	if (bHolding)
	{
		CancelHold(false);
	}
	ExplicitTargetStation.Reset();
	ExplicitVictim.Reset();
	FocusedStation.Reset();
	CurrentOption = FUOInteractionOption();
	bSecondaryHolding = false;
	SecondaryElapsed = 0.f;
}

float UUOInteractionComponent::GetHoldProgress() const
{
	if (!bHolding || HoldOption.HoldSeconds <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(HoldElapsed / HoldOption.HoldSeconds, 0.f, 1.f);
}

void UUOInteractionComponent::FinishHold()
{
	bHolding = false;
	HoldElapsed = 0.f;
	StopHoldLoop();

	AUOInteractableStation* Station = HoldStation.Get();
	UUOTaskManagerComponent* TaskManager = GetTaskManager();
	AUOPlayerState* Employee = GetEmployee();
	if (!Station || !TaskManager || !Employee)
	{
		return;
	}

	// Precision only matters for real work, not for picking things up
	const bool bIsWork = HoldOption.Kind == EUOInteractionKind::Work || HoldOption.Kind == EUOInteractionKind::DeliverItem;
	if (bIsWork && SuccessChance < 1.f && FMath::FRand() > SuccessChance)
	{
		TaskManager->CancelWork(Employee);
		TaskManager->RecordFailedAttempt(Employee, Station);
		OnInteractionFinished.Broadcast(Station, EUOInteractionResult::FailedAttempt);
		return;
	}

	const bool bAccepted = TaskManager->CompleteInteraction(Employee, Station);
	if (bAccepted)
	{
		PlayCompletionSounds(Station);
	}
	else
	{
		PlayDenied();
	}
	OnInteractionFinished.Broadcast(Station, bAccepted ? EUOInteractionResult::Succeeded : EUOInteractionResult::Rejected);
}

void UUOInteractionComponent::CancelHold(bool bNotify)
{
	bHolding = false;
	HoldElapsed = 0.f;
	StopHoldLoop();

	if (UUOTaskManagerComponent* TaskManager = GetTaskManager())
	{
		TaskManager->CancelWork(GetEmployee());
	}

	if (bNotify && HoldStation.IsValid())
	{
		OnInteractionFinished.Broadcast(HoldStation.Get(), EUOInteractionResult::Rejected);
	}
}

AUOPlayerState* UUOInteractionComponent::GetEmployee() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetPlayerState<AUOPlayerState>() : nullptr;
}

UUORoleSystemComponent* UUOInteractionComponent::GetRoleSystem() const
{
	const UWorld* World = GetWorld();
	const AUOGameMode* GameMode = World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
	return GameMode ? GameMode->GetRoleSystem() : nullptr;
}

AUOPlayerState* UUOInteractionComponent::FindVictimInFront() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UWorld* World = GetWorld();
	const AUOGameState* GameState = World ? World->GetGameState<AUOGameState>() : nullptr;
	if (!Pawn || !GameState)
	{
		return nullptr;
	}
	FVector Eye;
	FRotator View;
	Pawn->GetActorEyesViewPoint(Eye, View);
	const FVector Forward = View.Vector();

	AUOPlayerState* Best = nullptr;
	float BestDot = 0.82f;
	for (AUOPlayerState* Employee : GameState->GetEmployees())
	{
		const APawn* Other = Employee ? Employee->GetPawn() : nullptr;
		if (!Employee || !Employee->IsAlive() || !Other || Other == Pawn)
		{
			continue;
		}
		const FVector To = Other->GetActorLocation() + FVector(0.f, 0.f, 30.f) - Eye;
		if (To.Size() > FUOKillerTuning::KillRange + 60.f)
		{
			continue;
		}
		const float Dot = FVector::DotProduct(Forward, To.GetSafeNormal());
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = Employee;
		}
	}
	return Best;
}

void UUOInteractionComponent::UpdateSecondary(float DeltaTime)
{
	SecondaryOption = FUOInteractionOption();
	SecondaryVictim.Reset();
	const UUORoleSystemComponent* Roles = GetRoleSystem();
	AUOPlayerState* Employee = GetEmployee();
	if (!Roles || !Employee || !Roles->IsKillerServer(Employee->GetEmployeeId()))
	{
		if (bSecondaryHolding)
		{
			bSecondaryHolding = false;
		}
		return;
	}

	// A coworker within reach takes priority (when dismissals are enabled), then the focused station
	AUOPlayerState* Victim = bUseViewTrace ? FindVictimInFront() : ExplicitVictim.Get();
	if (Victim && Roles->IsKillEnabled())
	{
		SecondaryVictim = Victim;
		SecondaryOption = Roles->QueryKill(Employee, Victim);
	}
	else if (AUOInteractableStation* Station = FocusedStation.Get())
	{
		SecondaryOption = Roles->QuerySabotage(Employee, Station);
	}

	if (!bSecondaryHolding)
	{
		return;
	}
	const bool bSameTarget = SecondaryOption.Kind == EUOInteractionKind::Kill ? SecondaryVictim.IsValid() : FocusedStation.Get() == SecondaryStation.Get();
	if (!bSameTarget || !SecondaryOption.bAvailable || !SecondaryOption.IsSameAction(SecondaryHoldOption))
	{
		bSecondaryHolding = false;
		SecondaryElapsed = 0.f;
		OnSecondaryFinished.Broadcast(false);
		return;
	}
	SecondaryElapsed += DeltaTime;
	if (SecondaryElapsed >= SecondaryHoldOption.HoldSeconds)
	{
		FinishSecondary();
	}
}

void UUOInteractionComponent::BeginSecondary(AUOInteractableStation* ExplicitTarget, AUOPlayerState* InExplicitVictim)
{
	if (bSecondaryHolding || bHolding)
	{
		return;
	}
	if (!bUseViewTrace)
	{
		ExplicitVictim = InExplicitVictim;
		if (ExplicitTarget)
		{
			ExplicitTargetStation = ExplicitTarget;
		}
	}
	UpdateFocus();
	UpdateSecondary(0.f);
	if (!SecondaryOption.bAvailable)
	{
		PlayDenied();
		OnSecondaryFinished.Broadcast(false);
		return;
	}
	bSecondaryHolding = true;
	SecondaryElapsed = 0.f;
	SecondaryHoldOption = SecondaryOption;
	SecondaryStation = FocusedStation;
}

void UUOInteractionComponent::EndSecondary()
{
	if (bSecondaryHolding)
	{
		bSecondaryHolding = false;
		SecondaryElapsed = 0.f;
		OnSecondaryFinished.Broadcast(false);
	}
}

float UUOInteractionComponent::GetSecondaryProgress() const
{
	return bSecondaryHolding && SecondaryHoldOption.HoldSeconds > 0.f ? FMath::Clamp(SecondaryElapsed / SecondaryHoldOption.HoldSeconds, 0.f, 1.f) : 0.f;
}

void UUOInteractionComponent::FinishSecondary()
{
	bSecondaryHolding = false;
	SecondaryElapsed = 0.f;
	UUORoleSystemComponent* Roles = GetRoleSystem();
	AUOPlayerState* Employee = GetEmployee();
	bool bDone = false;
	if (Roles && Employee)
	{
		if (SecondaryHoldOption.Kind == EUOInteractionKind::Kill)
		{
			bDone = SecondaryVictim.IsValid() && Roles->PerformKill(Employee, SecondaryVictim.Get());
		}
		else if (AUOInteractableStation* Station = SecondaryStation.Get())
		{
			bDone = Roles->PerformSabotage(Employee, Station);
		}
	}
	if (!bDone)
	{
		PlayDenied();
	}
	OnSecondaryFinished.Broadcast(bDone);
}

UUOTaskManagerComponent* UUOInteractionComponent::GetTaskManager() const
{
	const UWorld* World = GetWorld();
	const AUOGameMode* GameMode = World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
	return GameMode ? GameMode->GetTaskManager() : nullptr;
}

float UUOInteractionComponent::GetWorkSpeed() const
{
	const AUOPlayerState* Employee = GetEmployee();
	return Employee ? Employee->GetWorkSpeedMultiplier() : 1.f;
}

// ~ Sounds

namespace UOInteractionAudioPrivate
{
	EUOInteractionSoundAction ToSoundAction(EUOInteractionKind Kind)
	{
		switch (Kind)
		{
		case EUOInteractionKind::PickUpItem:	return EUOInteractionSoundAction::PickUp;
		case EUOInteractionKind::DeliverItem:	return EUOInteractionSoundAction::Deliver;
		default:								return EUOInteractionSoundAction::Work;
		}
	}

	constexpr float LoopFadeOutSeconds = 0.15f;
}

void UUOInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHoldLoop();
	Super::EndPlay(EndPlayReason);
}

void UUOInteractionComponent::PlayHoldStartSounds(const AUOInteractableStation* Station)
{
	UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this);
	if (!Audio || !Station)
	{
		return;
	}

	const FUOInteractionSounds Sounds = FUOAudioRules::GetInteractionSounds(Station->GetStationType(), UOInteractionAudioPrivate::ToSoundAction(HoldOption.Kind));
	const FVector Where = Station->GetFocusLocation();
	if (!Sounds.Start.IsNone())
	{
		Audio->PlayAtLocation(Sounds.Start, Where);
	}
	StopHoldLoop();
	if (!Sounds.Loop.IsNone())
	{
		HoldLoopAudio = Audio->PlayAtLocation(Sounds.Loop, Where);
	}
}

void UUOInteractionComponent::PlayCompletionSounds(const AUOInteractableStation* Station) const
{
	UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this);
	if (!Audio || !Station)
	{
		return;
	}

	const FUOInteractionSounds Sounds = FUOAudioRules::GetInteractionSounds(Station->GetStationType(), UOInteractionAudioPrivate::ToSoundAction(HoldOption.Kind));
	const FVector Where = Station->GetFocusLocation();
	if (!Sounds.Complete.IsNone())
	{
		Audio->PlayAtLocation(Sounds.Complete, Where);
	}
	if (!Sounds.CompleteExtra.IsNone())
	{
		Audio->PlayAtLocation(Sounds.CompleteExtra, Where, 0.8f);
	}
}

void UUOInteractionComponent::PlayDenied() const
{
	if (!IsLocalHuman())
	{
		return;
	}
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
	{
		Audio->Play2D(UOSoundId::InteractDenied);
	}
}

void UUOInteractionComponent::StopHoldLoop()
{
	if (UAudioComponent* Loop = HoldLoopAudio.Get())
	{
		Loop->FadeOut(UOInteractionAudioPrivate::LoopFadeOutSeconds, 0.f);
		if (UUOAudioSubsystem::IsDebugEnabled())
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: stop loop %s (%s)"), Loop->Sound ? *Loop->Sound->GetName() : TEXT("?"), *GetNameSafe(GetOwner()));
		}
	}
	HoldLoopAudio = nullptr;
}

bool UUOInteractionComponent::IsLocalHuman() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}
