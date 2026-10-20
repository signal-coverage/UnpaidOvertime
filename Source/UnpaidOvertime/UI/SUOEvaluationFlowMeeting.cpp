// UNPAID: OVERTIME - evaluation screens of the killers and the meeting (M4.1): the killers' private fake rating, the
// emergency meeting card, the open debate, the live public vote, the rock-paper-scissors duel, the saving throw and
// the verdict.

#include "SUOEvaluationFlowWidget.h"
#include "SUOIcon.h"
#include "UOUIStyle.h"
#include "UOPlayerController.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "UOMeetingRules.h"
#include "UOBossQuestions.h"
#include "UOStarRating.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOEvaluationFlow"

namespace UOEvaluationFlowMeetingPrivate
{
	constexpr float CountdownWidth = 1080.f;

	EUOIconType PickIcon(EUORPSChoice Pick)
	{
		switch (Pick)
		{
		case EUORPSChoice::Rock:		return EUOIconType::Rock;
		case EUORPSChoice::Paper:		return EUOIconType::Paper;
		case EUORPSChoice::Scissors:	return EUOIconType::Scissors;
		default:						return EUOIconType::Circle;
		}
	}
}

const AUOPlayerState* SUOEvaluationFlowWidget::GetMe() const
{
	const AUOPlayerController* PC = Owner.Get();
	return PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
}

TArray<int32> SUOEvaluationFlowWidget::GetPresentablePins() const
{
	TArray<int32> Pins;
	const AUOPlayerState* Me = GetMe();
	const FUOEvaluationState* State = GetState();
	if (!Me || !State)
	{
		return Pins;
	}
	int32 Presented = 0;
	for (const FUOPresentedEvidenceView& View : State->Meeting.Presented)
	{
		Presented += View.PresenterId == Me->GetEmployeeId() ? 1 : 0;
	}
	if (Presented >= FUOMeetingTuning::MaxPresentedEvidence)
	{
		return Pins;
	}
	for (const int32 TraceId : Me->GetPinnedTraceIds())
	{
		if (!State->Meeting.Presented.ContainsByPredicate([TraceId](const FUOPresentedEvidenceView& View) { return View.Entry.Id == TraceId; }))
		{
			Pins.Add(TraceId);
		}
	}
	return Pins;
}

TArray<int32> SUOEvaluationFlowWidget::GetMeetingTargets() const
{
	TArray<int32> Targets;
	if (const AUOGameState* GameState = GetGameState())
	{
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && Employee->IsAlive() && Employee->GetEmployeeId() != GetMyId())
			{
				Targets.Add(Employee->GetEmployeeId());
			}
		}
	}
	return Targets;
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeEvidenceCard(const FUOTraceEntry& Entry, bool bSelected, int32 PresenterId, float WrapWidth) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const bool bIncident = FUOSuspicionModel::IsSabotageTrace(Entry.Kind) || Entry.Kind == EUOTraceKind::LastSeenNear;
	return SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.BorderBackgroundColor(bSelected ? FUOUIStyle::Gold() * FLinearColor(1.f, 1.f, 1.f, 0.7f) : FLinearColor::White)
		.Padding(FMargin(12.f, 7.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Entry.ClockTime)).Font(FUOUIStyle::Font("Bold", 13)).ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FUOTraceText::GetDeviceName(Entry.Device).ToUpper()).Font(FUOUIStyle::Label(7.f)).ColorAndOpacity(FUOUIStyle::TextDim())
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FUOTraceText::Describe(Entry, [this](int32 Id) { return NameOf(Id); }))
					.Font(FUOUIStyle::Body(13.f))
					.ColorAndOpacity(bIncident ? FUOUIStyle::Bad() : (FUOSuspicionModel::IsNoOutputTrace(Entry.Kind) ? FUOUIStyle::Warning() : FUOUIStyle::Text()))
					.WrapTextAt(WrapWidth)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility(PresenterId != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(FText::Format(LOCTEXT("PresentedBy", "put on the table by {0}"), FText::FromString(NameOf(PresenterId))))
					.Font(FUOUIStyle::Label(8.f))
					.ColorAndOpacity(FUOUIStyle::TextDim())
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SUOIcon).Type(EUOIconType::Check).Size(18.f).Thickness(3.f).Color(FUOUIStyle::Gold())
				.Visibility(bSelected ? EVisibility::Visible : EVisibility::Collapsed)
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeCountdown(float StartServerTime, float Seconds) const
{
	using namespace UOEvaluationFlowMeetingPrivate;
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const float Limit = FMath::Max(0.1f, Seconds);
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.HeightOverride(8.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(&Style.BarBackBrush)]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride_Lambda([this, StartServerTime, Limit]()
					{
						return FOptionalSize(960.f * FMath::Clamp(1.f - (GetServerTime() - StartServerTime) / Limit, 0.f, 1.f));
					})
					[
						SNew(SImage)
						.Image(&Style.BarFillBrush)
						.ColorAndOpacity_Lambda([this, StartServerTime, Limit]()
						{
							const float Left = Limit - (GetServerTime() - StartServerTime);
							return FSlateColor(Left <= FUOMeetingTuning::TickLastSeconds ? FUOUIStyle::AccentBright() : FUOUIStyle::Gold());
						})
					]
				]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(70.f)
			[
				SNew(STextBlock)
				.Font(FUOUIStyle::Title(28.f))
				.ColorAndOpacity_Lambda([this, StartServerTime, Limit]()
				{
					const float Left = Limit - (GetServerTime() - StartServerTime);
					return FSlateColor(Left <= FUOMeetingTuning::TickLastSeconds ? FUOUIStyle::AccentBright() : FLinearColor::White);
				})
				.Text_Lambda([this, StartServerTime, Limit]()
				{
					return FText::Format(LOCTEXT("CountdownSeconds", "{0}s"), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Limit - (GetServerTime() - StartServerTime)))));
				})
			]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakePresentedStrip(const FUOEvaluationState& State, float MaxHeight, float WrapWidth) const
{
	TSharedRef<SVerticalBox> Cards = SNew(SVerticalBox);
	for (const FUOPresentedEvidenceView& View : State.Meeting.Presented)
	{
		Cards->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 5.f)[MakeEvidenceCard(View.Entry, false, View.PresenterId, WrapWidth)];
	}
	if (State.Meeting.Presented.Num() == 0)
	{
		Cards->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("NothingOnTable", "Nothing on the table yet.")).Font(FUOUIStyle::Body(13.f)).ColorAndOpacity(FUOUIStyle::TextDim())
		];
	}
	return SNew(SBox)
		.MaxDesiredHeight(MaxHeight)
		[
			SNew(SScrollBox) + SScrollBox::Slot()[Cards]
		];
}

