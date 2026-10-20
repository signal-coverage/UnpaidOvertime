// UNPAID: OVERTIME - title screen.

#include "SUOMainMenuWidget.h"
#include "UOUIStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOMainMenu"

void SUOMainMenuWidget::Construct(const FArguments& InArgs)
{
	OnPlay = InArgs._OnPlay;
	OnSettings = InArgs._OnSettings;
	OnQuit = InArgs._OnQuit;

	const FUOUIStyle& Style = FUOUIStyle::Get();

	auto Clicked = [](FSimpleDelegate Delegate)
	{
		return FOnClicked::CreateLambda([Delegate]()
		{
			Delegate.ExecuteIfBound();
			return FReply::Handled();
		});
	};

	ChildSlot
	[
		SNew(SOverlay)

		// Readability shade on the left, cinematic bars top and bottom
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		[
			FUOUIStyle::MakeSideShade(1100.f, 0.92f, true)
		]

		+ SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(56.f)
			[
				SNew(SImage)
				.Image(&Style.WhiteBrush)
				.ColorAndOpacity(FLinearColor::Black)
			]
		]

		+ SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		[
			SNew(SBox)
			.HeightOverride(56.f)
			[
				SNew(SImage)
				.Image(&Style.WhiteBrush)
				.ColorAndOpacity(FLinearColor::Black)
			]
		]

		// Title block and menu
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(130.f, 0.f, 0.f, 20.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 14.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(28.f)
					.HeightOverride(3.f)
					[
						SNew(SImage)
						.Image(&Style.WhiteBrush)
						.ColorAndOpacity(FUOUIStyle::Accent())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Kicker", "A COOPERATIVE OFFICE HORROR"))
					.Font(FUOUIStyle::Label(12.f))
					.ColorAndOpacity(FUOUIStyle::AccentBright())
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TitleTop", "UNPAID:"))
				.Font(FUOUIStyle::Title(92.f))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(0.f, 3.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, -26.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TitleBottom", "OVERTIME"))
				.Font(FUOUIStyle::Title(92.f))
				.ColorAndOpacity(FUOUIStyle::Accent())
				.ShadowOffset(FVector2D(0.f, 3.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 56.f)
			[
				SNew(SBox)
				.WidthOverride(520.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Subtitle", "Clock in. Do your fair share. Someone always gets let go — make sure it isn't you."))
					.Font(FUOUIStyle::Body(17.f))
					.ColorAndOpacity(FLinearColor(0.78f, 0.78f, 0.8f))
					.AutoWrapText(true)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				FUOUIStyle::MakeMenuButton(LOCTEXT("Play", "Play"), Clicked(OnPlay))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				FUOUIStyle::MakeMenuButton(LOCTEXT("Settings", "Settings"), Clicked(OnSettings))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				FUOUIStyle::MakeMenuButton(LOCTEXT("Quit", "Quit"), Clicked(OnQuit))
			]
		]

		// Footer
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(130.f, 0.f, 0.f, 76.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Footer", "PROTOTYPE BUILD  ·  ALL HOURS ARE FINAL"))
			.Font(FUOUIStyle::Label(10.f))
			.ColorAndOpacity(FUOUIStyle::TextDim())
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 60.f, 76.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Legal", "This message was approved by Human Resources."))
			.Font(FUOUIStyle::Body(12.f))
			.ColorAndOpacity(FUOUIStyle::TextDim())
		]
	];
}

#undef LOCTEXT_NAMESPACE
