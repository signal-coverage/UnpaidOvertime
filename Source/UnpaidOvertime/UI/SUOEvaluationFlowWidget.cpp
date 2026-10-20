// UNPAID: OVERTIME - the end-of-day sequence on screen.

#include "SUOEvaluationFlowWidget.h"
#include "SUOEvaluationWidget.h"
#include "SUOIcon.h"
#include "UOUIStyle.h"
#include "UOPlayerController.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UODayCalendar.h"
#include "UOStarRating.h"
#include "UOAudioRules.h"
#include "UOAudioSubsystem.h"
#include "UOMeetingRules.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOEvaluationFlow"

namespace UOEvaluationFlowPrivate
{
	constexpr float CountdownBarWidth = 640.f;
	constexpr int32 TickFromSecond = 5;

	FText PurposeTitle(EUOQuestionPurpose Purpose)
	{
		switch (Purpose)
		{
		case EUOQuestionPurpose::Promotion:	return LOCTEXT("PromotionTitle", "PROMOTION QUESTION");
		case EUOQuestionPurpose::Defense:	return LOCTEXT("DefenseTitle", "DEFENSE QUESTION");
		default:							return LOCTEXT("TiebreakTitle", "TIEBREAK QUESTION");
		}
	}

	FLinearColor ResultColor(EUOAnswerResult Result)
	{
		return Result == EUOAnswerResult::Correct ? FUOUIStyle::Good() : FUOUIStyle::Bad();
	}

	FText ResultText(EUOAnswerResult Result)
	{
		switch (Result)
		{
		case EUOAnswerResult::Correct:	return LOCTEXT("ResultCorrect", "Correct.");
		case EUOAnswerResult::Wrong:	return LOCTEXT("ResultWrong", "Wrong.");
		case EUOAnswerResult::TimedOut:	return LOCTEXT("ResultTimeout", "Time's up.");
		default:						return FText::GetEmpty();
		}
	}

	FText KeyLabel(int32 Index)
	{
		return FText::AsNumber(Index + 1);
	}
}

void SUOEvaluationFlowWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	ChildSlot
	[
		SAssignNew(ContentSlot, SBox)
	];
	Rebuild();
}

const AUOGameState* SUOEvaluationFlowWidget::GetGameState() const
{
	const AUOPlayerController* PC = Owner.Get();
	return PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
}

const FUOEvaluationState* SUOEvaluationFlowWidget::GetState() const
{
	const AUOGameState* GameState = GetGameState();
	return GameState ? &GameState->GetEvaluation() : nullptr;
}

int32 SUOEvaluationFlowWidget::GetMyId() const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	return Me ? Me->GetEmployeeId() : INDEX_NONE;
}

bool SUOEvaluationFlowWidget::IsMyPrompt(EUOEvalPrompt Prompt) const
{
	const FUOEvaluationState* State = GetState();
	return State && State->Prompt == Prompt && (Prompt == EUOEvalPrompt::MatchOverChoice || State->PromptEmployeeId == GetMyId());
}

float SUOEvaluationFlowWidget::GetServerTime() const
{
	const AUOGameState* GameState = GetGameState();
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.f;
}

FString SUOEvaluationFlowWidget::NameOf(int32 EmployeeId) const
{
	if (EmployeeId == GetMyId())
	{
		return LOCTEXT("You", "You").ToString();
	}
	const AUOGameState* GameState = GetGameState();
	return GameState ? GameState->GetEmployeeName(EmployeeId) : FString();
}

void SUOEvaluationFlowWidget::Submit(EUOEvalChoiceType Type, int32 Value)
{
	if (AUOPlayerController* PC = Owner.Get())
	{
		PC->SubmitEvaluationChoice(FUOEvalChoice::Make(Type, Value));
	}
}

void SUOEvaluationFlowWidget::PlaySound(FName SoundId) const
{
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(Owner.Get()))
	{
		Audio->Play2D(SoundId);
	}
}

TArray<int32> SUOEvaluationFlowWidget::GetMyVoteTargets() const
{
	TArray<int32> Targets;
	if (const FUOEvaluationState* State = GetState())
	{
		for (const int32 Candidate : State->Candidates)
		{
			if (Candidate != GetMyId())
			{
				Targets.Add(Candidate);
			}
		}
	}
	return Targets;
}

void SUOEvaluationFlowWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FUOEvaluationState* State = GetState();
	if (!State)
	{
		return;
	}
	if (State->Revision != BuiltRevision)
	{
		Rebuild();
	}
	UpdateSounds(*State);
}