TSharedRef<SWidget> SUOEvaluationFlowWidget::MakeRPSIcon(EUORPSChoice Pick, float Size, const FLinearColor& Color) const
{
	return SNew(SUOIcon).Type(UOEvaluationFlowMeetingPrivate::PickIcon(Pick)).Size(Size).Thickness(FMath::Max(2.f, Size / 18.f)).Color(Color);
}

// ~ Rating (a killer's private fake rating)

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildRating(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOPlayerState* Me = GetMe();
	const bool bPrivate = Me && Me->GetKnownRole() == EUORole::Killer && Me->GetKillerView().bFakeRatingPending;
	if (!bPrivate)
	{
		return MakeFrame(LOCTEXT("RatingTitle", "FINALIZING RATINGS"), LOCTEXT("RatingSubtitle", "HR is double-checking the numbers. Please remain seated."),
			SNew(STextBlock).Text(LOCTEXT("RatingWait", "Stars are being stamped…")).Font(FUOUIStyle::Body(16.f)).ColorAndOpacity(FUOUIStyle::TextDim()),
			SNew(STextBlock).Text(LOCTEXT("RatingHint", "Everyone's review appears at the same time.")).Font(FUOUIStyle::Body(12.f)).ColorAndOpacity(FUOUIStyle::TextDim()), 900.f);
	}

	const FUOKillerPrivateView& View = Me->GetKillerView();
	TSharedRef<SHorizontalBox> Choices = SNew(SHorizontalBox);
	for (const int32 Stars : View.FakeAllowed)
	{
		FText Note;
		FLinearColor NoteColor = FUOUIStyle::Good();
		if (Stars == 1)
		{
			Note = LOCTEXT("FakeOneNote", "MANDATORY DEFENSE QUESTION · your answer is accepted, in public · never a cut-vote candidate");
			NoteColor = FUOUIStyle::Warning();
		}
		else if (Stars == 3)
		{
			Note = LOCTEXT("FakeThreeNote", "PROMOTION QUESTION · accepted, in public · a FORGED shield: looks real, never protects");
			NoteColor = FUOUIStyle::Gold();
		}
		else
		{
			Note = LOCTEXT("FakeTwoNote", "NOTHING SPECIAL · the quiet choice");
		}
		Choices->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(300.f)
			[
				SNew(SButton)
				.ButtonStyle(&Style.ChoiceButton)
				.ContentPadding(FMargin(18.f, 16.f))
				.Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this, Stars]()
				{
					const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::FakeRating, Stars);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[FUOUIStyle::MakeKeyCap(FText::AsNumber(Stars), 26.f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeStars(Stars, 24.f)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(Note).Font(FUOUIStyle::Label(9.f)).ColorAndOpacity(NoteColor).WrapTextAt(250.f)
					]
				]
			]
		];
	}

	const float Deadline = View.FakeDeadline;
	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(FUOUIStyle::Accent())
			.Padding(FMargin(20.f, 14.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("RatingFacts", "Your real work today earned {0} star(s). Nobody else will ever see that number."), FText::AsNumber(View.FakeRealStars)))
					.Font(FUOUIStyle::Font("Bold", 16))
					.ColorAndOpacity(FLinearColor::White)
					.WrapTextAt(900.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RatingRules", "Show 1, 2 or 3 stars, freely. You are never fired for zero stars. Any answer you give the boss tonight is accepted, but everyone hears it: whoever knows the truth will notice."))
					.Font(FUOUIStyle::Body(14.f))
					.ColorAndOpacity(FUOUIStyle::Text())
					.WrapTextAt(900.f)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			Choices
		];

	TSharedRef<SWidget> Footer =
		SNew(STextBlock)
		.Font(FUOUIStyle::Body(12.f))
		.ColorAndOpacity(FUOUIStyle::TextDim())
		.Text_Lambda([this, Deadline]()
		{
			return FText::Format(LOCTEXT("RatingTimer", "Private: the others only see \"finalizing ratings\". {0}s left, then your honest value is shown."),
				FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Deadline - GetServerTime()))));
		});

	return MakeFrame(LOCTEXT("FakeTitle", "YOUR RATING (PRIVATE)"), LOCTEXT("FakeSubtitle", "Choose the stars your coworkers will see on the review."), Body, Footer, 1060.f);
}

