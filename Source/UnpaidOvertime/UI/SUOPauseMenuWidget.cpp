// UNPAID: OVERTIME - pause menu.

#include "SUOPauseMenuWidget.h"
#include "UOUIStyle.h"
#include "UOGameState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOPauseMenu"

void SUOPauseMenuWidget::Construct(const FArguments& InArgs)
{
	const FUOUIStyle& Style = FUOUIStyle::Get();

	auto Clicked = [](FSimpleDelegate Delegate)
	{
		return FOnClicked::CreateLambda([Delegate]()
		{
			Delegate.ExecuteIfBound();
			return FReply::Handled();
		});
	};

	FText Status;
	if (const APlayerController* PC = InArgs._Owner.Get())
	{
		if (const AUOGameState* GameState = PC->GetWorld()->GetGameState<AUOGameState>())
		{
			Status = FText::Format(LOCTEXT("Status", "{0}  ·  DAY {1}  ·  {2}"), FText::FromString(GameState->GetCompanyName().ToUpper()),
				FText::AsNumber(GameState->GetDayNumber()), FText::FromString(GameState->GetClockText()));
		}
	}

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&Style.WhiteBrush)
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		[
			FUOUIStyle::MakeSideShade(900.f, 0.85f, true)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(130.f, 0.f, 0.f, 0.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Status)
				.Font(FUOUIStyle::Label(11.f))
				.ColorAndOpacity(FUOUIStyle::AccentBright())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "ON BREAK"))
				.Font(FUOUIStyle::Title(64.f))
				.ColorAndOpacity(FLinearColor::White)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 40.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Subtitle", "Your break is unpaid. Please return to your workstation promptly."))
				.Font(FUOUIStyle::Body(15.f))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.72f))
			]

			+ SVerticalBox::Slot().AutoHeight()[FUOUIStyle::MakeMenuButton(LOCTEXT("Resume", "Resume"), Clicked(InArgs._OnResume))]
			+ SVerticalBox::Slot().AutoHeight()[FUOUIStyle::MakeMenuButton(LOCTEXT("Settings", "Settings"), Clicked(InArgs._OnSettings))]
			+ SVerticalBox::Slot().AutoHeight()[FUOUIStyle::MakeMenuButton(LOCTEXT("QuitToMenu", "Quit to menu"), Clicked(InArgs._OnQuitToMenu))]
			+ SVerticalBox::Slot().AutoHeight()[FUOUIStyle::MakeMenuButton(LOCTEXT("QuitGame", "Quit game"), Clicked(InArgs._OnQuitGame))]
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(130.f, 0.f, 0.f, 60.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 10.f, 0.f)
			[
				FUOUIStyle::MakeKeyCap(LOCTEXT("EscKey", "ESC"), 30.f)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EscHint", "Back to work"))
				.Font(FUOUIStyle::Body(13.f))
				.ColorAndOpacity(FUOUIStyle::TextDim())
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
