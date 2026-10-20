// UNPAID: OVERTIME - end-of-day performance review panel.

#include "SUOEvaluationWidget.h"
#include "UOUIStyle.h"
#include "SUOIcon.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOStarRating.h"
#include "UOTaskBoard.h"
#include "UODayClock.h"
#include "UODayCalendar.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOEvaluation"

namespace UOEvaluationPrivate
{
	FLinearColor ConsequenceColor(EUOStarConsequence Consequence)
	{
		switch (Consequence)
		{
		case EUOStarConsequence::Fired:					return FUOUIStyle::Bad();
		case EUOStarConsequence::CutVoteCandidate:		return FUOUIStyle::Warning();
		case EUOStarConsequence::PromotionAvailable:	return FUOUIStyle::Gold();
		default:										return FUOUIStyle::Good();
		}
	}

	constexpr float BarWidth = 220.f;
}

void SUOEvaluationWidget::Construct(const FArguments& InArgs)
{
	using namespace UOEvaluationPrivate;

	OnContinue = InArgs._OnContinue;
	const FUOUIStyle& Style = FUOUIStyle::Get();

	const APlayerController* PC = InArgs._Owner.Get();
	const AUOGameState* GameState = PC ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	Accomplices = Me ? Me->GetKillerView().AccompliceIds : TArray<int32>();

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	FText Summary;
	FText OvertimeLine;
	FText Totals;
	FText Company;
	FText DayTitle;
	bool bForceClosed = false;

	if (GameState)
	{
		bForceClosed = GameState->WasDayForceClosed();
		Company = FText::FromString(GameState->GetCompanyName().ToUpper());
		DayTitle = FText::Format(LOCTEXT("DayTitle", "END OF {0}"), FUODayCalendar::FormatDayName(GameState->GetDayNumber())).ToUpper();
		Summary = bForceClosed
			? FText::Format(LOCTEXT("ForceClosed", "The building closed at {0} with work still pending. Pending work counts as not done."), FText::FromString(GameState->GetClockText()))
			: FText::Format(LOCTEXT("AllDone", "All work done at {0}. HR is suspicious."), FText::FromString(GameState->GetClockText()));

		if (GameState->IsUnpaidOvertime())
		{
			const float OvertimeMinutes = FUODayClock::ComputeTimeOfDayMinutes(GameState->GetShiftElapsedSeconds(), GameState->GetShiftLengthSeconds()) - FUODayClock::ShiftEndMinutes;
			OvertimeLine = FText::Format(LOCTEXT("Overtime", "Unpaid overtime: {0} min. Logged by HR, compensated by nobody."), FText::AsNumber(FMath::Max(0, FMath::RoundToInt(OvertimeMinutes))));
		}

		const TArray<FUOTaskInstance>& Tasks = GameState->GetTasks();
		Totals = FText::Format(LOCTEXT("Totals", "{0} of {1} sub-objectives completed"), FText::AsNumber(FUOTaskBoard::CountDone(Tasks)), FText::AsNumber(FUOTaskBoard::CountTotal(Tasks)));

		TArray<const AUOPlayerState*> Employees;
		float ScaleMax = 1.f;
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			// Only employees rated today (people fired on earlier days are gone)
			if (Employee && Employee->HasEvaluation())
			{
				Employees.Add(Employee);
				ScaleMax = FMath::Max(ScaleMax, FMath::Max(static_cast<float>(Employee->GetCompletedCount()), Employee->GetFairShare() * 1.5f));
			}
		}
		Employees.Sort([](const AUOPlayerState& A, const AUOPlayerState& B) { return A.GetCompletedCount() > B.GetCompletedCount(); });

		for (const AUOPlayerState* Employee : Employees)
		{
			Rows->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				MakeEmployeeRow(*Employee, Employee == Me, ScaleMax)
			];
		}
	}

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&Style.WhiteBrush)
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(1120.f)
			[
				SNew(SBorder)
				.BorderImage(&Style.PanelSolidBrush)
				.Padding(FMargin(48.f, 40.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::Format(LOCTEXT("Kicker", "{0}  ·  {1}"), DayTitle, Company))
								.Font(FUOUIStyle::Label(11.f))
								.ColorAndOpacity(FUOUIStyle::AccentBright())
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 6.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Title", "PERFORMANCE REVIEW"))
								.Font(FUOUIStyle::Title(44.f))
								.ColorAndOpacity(FLinearColor::White)
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Bottom)
						.Padding(24.f, 0.f, 0.f, 8.f)
						[
							SNew(STextBlock)
							.Text(Totals)
							.Font(FUOUIStyle::Font("Bold", 14))
							.ColorAndOpacity(FUOUIStyle::Text())
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 14.f, 0.f, 2.f)
					[
						SNew(STextBlock)
						.Text(Summary)
						.Font(FUOUIStyle::Body(15.f))
						.ColorAndOpacity(bForceClosed ? FUOUIStyle::Bad() : FUOUIStyle::Good())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 22.f)
					[
						SNew(STextBlock)
						.Text(OvertimeLine)
						.Font(FUOUIStyle::Body(14.f))
						.ColorAndOpacity(FUOUIStyle::Warning())
						.Visibility(OvertimeLine.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					]

					// Column headers
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(16.f, 0.f, 16.f, 8.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("ColEmployee", "Employee"))]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(BarWidth + 120.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("ColContribution", "Done vs fair share"))]]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(110.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("ColStars", "Rating"))]]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(250.f)[FUOUIStyle::MakeSectionLabel(LOCTEXT("ColConsequence", "Outcome"))]]
					]

					+ SVerticalBox::Slot()
					.MaxHeight(440.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							Rows
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 22.f, 0.f, 20.f)
					[
						FUOUIStyle::MakeDivider()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Rules", "3 stars: 100% of fair share  ·  2 stars: 50%  ·  1 star: anything  ·  0 stars: nothing"))
							.Font(FUOUIStyle::Body(12.f))
							.ColorAndOpacity(FUOUIStyle::TextDim())
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							FUOUIStyle::MakeButton(LOCTEXT("Continue", "Continue"), FOnClicked::CreateLambda([this]()
							{
								OnContinue.ExecuteIfBound();
								return FReply::Handled();
							}), true, true, 180.f)
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SUOEvaluationWidget::MakeEmployeeRow(const AUOPlayerState& Employee, bool bIsMe, float ScaleMax) const
{
	using namespace UOEvaluationPrivate;

	const FUOUIStyle& Style = FUOUIStyle::Get();
	const float Done = static_cast<float>(Employee.GetCompletedCount());
	const float Fair = Employee.GetFairShare();
	const float DoneFraction = FMath::Clamp(Done / ScaleMax, 0.f, 1.f);
	const float FairFraction = FMath::Clamp(Fair / ScaleMax, 0.f, 1.f);
	const FLinearColor Outcome = ConsequenceColor(Employee.GetConsequence());

	static const FNumberFormattingOptions OneDecimal = FNumberFormattingOptions().SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);

	TSharedRef<SHorizontalBox> Stars = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < FUOStarRating::MaxStars; ++Index)
	{
		const bool bFilled = Index < Employee.GetStars();
		Stars->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SUOIcon)
			.Type(bFilled ? EUOIconType::StarFilled : EUOIconType::StarEmpty)
			.Size(22.f)
			.Color(bFilled ? FUOUIStyle::Gold() : FLinearColor(1.f, 1.f, 1.f, 0.22f))
		];
	}

	return SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.Padding(FMargin(16.f, 12.f))
		[
			SNew(SHorizontalBox)

			// Color chip + name
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(4.f)
				.HeightOverride(26.f)
				[
					SNew(SImage)
					.Image(&Style.WhiteBrush)
					.ColorAndOpacity(Employee.GetEmployeeColor())
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Employee.GetDisplayName()))
					.Font(FUOUIStyle::Font("Bold", 16))
					// Killers only: a fellow killer's name in red
					.ColorAndOpacity(Accomplices.Contains(Employee.GetEmployeeId()) ? FUOUIStyle::AccentBright() : FLinearColor::White)
					.ToolTipText(Accomplices.Contains(Employee.GetEmployeeId()) ? LOCTEXT("AccompliceTip", "Your accomplice") : FText::GetEmpty())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(&Style.PillAccentBrush)
					.Padding(FMargin(7.f, 2.f))
					.Visibility(bIsMe ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("You", "YOU"))
						.Font(FUOUIStyle::Label(9.f))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					.Visibility(Employee.HasShield() ? EVisibility::Visible : EVisibility::Collapsed)
					.ToolTipText(LOCTEXT("ShieldTip", "Promotion shields: each one cancels one expulsion (firing, cut vote or meeting)"))
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SUOIcon)
						.Type(EUOIconType::Shield)
						.Size(18.f)
						.Color(FUOUIStyle::Gold())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						// Your own forged shields are marked for you only
						.Text(bIsMe && Employee.GetFakeShieldCount() > 0
							? FText::Format(LOCTEXT("ShieldCountFake", "×{0} ({1} fake)"), FText::AsNumber(Employee.GetDisplayedShieldCount()), FText::AsNumber(Employee.GetFakeShieldCount()))
							: FText::Format(LOCTEXT("ShieldCount", "×{0}"), FText::AsNumber(Employee.GetDisplayedShieldCount())))
						.Font(FUOUIStyle::Label(11.f))
						.ColorAndOpacity(FUOUIStyle::Gold())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(&Style.CardBrush)
					.BorderBackgroundColor(FUOUIStyle::Warning())
					.Padding(FMargin(7.f, 2.f))
					.Visibility(Employee.IsOnProbation() ? EVisibility::Visible : EVisibility::Collapsed)
					.ToolTipText(LOCTEXT("ProbationTip", "On probation: 2 real stars today, or fired"))
					[
						SNew(STextBlock).Text(LOCTEXT("Probation", "PROBATION")).Font(FUOUIStyle::Label(9.f)).ColorAndOpacity(FUOUIStyle::Warning())
					]
				]


			]

			// Contribution bar with a fair-share marker
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(BarWidth + 120.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(BarWidth)
						.HeightOverride(10.f)
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								SNew(SImage).Image(&Style.BarBackBrush)
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Left)
							[
								SNew(SBox)
								.WidthOverride(FMath::Max(DoneFraction * BarWidth, Done > 0.f ? 4.f : 0.f))
								[
									SNew(SImage)
									.Image(&Style.BarFillBrush)
									.ColorAndOpacity(Outcome)
								]
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Left)
							.Padding(FMargin(FMath::Max(0.f, FairFraction * BarWidth - 1.f), -3.f, 0.f, -3.f))
							[
								SNew(SBox)
								.WidthOverride(2.f)
								[
									SNew(SImage)
									.Image(&Style.WhiteBrush)
									.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
								]
							]
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(12.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("DoneVsFair", "{0} / {1}"), FText::AsNumber(Employee.GetCompletedCount()), FText::AsNumber(Fair, &OneDecimal)))
						.Font(FUOUIStyle::Font("Bold", 14))
						.ColorAndOpacity(FUOUIStyle::Text())
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(110.f)
				[
					Stars
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(250.f)
				[
					SNew(STextBlock)
					.Text(FUOStarRating::GetConsequenceLabel(Employee.GetConsequence()).ToUpper())
					.Font(FUOUIStyle::Label(10.f))
					.ColorAndOpacity(Outcome)
					.AutoWrapText(true)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