// ~ Emergency meeting

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildEmergency(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	FString LastSeen;
	for (const int32 Id : State.LastSeenIds)
	{
		LastSeen += (LastSeen.IsEmpty() ? TEXT("") : TEXT(", ")) + NameOf(Id);
	}
	TSharedRef<SWidget> Body =
		SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.BorderBackgroundColor(FUOUIStyle::Bad())
		.Padding(FMargin(26.f, 22.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
			[
				SNew(SUOIcon).Type(EUOIconType::Cross).Size(48.f).Thickness(4.f).Color(FUOUIStyle::Bad())
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("VictimLine", "{0} has been dismissed."), FText::FromString(NameOf(State.VictimId))))
					.Font(FUOUIStyle::Font("Bold", 28)).ColorAndOpacity(FLinearColor::White).WrapTextAt(760.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("VictimWhere", "Found in the {0} at {1}, next to a termination letter. Nobody saw a thing. Someone in this office did this."),
						FUORooms::GetDisplayName(State.VictimRoom), FText::FromString(State.VictimClockTime)))
					.Font(FUOUIStyle::Body(16.f)).ColorAndOpacity(FUOUIStyle::Text()).WrapTextAt(760.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility(LastSeen.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					.Text(FText::Format(LOCTEXT("LastSeenLine", "Public note: last seen near the {0} before the incident: {1}."), FUORooms::GetDisplayName(State.VictimRoom), FText::FromString(LastSeen)))
					.Font(FUOUIStyle::Font("Bold", 15)).ColorAndOpacity(FUOUIStyle::Warning()).WrapTextAt(760.f)
				]
			]
		];

	return MakeFrame(LOCTEXT("EmergencyTitle", "EMERGENCY MEETING"), LOCTEXT("EmergencySubtitle", "The clock is stopped. Open debate, then a public vote. Then the shift resumes."), Body,
		MakeContinueFooter(LOCTEXT("EmergencyHint", "Everything logged so far can be put on the table."), LOCTEXT("OpenMeeting", "Open the meeting")), 1000.f);
}