void SUOEvaluationFlowWidget::UpdateSounds(const FUOEvaluationState& State)
{
	// Step entry stingers
	if (State.Step != HeardStep)
	{
		HeardStep = State.Step;
		if (State.Step == EUOEvalStep::Firings && State.Firings.ContainsByPredicate([](const auto& F) { return F.Outcome == EUODirectFiringOutcome::Fired; }))
		{
			PlaySound(UOSoundId::Stamp);
		}
		else if (State.Step == EUOEvalStep::Outcome && State.CutEmployeeId != INDEX_NONE)
		{
			PlaySound(UOSoundId::Stamp);
		}
		else if (State.Step == EUOEvalStep::MatchOver)
		{
			PlaySound(State.MatchResult == EUOMatchResult::Victory ? UOSoundId::TaskAllDone : UOSoundId::Fired);
		}
		else if (State.Step == EUOEvalStep::Debate)
		{
			PlaySound(UOSoundId::MeetingBell);
		}
		else if (State.Step == EUOEvalStep::EmergencyMeeting)
		{
			PlaySound(UOSoundId::KillSting);
			PlaySound(UOSoundId::MeetingBell);
		}
		HeardVotes = 0;
		HeardMeetingTick = -1;
		if (State.Step == EUOEvalStep::Debate || State.Step == EUOEvalStep::EmergencyMeeting || State.Step == EUOEvalStep::Review)
		{
			HeardStatements = 0;
			HeardBallotSignature = 0;
			HeardDuelRounds = 0;
			bHeardVerdict = false;
			bHeardProbation = false;
			bHeardForgery = false;
		}
	}

	// The meeting: statements, live ballots, the last 3 seconds of a countdown, duel reveals, the verdict
	const FUOMeetingView& Meeting = State.Meeting;
	if (Meeting.Statements.Num() > HeardStatements)
	{
		HeardStatements = Meeting.Statements.Num();
		PlaySound(UOSoundId::TerminalBeep);
	}
	int32 Signature = Meeting.Ballots.Num() * 977;
	for (const FUOMeetingBallotView& Ballot : Meeting.Ballots)
	{
		Signature = Signature * 31 + Ballot.VoterId * 7 + Ballot.TargetId + 3;
	}
	if (Signature != HeardBallotSignature)
	{
		if (HeardBallotSignature != 0 || Meeting.Ballots.Num() > 0)
		{
			PlaySound(UOSoundId::VoteTick);
		}
		HeardBallotSignature = Signature;
	}
	const bool bCountdown = (State.Step == EUOEvalStep::Debate || State.Step == EUOEvalStep::MeetingVote) && Meeting.PhaseSeconds > 0.f;
	const bool bDuelCountdown = State.Step == EUOEvalStep::Duel && Meeting.Duel.bPicking && IsMyPrompt(EUOEvalPrompt::DuelPick);
	if (bCountdown || bDuelCountdown)
	{
		const float Remaining = bCountdown ? Meeting.PhaseSeconds - (GetServerTime() - Meeting.PhaseStartServerTime) : Meeting.Duel.PickDeadline - GetServerTime();
		const int32 Second = FMath::CeilToInt(Remaining);
		if (Second <= FUOMeetingTuning::TickLastSeconds && Second > 0 && Second != HeardMeetingTick)
		{
			HeardMeetingTick = Second;
			PlaySound(UOSoundId::TimerTick);
		}
	}
	if (Meeting.Duel.Rounds.Num() > HeardDuelRounds)
	{
		HeardDuelRounds = Meeting.Duel.Rounds.Num();
		HeardMeetingTick = -1;
		PlaySound(UOSoundId::RPSReveal);
	}
	const bool bForgery = Meeting.ShieldCheck == EUOShieldCheck::Forgery || State.CutShieldCheck == EUOShieldCheck::Forgery
		|| State.Firings.ContainsByPredicate([](const FUOFiringView& F) { return F.bForgeryExposed; });
	if (bForgery && !bHeardForgery)
	{
		bHeardForgery = true;
		PlaySound(UOSoundId::ForgeryReveal);
	}
	if (Meeting.bProbation && !bHeardProbation)
	{
		bHeardProbation = true;
		PlaySound(UOSoundId::ProbationStamp);
	}
	if (State.Step == EUOEvalStep::Verdict && Meeting.bResolved && !bHeardVerdict)
	{
		bHeardVerdict = true;
		PlaySound(Meeting.ExpelledId != INDEX_NONE ? UOSoundId::Gavel : UOSoundId::BossCorrect);
	}

	// Boss question: appear, last-seconds ticks, result
	const FUOQuestionView& Question = State.Question;
	if (Question.bActive && Question.StartServerTime != HeardQuestionStart)
	{
		HeardQuestionStart = Question.StartServerTime;
		bHeardQuestionResult = false;
		HeardTickSecond = -1;
		PlaySound(UOSoundId::BossQuestion);
	}
	if (Question.bActive && !Question.IsResolved() && Question.AskedEmployeeId == GetMyId())
	{
		const float Remaining = Question.TimeLimitSeconds - (GetServerTime() - Question.StartServerTime);
		const int32 Second = FMath::CeilToInt(Remaining);
		if (Second <= UOEvaluationFlowPrivate::TickFromSecond && Second > 0 && Second != HeardTickSecond)
		{
			HeardTickSecond = Second;
			PlaySound(UOSoundId::BossTick);
		}
	}
	if (Question.bActive && Question.IsResolved() && !bHeardQuestionResult)
	{
		bHeardQuestionResult = true;
		PlaySound(Question.Result == EUOAnswerResult::Correct ? UOSoundId::BossCorrect : UOSoundId::BossWrong);
	}

	// Public cut votes
	if (State.Votes.Num() > HeardVotes)
	{
		HeardVotes = State.Votes.Num();
		PlaySound(UOSoundId::VoteCast);
	}
}

bool SUOEvaluationFlowWidget::HandleKey(const FKey& Key)
{
	const FUOEvaluationState* State = GetState();
	if (!State)
	{
		return false;
	}

	int32 Number = INDEX_NONE;
	static const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven };
	static const FKey NumpadKeys[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NumberKeys); ++Index)
	{
		if (Key == NumberKeys[Index] || Key == NumpadKeys[Index])
		{
			Number = Index;
		}
	}

	if (IsMyPrompt(EUOEvalPrompt::Continue) && (Key == EKeys::Enter || Key == EKeys::SpaceBar))
	{
		Submit(EUOEvalChoiceType::Continue);
		return true;
	}
	if ((IsMyPrompt(EUOEvalPrompt::OfferPromotion) || IsMyPrompt(EUOEvalPrompt::OfferDefense)) && (Key == EKeys::Y || Key == EKeys::N || Number == 0 || Number == 1))
	{
		Submit((Key == EKeys::Y || Number == 0) ? EUOEvalChoiceType::AcceptOffer : EUOEvalChoiceType::DeclineOffer);
		return true;
	}
	if (IsMyPrompt(EUOEvalPrompt::AnswerQuestion) && State->Question.Options.IsValidIndex(Number))
	{
		Submit(EUOEvalChoiceType::Answer, Number);
		return true;
	}
	if (IsMyPrompt(EUOEvalPrompt::CastVote))
	{
		const TArray<int32> Targets = GetMyVoteTargets();
		if (Targets.IsValidIndex(Number))
		{
			Submit(EUOEvalChoiceType::Vote, Targets[Number]);
			return true;
		}
	}
	if (IsMyPrompt(EUOEvalPrompt::MatchOverChoice) && (Number == 0 || Number == 1))
	{
		Submit(Number == 0 ? EUOEvalChoiceType::NewHire : EUOEvalChoiceType::MainMenu);
		return true;
	}
	// Meeting: present pinned lines (1-3) or skip (S); vote for a card (1-7) or abstain (0); rock-paper-scissors (1-3)
	if (IsMyPrompt(EUOEvalPrompt::Debate))
	{
		const TArray<int32> Pins = GetPresentablePins();
		if (Pins.IsValidIndex(Number) && Number < 3)
		{
			Submit(EUOEvalChoiceType::PresentEvidence, Pins[Number]);
			return true;
		}
		if (Key == EKeys::S)
		{
			Submit(EUOEvalChoiceType::SkipDebate);
			return true;
		}
	}
	if (IsMyPrompt(EUOEvalPrompt::MeetingVote))
	{
		const TArray<int32> Targets = GetMeetingTargets();
		if (Targets.IsValidIndex(Number))
		{
			Submit(EUOEvalChoiceType::MeetingVote, Targets[Number]);
			return true;
		}
		if (Key == EKeys::Zero || Key == EKeys::NumPadZero)
		{
			Submit(EUOEvalChoiceType::MeetingVote, INDEX_NONE);
			return true;
		}
	}
	if (IsMyPrompt(EUOEvalPrompt::DuelPick) && Number >= 0 && Number <= 2)
	{
		Submit(EUOEvalChoiceType::DuelPick, Number + 1);
		return true;
	}
	// The killer's private rating: 1-3 picks that many stars
	if (State->Step == EUOEvalStep::Rating && Number >= 0 && Number <= 2)
	{
		const AUOPlayerState* Me = GetMe();
		if (Me && Me->GetKillerView().bFakeRatingPending && Me->GetKillerView().FakeAllowed.Contains(Number + 1))
		{
			Submit(EUOEvalChoiceType::FakeRating, Number + 1);
			return true;
		}
	}
	return false;
}

