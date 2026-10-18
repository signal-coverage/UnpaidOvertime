// UNPAID: OVERTIME - per-employee state, shared by humans and AI employees.

#include "UOPlayerState.h"
#include "Net/UnrealNetwork.h"

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
}

void AUOPlayerState::InitEmployee(int32 InEmployeeId, const FString& InDisplayName, bool bInIsAI, const FLinearColor& InColor, float InWorkSpeedMultiplier)
{
	EmployeeId = InEmployeeId;
	bIsAIEmployee = bInIsAI;
	EmployeeColor = InColor;
	WorkSpeedMultiplier = FMath::Max(0.1f, InWorkSpeedMultiplier);
	SetPlayerName(InDisplayName);
	ResetForNewDay();
}

void AUOPlayerState::ResetForNewDay()
{
	bAlive = true;
	CarriedItem = EUOItemType::None;
	CompletedCount = 0;
	bEvaluated = false;
	Stars = 0;
	FairShare = 0.f;
	Consequence = EUOStarConsequence::Safe;
}

void AUOPlayerState::SetEvaluation(int32 InStars, float InFairShare, EUOStarConsequence InConsequence)
{
	bEvaluated = true;
	Stars = InStars;
	FairShare = InFairShare;
	Consequence = InConsequence;
}