// ~ Debate

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildDebate(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FUOMeetingView& Meeting = State.Meeting;
	const bool bMine = IsMyPrompt(EUOEvalPrompt::Debate);

	// Statements feed (newest at the bottom)
	TSharedRef<SVerticalBox> Feed = SNew(SVerticalBox);
	for (const FUODebateStatementView& Statement : Meeting.Statements)
	{
		const FLinearColor Accent = Statement.Kind == 0 ? FUOUIStyle::AccentBright() : (Statement.Kind == 1 ? FUOUIStyle::Warning() : FUOUIStyle::TextDim());
		Feed->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.Padding(FMargin(14.f, 9.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SBox).WidthOverride(4.f).HeightOverride(36.f)[SNew(SImage).Image(&Style.WhiteBrush).ColorAndOpacity(Accent)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[MakeEmployeeChip(Statement.SpeakerId, 13.f)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Quoted", "“{0}”"), Statement.Text))
						.Font(FUOUIStyle::Body(14.f))
						.ColorAndOpacity(Statement.Kind == 3 ? FUOUIStyle::TextDim() : FLinearColor::White)
						.WrapTextAt(560.f)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Visibility(Statement.TraceIds.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(FText::Format(LOCTEXT("PutOnTable", "+ {0} line(s) put on the table"), FText::AsNumber(Statement.TraceIds.Num())))
						.Font(FUOUIStyle::Label(8.f))
						.ColorAndOpacity(FUOUIStyle::Gold())
					]
				]
			]
		];
	}
	if (Meeting.Statements.Num() == 0)
	{
		Feed->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("Silence", "Awkward silence. Someone clears their throat…")).Font(FUOUIStyle::Body(14.f)).ColorAndOpacity(FUOUIStyle::TextDim())
		];
	}

	// The human's pinned lines
	TSharedRef<SVerticalBox> Pins = SNew(SVerticalBox);
	if (bMine)
	{
		const TArray<int32> Presentable = GetPresentablePins();
		const AUOGameState* GameState = GetGameState();
		for (int32 Index = 0; Index < Presentable.Num() && GameState; ++Index)
		{
			const FUOTraceEntry* Entry = GameState->FindTrace(Presentable[Index]);
			if (!Entry)
			{
				continue;
			}
			const int32 TraceId = Presentable[Index];
			Pins->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 5.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					FUOUIStyle::MakeKeyCap(FText::AsNumber(Index + 1), 22.f)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
					.ContentPadding(FMargin(0.f))
					.Cursor(EMouseCursor::Hand)
					.ToolTipText(LOCTEXT("PresentTip", "Put this line on the table for everyone"))
					.OnClicked_Lambda([this, TraceId]()
					{
						const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::PresentEvidence, TraceId);
						return FReply::Handled();
					})
					[
						MakeEvidenceCard(*Entry, false)
					]
				]
			];
		}
		if (Presentable.Num() == 0)
		{
			Pins->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoPins", "No pinned lines left to present. Pin lines on the terminals during the shift (up to 3 per meeting go on the table)."))
				.Font(FUOUIStyle::Body(12.f)).ColorAndOpacity(FUOUIStyle::TextDim()).WrapTextAt(560.f)
			];
		}
	}

	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)[MakeCountdown(Meeting.PhaseStartServerTime, Meeting.PhaseSeconds)]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.6f).Padding(0.f, 0.f, 18.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("TheFloor", "The floor"))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).MaxDesiredHeight(bMine ? 300.f : 470.f)[SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll) + SScrollBox::Slot()[Feed]]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 8.f)
				[
					SNew(SBox).Visibility(bMine ? EVisibility::Visible : EVisibility::Collapsed)[FUOUIStyle::MakeSectionLabel(LOCTEXT("YourPins", "Your pinned lines: click or 1-3 to present"))]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).MaxDesiredHeight(170.f)[SNew(SScrollBox) + SScrollBox::Slot()[Pins]]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.4f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("OnTheTable", "On the table (public)"))]
				+ SVerticalBox::Slot().AutoHeight()[MakePresentedStrip(State, 480.f, 290.f)]
			]
		];

	TSharedRef<SWidget> Footer =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DebateHint", "Say what you saw. Then everyone votes, in public. The AIs may be wrong: they only know what they saw, read or heard here."))
			.Font(FUOUIStyle::Body(12.f))
			.ColorAndOpacity(FUOUIStyle::TextDim())
			.WrapTextAt(760.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBox).Visibility(bMine ? EVisibility::Visible : EVisibility::Collapsed)[FUOUIStyle::MakeKeyCap(LOCTEXT("KeyS", "S"), 26.f)]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox)
			.Visibility(bMine ? EVisibility::Visible : EVisibility::Collapsed)
			[
				FUOUIStyle::MakeButton(LOCTEXT("SkipDebate", "Skip to the vote"), FOnClicked::CreateLambda([this]()
				{
					const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::SkipDebate);
					return FReply::Handled();
				}), false, true, 200.f)
			]
		];

	return MakeFrame(State.bEmergency ? LOCTEXT("EmergencyDebateTitle", "EMERGENCY MEETING · DEBATE") : LOCTEXT("DebateTitle", "MEETING · OPEN DEBATE"),
		LOCTEXT("DebateSubtitle", "60 seconds to make your case. Anyone may put up to 3 pinned lines on the table; they become public."), Body, Footer, 1180.f);
}