void SUOEvaluationFlowWidget::Rebuild()
{
	const FUOEvaluationState* State = GetState();
	if (!State || !ContentSlot.IsValid())
	{
		return;
	}
	BuiltRevision = State->Revision;

	TSharedRef<SWidget> Content = SNullWidget::NullWidget;
	switch (State->Step)
	{
	case EUOEvalStep::Review:
		Content = SNew(SUOEvaluationWidget)
			.Owner(Owner)
			.OnContinue_Lambda([this]() { Submit(EUOEvalChoiceType::Continue); });
		break;
	case EUOEvalStep::Firings:
		Content = BuildFirings(*State);
		break;
	case EUOEvalStep::Promotion:
	case EUOEvalStep::Defense:
		Content = BuildQuestionStep(*State);
		break;
	case EUOEvalStep::CutVote:
	case EUOEvalStep::Tiebreak:
		Content = BuildCutVote(*State);
		break;
	case EUOEvalStep::Outcome:
		Content = BuildOutcome(*State);
		break;
	case EUOEvalStep::MatchOver:
		Content = BuildMatchOver(*State);
		break;
	case EUOEvalStep::Rating:
		Content = BuildRating(*State);
		break;
	case EUOEvalStep::EmergencyMeeting:
		Content = BuildEmergency(*State);
		break;
	case EUOEvalStep::Debate:
		Content = BuildDebate(*State);
		break;
	case EUOEvalStep::MeetingVote:
		Content = BuildMeetingVote(*State);
		break;
	case EUOEvalStep::Duel:
		Content = BuildDuel(*State);
		break;
	case EUOEvalStep::SavingThrow:
		Content = BuildSavingThrow(*State);
		break;
	case EUOEvalStep::Verdict:
		Content = BuildVerdict(*State);
		break;
	default:
		break;
	}

	const FUOUIStyle& Style = FUOUIStyle::Get();
	const bool bOwnBackdrop = State->Step == EUOEvalStep::Review;
	ContentSlot->SetContent(
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&Style.WhiteBrush)
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, State->Step == EUOEvalStep::MatchOver ? 0.86f : 0.72f))
			.Visibility(bOwnBackdrop ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
		]

		+ SOverlay::Slot()
		[
			Content
		]);
}

// ~ Building blocks

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeFrame(const FText& Title, const FText& Subtitle, TSharedRef<SWidget> Body, TSharedRef<SWidget> Footer, float Width) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOGameState* GameState = GetGameState();
	const FText Kicker = GameState
		? FText::Format(LOCTEXT("Kicker", "{0}  ·  {1}"), FUODayCalendar::FormatDayName(GameState->GetDayNumber()), FText::FromString(GameState->GetCompanyName()))
		: FText::GetEmpty();

	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(Width)
			[
				SNew(SBorder)
				.BorderImage(&Style.PanelSolidBrush)
				.Padding(FMargin(48.f, 38.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Kicker.ToUpper())
						.Font(FUOUIStyle::Label(11.f))
						.ColorAndOpacity(FUOUIStyle::AccentBright())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(FUOUIStyle::Title(40.f))
						.ColorAndOpacity(FLinearColor::White)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f, 0.f, 22.f)
					[
						SNew(STextBlock)
						.Text(Subtitle)
						.Font(FUOUIStyle::Body(15.f))
						.ColorAndOpacity(FUOUIStyle::Text())
						// Fixed wrap width: AutoWrapText measures one frame late after each rebuild
						.WrapTextAt(Width - 96.f)
						.Visibility(Subtitle.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						Body
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 24.f, 0.f, 18.f)
					[
						FUOUIStyle::MakeDivider()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						Footer
					]
				]
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeContinueFooter(const FText& Hint, const FText& Label) const
{
	const bool bMine = IsMyPrompt(EUOEvalPrompt::Continue);
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Hint)
			.Font(FUOUIStyle::Body(12.f))
			.ColorAndOpacity(FUOUIStyle::TextDim())
			.AutoWrapText(true)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.Visibility(bMine ? EVisibility::Visible : EVisibility::Collapsed)
			[
				FUOUIStyle::MakeButton(Label, FOnClicked::CreateLambda([this]()
				{
					const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::Continue);
					return FReply::Handled();
				}), true, true, 200.f)
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeEmployeeChip(int32 EmployeeId, float FontSize) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOGameState* GameState = GetGameState();
	const AUOPlayerState* Employee = GameState ? GameState->FindEmployee(EmployeeId) : nullptr;
	const FLinearColor Color = Employee ? Employee->GetEmployeeColor() : FLinearColor::Gray;
	const bool bMe = EmployeeId == GetMyId();
	// Everyone sees real and forged shields alike
	const int32 Shields = Employee ? Employee->GetDisplayedShieldCount() : 0;
	const bool bShield = Shields > 0;
	const bool bProbation = Employee && Employee->IsOnProbation();
	bool bRevealedKiller = false;
	const bool bRevealed = GameState && GameState->IsRoleRevealed(EmployeeId, bRevealedKiller);
	const AUOPlayerState* MeState = GetMe();
	const bool bAccomplice = MeState && MeState->GetKillerView().AccompliceIds.Contains(EmployeeId);

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(4.f)
			.HeightOverride(FontSize * 1.6f)
			[
				SNew(SImage)
				.Image(&Style.WhiteBrush)
				.ColorAndOpacity(Color)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Employee ? Employee->GetDisplayName() : FString()))
			.Font(FUOUIStyle::Font("Bold", FontSize))
			.ColorAndOpacity(FLinearColor::White)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.PillAccentBrush)
			.Padding(FMargin(7.f, 2.f))
			.Visibility(bMe ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("YouPill", "YOU"))
				.Font(FUOUIStyle::Label(9.f))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility(bShield ? EVisibility::Visible : EVisibility::Collapsed)
			.ToolTipText(LOCTEXT("ShieldTip", "Promotion shields: each one cancels one expulsion (firing, cut vote or meeting)"))
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SUOIcon)
				.Type(EUOIconType::Shield)
				.Size(FontSize * 1.1f)
				.Color(FUOUIStyle::Gold())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ShieldCount", "×{0}"), FText::AsNumber(Shields)))
				.Font(FUOUIStyle::Font("Bold", FontSize * 0.85f))
				.ColorAndOpacity(FUOUIStyle::Gold())
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(FUOUIStyle::Warning())
			.Padding(FMargin(6.f, 1.f))
			.Visibility(bProbation ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(STextBlock).Text(LOCTEXT("ProbationPill", "PROBATION")).Font(FUOUIStyle::Label(8.f)).ColorAndOpacity(FUOUIStyle::Warning())
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.PillAccentBrush)
			.Padding(FMargin(6.f, 1.f))
			.Visibility(bAccomplice || (bRevealed && bRevealedKiller) ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(STextBlock)
				.Text(bAccomplice ? LOCTEXT("AccomplicePill", "ACCOMPLICE") : LOCTEXT("SaboteurPill", "SABOTEUR"))
				.Font(FUOUIStyle::Label(8.f))
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeStars(int32 Stars, float Size) const
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < FUOStarRating::MaxStars; ++Index)
	{
		const bool bFilled = Index < Stars;
		Box->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SUOIcon)
			.Type(bFilled ? EUOIconType::StarFilled : EUOIconType::StarEmpty)
			.Size(Size)
			.Color(bFilled ? FUOUIStyle::Gold() : FLinearColor(1.f, 1.f, 1.f, 0.22f))
		];
	}
	return Box;
}

