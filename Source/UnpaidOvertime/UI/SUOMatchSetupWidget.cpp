// UNPAID: OVERTIME - pre-match setup screen.

#include "SUOMatchSetupWidget.h"
#include "UOKillerRules.h"
#include "UOUIStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOMatchSetup"

void SUOMatchSetupWidget::Construct(const FArguments& InArgs)
{
	OnCreateCompany = InArgs._OnCreateCompany;
	OnBack = InArgs._OnBack;

	const FUOUIStyle& Style = FUOUIStyle::Get();

	auto Choices = [](TArray<TSharedRef<SWidget>> Buttons)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			Row->AddSlot()
			.FillWidth(1.f)
			.Padding(Index == 0 ? 0.f : 8.f, 0.f, 0.f, 0.f)
			[
				Buttons[Index]
			];
		}
		return Row;
	};

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&Style.WhiteBrush)
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(640.f)
			[
				SNew(SBorder)
				.BorderImage(&Style.PanelSolidBrush)
				.Padding(FMargin(44.f, 38.f))
				[
					SNew(SVerticalBox)

					// Header
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Kicker", "STEP 1 OF 1  ·  ONBOARDING"))
						.Font(FUOUIStyle::Label(10.f))
						.ColorAndOpacity(FUOUIStyle::AccentBright())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 6.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "REGISTER YOUR COMPANY"))
						.Font(FUOUIStyle::Heading(28.f))
						.ColorAndOpacity(FLinearColor::White)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 28.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "HR needs a few details before anyone is allowed to clock in."))
						.Font(FUOUIStyle::Body(14.f))
						.ColorAndOpacity(FUOUIStyle::TextDim())
					]

					// Company name
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("CompanyNameLabel", "Company name (required)"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(NameBox, SEditableTextBox)
						.Style(&Style.TextBoxStyle)
						.HintText(FText::Format(LOCTEXT("CompanyNameHint", "e.g. Synergy Dynamics — up to {0} characters"), FText::AsNumber(FUOMatchConfig::MaxCompanyNameLength)))
						.OnTextChanged(this, &SUOMatchSetupWidget::HandleNameChanged)
						.OnTextCommitted(this, &SUOMatchSetupWidget::HandleNameCommitted)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f, 0.f, 24.f)
					[
						SNew(STextBlock)
						.Text(this, &SUOMatchSetupWidget::GetValidationText)
						.ColorAndOpacity(this, &SUOMatchSetupWidget::GetValidationColor)
						.Font(FUOUIStyle::Body(12.f))
						.AutoWrapText(true)
					]

					// Tasks per day
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("TasksLabel", "Tasks per day — shared by the whole team"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 22.f)
					[
						MakeTaskStepper()
					]

					// Day length
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("DayLengthLabel", "Day length"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 22.f)
					[
						Choices({
							FUOUIStyle::MakeChoice(UOText::GetDayLengthName(EUODayLength::Short),
								[this]() { return Config.DayLength == EUODayLength::Short; }, [this]() { Config.DayLength = EUODayLength::Short; }),
							FUOUIStyle::MakeChoice(UOText::GetDayLengthName(EUODayLength::Normal),
								[this]() { return Config.DayLength == EUODayLength::Normal; }, [this]() { Config.DayLength = EUODayLength::Normal; }),
							FUOUIStyle::MakeChoice(UOText::GetDayLengthName(EUODayLength::Long),
								[this]() { return Config.DayLength == EUODayLength::Long; }, [this]() { Config.DayLength = EUODayLength::Long; })
						})
					]

					// AI difficulty
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("DifficultyLabel", "Coworker competence"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 26.f)
					[
						Choices({
							FUOUIStyle::MakeChoice(UOText::GetDifficultyName(EUOAIDifficulty::Easy),
								[this]() { return Config.AIDifficulty == EUOAIDifficulty::Easy; }, [this]() { Config.AIDifficulty = EUOAIDifficulty::Easy; }),
							FUOUIStyle::MakeChoice(UOText::GetDifficultyName(EUOAIDifficulty::Medium),
								[this]() { return Config.AIDifficulty == EUOAIDifficulty::Medium; }, [this]() { Config.AIDifficulty = EUOAIDifficulty::Medium; }),
							FUOUIStyle::MakeChoice(UOText::GetDifficultyName(EUOAIDifficulty::Hard),
								[this]() { return Config.AIDifficulty == EUOAIDifficulty::Hard; }, [this]() { Config.AIDifficulty = EUOAIDifficulty::Hard; })
						})
					]

					// The saboteur (design doc 2.2 / 6)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("KillerLabel", "The saboteur"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 10.f)
					[
						Choices({
							FUOUIStyle::MakeChoice(LOCTEXT("KillerRandom", "Anyone (random)"),
								[this]() { return Config.KillerAssignment == EUOKillerAssignment::Random; }, [this]() { Config.KillerAssignment = EUOKillerAssignment::Random; }),
							FUOUIStyle::MakeChoice(LOCTEXT("KillerAI", "Always an AI"),
								[this]() { return Config.KillerAssignment == EUOKillerAssignment::AlwaysAI; }, [this]() { Config.KillerAssignment = EUOKillerAssignment::AlwaysAI; })
#if !UE_BUILD_SHIPPING
							, FUOUIStyle::MakeChoice(LOCTEXT("KillerMe", "Always me (debug)"),
								[this]() { return Config.KillerAssignment == EUOKillerAssignment::AlwaysHuman; }, [this]() { Config.KillerAssignment = EUOKillerAssignment::AlwaysHuman; })
#endif
						})
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 26.f)
					[
						Choices({
							FUOUIStyle::MakeChoice(LOCTEXT("NoKill", "Saboteur cannot dismiss"),
								[this]() { return !Config.bKillerCanKill; }, [this]() { Config.bKillerCanKill = false; }),
							FUOUIStyle::MakeChoice(LOCTEXT("Kill", "Saboteurs can dismiss (cooldown)"),
								[this]() { return Config.bKillerCanKill; }, [this]() { Config.bKillerCanKill = true; })
						})
					]

					// Team size: killers = floor(size / 3)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 14.f)
					[
						Choices({
							FUOUIStyle::MakeChoice(LOCTEXT("Team4", "4 employees"), [this]() { return Config.TeamSize == 4; }, [this]() { Config.TeamSize = 4; }),
							FUOUIStyle::MakeChoice(LOCTEXT("Team5", "5"), [this]() { return Config.TeamSize == 5; }, [this]() { Config.TeamSize = 5; }),
							FUOUIStyle::MakeChoice(LOCTEXT("Team6", "6"), [this]() { return Config.TeamSize == 6; }, [this]() { Config.TeamSize = 6; }),
							FUOUIStyle::MakeChoice(LOCTEXT("Team7", "7"), [this]() { return Config.TeamSize == 7; }, [this]() { Config.TeamSize = 7; }),
							FUOUIStyle::MakeChoice(LOCTEXT("Team8", "8"), [this]() { return Config.TeamSize == 8; }, [this]() { Config.TeamSize = 8; })
						})
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 14.f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							const int32 Killers = FUOKillerTuning::GetKillerCount(Config.GetTotalEmployees());
							return FText::Format(LOCTEXT("TeamSummary", "Team: {0} human + {1} AI coworkers. {2} of you {2}|plural(one=is a saboteur,other=are saboteurs)."),
								FText::AsNumber(Config.HumanPlayers), FText::AsNumber(Config.GetAIPlayers()), FText::AsNumber(Killers));
						})
						.Font(FUOUIStyle::Body(12.f))
						.ColorAndOpacity(FUOUIStyle::TextDim())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 22.f)
					[
						FUOUIStyle::MakeDivider()
					]

					// Footer: team summary, Back, Create
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNullWidget::NullWidget
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 10.f, 0.f)
						[
							FUOUIStyle::MakeButton(LOCTEXT("Back", "Back"), FOnClicked::CreateLambda([this]()
							{
								OnBack.ExecuteIfBound();
								return FReply::Handled();
							}), false)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							FUOUIStyle::MakeButton(LOCTEXT("CreateButton", "Create company & clock in"),
								FOnClicked::CreateSP(this, &SUOMatchSetupWidget::HandleCreateClicked), true,
								TAttribute<bool>::CreateSP(this, &SUOMatchSetupWidget::IsCreateEnabled))
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SUOMatchSetupWidget::MakeTaskStepper()
{
	auto Step = [this](int32 Delta, const FText& Label)
	{
		return SNew(SBox)
			.WidthOverride(44.f)
			.HeightOverride(40.f)
			[
				SNew(SButton)
				.ButtonStyle(&FUOUIStyle::Get().ChoiceButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.IsEnabled_Lambda([this, Delta]()
				{
					const int32 Next = Config.TasksPerDay + Delta;
					return Next >= FUOMatchConfig::MinTasksPerDay && Next <= FUOMatchConfig::MaxTasksPerDay;
				})
				.OnClicked_Lambda([this, Delta]()
				{
					Config.TasksPerDay = FMath::Clamp(Config.TasksPerDay + Delta, FUOMatchConfig::MinTasksPerDay, FUOMatchConfig::MaxTasksPerDay);
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(FUOUIStyle::Font("Bold", 18))
				]
			];
	};

	TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
	for (int32 Count = FUOMatchConfig::MinTasksPerDay; Count <= FUOMatchConfig::MaxTasksPerDay; ++Count)
	{
		Pips->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(14.f)
			.HeightOverride(14.f)
			[
				SNew(SImage)
				.Image(&FUOUIStyle::Get().DotBrush)
				.ColorAndOpacity_Lambda([this, Count]()
				{
					return FSlateColor(Count <= Config.TasksPerDay ? FUOUIStyle::Accent() : FLinearColor(1.f, 1.f, 1.f, 0.1f));
				})
			]
		];
	}

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			Step(-1, LOCTEXT("TasksMinus", "−"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(18.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(34.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::AsNumber(Config.TasksPerDay); })
				.Font(FUOUIStyle::Font("Bold", 26))
				.Justification(ETextJustify::Center)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 18.f, 0.f)
		[
			Pips
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			Step(1, LOCTEXT("TasksPlus", "+"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("TasksRange", "{0} to {1}"), FText::AsNumber(FUOMatchConfig::MinTasksPerDay), FText::AsNumber(FUOMatchConfig::MaxTasksPerDay)))
			.Font(FUOUIStyle::Body(12.f))
			.ColorAndOpacity(FUOUIStyle::TextDim())
		];
}

TSharedPtr<SWidget> SUOMatchSetupWidget::GetInitialFocusTarget() const
{
	return NameBox;
}

void SUOMatchSetupWidget::SetCompanyName(const FString& Name)
{
	if (NameBox.IsValid())
	{
		NameBox->SetText(FText::FromString(Name));
	}
	HandleNameChanged(FText::FromString(Name));
}

FUOConfigValidationResult SUOMatchSetupWidget::SubmitForAutomation()
{
	HandleCreateClicked();
	return Config.Validate();
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
		return LOCTEXT("NameRequired", "Type a company name to continue.");
	}
	return Result.GetMessage();
}

FSlateColor SUOMatchSetupWidget::GetValidationColor() const
{
	if (!AuthorityError.IsEmpty())
	{
		return FSlateColor(FUOUIStyle::Bad());
	}

	const FUOConfigValidationResult Result = Config.Validate();
	if (Result.bIsValid)
	{
		return FSlateColor(FUOUIStyle::Good());
	}
	return FSlateColor(bNameTouched ? FUOUIStyle::Bad() : FUOUIStyle::TextDim());
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

#undef LOCTEXT_NAMESPACE
