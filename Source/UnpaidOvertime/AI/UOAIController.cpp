// UNPAID: OVERTIME - AI employee brain.

#include "UOAIController.h"
#include "UOAIEmployee.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOInteractableStation.h"
#include "UOInteractionComponent.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskBoard.h"
#include "UOTaskGenerator.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

namespace UOAIControllerPrivate
{
	constexpr float FinalAcceptanceRadius = 45.f;
	constexpr float WaypointAcceptanceRadius = 90.f;
	constexpr float StuckSeconds = 2.f;
	constexpr float StuckProgressEpsilon = 20.f;
	constexpr float SidestepDistance = 170.f;
	constexpr int32 MaxStuckBeforeHub = 3;
	constexpr int32 MaxStuckBeforeGiveUp = 7;
	constexpr float PathProbeRadius = 30.f;
	constexpr float RetryDelaySeconds = 0.6f;
	constexpr float CompletePauseSeconds = 0.5f;
	constexpr float BaseWalkSpeed = 320.f;
}

AUOAIController::AUOAIController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
	Rng.Initialize(1);
}

void AUOAIController::ConfigureEmployee(const FUOAIDifficultyParams& InDifficulty, const FVector& InHubLocation, int32 Seed)
{
	Difficulty = InDifficulty;
	HubLocation = InHubLocation;
	Rng.Initialize(Seed);

	if (UUOInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->SuccessChance = Difficulty.Precision;
	}

	if (const ACharacter* AsCharacter = Cast<ACharacter>(GetPawn()))
	{
		AsCharacter->GetCharacterMovement()->MaxWalkSpeed = UOAIControllerPrivate::BaseWalkSpeed * FMath::Clamp(Difficulty.WorkSpeedMultiplier, 0.7f, 1.4f);
	}

	EnterState(EUOAIState::Idle);
}

void AUOAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (UUOInteractionComponent* Interaction = GetInteraction())
	{
		InteractionFinishedHandle = Interaction->OnInteractionFinished.AddUObject(this, &AUOAIController::HandleInteractionFinished);
	}
}

void AUOAIController::OnUnPossess()
{
	if (UUOInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->OnInteractionFinished.Remove(InteractionFinishedHandle);
	}
	Super::OnUnPossess();
}

void AUOAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AUOGameState* GameState = GetUOGameState();
	const AUOPlayerState* Employee = GetEmployee();
	if (!GetPawn() || !GameState || !Employee || !Employee->IsAlive() || !GameState->IsWorkPhase())
	{
		if (State != EUOAIState::Idle)
		{
			if (UUOInteractionComponent* Interaction = GetInteraction())
			{
				Interaction->ClearTarget();
			}
			Waypoints.Reset();
			State = EUOAIState::Idle;
		}
		return;
	}

	// Someone else finished our sub-objective (or the claim vanished): rethink.
	// A pending success is processed first, since our own completion also ends the claim.
	const bool bPendingSuccess = bHasInteractionResult && LastInteractionResult == EUOInteractionResult::Succeeded;
	if (ClaimedSubObjectiveId != INDEX_NONE && State != EUOAIState::Complete && !bPendingSuccess && !IsClaimStillValid())
	{
		AbandonCurrentWork();
		return;
	}

	StateTimer -= DeltaSeconds;

	switch (State)
	{
	case EUOAIState::Idle:
		EnterState(EUOAIState::Think);
		break;

	case EUOAIState::Think:
		if (StateTimer <= 0.f)
		{
			EnterState(EUOAIState::Claim);
		}
		break;

	case EUOAIState::Claim:
		TickClaim();
		break;

	case EUOAIState::GoToItemSource:
		TickGoTo(EUOAIState::PickUp);
		break;

	case EUOAIState::PickUp:
		TickPickUp();
		break;

	case EUOAIState::GoToStation:
		TickGoTo(EUOAIState::Work);
		break;

	case EUOAIState::Work:
		TickWork(DeltaSeconds);
		break;

	case EUOAIState::Complete:
		if (StateTimer <= 0.f)
		{
			EnterState(EUOAIState::Idle);
		}
		break;
	}
}