// ~ Live vote

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildMeetingVote(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FUOMeetingView& Meeting = State.Meeting;
	const bool bMine = IsMyPrompt(EUOEvalPrompt::MeetingVote);
	const AUOGameState* GameState = GetGameState();
	const int32 MyId = GetMyId();
	const FUOMeetingBallotView* MyBallot = Meeting.Ballots.FindByPredicate([MyId](const FUOMeetingBallotView& Ballot) { return Ballot.VoterId == MyId; });
	const int32 MyTarget = MyBallot ? MyBallot->TargetId : INDEX_NONE;
	const TArray<int32> MyTargets = GetMeetingTargets();

	TArray<int32> Alive;
	if (GameState)
	{
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && Employee->IsAlive())
			{
				Alive.Add(Employee->GetEmployeeId());
			}
		}
	}

	TSharedRef<SWrapBox> Cards = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 10.f));
	for (const int32 Candidate : Alive)
	{
		TArray<int32> Voters;
		for (const FUOMeetingBallotView& Ballot : Meeting.Ballots)
		{
			if (Ballot.TargetId == Candidate)
			{
				Voters.Add(Ballot.VoterId);
			}
		}
		const int32 TallyIndex = Meeting.TallyIds.IndexOfByKey(Candidate);
		const int32 Votes = Meeting.bTallyFinal && Meeting.TallyVotes.IsValidIndex(TallyIndex) ? Meeting.TallyVotes[TallyIndex] : Voters.Num();
		const bool bLeader = Meeting.bTallyFinal && Meeting.Leaders.Contains(Candidate);
		const bool bMyPick = MyTarget == Candidate;
		const int32 KeyIndex = MyTargets.IndexOfByKey(Candidate);
		FString VoterNames;
		for (const int32 Voter : Voters)
		{
			VoterNames += (VoterNames.IsEmpty() ? TEXT("") : TEXT(", ")) + NameOf(Voter) + (Meeting.IgnoredVoters.Contains(Voter) ? TEXT(" (not counted)") : TEXT(""));
		}

		TSharedRef<SWidget> Card =
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(bLeader ? FUOUIStyle::Bad() : (bMyPick ? FUOUIStyle::Gold() : FLinearColor::White))
			.Padding(FMargin(14.f, 12.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeEmployeeChip(Candidate, 14.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock).Text(FText::AsNumber(Votes)).Font(FUOUIStyle::Title(32.f)).ColorAndOpacity(Votes > 0 ? FUOUIStyle::AccentBright() : FUOUIStyle::TextDim())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(6.f, 0.f, 0.f, 5.f)
					[
						SNew(STextBlock).Text(Votes == 1 ? LOCTEXT("MVoteOne", "vote") : LOCTEXT("MVoteMany", "votes")).Font(FUOUIStyle::Body(12.f)).ColorAndOpacity(FUOUIStyle::TextDim())
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(VoterNames.IsEmpty() ? LOCTEXT("NoVotersYet", "—") : FText::FromString(VoterNames))
					.Font(FUOUIStyle::Body(11.f)).ColorAndOpacity(FUOUIStyle::TextDim()).WrapTextAt(230.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SBox)
					.Visibility(bMine && KeyIndex != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[FUOUIStyle::MakeKeyCap(FText::AsNumber(KeyIndex + 1), 22.f)]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							FUOUIStyle::MakeButton(bMyPick ? LOCTEXT("YourVoteBtn", "Your vote") : LOCTEXT("VoteHere", "Vote"), FOnClicked::CreateLambda([this, Candidate]()
							{
								const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::MeetingVote, Candidate);
								return FReply::Handled();
							}), !bMyPick, true, 120.f)
						]
					]
				]
			];
		Cards->AddSlot()[SNew(SBox).WidthOverride(268.f)[Card]];
	}

	FText Status;
	if (Meeting.bTallyFinal)
	{
		if (Meeting.Leaders.Num() == 0)
		{
			Status = LOCTEXT("TallyNobody", "Time. Nobody received a counted vote: nobody leaves.");
		}
		else if (Meeting.Leaders.Num() == 1)
		{
			Status = FText::Format(LOCTEXT("TallyOne", "Time. Most votes: {0}. Unless a shield or the boss saves them, they leave."), FText::FromString(NameOf(Meeting.Leaders[0])));
		}
		else
		{
			Status = LOCTEXT("TallyTie", "Time. A TIE: rock-paper-scissors decides who leaves.");
		}
	}
	else if (bMine)
	{
		Status = MyTarget == INDEX_NONE
			? LOCTEXT("YouAbstain", "You abstain (default). Click a card or press its number to vote; you can change it until the time runs out.")
			: FText::Format(LOCTEXT("YouVoteFor", "Your vote: {0}. Click another card to change it, or 0 to abstain."), FText::FromString(NameOf(MyTarget)));
	}
	else
	{
		Status = LOCTEXT("Watching", "Votes are public and live.");
	}

	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
		[
			SNew(SBox).Visibility(Meeting.bTallyFinal ? EVisibility::Collapsed : EVisibility::Visible)[MakeCountdown(Meeting.PhaseStartServerTime, Meeting.PhaseSeconds)]
		]
		+ SVerticalBox::Slot().AutoHeight()[Cards]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 8.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("TableDuringVote", "On the table"))]
		+ SVerticalBox::Slot().AutoHeight()[MakePresentedStrip(State, 150.f, 960.f)];

	TSharedRef<SWidget> Footer =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Status).Font(FUOUIStyle::Font("Bold", 14)).ColorAndOpacity(Meeting.bTallyFinal ? FUOUIStyle::Warning() : FUOUIStyle::Text()).WrapTextAt(820.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBox).Visibility(bMine ? EVisibility::Visible : EVisibility::Collapsed)[FUOUIStyle::MakeKeyCap(LOCTEXT("Key0", "0"), 26.f)]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox)
			.Visibility(bMine ? EVisibility::Visible : EVisibility::Collapsed)
			[
				FUOUIStyle::MakeButton(LOCTEXT("Abstain", "Abstain"), FOnClicked::CreateLambda([this]()
				{
					const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::MeetingVote, INDEX_NONE);
					return FReply::Handled();
				}), false, MyTarget != INDEX_NONE, 160.f)
			]
		];

	return MakeFrame(State.bEmergency ? LOCTEXT("EmergencyVoteTitle", "EMERGENCY MEETING · VOTE") : LOCTEXT("MeetingVoteTitle", "MEETING · VOTE"),
		LOCTEXT("MeetingVoteSubtitle", "15 seconds. One public vote each, for anyone but yourself; change it as often as you like. Most votes leaves. The most voted's own votes are shown but do not count."),
		Body, Footer, 1220.f);
}

