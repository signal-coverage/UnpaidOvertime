// UNPAID: OVERTIME - pre-match setup screen.

#include "SUOMatchSetupWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOMatchSetup"

namespace UOMatchSetupPrivate
{
	const FLinearColor Accent(1.f, 0.72f, 0.2f);
	const FLinearColor SelectedButton(0.95f, 0.6f, 0.12f);
	const FLinearColor NormalButton(0.22f, 0.24f, 0.3f);
	const FLinearColor ErrorColor(1.f, 0.35f, 0.3f);
	const FLinearColor OkColor(0.4f, 0.9f, 0.45f);
	const FLinearColor HintColor(0.7f, 0.7f, 0.75f);

	FSlateFontInfo Font(const ANSICHAR* Typeface, float Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}
}

void SUOMatchSetupWidget::Construct(const FArguments& InArgs)
{
	using namespace UOMatchSetupPrivate;

	OnCreateCompany = InArgs._OnCreateCompany;

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&BackdropBrush)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(620.f)
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(FMargin(36.f, 28.f))
				[
					SNew(SVerticalBox)

					// Title
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "UNPAID: OVERTIME"))
						.Font(Font("Bold", 34))
						.ColorAndOpacity(Accent)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 18.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "Register your company. HR insists."))
						.Font(Font("Italic", 13))
						.ColorAndOpacity(HintColor)
					]

					// Company name
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeSectionLabel(LOCTEXT("CompanyNameLabel", "Company name (required)"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f)
					[
						SAssignNew(NameBox, SEditableTextBox)
						.Font(Font("Regular", 16))
						.HintText(FText::Format(LOCTEXT("CompanyNameHint", "Up to {0} characters"), FText::AsNumber(FUOMatchConfig::MaxCompanyNameLength)))
						.OnTextChanged(this, &SUOMatchSetupWidget::HandleNameChanged)
						.OnTextCommitted(this, &SUOMatchSetupWidget::HandleNameCommitted)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 14.f)
					[
						SNew(STextBlock)
						.Text(this, &SUOMatchSetupWidget::GetValidationText)
						.ColorAndOpacity(this, &SUOMatchSetupWidget::GetValidationColor)
						.Font(Font("Regular", 11))
						.AutoWrapText(true)
					]

					// Tasks per day
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeSectionLabel(LOCTEXT("TasksLabel", "Tasks per day (total, shared by everyone)"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 14.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeStepButton(LOCTEXT("TasksMinus", "-"), -1)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(16.f, 0.f)
						[
							SNew(SBox)
							.WidthOverride(40.f)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return FText::AsNumber(Config.TasksPerDay); })
								.Font(Font("Bold", 20))
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeStepButton(LOCTEXT("TasksPlus", "+"), 1)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(16.f, 0.f)
						[
							SNew(STextBlock)
							.Text(FText::Format(LOCTEXT("TasksRange", "({0} to {1})"), FText::AsNumber(FUOMatchConfig::MinTasksPerDay), FText::AsNumber(FUOMatchConfig::MaxTasksPerDay)))
							.ColorAndOpacity(HintColor)
						]
					]

					// Day length
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeSectionLabel(LOCTEXT("DayLengthLabel", "Day length"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 14.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							MakeChoiceButton(UOText::GetDayLengthName(EUODayLength::Short),
								[this]() { return Config.DayLength == EUODayLength::Short; },
								[this]() { Config.DayLength = EUODayLength::Short; })
						]

						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							MakeChoiceButton(UOText::GetDayLengthName(EUODayLength::Normal),
								[this]() { return Config.DayLength == EUODayLength::Normal; },
								[this]() { Config.DayLength = EUODayLength::Normal; })
						]

						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeChoiceButton(UOText::GetDayLengthName(EUODayLength::Long),
								[this]() { return Config.DayLength == EUODayLength::Long; },
								[this]() { Config.DayLength = EUODayLength::Long; })
						]
					]

					// AI difficulty
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeSectionLabel(LOCTEXT("DifficultyLabel", "AI coworker difficulty"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 14.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							MakeChoiceButton(UOText::GetDifficultyName(EUOAIDifficulty::Easy),
								[this]() { return Config.AIDifficulty == EUOAIDifficulty::Easy; },
								[this]() { Config.AIDifficulty = EUOAIDifficulty::Easy; })
						]

						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							MakeChoiceButton(UOText::GetDifficultyName(EUOAIDifficulty::Medium),
								[this]() { return Config.AIDifficulty == EUOAIDifficulty::Medium; },
								[this]() { Config.AIDifficulty = EUOAIDifficulty::Medium; })
						]

						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeChoiceButton(UOText::GetDifficultyName(EUOAIDifficulty::Hard),
								[this]() { return Config.AIDifficulty == EUOAIDifficulty::Hard; },
								[this]() { Config.AIDifficulty = EUOAIDifficulty::Hard; })
						]
					]

					// Team summary
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 20.f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("TeamSummary", "Team: {0} human + {1} AI coworkers. Seed: random."), FText::AsNumber(Config.HumanPlayers), FText::AsNumber(Config.GetAIPlayers())))
						.ColorAndOpacity(HintColor)
					]

					// Create
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					[
						SNew(SButton)
						.IsEnabled(this, &SUOMatchSetupWidget::IsCreateEnabled)
						.ButtonColorAndOpacity(SelectedButton)
						.ContentPadding(FMargin(28.f, 10.f))
						.OnClicked(this, &SUOMatchSetupWidget::HandleCreateClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CreateButton", "Create company and clock in"))
							.Font(Font("Bold", 16))
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
			]
		]
	];
}