void AUOAIController::EnterState(EUOAIState NewState)
{
	State = NewState;
	bHasInteractionResult = false;

	switch (NewState)
	{
	case EUOAIState::Idle:
		StateTimer = 0.f;
		break;

	case EUOAIState::Think:
		StateTimer = Difficulty.ThinkDelaySeconds * Rng.FRandRange(0.6f, 1.4f);
		break;

	case EUOAIState::GoToItemSource:
		if (const AUOInteractableStation* Source = ItemSourceStation.Get())
		{
			MoveToPoint(Source->GetInteractionLocation());
		}
		break;

	case EUOAIState::PickUp:
		StateTimer = 2.f;
		if (AUOInteractableStation* Source = ItemSourceStation.Get())
		{
			FaceLocation(Source->GetActorLocation());
			StartInteractionWith(Source);
		}
		break;

	case EUOAIState::GoToStation:
		if (const AUOInteractableStation* Station = TargetStation.Get())
		{
			MoveToPoint(Station->GetInteractionLocation());
		}
		break;

	case EUOAIState::Work:
	{
		// Generous timeout: the hold itself takes WorkSeconds / speed, retries included
		const FUOSubObjective* Sub = FUOTaskBoard::FindSubObjective(GetUOGameState()->GetTasks(), ClaimedSubObjectiveId);
		const float WorkSeconds = Sub ? Sub->WorkSeconds : 5.f;
		StateTimer = WorkSeconds / FMath::Max(0.1f, Difficulty.WorkSpeedMultiplier) * 6.f + 5.f;
		if (AUOInteractableStation* Station = TargetStation.Get())
		{
			FaceLocation(Station->GetActorLocation());
			StartInteractionWith(Station);
		}
		break;
	}

	case EUOAIState::Complete:
		StateTimer = UOAIControllerPrivate::CompletePauseSeconds;
		ClaimedSubObjectiveId = INDEX_NONE;
		TargetStation.Reset();
		ItemSourceStation.Reset();
		break;

	default:
		break;
	}
}

void AUOAIController::TickClaim()
{
	if (ChooseAndClaim())
	{
		const AUOPlayerState* Employee = GetEmployee();
		const FUOSubObjective* Sub = FUOTaskBoard::FindSubObjective(GetUOGameState()->GetTasks(), ClaimedSubObjectiveId);
		if (Sub && Sub->bRequiresItem && Employee->GetCarriedItem() != Sub->ItemType)
		{
			ItemSourceStation = GetTaskManager()->FindNearestItemSource(Sub->ItemType, GetPawn()->GetActorLocation());
			if (!ItemSourceStation.IsValid())
			{
				AbandonCurrentWork();
				return;
			}
			EnterState(EUOAIState::GoToItemSource);
		}
		else
		{
			EnterState(EUOAIState::GoToStation);
		}
		return;
	}

	// Nothing to grab: think again later
	EnterState(EUOAIState::Think);
}

void AUOAIController::TickGoTo(EUOAIState NextState)
{
	switch (UpdateMovement(GetWorld()->GetDeltaSeconds()))
	{
	case EMoveStatus::Arrived:
		EnterState(NextState);
		break;
	case EMoveStatus::Failed:
		AbandonCurrentWork();
		break;
	default:
		break;
	}
}

void AUOAIController::TickPickUp()
{
	if (bHasInteractionResult)
	{
		if (LastInteractionResult == EUOInteractionResult::Succeeded)
		{
			EnterState(EUOAIState::GoToStation);
		}
		else
		{
			AbandonCurrentWork();
		}
		return;
	}

	if (StateTimer <= 0.f)
	{
		AbandonCurrentWork();
	}
}

void AUOAIController::TickWork(float DeltaSeconds)
{
	if (bHasInteractionResult)
	{
		bHasInteractionResult = false;
		switch (LastInteractionResult)
		{
		case EUOInteractionResult::Succeeded:
			EnterState(EUOAIState::Complete);
			return;

		case EUOInteractionResult::FailedAttempt:
			// Botched it: try again after a short pause
			RetryTimer = UOAIControllerPrivate::RetryDelaySeconds;
			break;

		case EUOInteractionResult::Rejected:
		default:
			AbandonCurrentWork();
			return;
		}
	}

	if (RetryTimer > 0.f)
	{
		RetryTimer -= DeltaSeconds;
		if (RetryTimer <= 0.f)
		{
			StartInteractionWith(TargetStation.Get());
		}
	}

	if (StateTimer <= 0.f)
	{
		AbandonCurrentWork();
	}
}