// ~ Direct firings

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildFirings(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const FUOFiringView& Firing : State.Firings)
	{
		FText Verdict;
		FText Detail;
		FLinearColor Color = FUOUIStyle::Bad();
		EUOIconType Icon = EUOIconType::Cross;
		switch (Firing.Outcome)
		{
		case EUODirectFiringOutcome::Fired:
			Verdict = LOCTEXT("Fired", "FIRED");
			Detail = Firing.bProbation ? LOCTEXT("FiredProbation", "Failed probation: fewer than 2 real stars. Effective immediately.") : LOCTEXT("FiredDetail", "Zero stars. Effective immediately.");
			if (Firing.bForgeryExposed)
			{
				Detail = FText::Format(LOCTEXT("FiredForgery", "{0} The shield was a forgery."), Detail);
			}
			break;
		case EUODirectFiringOutcome::ShieldUsed:
			Verdict = LOCTEXT("ShieldUsed", "SHIELD USED");
			Detail = Firing.bProbation
				? FText::Format(LOCTEXT("ShieldUsedProbation", "Failed probation, but a promotion shield absorbed it. Shields left: {0}."), FText::AsNumber(Firing.ShieldsLeft))
				: FText::Format(LOCTEXT("ShieldUsedDetail", "Zero stars, but a promotion shield absorbed it. Shields left: {0}."), FText::AsNumber(Firing.ShieldsLeft));
			Color = FUOUIStyle::Gold();
			Icon = EUOIconType::Shield;
			break;
		default:
			Verdict = LOCTEXT("Exempt", "EXEMPT");
			Detail = LOCTEXT("ExemptDetail", "Management has other plans.");
			Color = FUOUIStyle::Warning();
			Icon = EUOIconType::Circle;
			break;
		}

		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.Padding(FMargin(18.f, 14.f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[MakeEmployeeChip(Firing.EmployeeId, 18.f)]
					+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Detail)
						.Font(FUOUIStyle::Body(13.f))
						.ColorAndOpacity(FUOUIStyle::TextDim())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SUOIcon)
					.Type(Icon)
					.Size(30.f)
					.Thickness(3.f)
					.Color(Color)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Verdict)
					.Font(FUOUIStyle::Title(26.f))
					.ColorAndOpacity(Color)
				]
			]
		];
	}

	return MakeFrame(LOCTEXT("FiringsTitle", "DIRECT FIRINGS"),
		LOCTEXT("FiringsSubtitle", "Zero stars means zero future here, and so does a failed probation. Each promotion shield cancels one firing."),
		Rows,
		MakeContinueFooter(LOCTEXT("FiringsHint", "Fired employees are gone for the rest of the match."), LOCTEXT("ContinueBtn", "Continue")));
}

