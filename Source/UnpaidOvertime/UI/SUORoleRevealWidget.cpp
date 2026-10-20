// UNPAID: OVERTIME - the private role card.

#include "SUORoleRevealWidget.h"
#include "UOUIStyle.h"
#include "UOPlayerController.h"
#include "UOPlayerState.h"
#include "UOGameState.h"
#include "UORoleSystemComponent.h"
#include "UOGameMode.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UORoleReveal"

void SUORoleRevealWidget::Construct(const FArguments& InArgs)
{
	OnDismiss = InArgs._OnDismiss;
	const AUOPlayerController* PC = InArgs._Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	const bool bKiller = Me && Me->GetKnownRole() == EUORole::Killer;
	const AUOGameMode* GameMode = PC && PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	const bool bCanKill = GameMode && GameMode->GetMatchConfig().bKillerCanKill;
	const FUOUIStyle& Style = FUOUIStyle::Get();

	const FLinearColor RoleColor = bKiller ? FUOUIStyle::AccentBright() : FUOUIStyle::Gold();
	const FText Kicker = LOCTEXT("Kicker", "PRIVATE  ·  FOR YOUR EYES ONLY");
	const FText Title = bKiller ? LOCTEXT("TitleKiller", "YOU ARE A SABOTEUR") : LOCTEXT("TitleEmployee", "YOU ARE AN EMPLOYEE");
	const FText Tagline = bKiller ? LOCTEXT("TagKiller", "Be among the last ones standing.") : LOCTEXT("TagEmployee", "Do your job. Watch your coworkers. Some of them are not who they say.");

	// Rule 1: killers know each other
	FString AccompliceNames;
	if (bKiller && Me)
	{
		const AUOGameState* GameState = PC->GetWorld() ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
		for (const int32 Id : Me->GetKillerView().AccompliceIds)
		{
			AccompliceNames += (AccompliceNames.IsEmpty() ? TEXT("") : TEXT(", ")) + (GameState ? GameState->GetEmployeeName(Id) : FString::FromInt(Id));
		}
	}

	TArray<FText> Lines;
	if (bKiller)
	{
		Lines.Add(LOCTEXT("K1", "Your work is real and counts like anyone's: it buys you credibility."));
		Lines.Add(LOCTEXT("K2", "Hold F at machines, desk PCs, cameras and plants to sabotage. Every sabotage leaves a trace."));
		Lines.Add(LOCTEXT("K3", "At the review you choose the stars the others see: 1, 2 or 3. Shown 1: a mandatory defense question. Shown 3: a promotion question and a forged shield. The boss accepts any answer you give, but everyone hears it."));
		Lines.Add(LOCTEXT("K4", "You are never fired for zero stars. A cut vote or the meeting can still get you, and forged shields never protect."));
		if (bCanKill)
		{
			Lines.Add(LOCTEXT("K5", "Dismissals are on: hold F next to a coworker nobody else can see. A cooldown, no daily limit; every kill is easier to trace."));
		}
		Lines.Add(LOCTEXT("K6", "When only saboteurs are left, you take over the company together."));
	}
	else
	{
		Lines.Add(LOCTEXT("E1", "Finish your share of the tasks: stars are earned by individual contribution."));
		Lines.Add(LOCTEXT("E2", "Terminals keep logs: printers, desk PCs, the badge reader, the security monitor, the supply sheet. Pin what looks wrong."));
		Lines.Add(LOCTEXT("E3", "Every evening there is a meeting: debate, put your pinned lines on the table, then vote. Most votes leaves."));
		Lines.Add(LOCTEXT("E4", "A promotion shield saves you from any expulsion. Without one, the right answer to the boss keeps you, on probation."));
	}

	TSharedRef<SVerticalBox> LineBox = SNew(SVerticalBox);
	for (const FText& Line : Lines)
	{
		LineBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 7.f, 12.f, 0.f)
			[
				SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)[SNew(SImage).Image(&Style.DotBrush).ColorAndOpacity(RoleColor)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(Line).Font(FUOUIStyle::Body(17.f)).ColorAndOpacity(FUOUIStyle::Text()).WrapTextAt(900.f)
			]
		];
	}

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage).Image(&Style.WhiteBrush).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(1040.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Kicker).Font(FUOUIStyle::Label(12.f)).ColorAndOpacity(FUOUIStyle::TextDim())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SBox).HeightOverride(4.f).WidthOverride(140.f).HAlign(HAlign_Left)
					[
						SNew(SImage).Image(&Style.WhiteBrush).ColorAndOpacity(RoleColor)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(FUOUIStyle::Title(54.f))
					.ColorAndOpacity_Lambda([this, RoleColor]()
					{
						// A slow fade-in from the role color to white (red stays red for the saboteur)
						const float Alpha = FMath::Clamp(Age / 0.8f, 0.f, 1.f);
						FLinearColor Color = RoleColor;
						Color.A = Alpha;
						return FSlateColor(Color);
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 28.f)
				[
					SNew(STextBlock).Text(Tagline).Font(FUOUIStyle::Font("Bold", 22)).ColorAndOpacity(FLinearColor::White).WrapTextAt(1000.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
				[
					SNew(SBorder)
					.BorderImage(&Style.PillAccentBrush)
					.Padding(FMargin(14.f, 8.f))
					.Visibility(AccompliceNames.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Accomplice", "Your accomplice: {0}"), FText::FromString(AccompliceNames)))
						.Font(FUOUIStyle::Font("Bold", 20))
						.ColorAndOpacity(FLinearColor::White)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					LineBox
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						FUOUIStyle::MakeKeyCap(LOCTEXT("KeyEnter", "Enter"), 28.f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						FUOUIStyle::MakeButton(LOCTEXT("ClockIn", "Clock in"), FOnClicked::CreateLambda([this]()
						{
							OnDismiss.ExecuteIfBound();
							return FReply::Handled();
						}), true, true, 200.f)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(bKiller ? (AccompliceNames.IsEmpty() ? LOCTEXT("SecretKiller", "Nobody else knows. Keep it that way.") : LOCTEXT("SecretKillers", "Only your accomplice knows. Never vote against each other."))
							: LOCTEXT("SecretEmployee", "The saboteurs got a card like this one, too."))
						.Font(FUOUIStyle::Body(13.f))
						.ColorAndOpacity(FUOUIStyle::TextDim())
					]
				]
			]
		]
	];
}

void SUORoleRevealWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Age += InDeltaTime;
}

#undef LOCTEXT_NAMESPACE