bool AUOAIController::ChooseAndClaim()
{
	AUOGameState* GameState = GetUOGameState();
	UUOTaskManagerComponent* TaskManager = GetTaskManager();
	AUOPlayerState* Employee = GetEmployee();
	const APawn* MyPawn = GetPawn();
	if (!GameState || !TaskManager || !Employee || !MyPawn)
	{
		return false;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();
	const EUOItemType Carried = Employee->GetCarriedItem();

	const FUOSubObjective* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	bool bAnyUsesCarriedItem = false;

	for (const FUOTaskInstance& Task : GameState->GetTasks())
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (Sub.State != EUOSubObjectiveState::Open)
			{
				continue;
			}

			const AUOInteractableStation* Station = TaskManager->FindStation(Sub.StationId);
			if (!Station)
			{
				continue;
			}

			const bool bUsesCarried = Carried != EUOItemType::None && Sub.bRequiresItem && Sub.ItemType == Carried;
			bAnyUsesCarriedItem |= bUsesCarried;

			// Travel estimate: via the item source when an item is still needed
			float Score = FVector::Dist2D(MyLocation, Station->GetInteractionLocation());
			if (Sub.bRequiresItem && !bUsesCarried)
			{
				if (const AUOInteractableStation* Source = TaskManager->FindNearestItemSource(Sub.ItemType, MyLocation))
				{
					Score = FVector::Dist2D(MyLocation, Source->GetInteractionLocation())
						+ FVector::Dist2D(Source->GetInteractionLocation(), Station->GetInteractionLocation());
				}
				if (TaskManager->CountUnclaimedItemDemand(Sub.ItemType) <= 0)
				{
					continue;
				}
			}
			if (bUsesCarried)
			{
				Score -= 5000.f;
			}
			Score += Rng.FRandRange(0.f, 600.f);

			if (Score < BestScore)
			{
				BestScore = Score;
				Best = &Sub;
			}
		}
	}

	// Carrying something nobody needs: put it back
	if (Carried != EUOItemType::None && !bAnyUsesCarriedItem)
	{
		if (UUOInteractionComponent* Interaction = GetInteraction())
		{
			Interaction->DropItem();
		}
	}

	if (!Best)
	{
		return false;
	}

	const int32 SubId = Best->Id;
	const FName StationId = Best->StationId;
	if (!TaskManager->TryClaim(Employee, SubId))
	{
		return false;
	}

	ClaimedSubObjectiveId = SubId;
	TargetStation = TaskManager->FindStation(StationId);
	ItemSourceStation.Reset();
	return TargetStation.IsValid();
}

void AUOAIController::AbandonCurrentWork()
{
	if (UUOInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->ClearTarget();
	}

	if (ClaimedSubObjectiveId != INDEX_NONE)
	{
		if (UUOTaskManagerComponent* TaskManager = GetTaskManager())
		{
			TaskManager->ReleaseClaim(GetEmployee(), ClaimedSubObjectiveId);
		}
	}

	ClaimedSubObjectiveId = INDEX_NONE;
	TargetStation.Reset();
	ItemSourceStation.Reset();
	Waypoints.Reset();
	RetryTimer = 0.f;
	EnterState(EUOAIState::Idle);
}

bool AUOAIController::IsClaimStillValid() const
{
	const AUOGameState* GameState = GetUOGameState();
	const AUOPlayerState* Employee = GetEmployee();
	if (!GameState || !Employee)
	{
		return false;
	}

	const FUOSubObjective* Sub = FUOTaskBoard::FindSubObjective(GameState->GetTasks(), ClaimedSubObjectiveId);
	if (!Sub || Sub->IsDone())
	{
		return false;
	}

	// While our hold is in progress the task manager may have re-claimed it for us; anything else means we lost it
	return Sub->State == EUOSubObjectiveState::Claimed && Sub->ClaimedBy == Employee->GetEmployeeId();
}

void AUOAIController::StartInteractionWith(AUOInteractableStation* Station)
{
	UUOInteractionComponent* Interaction = GetInteraction();
	if (!Interaction || !Station)
	{
		bHasInteractionResult = true;
		LastInteractionResult = EUOInteractionResult::Rejected;
		return;
	}
	Interaction->BeginInteract(Station);
}