// ~ Promotions, defenses (and the question card)

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildQuestionStep(const FUOEvaluationState& State) const
{
	const bool bPromotion = State.Step == EUOEvalStep::Promotion;
	TSharedRef<SWidget> Active = SNullWidget::NullWidget;
	if (IsMyPrompt(EUOEvalPrompt::OfferPromotion) || IsMyPrompt(EUOEvalPrompt::OfferDefense))
	{
		Active = BuildOfferCard(State);
	}
	else if (State.Question.bActive)
	{
		Active = BuildQuestionCard(State);
	}

	return MakeFrame(bPromotion ? LOCTEXT("PromotionsTitle", "PROMOTIONS") : LOCTEXT("DefensesTitle", "DEFENSES"),
		bPromotion
			? LOCTEXT("PromotionsSubtitle", "Three stars earn a hard question from the boss. A correct answer is a promotion: one more shield against any expulsion. Shields stack (up to 2).")
			: LOCTEXT("DefensesSubtitle", "One star puts you on the cut list. Answer the boss correctly and you are off it, for today."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Active]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)[BuildAttemptLog(State)],
		SNew(STextBlock)
		.Text(LOCTEXT("QuestionHint", "The boss only asks about things you could have seen today."))
		.Font(FUOUIStyle::Body(12.f))
		.ColorAndOpacity(FUOUIStyle::TextDim()));
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildOfferCard(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const bool bPromotion = State.Prompt == EUOEvalPrompt::OfferPromotion;
	const FText Title = bPromotion ? LOCTEXT("OfferPromotionTitle", "Attempt a promotion?") : LOCTEXT("OfferDefenseTitle", "Defend yourself?");
	const FText Body = bPromotion
		? LOCTEXT("OfferPromotionBody", "You earned three stars. Answer one of the boss's harder questions to win a promotion shield: it cancels one firing, cut or meeting expulsion (they stack up to 2). A wrong answer costs nothing.")
		: LOCTEXT("OfferDefenseBody", "You earned one star, so you are a cut-vote candidate. Answer the boss correctly and nobody can vote you out today. A wrong answer keeps you on the list.");

	auto Choice = [this](bool bAccept)
	{
		return FOnClicked::CreateLambda([this, bAccept]()
		{
			const_cast<SUOEvaluationFlowWidget*>(this)->Submit(bAccept ? EUOEvalChoiceType::AcceptOffer : EUOEvalChoiceType::DeclineOffer);
			return FReply::Handled();
		});
	};

	return SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.Padding(FMargin(28.f, 24.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SUOIcon)
					.Type(bPromotion ? EUOIconType::Shield : EUOIconType::StarFilled)
					.Size(30.f)
					.Color(bPromotion ? FUOUIStyle::Gold() : FUOUIStyle::Warning())
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(FUOUIStyle::Heading(24.f))
					.ColorAndOpacity(FLinearColor::White)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 12.f, 0.f, 20.f)
			[
				SNew(STextBlock)
				.Text(Body)
				.Font(FUOUIStyle::Body(15.f))
				.ColorAndOpacity(FUOUIStyle::Text())
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[FUOUIStyle::MakeKeyCap(LOCTEXT("KeyY", "Y"), 26.f)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 24.f, 0.f)[FUOUIStyle::MakeButton(LOCTEXT("OfferYes", "Ask me"), Choice(true), true, true, 180.f)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[FUOUIStyle::MakeKeyCap(LOCTEXT("KeyN", "N"), 26.f)]
				+ SHorizontalBox::Slot().AutoWidth()[FUOUIStyle::MakeButton(LOCTEXT("OfferNo", "Not today"), Choice(false), false, true, 180.f)]
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildQuestionCard(const FUOEvaluationState& State) const
{
	using namespace UOEvaluationFlowPrivate;

	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FUOQuestionView& Question = State.Question;
	const bool bMine = Question.AskedEmployeeId == GetMyId();
	const bool bResolved = Question.IsResolved();
	const bool bInteractive = bMine && !bResolved && IsMyPrompt(EUOEvalPrompt::AnswerQuestion);

	FText Header;
	if (bMine)
	{
		Header = PurposeTitle(Question.Purpose);
	}
	else if (!bResolved)
	{
		Header = FText::Format(LOCTEXT("AIAnswering", "{0} is answering the boss…"), FText::FromString(NameOf(Question.AskedEmployeeId)));
	}
	else
	{
		Header = FText::Format(LOCTEXT("AIAnswered", "{0} answered the boss"), FText::FromString(NameOf(Question.AskedEmployeeId)));
	}

	TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Question.Options.Num(); ++Index)
	{
		const bool bCorrect = bResolved && Index == Question.CorrectIndex;
		const bool bChosenWrong = bResolved && Index == Question.ChosenIndex && Index != Question.CorrectIndex;
		const bool bChosen = Index == Question.ChosenIndex;
		const FLinearColor Mark = bCorrect ? FUOUIStyle::Good() : (bChosenWrong ? FUOUIStyle::Bad() : FLinearColor(1.f, 1.f, 1.f, 0.f));

		TSharedRef<SWidget> Row =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
			[
				FUOUIStyle::MakeKeyCap(KeyLabel(Index), 28.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Question.Options[Index])
				.Font(FUOUIStyle::Font("Bold", 17))
				.ColorAndOpacity(bResolved && !bCorrect && !bChosen ? FUOUIStyle::TextDim() : FLinearColor::White)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SUOIcon)
				.Type(bCorrect ? EUOIconType::Check : EUOIconType::Cross)
				.Size(22.f)
				.Thickness(3.f)
				.Color(Mark)
				.Visibility(bCorrect || bChosenWrong ? EVisibility::Visible : EVisibility::Hidden)
			];

		TSharedRef<SWidget> Wrapped = bInteractive
			? StaticCastSharedRef<SWidget>(SNew(SButton)
				.ButtonStyle(&Style.ChoiceButton)
				.ContentPadding(FMargin(14.f, 10.f))
				.Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this, Index]()
				{
					const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::Answer, Index);
					return FReply::Handled();
				})
				[
					Row
				])
			: StaticCastSharedRef<SWidget>(SNew(SBorder)
				.BorderImage(&Style.CardBrush)
				.BorderBackgroundColor(bCorrect ? FUOUIStyle::Good() * FLinearColor(1.f, 1.f, 1.f, 0.35f) : (bChosenWrong ? FUOUIStyle::Bad() * FLinearColor(1.f, 1.f, 1.f, 0.35f) : FLinearColor::White))
				.Padding(FMargin(14.f, 10.f))
				[
					Row
				]);

		Options->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			Wrapped
		];
	}

	// Countdown (own question) or result line
	TSharedRef<SWidget> Status = SNullWidget::NullWidget;
	if (bResolved)
	{
		FText Line = ResultText(Question.Result);
		if (!bMine)
		{
			// Options that are already quoted (poster texts) are not quoted twice
			const FText Chosen = Question.Options.IsValidIndex(Question.ChosenIndex) ? Question.Options[Question.ChosenIndex] : LOCTEXT("Nothing", "nothing");
			Line = Chosen.ToString().StartsWith(TEXT("“"))
				? FText::Format(LOCTEXT("AIResultQuoted", "{0} {1} chose {2}."), ResultText(Question.Result), FText::FromString(NameOf(Question.AskedEmployeeId)), Chosen)
				: FText::Format(LOCTEXT("AIResult", "{0} {1} chose “{2}”."), ResultText(Question.Result), FText::FromString(NameOf(Question.AskedEmployeeId)), Chosen);
		}
		Status = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SUOIcon)
				.Type(Question.Result == EUOAnswerResult::Correct ? EUOIconType::Check : EUOIconType::Cross)
				.Size(26.f)
				.Thickness(3.f)
				.Color(ResultColor(Question.Result))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Line)
				.Font(FUOUIStyle::Font("Bold", 17))
				.ColorAndOpacity(ResultColor(Question.Result))
			];
	}
	else if (bMine)
	{
		const float Limit = FMath::Max(0.1f, Question.TimeLimitSeconds);
		const float Start = Question.StartServerTime;
		Status = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.WidthOverride(CountdownBarWidth)
				.HeightOverride(8.f)
				.HAlign(HAlign_Left)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SBox).WidthOverride(CountdownBarWidth)[SNew(SImage).Image(&Style.BarBackBrush)]]
					+ SOverlay::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride_Lambda([this, Limit, Start]()
						{
							return FOptionalSize(CountdownBarWidth * FMath::Clamp(1.f - (GetServerTime() - Start) / Limit, 0.f, 1.f));
						})
						[
							SNew(SImage)
							.Image(&Style.BarFillBrush)
							.ColorAndOpacity_Lambda([this, Limit, Start]()
							{
								const float Left = Limit - (GetServerTime() - Start);
								return FSlateColor(Left <= TickFromSecond ? FUOUIStyle::AccentBright() : FUOUIStyle::Gold());
							})
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, Limit, Start]()
				{
					return FText::Format(LOCTEXT("SecondsLeft", "{0}s  ·  press 1-{1} or click"), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Limit - (GetServerTime() - Start)))),
						FText::AsNumber(GetState() ? GetState()->Question.Options.Num() : 4));
				})
				.Font(FUOUIStyle::Body(12.f))
				.ColorAndOpacity(FUOUIStyle::TextDim())
			];
	}
	else
	{
		Status = SNew(STextBlock)
			.Text(LOCTEXT("AIThinking", "Thinking about what they saw today…"))
			.Font(FUOUIStyle::Body(14.f))
			.ColorAndOpacity(FUOUIStyle::TextDim());
	}

	return SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.Padding(FMargin(28.f, 22.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Header.ToUpper())
					.Font(FUOUIStyle::Label(11.f))
					.ColorAndOpacity(Question.Purpose == EUOQuestionPurpose::Promotion ? FUOUIStyle::Gold() : FUOUIStyle::Warning())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeEmployeeChip(Question.AskedEmployeeId, 13.f)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 18.f)
			[
				SNew(STextBlock)
				.Text(Question.Prompt)
				.Font(FUOUIStyle::Font("Bold", 24))
				.ColorAndOpacity(FLinearColor::White)
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Options
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				Status
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildAttemptLog(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	TSharedRef<SVerticalBox> Log = SNew(SVerticalBox);
	for (const FUOAttemptView& Attempt : State.Attempts)
	{
		FText What;
		FLinearColor Color = FUOUIStyle::TextDim();
		EUOIconType Icon = EUOIconType::Circle;
		if (!Attempt.bAttempted)
		{
			What = LOCTEXT("Declined", "Declined the question");
		}
		else if (Attempt.Purpose == EUOQuestionPurpose::Promotion)
		{
			const bool bOk = Attempt.Result == EUOAnswerResult::Correct;
			What = bOk
				? (Attempt.bGainedShield
					? FText::Format(LOCTEXT("PromotedLog", "Promoted: +1 shield (×{0})"), FText::AsNumber(Attempt.ShieldsAfter))
					: FText::Format(LOCTEXT("PromotedCapLog", "Promoted, but already at the shield cap (×{0})"), FText::AsNumber(Attempt.ShieldsAfter)))
				: LOCTEXT("NotPromotedLog", "Missed the promotion (no harm done)");
			Color = bOk ? FUOUIStyle::Gold() : FUOUIStyle::TextDim();
			Icon = bOk ? EUOIconType::Shield : EUOIconType::Cross;
		}
		else if (Attempt.Purpose == EUOQuestionPurpose::Defense)
		{
			const bool bOk = Attempt.Result == EUOAnswerResult::Correct;
			What = bOk ? LOCTEXT("DefendedLog", "Defended: off the cut list today") : LOCTEXT("NotDefendedLog", "Failed the defense: still a candidate");
			Color = bOk ? FUOUIStyle::Good() : FUOUIStyle::Bad();
			Icon = bOk ? EUOIconType::Check : EUOIconType::Cross;
		}
		else
		{
			const bool bOk = Attempt.Result == EUOAnswerResult::Correct;
			What = bOk ? LOCTEXT("TieOkLog", "Tiebreak: correct") : LOCTEXT("TieBadLog", "Tiebreak: wrong");
			Color = bOk ? FUOUIStyle::Good() : FUOUIStyle::Bad();
			Icon = bOk ? EUOIconType::Check : EUOIconType::Cross;
		}

		Log->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.Padding(FMargin(14.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MakeEmployeeChip(Attempt.EmployeeId, 14.f)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SUOIcon).Type(Icon).Size(18.f).Thickness(2.5f).Color(Color)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(What).Font(FUOUIStyle::Font("Bold", 13)).ColorAndOpacity(Color)
				]
			]
		];
	}
	return Log;
}

