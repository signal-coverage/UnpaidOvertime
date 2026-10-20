// UNPAID: OVERTIME - per-employee state, shared by humans and AI employees.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "UOCoreTypes.h"
#include "UOEvaluationTypes.h"
#include "UOKillerTypes.h"
#include "UOPlayerState.generated.h"

/** What only a killer sees about itself (replicated to its owner only) */
USTRUCT(BlueprintType)
struct FUOKillerPrivateView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	float Heat = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	float MaxHeat = 100.f;

	/** Shift seconds each sabotage type is ready (index = EUOSabotageType) */
	UPROPERTY(BlueprintReadOnly, Category="Killer")
	TArray<float> ReadyAt;

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	bool bKillEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	float KillReadyAt = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	int32 KillsToday = 0;

	/** Kills this match (each one makes the next easier to trace) */
	UPROPERTY(BlueprintReadOnly, Category="Killer")
	int32 KillsThisMatch = 0;

	/** Rule 1: the fellow killers (killers know each other) */
	UPROPERTY(BlueprintReadOnly, Category="Killer")
	TArray<int32> AccompliceIds;

	// ~ Private fake rating choice at the review

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	bool bFakeRatingPending = false;

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	TArray<int32> FakeAllowed;

	/** Stars its real work earned today (hidden from everyone else) */
	UPROPERTY(BlueprintReadOnly, Category="Killer")
	int32 FakeRealStars = 0;

	UPROPERTY(BlueprintReadOnly, Category="Killer")
	int32 FakeDefaultStars = 1;

	/** Server world time the choice times out (to the default) */
	UPROPERTY(BlueprintReadOnly, Category="Killer")
	float FakeDeadline = 0.f;
};

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

	/** Server: set up the record when the employee joins the match (clears match state and day results) */
	void InitEmployee(int32 InEmployeeId, const FString& InDisplayName, bool bInIsAI, const FLinearColor& InColor, float InWorkSpeedMultiplier);

	/** Server: resets per-day results (alive state, shield and exit survive across days) */
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
	void SetWorkSpeedMultiplier(float InMultiplier) { WorkSpeedMultiplier = FMath::Max(0.1f, InMultiplier); }

	int32 GetStars() const { return Stars; }
	float GetFairShare() const { return FairShare; }
	EUOStarConsequence GetConsequence() const { return Consequence; }
	bool HasEvaluation() const { return bEvaluated; }

	/** Completed / fair share, as shown on the review (public: AI voters may use it) */
	float GetContributionRatio() const { return ContributionRatio; }

	/** Server: stores the end-of-day evaluation */
	void SetEvaluation(int32 InStars, float InFairShare, EUOStarConsequence InConsequence, float InContributionRatio = 0.f);

	/** Server: marks the employee as out of the game */
	void SetAlive(bool bInAlive) { bAlive = bInAlive; }

	/** Server: the employee left the match (fired or cut) */
	void MarkEliminated(EUOExitCause Cause, int32 DayNumber);

	EUOExitCause GetExitCause() const { return ExitCause; }

	// ~ Secret information (replicated to the owner only; other clients see Unknown / defaults)

	/** The owner's own role. Server rules must ask URoleSystem; AIs ask it for their own role only. */
	EUORole GetKnownRole() const { return SecretRole; }
	/** Server: set by the role system */
	void SetSecretRole(EUORole InRole) { SecretRole = InRole; }

	/** Real stars (differ from the shown ones only for a killer) */
	int32 GetRealStars() const { return RealStars; }
	void SetRealStars(int32 InStars) { RealStars = InStars; }

	/** Trace entries the owner pinned as evidence (max FUOKillerTuning::MaxPins); presented at meetings */
	const TArray<int32>& GetPinnedTraceIds() const { return PinnedTraceIds; }
	TArray<int32>& GetMutablePinnedTraceIds() { return PinnedTraceIds; }

	const FUOKillerPrivateView& GetKillerView() const { return KillerView; }
	FUOKillerPrivateView& GetMutableKillerView() { return KillerView; }

	/** Server: the shown (public) review numbers, overwritten by the killer's fake rating */
	void SetShownEvaluation(int32 InStars, int32 InCompleted, EUOStarConsequence InConsequence, float InContributionRatio)
	{
		Stars = InStars;
		CompletedCount = InCompleted;
		Consequence = InConsequence;
		ContributionRatio = InContributionRatio;
	}
	int32 GetExitDay() const { return ExitDay; }

	/** Server rules: real promotion shields held. They stack; each one cancels one expulsion of any kind. */
	int32 GetShieldCount() const { return Shields; }
	void SetShieldCount(int32 InShields) { Shields = FMath::Max(0, InShields); RefreshShownShields(); }

	/** Server rules (and the owner): forged shields from a killer's fake promotion. They never protect. */
	int32 GetFakeShieldCount() const { return FakeShields; }
	void SetFakeShieldCount(int32 InShields) { FakeShields = FMath::Max(0, InShields); RefreshShownShields(); }

	/** What everyone sees: real and forged shields look the same */
	int32 GetDisplayedShieldCount() const { return ShownShields; }
	bool HasShield() const { return ShownShields > 0; }

	/** Saved at a meeting by a boss question: 2+ real stars at the next review, or fired (public) */
	bool IsOnProbation() const { return bOnProbation; }
	void SetOnProbation(bool bIn) { bOnProbation = bIn; }

	/** The killers are exempt from direct firings for 0 stars */
	bool IsExemptFromDirectFiring() const { return bExemptFromDirectFiring; }
	void SetExemptFromDirectFiring(bool bInExempt) { bExemptFromDirectFiring = bInExempt; }

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

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Evaluation")
	float ContributionRatio = 0.f;

	/** Owner only: real shields (a public count would expose a forgery) */
	UPROPERTY(Replicated)
	int32 Shields = 0;

	/** Owner only: forged shields */
	UPROPERTY(Replicated)
	int32 FakeShields = 0;

	/** Public: real + forged, capped */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	int32 ShownShields = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	bool bOnProbation = false;

	/** Server only, never replicated (hidden role information) */
	bool bExemptFromDirectFiring = false;

	void RefreshShownShields();

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	EUOExitCause ExitCause = EUOExitCause::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	int32 ExitDay = 0;

	/** Owner only */
	UPROPERTY(Replicated)
	EUORole SecretRole = EUORole::Unknown;

	/** Owner only */
	UPROPERTY(Replicated)
	int32 RealStars = 0;

	/** Owner only */
	UPROPERTY(Replicated)
	TArray<int32> PinnedTraceIds;

	/** Owner only (killer) */
	UPROPERTY(Replicated)
	FUOKillerPrivateView KillerView;
};