// ~ Rock-paper-scissors duel

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildDuel(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FUODuelView& Duel = State.Meeting.Duel;
	const bool bPicking = IsMyPrompt(EUOEvalPrompt::DuelPick);
	const FUODuelRoundView* Last = Duel.Rounds.Num() > 0 ? &Duel.Rounds.Last() : nullptr;
	const bool bShowLast = Last && !Duel.bPicking;

	auto Side = [&](int32 EmployeeId, int32 Wins, bool bA) -> TSharedRef<SWidget>
	{
		const EUORPSChoice Pick = bShowLast ? (bA ? Last->PickA : Last->PickB) : EUORPSChoice::None;
		const bool bWonRound = bShowLast && Last->Winner == (bA ? 0 : 1);
		const bool bLost = Duel.LoserId == EmployeeId;
		TSharedRef<SHorizontalBox> Dots = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < FUOMeetingTuning::DuelWinsNeeded; ++Index)
		{
			Dots->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SUOIcon).Type(Index < Wins ? EUOIconType::StarFilled : EUOIconType::StarEmpty).Size(22.f).Color(Index < Wins ? FUOUIStyle::Gold() : FLinearColor(1.f, 1.f, 1.f, 0.25f))
			];
		}
		return SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(bLost ? FUOUIStyle::Bad() : (bWonRound ? FUOUIStyle::Gold() : FLinearColor::White))
			.Padding(FMargin(22.f, 18.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[MakeEmployeeChip(EmployeeId, 18.f)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 0.f)[Dots]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 18.f, 0.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(120.f)
					.HeightOverride(120.f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SOverlay)
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SBox).Visibility(Pick != EUORPSChoice::None ? EVisibility::Visible : EVisibility::Collapsed)
							[
								MakeRPSIcon(Pick, 110.f, bWonRound ? FUOUIStyle::Gold() : FUOUIStyle::Text())
							]
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Visibility(Pick == EUORPSChoice::None ? EVisibility::Visible : EVisibility::Collapsed)
							.Text(Duel.bPicking ? LOCTEXT("Choosing", "choosing…") : LOCTEXT("QuestionMark", "?"))
							.Font(FUOUIStyle::Title(28.f))
							.ColorAndOpacity(FUOUIStyle::TextDim())
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Pick != EUORPSChoice::None ? FUOMeetingText::GetPickName(Pick).ToUpper() : FText::GetEmpty())
					.Font(FUOUIStyle::Label(12.f))
					.ColorAndOpacity(bWonRound ? FUOUIStyle::Gold() : FUOUIStyle::TextDim())
				]
			];
	};

	TSharedRef<SWidget> PickRow = SNullWidget::NullWidget;
	if (bPicking)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		const EUORPSChoice Options[] = { EUORPSChoice::Rock, EUORPSChoice::Paper, EUORPSChoice::Scissors };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const EUORPSChoice Option = Options[Index];
			Row->AddSlot()
			.AutoWidth()
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(200.f)
				[
					SNew(SButton)
					.ButtonStyle(&Style.ChoiceButton)
					.ContentPadding(FMargin(14.f, 12.f))
					.Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([this, Option]()
					{
						const_cast<SUOEvaluationFlowWidget*>(this)->Submit(EUOEvalChoiceType::DuelPick, static_cast<int32>(Option));
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[FUOUIStyle::MakeKeyCap(FText::AsNumber(Index + 1), 24.f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[MakeRPSIcon(Option, 42.f, FUOUIStyle::Text())]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FUOMeetingText::GetPickName(Option)).Font(FUOUIStyle::Font("Bold", 16))]
					]
				]
			];
		}
		PickRow = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MakeCountdown(Duel.PickDeadline - FUOMeetingTuning::DuelPickSeconds, FUOMeetingTuning::DuelPickSeconds)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Row];
	}

	FText Status;
	if (Duel.LoserId != INDEX_NONE)
	{
		Status = Duel.LoserId == GetMyId()
			? LOCTEXT("DuelLostYou", "You lose the duel. A shield or the boss may still save you.")
			: FText::Format(LOCTEXT("DuelLost", "{0} loses the duel. A shield or the boss may still save them."), FText::FromString(NameOf(Duel.LoserId)));
	}
	else if (bShowLast)
	{
		Status = Last->Winner == INDEX_NONE ? LOCTEXT("DuelDraw", "Draw. Again.")
			: FText::Format(LOCTEXT("DuelRoundWon", "{0} takes the round."), FText::FromString(NameOf(Last->Winner == 0 ? Duel.SideA : Duel.SideB)));
	}
	else
	{
		Status = bPicking ? LOCTEXT("PickNow", "Pick: 1 rock, 2 paper, 3 scissors. No pick in time = a random one.") : LOCTEXT("TheyPick", "Rock, paper, scissors…");
	}

	FText Subtitle = LOCTEXT("DuelSubtitle", "A tie. Rock-paper-scissors, best of 3: the loser leaves.");
	if (Duel.TiedIds.Num() > 2)
	{
		Subtitle = FText::Format(LOCTEXT("DuelSubtitleN", "A {0}-way tie. A draw picked two of them. Rock-paper-scissors, best of 3: the loser leaves."), FText::AsNumber(Duel.TiedIds.Num()));
	}

	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Side(Duel.SideA, Duel.WinsA, true)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(26.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Versus", "VS")).Font(FUOUIStyle::Title(40.f)).ColorAndOpacity(FUOUIStyle::AccentBright())
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[Side(Duel.SideB, Duel.WinsB, false)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f)[PickRow];

	return MakeFrame(LOCTEXT("DuelTitle", "TIEBREAK · ROCK, PAPER, SCISSORS"), Subtitle, Body,
		SNew(STextBlock).Text(Status).Font(FUOUIStyle::Font("Bold", 15)).ColorAndOpacity(FUOUIStyle::Text()).WrapTextAt(1000.f), 1100.f);
}