// ~ Cut vote

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildCutVote(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOGameState* GameState = GetGameState();
	const bool bVoting = IsMyPrompt(EUOEvalPrompt::CastVote);
	const TArray<int32> MyTargets = GetMyVoteTargets();

	// Candidate cards (up to 3 per row width; a lone candidate does not stretch across the panel)
	const float CardWidth = FMath::Min(360.f, (1144.f - 10.f * FMath::Max(0, State.Candidates.Num() - 1)) / FMath::Max(1, State.Candidates.Num()));
	TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
	for (const int32 Candidate : State.Candidates)
	{
		const AUOPlayerState* Employee = GameState ? GameState->FindEmployee(Candidate) : nullptr;
		int32 Votes = 0;
		for (const FUOVoteView& Vote : State.Votes)
		{
			Votes += Vote.CandidateId == Candidate ? 1 : 0;
		}
		const bool bTied = State.TiedCandidates.Contains(Candidate);
		const bool bCut = State.CutEmployeeId == Candidate;
		const int32 TargetIndex = MyTargets.IndexOfByKey(Candidate);
		static const FNumberFormattingOptions OneDecimal = FNumberFormattingOptions().SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);

		Cards->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(CardWidth)
			[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(bCut ? FUOUIStyle::Bad() : (bTied ? FUOUIStyle::Warning() : FLinearColor::White))
			.Padding(FMargin(16.f, 14.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeEmployeeChip(Candidate, 15.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 8.f, 0.f, 0.f)[MakeStars(Employee ? Employee->GetStars() : 1, 16.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("CandidateWork", "Did {0} of a {1} fair share"), FText::AsNumber(Employee ? Employee->GetCompletedCount() : 0),
						FText::AsNumber(Employee ? Employee->GetFairShare() : 0.f, &OneDecimal)))
					.Font(FUOUIStyle::Body(12.f))
					.ColorAndOpacity(FUOUIStyle::TextDim())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 10.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(Votes))
						.Font(FUOUIStyle::Title(34.f))
						.ColorAndOpacity(Votes > 0 ? FUOUIStyle::AccentBright() : FUOUIStyle::TextDim())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(6.f, 0.f, 0.f, 6.f)
					[
						SNew(STextBlock)
						.Text(Votes == 1 ? LOCTEXT("VoteOne", "vote") : LOCTEXT("VoteMany", "votes"))
						.Font(FUOUIStyle::Body(13.f))
						.ColorAndOpacity(FUOUIStyle::TextDim())
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 10.f, 0.f, 0.f)
				[
					SNew(SBox)
					.Visibility(bVoting && TargetIndex != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[FUOUIStyle::MakeKeyCap(FText::AsNumber(TargetIndex + 1), 26.f)]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							FUOUIStyle::MakeButton(LOCTEXT("VoteBtn", "Vote out"), FOnClicked::CreateLambda([this, Candidate]()
							{
								const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::Vote, Candidate);
								return FReply::Handled();
							}), true, true, 140.f)
						]
					]
				]
			]
			]
		];
	}

	// Public vote feed
	TSharedRef<SWrapBox> Feed = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 8.f));
	for (const FUOVoteView& Vote : State.Votes)
	{
		const FText Line = Vote.CandidateId == INDEX_NONE
			? FText::Format(LOCTEXT("Abstains", "{0} abstains"), FText::FromString(NameOf(Vote.VoterId)))
			: FText::Format(LOCTEXT("VotesFor", "{0}  →  {1}"), FText::FromString(NameOf(Vote.VoterId)), FText::FromString(NameOf(Vote.CandidateId)));
		Feed->AddSlot()
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.Padding(FMargin(12.f, 6.f))
			[
				SNew(STextBlock).Text(Line).Font(FUOUIStyle::Font("Bold", 13)).ColorAndOpacity(FUOUIStyle::Text())
			]
		];
	}

	FText Status;
	if (bVoting)
	{
		Status = MyTargets.Num() == 1
			? LOCTEXT("YourVoteOne", "Your vote is public. Press 1 or click the card.")
			: FText::Format(LOCTEXT("YourVote", "Your vote is public. Press 1-{0} or click a card."), FText::AsNumber(MyTargets.Num()));
	}
	else if (!State.bTallyFinal)
	{
		Status = FText::Format(LOCTEXT("Revealing", "Revealing votes… {0} to go."), FText::AsNumber(State.PendingVotes));
	}
	else if (State.TiedCandidates.Num() > 1)
	{
		Status = LOCTEXT("Tie", "TIE. The tied candidates answer the boss: a wrong answer leaves first (the first to fail), else the slowest.");
	}
	else if (State.CutShieldCheck == EUOShieldCheck::ShieldUsed)
	{
		Status = FText::Format(LOCTEXT("CutShieldUsed", "{0} was voted out, but a promotion shield absorbs it. Nobody is cut today."), FText::FromString(NameOf(State.CutTargetId)));
	}
	else if (State.CutShieldCheck == EUOShieldCheck::Forgery)
	{
		Status = FText::Format(LOCTEXT("CutForgery", "{0} reaches for a shield... The shield was a forgery."), FText::FromString(NameOf(State.CutTargetId)));
	}
	else
	{
		int32 Leader = INDEX_NONE;
		int32 Best = -1;
		for (const int32 Candidate : State.Candidates)
		{
			const int32 Votes = State.Votes.FilterByPredicate([Candidate](const FUOVoteView& V) { return V.CandidateId == Candidate; }).Num();
			if (Votes > Best)
			{
				Best = Votes;
				Leader = Candidate;
			}
		}
		Status = FText::Format(LOCTEXT("TallyDone", "The votes are in. Most votes: {0} ({1})."), FText::FromString(NameOf(Leader)), FText::AsNumber(Best));
	}

	TSharedRef<SWidget> QuestionOverlay = State.Step == EUOEvalStep::Tiebreak && State.Question.bActive ? BuildQuestionCard(State) : SNullWidget::NullWidget;

	return MakeFrame(State.Step == EUOEvalStep::Tiebreak ? LOCTEXT("TiebreakFrameTitle", "TIEBREAK") : LOCTEXT("CutVoteTitle", "CUT VOTE"),
		LOCTEXT("CutVoteSubtitle", "Budget cuts. Everyone still employed votes, publicly, for one of the one-star employees. Most votes leaves."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Cards]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)[QuestionOverlay]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 8.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("PublicVotes", "Public votes"))]
		+ SVerticalBox::Slot().AutoHeight()[Feed],
		SNew(STextBlock)
		.Text(Status)
		.Font(FUOUIStyle::Font("Bold", 14))
		.ColorAndOpacity(State.TiedCandidates.Num() > 1 ? FUOUIStyle::Warning() : FUOUIStyle::Text())
		.AutoWrapText(true),
		1240.f);
}

