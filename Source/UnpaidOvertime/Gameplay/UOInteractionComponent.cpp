// UNPAID: OVERTIME - aim-and-use interaction with hold-to-interact.

#include "UOInteractionComponent.h"
#include "UOInteractableStation.h"
#include "UOPlayerState.h"
#include "UOTaskManagerComponent.h"
#include "UOGameMode.h"
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
		return;
	}
	bHolding = true;
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
	return TaskManager && TaskManager->DropCarriedItem(GetEmployee());
}

void UUOInteractionComponent::ClearTarget()
{
	if (bHolding)
	{
		CancelHold(false);
	}
	ExplicitTargetStation.Reset();
	FocusedStation.Reset();
	CurrentOption = FUOInteractionOption();
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
		OnInteractionFinished.Broadcast(Station, EUOInteractionResult::FailedAttempt);
		return;
	}

	const bool bAccepted = TaskManager->CompleteInteraction(Employee, Station);
	OnInteractionFinished.Broadcast(Station, bAccepted ? EUOInteractionResult::Succeeded : EUOInteractionResult::Rejected);
}

void UUOInteractionComponent::CancelHold(bool bNotify)
{
	bHolding = false;
	HoldElapsed = 0.f;

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