// ~ Saving throw

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildSavingThrow(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FUOMeetingView& Meeting = State.Meeting;
	const FText Who = FText::FromString(NameOf(Meeting.CandidateId));

	FText Headline;
	FText Line;
	FLinearColor Color = FUOUIStyle::Warning();
	EUOIconType Icon = EUOIconType::Circle;
	switch (Meeting.ShieldCheck)
	{
	case EUOShieldCheck::ShieldUsed:
		Headline = FText::Format(LOCTEXT("SaveShieldHead", "{0} raises a promotion shield."), Who);
		Line = LOCTEXT("SaveShieldLine", "It absorbs the vote. The shield is spent; nobody leaves this meeting.");
		Color = FUOUIStyle::Gold();
		Icon = EUOIconType::Shield;
		break;
	case EUOShieldCheck::Forgery:
		Headline = LOCTEXT("ForgeryHead", "The shield was a forgery.");
		Line = FText::Format(LOCTEXT("ForgeryLine", "{0}'s shield crumbles to confetti. It never protected anything. Only the boss can save them now."), Who);
		Color = FUOUIStyle::Bad();
		Icon = EUOIconType::Cross;
		break;
	default:
		Headline = FText::Format(LOCTEXT("NoShieldHead", "{0} has no shield."), Who);
		Line = LOCTEXT("NoShieldLine", "One question from the boss. Right: they stay, on probation (2 real stars at the next review or fired). Wrong: they leave.");
		break;
	}

	TSharedRef<SWidget> Card =
		SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.BorderBackgroundColor(Color)
		.Padding(FMargin(24.f, 18.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SUOIcon).Type(Meeting.ShieldCheck == EUOShieldCheck::None ? EUOIconType::Circle : EUOIconType::Shield).Size(48.f).Thickness(3.f).Color(Color)]
				+ SOverlay::Slot()
				[
					SNew(SUOIcon).Type(EUOIconType::Cross).Size(48.f).Thickness(5.f).Color(FUOUIStyle::AccentBright())
					.Visibility(Meeting.ShieldCheck == EUOShieldCheck::Forgery ? EVisibility::Visible : EVisibility::Collapsed)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Headline).Font(FUOUIStyle::Font("Bold", 24)).ColorAndOpacity(FLinearColor::White).WrapTextAt(860.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[SNew(STextBlock).Text(Line).Font(FUOUIStyle::Body(15.f)).ColorAndOpacity(FUOUIStyle::Text()).WrapTextAt(860.f)]
			]
		];

	TSharedRef<SWidget> Body =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Card]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)[State.Question.bActive ? BuildQuestionCard(State) : SNullWidget::NullWidget];

	return MakeFrame(LOCTEXT("SavingTitle", "VOTED OUT?"), FText::Format(LOCTEXT("SavingSubtitle", "{0} got the most votes. A real shield or the right answer can still save them."), Who),
		Body, SNew(STextBlock).Text(LOCTEXT("SavingHint", "Forged shields never protect anyone.")).Font(FUOUIStyle::Body(12.f)).ColorAndOpacity(FUOUIStyle::TextDim()), 1100.f);
}