void AUOAIController::HandleInteractionFinished(AUOInteractableStation* Station, EUOInteractionResult Result)
{
	bHasInteractionResult = true;
	LastInteractionResult = Result;
}

void AUOAIController::MoveToPoint(const FVector& Target)
{
	Waypoints.Reset();
	StuckTimer = 0.f;
	StuckCount = 0;
	BestDistanceToWaypoint = TNumericLimits<float>::Max();

	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	const FVector From = MyPawn->GetActorLocation();
	if (!HasClearPath(From, Target) && FVector::Dist2D(From, HubLocation) > UOAIControllerPrivate::WaypointAcceptanceRadius)
	{
		Waypoints.Add(HubLocation);
	}
	Waypoints.Add(Target);
}

AUOAIController::EMoveStatus AUOAIController::UpdateMovement(float DeltaSeconds)
{
	using namespace UOAIControllerPrivate;

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return EMoveStatus::Failed;
	}
	if (Waypoints.IsEmpty())
	{
		return EMoveStatus::Arrived;
	}

	const FVector Location = MyPawn->GetActorLocation();
	FVector ToWaypoint = Waypoints[0] - Location;
	ToWaypoint.Z = 0.f;
	const float Distance = ToWaypoint.Size();
	const float Acceptance = Waypoints.Num() == 1 ? FinalAcceptanceRadius : WaypointAcceptanceRadius;

	if (Distance <= Acceptance)
	{
		Waypoints.RemoveAt(0);
		StuckTimer = 0.f;
		BestDistanceToWaypoint = TNumericLimits<float>::Max();
		return Waypoints.IsEmpty() ? EMoveStatus::Arrived : EMoveStatus::Moving;
	}

	const FVector Direction = ToWaypoint / Distance;
	MyPawn->AddMovementInput(Direction, 1.f);

	// Stuck detection: no meaningful progress for a while -> sidestep, then route via the hub, then give up
	if (Distance < BestDistanceToWaypoint - StuckProgressEpsilon)
	{
		BestDistanceToWaypoint = Distance;
		StuckTimer = 0.f;
	}
	else
	{
		StuckTimer += DeltaSeconds;
		if (StuckTimer >= StuckSeconds)
		{
			StuckTimer = 0.f;
			BestDistanceToWaypoint = TNumericLimits<float>::Max();
			++StuckCount;

			if (StuckCount >= MaxStuckBeforeGiveUp)
			{
				Waypoints.Reset();
				return EMoveStatus::Failed;
			}

			const FVector Side = FVector(-Direction.Y, Direction.X, 0.f) * SidestepSign;
			SidestepSign = -SidestepSign;
			Waypoints.Insert(Location + Side * SidestepDistance + Direction * 40.f, 0);

			if (StuckCount == MaxStuckBeforeHub && FVector::Dist2D(Location, HubLocation) > WaypointAcceptanceRadius)
			{
				Waypoints.Insert(HubLocation, 1);
			}
		}
	}

	return EMoveStatus::Moving;
}

bool AUOAIController::HasClearPath(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start(From.X, From.Y, From.Z);
	const FVector End(To.X, To.Y, From.Z);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(UOAIPathProbe), false, GetPawn());
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	return !World->SweepTestByObjectType(Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(UOAIControllerPrivate::PathProbeRadius), Params);
}

void AUOAIController::FaceLocation(const FVector& Location)
{
	if (APawn* MyPawn = GetPawn())
	{
		const FVector ToTarget = Location - MyPawn->GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			MyPawn->SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
		}
	}
}

AUOPlayerState* AUOAIController::GetEmployee() const
{
	return GetPlayerState<AUOPlayerState>();
}

AUOGameState* AUOAIController::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

UUOTaskManagerComponent* AUOAIController::GetTaskManager() const
{
	const UWorld* World = GetWorld();
	const AUOGameMode* GameMode = World ? World->GetAuthGameMode<AUOGameMode>() : nullptr;
	return GameMode ? GameMode->GetTaskManager() : nullptr;
}

UUOInteractionComponent* AUOAIController::GetInteraction() const
{
	const AUOAIEmployee* Employee = Cast<AUOAIEmployee>(GetPawn());
	return Employee ? Employee->GetInteraction() : nullptr;
}
