// UNPAID: OVERTIME - the end-of-day sequence on screen: review, firings, boss questions, cut vote, meeting, outcome,
// match over.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UOEvaluationTypes.h"
#include "UOKillerTypes.h"

class AUOPlayerController;
class AUOGameState;
class SBox;
struct FUOEvaluationState;
struct FKey;

/**
 *  Read-only view of AUOGameState::Evaluation. Rebuilds its content whenever the director bumps the revision and
 *  sends the local player's choices through AUOPlayerController::SubmitEvaluationChoice. Keyboard: 1-4 answer or
 *  vote, Y/N for offers, Enter to continue; meeting: 1-3 present a pinned line / vote for a card / rock-paper-scissors,
 *  0 abstain, S skip the debate. Plays the question/vote/firing/meeting sounds as the state changes.
 */
class SUOEvaluationFlowWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOEvaluationFlowWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AUOPlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Keyboard shortcut from the menu root; true when used */
	bool HandleKey(const FKey& Key);

private:

	TWeakObjectPtr<AUOPlayerController> Owner;
	TSharedPtr<SBox> ContentSlot;
	int32 BuiltRevision = INDEX_NONE;

	// Sound bookkeeping (what was already heard)
	EUOEvalStep HeardStep = EUOEvalStep::None;
	float HeardQuestionStart = -1.f;
	bool bHeardQuestionResult = false;
	int32 HeardVotes = 0;
	int32 HeardTickSecond = -1;
	int32 HeardBallotSignature = 0;
	int32 HeardStatements = 0;
	int32 HeardDuelRounds = 0;
	int32 HeardMeetingTick = -1;
	bool bHeardVerdict = false;
	bool bHeardProbation = false;
	bool bHeardForgery = false;

	const AUOGameState* GetGameState() const;
	const FUOEvaluationState* GetState() const;
	int32 GetMyId() const;
	bool IsMyPrompt(EUOEvalPrompt Prompt) const;
	float GetServerTime() const;
	FString NameOf(int32 EmployeeId) const;

	void Submit(EUOEvalChoiceType Type, int32 Value = INDEX_NONE);
	void Rebuild();
	void UpdateSounds(const FUOEvaluationState& State);
	void PlaySound(FName SoundId) const;

	/** Candidates the local player may vote for, in on-screen order */
	TArray<int32> GetMyVoteTargets() const;

	TSharedRef<SWidget> MakeFrame(const FText& Title, const FText& Subtitle, TSharedRef<SWidget> Body, TSharedRef<SWidget> Footer, float Width = 1120.f) const;
	TSharedRef<SWidget> MakeContinueFooter(const FText& Hint, const FText& Label) const;
	TSharedRef<SWidget> MakeEmployeeChip(int32 EmployeeId, float FontSize = 16.f) const;
	TSharedRef<SWidget> MakeStars(int32 Stars, float Size = 18.f) const;

	TSharedRef<SWidget> BuildFirings(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildQuestionStep(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildOfferCard(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildQuestionCard(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildAttemptLog(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildCutVote(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildOutcome(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildMatchOver(const FUOEvaluationState& State) const;

	// ~ Killers and the meeting (SUOEvaluationFlowMeeting.cpp)
	TSharedRef<SWidget> BuildRating(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildEmergency(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildDebate(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildMeetingVote(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildDuel(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildSavingThrow(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> BuildVerdict(const FUOEvaluationState& State) const;
	TSharedRef<SWidget> MakeEvidenceCard(const struct FUOTraceEntry& Entry, bool bSelected, int32 PresenterId = INDEX_NONE, float WrapWidth = 560.f) const;
	TSharedRef<SWidget> MakeCountdown(float StartServerTime, float Seconds) const;
	TSharedRef<SWidget> MakePresentedStrip(const FUOEvaluationState& State, float MaxHeight, float WrapWidth) const;
	TSharedRef<SWidget> MakeRPSIcon(EUORPSChoice Pick, float Size, const FLinearColor& Color) const;
	const class AUOPlayerState* GetMe() const;
	/** Pinned lines the local player may still put on the table */
	TArray<int32> GetPresentablePins() const;
	/** Alive coworkers the local player may vote for, in on-screen order */
	TArray<int32> GetMeetingTargets() const;
};