// ~ Verdict

TSharedRef<SWidget> SUOEvaluationFlowWidget::BuildVerdict(const FUOEvaluationState& State) const
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FUOMeetingView& Meeting = State.Meeting;
	const FText Continue = State.bEmergency ? LOCTEXT("BackToWork", "Back to work") : LOCTEXT("ContinueVerdict", "Continue");

	FText Headline;
	FText Line;
	FLinearColor Color = FUOUIStyle::TextDim();
	EUOIconType Icon = EUOIconType::Circle;
	bool bStamp = false;
	if (Meeting.ExpelledId != INDEX_NONE)
	{
		const FText Who = FText::FromString(NameOf(Meeting.ExpelledId));
		Headline = Meeting.bExpelledWasKiller ? FText::Format(LOCTEXT("WasSaboteur", "{0} WAS A SABOTEUR."), Who) : FText::Format(LOCTEXT("WasEmployee", "{0} WAS AN EMPLOYEE."), Who);
		Line = Meeting.ExpelledId == GetMyId()
			? LOCTEXT("YouOut", "You have been voted out. Security is already holding the door.")
			: (Meeting.bExpelledWasKiller ? LOCTEXT("SaboteurOut", "Voted out, and rightly so. If there were more of them, they are still among you.")
				: LOCTEXT("InnocentOut", "Voted out, and innocent. Whoever did it is still on the payroll."));
		Color = Meeting.bExpelledWasKiller ? FUOUIStyle::Gold() : FUOUIStyle::Bad();
		Icon = EUOIconType::Cross;
	}
	else if (Meeting.bSaved)
	{
		Headline = Meeting.bProbation ? LOCTEXT("SavedHeadProbation", "SAVED, ON PROBATION") : LOCTEXT("SavedHeadShield", "SAVED BY A SHIELD");
		Line = Meeting.Note;
		Color = Meeting.bProbation ? FUOUIStyle::Warning() : FUOUIStyle::Gold();
		Icon = Meeting.bProbation ? EUOIconType::Circle : EUOIconType::Shield;
		bStamp = Meeting.bProbation;
	}
	else
	{
		Headline = LOCTEXT("NobodyLeaves", "NOBODY LEAVES");
		Line = Meeting.Note.IsEmpty() ? LOCTEXT("NobodyLeavesLine", "Nobody got a counted vote. Back to the spreadsheets.") : Meeting.Note;
	}

	TSharedRef<SWidget> Body =
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(Color)
			.Padding(FMargin(26.f, 24.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
				[
					SNew(SUOIcon).Type(Icon).Size(48.f).Thickness(4.f).Color(Color)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Headline).Font(FUOUIStyle::Title(36.f)).ColorAndOpacity(Color).WrapTextAt(780.f)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[SNew(STextBlock).Text(Line).Font(FUOUIStyle::Body(16.f)).ColorAndOpacity(FUOUIStyle::Text()).WrapTextAt(780.f)]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(0.f, -18.f, -10.f, 0.f)
		[
			SNew(SBox)
			.Visibility(bStamp ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-8.f))))
			[
				SNew(SBorder)
				.BorderImage(&Style.CardBrush)
				.BorderBackgroundColor(FUOUIStyle::Warning())
				.Padding(FMargin(14.f, 6.f))
				[
					SNew(STextBlock).Text(LOCTEXT("ProbationStamp", "ON PROBATION")).Font(FUOUIStyle::Title(26.f)).ColorAndOpacity(FUOUIStyle::Warning())
				]
			]
		];

	return MakeFrame(LOCTEXT("VerdictTitle", "VERDICT"), FText::GetEmpty(), Body,
		MakeContinueFooter(Meeting.ExpelledId != INDEX_NONE ? LOCTEXT("VerdictHint", "Roles are revealed only when someone leaves.") : LOCTEXT("VerdictHintNo", "No role was revealed."), Continue), 1000.f);
}

#undef LOCTEXT_NAMESPACE