// ~ Outcome

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildOutcome(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOGameState* GameState = GetGameState();
	const int32 NextDay = GameState ? GameState->GetDayNumber() + 1 : 2;

	// The day in one list
	TSharedRef<SVerticalBox> Summary = SNew(SVerticalBox);
	auto AddLine = [&Summary](const FText& Text, const FLinearColor& Color, EUOIconType Icon)
	{
		Summary->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[SNew(SUOIcon).Type(Icon).Size(18.f).Thickness(2.5f).Color(Color)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Text).Font(FUOUIStyle::Body(15.f)).ColorAndOpacity(FUOUIStyle::Text())]
		];
	};
	for (const FUOFiringView& Firing : State.Firings)
	{
		if (Firing.Outcome == EUODirectFiringOutcome::Fired)
		{
			AddLine(FText::Format(LOCTEXT("SumFired", "{0} was fired (zero stars)."), FText::FromString(NameOf(Firing.EmployeeId))), FUOUIStyle::Bad(), EUOIconType::Cross);
		}
		else if (Firing.Outcome == EUODirectFiringOutcome::ShieldUsed)
		{
			AddLine(FText::Format(LOCTEXT("SumShield", "{0} survived a firing with a promotion shield ({1} left)."), FText::FromString(NameOf(Firing.EmployeeId)), FText::AsNumber(Firing.ShieldsLeft)), FUOUIStyle::Gold(), EUOIconType::Shield);
		}
	}
	for (const FUOAttemptView& Attempt : State.Attempts)
	{
		if (Attempt.bGainedShield)
		{
			AddLine(FText::Format(LOCTEXT("SumPromoted", "{0} was promoted (shields ×{1})."), FText::FromString(NameOf(Attempt.EmployeeId)), FText::AsNumber(Attempt.ShieldsAfter)), FUOUIStyle::Gold(), EUOIconType::Shield);
		}
		else if (Attempt.Purpose == EUOQuestionPurpose::Defense && Attempt.Result == EUOAnswerResult::Correct)
		{
			AddLine(FText::Format(LOCTEXT("SumDefended", "{0} talked their way off the cut list."), FText::FromString(NameOf(Attempt.EmployeeId))), FUOUIStyle::Good(), EUOIconType::Check);
		}
	}
	if (State.CutShieldCheck == EUOShieldCheck::ShieldUsed)
	{
		AddLine(FText::Format(LOCTEXT("SumCutShield", "{0} was voted out in the cut vote; a shield absorbed it."), FText::FromString(NameOf(State.CutTargetId))), FUOUIStyle::Gold(), EUOIconType::Shield);
	}
	if (State.Meeting.bResolved)
	{
		const FUOMeetingView& Meeting = State.Meeting;
		if (Meeting.ExpelledId != INDEX_NONE)
		{
			AddLine(Meeting.bExpelledWasKiller
				? FText::Format(LOCTEXT("SumExpelledKiller", "{0} was voted out at the meeting. They were a SABOTEUR."), FText::FromString(NameOf(Meeting.ExpelledId)))
				: FText::Format(LOCTEXT("SumExpelledInnocent", "{0} was voted out at the meeting. They were an employee."), FText::FromString(NameOf(Meeting.ExpelledId))),
				Meeting.bExpelledWasKiller ? FUOUIStyle::Gold() : FUOUIStyle::Bad(), EUOIconType::Cross);
		}
		else if (Meeting.bSaved)
		{
			AddLine(Meeting.bProbation
				? FText::Format(LOCTEXT("SumProbation", "{0} talked their way out of the meeting: on probation."), FText::FromString(NameOf(Meeting.CandidateId)))
				: FText::Format(LOCTEXT("SumMeetingShield", "{0} survived the meeting with a promotion shield."), FText::FromString(NameOf(Meeting.CandidateId))),
				FUOUIStyle::Warning(), Meeting.bProbation ? EUOIconType::Circle : EUOIconType::Shield);
		}
		else
		{
			AddLine(LOCTEXT("SumNobodyMeeting", "The meeting voted nobody out."), FUOUIStyle::TextDim(), EUOIconType::Circle);
		}
	}
	if (GameState)
	{
		AddLine(FText::Format(LOCTEXT("SumAlive", "{0} employees remain."), FText::AsNumber(GameState->CountAliveEmployees())), FUOUIStyle::TextDim(), EUOIconType::Circle);
	}

	const bool bCut = State.CutEmployeeId != INDEX_NONE;
	const FText Button = State.bOutcomeEndsMatch
		? LOCTEXT("OutcomeContinue", "Continue")
		: FText::Format(LOCTEXT("StartDay", "Start {0}"), FUODayCalendar::FormatDayName(NextDay));

	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(bCut ? FUOUIStyle::Bad() : FUOUIStyle::Good())
			.Padding(FMargin(26.f, 22.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
				[
					SNew(SUOIcon).Type(bCut ? EUOIconType::Cross : EUOIconType::Check).Size(44.f).Thickness(4.f).Color(bCut ? FUOUIStyle::Bad() : FUOUIStyle::Good())
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(State.OutcomeHeadline).Font(FUOUIStyle::Font("Bold", 28)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(State.OutcomeLine).Font(FUOUIStyle::Body(16.f)).ColorAndOpacity(FUOUIStyle::Text()).AutoWrapText(true)
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)[Summary];

	return MakeFrame(LOCTEXT("OutcomeTitle", "END OF DAY"), FText::GetEmpty(), Body,
		MakeContinueFooter(State.bOutcomeEndsMatch ? LOCTEXT("OutcomeEndHint", "This changes everything.") : LOCTEXT("OutcomeHint", "Tomorrow: same number of tasks, harder ones. Coworkers get sharper too."), Button),
		1000.f);
}

// ~ Match over

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildMatchOver(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOGameState* GameState = GetGameState();
	const bool bVictory = State.MatchResult == EUOMatchResult::Victory;
	const FText DayName = GameState ? FUODayCalendar::FormatDayName(GameState->GetDayNumber()) : FText::GetEmpty();

	FText Cause;
	if (bVictory && State.bKillerVictory && State.bKillersTakeOver)
	{
		Cause = LOCTEXT("TakeoverVictoryLine", "Only the saboteurs are left. Corporate is impressed by your \"cost-cutting initiative\": the company is yours now.");
	}
	else if (bVictory && State.bKillerVictory)
	{
		Cause = LOCTEXT("KillerVictoryLine", "Nobody left to suspect you. Corporate is impressed by your \"cost-cutting initiative\": the saboteur gets promoted.");
	}
	else if (!bVictory && State.bKillersTakeOver)
	{
		Cause = LOCTEXT("TakeoverLossLine", "Only the saboteurs are left. They take over the company, and your desk is the first thing they shred.");
	}
	else if (bVictory)
	{
		Cause = State.bHumanConvictedKiller
			? LOCTEXT("VictoryLineHero", "Everyone else is gone, and you exposed the saboteur. Regional Manager of an empty building, with a framed commendation nobody will see.")
			: LOCTEXT("VictoryLine", "Everyone else is gone. Congratulations: you have been promoted to Regional Manager of an empty building. The coffee is all yours.");
	}
	else if (State.HumanExitCause == EUOExitCause::CutVote)
	{
		Cause = FText::Format(LOCTEXT("CauseCut", "Voted out by your coworkers on {0}. Democracy is a beautiful thing."), DayName);
	}
	else if (State.HumanExitCause == EUOExitCause::Convicted)
	{
		const AUOPlayerState* Me = GetMe();
		Cause = Me && Me->GetKnownRole() == EUORole::Killer
			? FText::Format(LOCTEXT("CauseConvictedKiller", "Voted out at the meeting on {0}. They were right: you were a saboteur."), DayName)
			: FText::Format(LOCTEXT("CauseConvicted", "Voted out at the meeting on {0}. You were innocent. The saboteurs thank you for your service."), DayName);
	}
	else if (State.HumanExitCause == EUOExitCause::Dismissed)
	{
		Cause = FText::Format(LOCTEXT("CauseDismissed", "Dismissed on {0}. They found you next to a termination letter. HR has already reassigned your stapler."), DayName);
	}
	else
	{
		Cause = FText::Format(LOCTEXT("CauseFired", "Zero stars on {0}. Security already packed your box."), DayName);
	}

	auto Stat = [](const FText& Label, const FText& Value, const FLinearColor& Color)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[FUOUIStyle::MakeSectionLabel(Label)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[SNew(STextBlock).Text(Value).Font(FUOUIStyle::Title(44.f)).ColorAndOpacity(Color)];
	};

	auto Choice = [this](EUOEvalChoiceType Type)
	{
		return FOnClicked::CreateLambda([this, Type]()
		{
			const_cast<SUOEvaluationFlowWidget*>(this)->Submit(Type);
			return FReply::Handled();
		});
	};

	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Cause).Font(FUOUIStyle::Body(18.f)).ColorAndOpacity(FUOUIStyle::Text()).WrapTextAt(900.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 64.f, 0.f)[Stat(LOCTEXT("DaysSurvived", "Days survived"), FText::AsNumber(State.DaysSurvived), FLinearColor::White)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 24.f, 0.f)[Stat(LOCTEXT("Best", "Best record"), FText::AsNumber(State.BestDaysSurvived), FUOUIStyle::Gold())]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(&Style.PillAccentBrush)
				.Padding(FMargin(10.f, 4.f))
				.Visibility(State.bNewRecord ? EVisibility::Visible : EVisibility::Collapsed)
				[
					SNew(STextBlock).Text(LOCTEXT("NewRecord", "NEW RECORD")).Font(FUOUIStyle::Label(12.f))
				]
			]
		];

	TSharedRef<SWidget> Footer =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GameOverHint", "Records are kept per AI difficulty."))
			.Font(FUOUIStyle::Body(12.f))
			.ColorAndOpacity(FUOUIStyle::TextDim())
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[FUOUIStyle::MakeKeyCap(LOCTEXT("Key1", "1"), 26.f)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 20.f, 0.f)[FUOUIStyle::MakeButton(LOCTEXT("NewHire", "New hire"), Choice(EUOEvalChoiceType::NewHire), true, true, 180.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[FUOUIStyle::MakeKeyCap(LOCTEXT("Key2", "2"), 26.f)]
		+ SHorizontalBox::Slot().AutoWidth()[FUOUIStyle::MakeButton(LOCTEXT("MainMenu", "Main menu"), Choice(EUOEvalChoiceType::MainMenu), false, true, 180.f)];

	const FText Title = bVictory
		? (State.bKillersTakeOver ? LOCTEXT("TakeoverTitle", "THE SABOTEURS TAKE OVER")
			: (State.bKillerVictory ? LOCTEXT("KillerVictoryTitle", "THE KILLER GETS PROMOTED") : LOCTEXT("VictoryTitle", "LAST ONE STANDING")))
		: (State.bKillersTakeOver ? LOCTEXT("TakeoverLossTitle", "THE SABOTEURS TAKE OVER")
			: (State.HumanExitCause == EUOExitCause::Dismissed ? LOCTEXT("DismissedTitle", "YOU'VE BEEN DISMISSED") : LOCTEXT("GameOverTitle", "YOU'VE BEEN LET GO")));
	return MakeFrame(Title, FText::GetEmpty(), Body, Footer, 1000.f);
}

#undef LOCTEXT_NAMESPACE