TSharedPtr<SWidget> SUOMatchSetupWidget::GetInitialFocusTarget() const
{
	return NameBox;
}

TSharedRef<SWidget> SUOMatchSetupWidget::MakeSectionLabel(const FText& Label) const
{
	return SNew(STextBlock)
		.Text(Label)
		.Font(UOMatchSetupPrivate::Font("Bold", 13))
		.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f));
}

TSharedRef<SWidget> SUOMatchSetupWidget::MakeChoiceButton(const FText& Label, TFunction<bool()> IsSelected, TFunction<void()> OnSelect) const
{
	return SNew(SButton)
		.ContentPadding(FMargin(18.f, 6.f))
		.ButtonColorAndOpacity_Lambda([IsSelected]()
		{
			return FSlateColor(IsSelected() ? UOMatchSetupPrivate::SelectedButton : UOMatchSetupPrivate::NormalButton);
		})
		.OnClicked_Lambda([OnSelect]()
		{
			OnSelect();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(UOMatchSetupPrivate::Font("Bold", 13))
			.ColorAndOpacity_Lambda([IsSelected]()
			{
				return FSlateColor(IsSelected() ? FLinearColor::Black : FLinearColor::White);
			})
		];
}

TSharedRef<SWidget> SUOMatchSetupWidget::MakeStepButton(const FText& Label, int32 Delta)
{
	return SNew(SButton)
		.ContentPadding(FMargin(14.f, 4.f))
		.ButtonColorAndOpacity(UOMatchSetupPrivate::NormalButton)
		.IsEnabled_Lambda([this, Delta]()
		{
			const int32 Next = Config.TasksPerDay + Delta;
			return Next >= FUOMatchConfig::MinTasksPerDay && Next <= FUOMatchConfig::MaxTasksPerDay;
		})
		.OnClicked(this, &SUOMatchSetupWidget::HandleTasksStep, Delta)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(UOMatchSetupPrivate::Font("Bold", 18))
		];
}

void SUOMatchSetupWidget::HandleNameChanged(const FText& NewText)
{
	Config.CompanyName = NewText.ToString();
	bNameTouched = true;
	AuthorityError = FText::GetEmpty();
}

void SUOMatchSetupWidget::HandleNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	Config.CompanyName = NewText.ToString();
	if (CommitType == ETextCommit::OnEnter && IsCreateEnabled())
	{
		HandleCreateClicked();
	}
}

FText SUOMatchSetupWidget::GetValidationText() const
{
	if (!AuthorityError.IsEmpty())
	{
		return AuthorityError;
	}

	const FUOConfigValidationResult Result = Config.Validate();
	if (Result.bIsValid)
	{
		return FText::Format(LOCTEXT("NameOk", "\"{0}\" is now hiring. Unfortunately."), FText::FromString(Config.GetSanitizedCompanyName()));
	}
	if (!bNameTouched && Result.Error == EUOConfigError::CompanyNameEmpty)
	{
		return LOCTEXT("NameRequired", "Type a company name to enable Create.");
	}
	return Result.GetMessage();
}

FSlateColor SUOMatchSetupWidget::GetValidationColor() const
{
	if (!AuthorityError.IsEmpty())
	{
		return FSlateColor(UOMatchSetupPrivate::ErrorColor);
	}

	const FUOConfigValidationResult Result = Config.Validate();
	if (Result.bIsValid)
	{
		return FSlateColor(UOMatchSetupPrivate::OkColor);
	}
	return FSlateColor(bNameTouched ? UOMatchSetupPrivate::ErrorColor : UOMatchSetupPrivate::HintColor);
}

bool SUOMatchSetupWidget::IsCreateEnabled() const
{
	return Config.Validate().bIsValid;
}

FReply SUOMatchSetupWidget::HandleCreateClicked()
{
	if (!IsCreateEnabled())
	{
		return FReply::Handled();
	}

	if (OnCreateCompany.IsBound())
	{
		const FUOConfigValidationResult Result = OnCreateCompany.Execute(Config);
		AuthorityError = Result.bIsValid ? FText::GetEmpty() : Result.GetMessage();
	}
	return FReply::Handled();
}

FReply SUOMatchSetupWidget::HandleTasksStep(int32 Delta)
{
	Config.TasksPerDay = FMath::Clamp(Config.TasksPerDay + Delta, FUOMatchConfig::MinTasksPerDay, FUOMatchConfig::MaxTasksPerDay);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
