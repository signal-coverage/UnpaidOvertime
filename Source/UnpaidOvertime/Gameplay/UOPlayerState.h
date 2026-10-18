// UNPAID: OVERTIME - per-employee state, shared by humans and AI employees.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "UOCoreTypes.h"
#include "UOPlayerState.generated.h"

/**
 *  Employee record. Humans and AIs both own one (AI controllers request a player state),
 *  so tasks, stars and evidence treat every employee the same way.
 */
UCLASS()
class AUOPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	AUOPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: set up the record when the employee joins the day */
	void InitEmployee(int32 InEmployeeId, const FString& InDisplayName, bool bInIsAI, const FLinearColor& InColor, float InWorkSpeedMultiplier);

	/** Server: resets per-day results */
	void ResetForNewDay();

	int32 GetEmployeeId() const { return EmployeeId; }
	FString GetDisplayName() const { return GetPlayerName(); }
	bool IsAIEmployee() const { return bIsAIEmployee; }
	bool IsAlive() const { return bAlive; }
	FLinearColor GetEmployeeColor() const { return EmployeeColor; }

	EUOItemType GetCarriedItem() const { return CarriedItem; }
	bool IsCarryingItem() const { return CarriedItem != EUOItemType::None; }

	/** Server only: item changes go through the task manager */
	void SetCarriedItem(EUOItemType NewItem) { CarriedItem = NewItem; }

	int32 GetCompletedCount() const { return CompletedCount; }
	void SetCompletedCount(int32 NewCount) { CompletedCount = NewCount; }

	float GetWorkSpeedMultiplier() const { return WorkSpeedMultiplier; }

	int32 GetStars() const { return Stars; }
	float GetFairShare() const { return FairShare; }
	EUOStarConsequence GetConsequence() const { return Consequence; }
	bool HasEvaluation() const { return bEvaluated; }

	/** Server: stores the end-of-day evaluation */
	void SetEvaluation(int32 InStars, float InFairShare, EUOStarConsequence InConsequence);

	/** Server: marks the employee as out of the game */
	void SetAlive(bool bInAlive) { bAlive = bInAlive; }

protected:

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	int32 EmployeeId = INDEX_NONE;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	bool bIsAIEmployee = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	bool bAlive = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	FLinearColor EmployeeColor = FLinearColor::White;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	EUOItemType CarriedItem = EUOItemType::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	int32 CompletedCount = 0;

	/** Server-decided work speed (1 for humans, difficulty-driven for AIs) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Employee")
	float WorkSpeedMultiplier = 1.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Evaluation")
	bool bEvaluated = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Evaluation")
	int32 Stars = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Evaluation")
	float FairShare = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Evaluation")
	EUOStarConsequence Consequence = EUOStarConsequence::Safe;
};
