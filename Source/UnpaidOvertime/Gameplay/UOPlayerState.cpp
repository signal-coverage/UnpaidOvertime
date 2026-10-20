// UNPAID: OVERTIME - per-employee state, shared by humans and AI employees.

#include "UOPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "UOEvaluationRules.h"

AUOPlayerState::AUOPlayerState()
{
}

void AUOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUOPlayerState, EmployeeId);
	DOREPLIFETIME(AUOPlayerState, bIsAIEmployee);
	DOREPLIFETIME(AUOPlayerState, bAlive);
	DOREPLIFETIME(AUOPlayerState, EmployeeColor);
	DOREPLIFETIME(AUOPlayerState, CarriedItem);
	DOREPLIFETIME(AUOPlayerState, CompletedCount);
	DOREPLIFETIME(AUOPlayerState, WorkSpeedMultiplier);
	DOREPLIFETIME(AUOPlayerState, bEvaluated);
	DOREPLIFETIME(AUOPlayerState, Stars);
	DOREPLIFETIME(AUOPlayerState, FairShare);
	DOREPLIFETIME(AUOPlayerState, Consequence);
	DOREPLIFETIME(AUOPlayerState, ContributionRatio);
	DOREPLIFETIME(AUOPlayerState, ShownShields);
	DOREPLIFETIME(AUOPlayerState, bOnProbation);
	DOREPLIFETIME(AUOPlayerState, ExitCause);
	DOREPLIFETIME(AUOPlayerState, ExitDay);

	// Hidden information never reaches other clients (AI player states have no owning connection: server only)
	DOREPLIFETIME_CONDITION(AUOPlayerState, SecretRole, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUOPlayerState, RealStars, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUOPlayerState, PinnedTraceIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUOPlayerState, KillerView, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUOPlayerState, Shields, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUOPlayerState, FakeShields, COND_OwnerOnly);
}

void AUOPlayerState::InitEmployee(int32 InEmployeeId, const FString& InDisplayName, bool bInIsAI, const FLinearColor& InColor, float InWorkSpeedMultiplier)
{
	EmployeeId = InEmployeeId;
	bIsAIEmployee = bInIsAI;
	EmployeeColor = InColor;
	WorkSpeedMultiplier = FMath::Max(0.1f, InWorkSpeedMultiplier);
	SetPlayerName(InDisplayName);
	bAlive = true;
	Shields = 0;
	FakeShields = 0;
	ShownShields = 0;
	bOnProbation = false;
	bExemptFromDirectFiring = false;
	ExitCause = EUOExitCause::None;
	ExitDay = 0;
	SecretRole = EUORole::Unknown;
	KillerView = FUOKillerPrivateView();
	ResetForNewDay();
}

void AUOPlayerState::ResetForNewDay()
{
	CarriedItem = EUOItemType::None;
	CompletedCount = 0;
	bEvaluated = false;
	Stars = 0;
	FairShare = 0.f;
	Consequence = EUOStarConsequence::Safe;
	ContributionRatio = 0.f;
	RealStars = 0;
	PinnedTraceIds.Reset();
	KillerView.bFakeRatingPending = false;
}

void AUOPlayerState::SetEvaluation(int32 InStars, float InFairShare, EUOStarConsequence InConsequence, float InContributionRatio)
{
	bEvaluated = true;
	Stars = InStars;
	FairShare = InFairShare;
	Consequence = InConsequence;
	ContributionRatio = InContributionRatio;
	RealStars = InStars;
}

void AUOPlayerState::MarkEliminated(EUOExitCause Cause, int32 DayNumber)
{
	bAlive = false;
	ExitCause = Cause;
	ExitDay = DayNumber;
	CarriedItem = EUOItemType::None;
}

void AUOPlayerState::RefreshShownShields()
{
	ShownShields = FUOEvaluationRules::GetDisplayedShields(Shields, FakeShields);
}
