// UNPAID: OVERTIME - in-game HUD.

#include "SUOHUDWidget.h"
#include "UOUIStyle.h"
#include "SUOIcon.h"
#include "UOGameState.h"
#include "UOGameMode.h"
#include "UOPlayerState.h"
#include "UOCharacter.h"
#include "UOInteractionComponent.h"
#include "UOInteractableStation.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskBoard.h"
#include "UOStarRating.h"
#include "UODayClock.h"
#include "UODayCalendar.h"
#include "UODayDifficulty.h"
#include "UOAIEmployee.h"
#include "UOKillerRules.h"
#include "UOInvestigation.h"
#include "UOPlayerController.h"
#include "UOGameUserSettings.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOHUD"

namespace UOHUDPrivate
{
	constexpr float TaskPanelWidth = 430.f;

	// Coworker name tags
	constexpr float NameTagHeadOffset = 108.f;
	constexpr float NameTagFadeStart = 1100.f;
	constexpr float NameTagMaxDistance = 1700.f;
	constexpr int32 FeedLines = 4;

	FString FormatCountdown(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	FLinearColor ItemColor(EUOItemType Item)
	{
		return Item == EUOItemType::InkCartridge ? FLinearColor(0.05f, 0.05f, 0.07f) : FLinearColor(0.95f, 0.95f, 0.9f);
	}
}

void SUOHUDWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	const FUOUIStyle& Style = FUOUIStyle::Get();

	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SOverlay)

		// Overtime edge glow
		+ SOverlay::Slot().HAlign(HAlign_Left)[MakeEdgeGlow(false, true, 260.f)]
		+ SOverlay::Slot().HAlign(HAlign_Right)[MakeEdgeGlow(false, false, 260.f)]
		+ SOverlay::Slot().VAlign(VAlign_Top)[MakeEdgeGlow(true, true, 160.f)]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[MakeEdgeGlow(true, false, 160.f)]

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(36.f, 32.f))
		[
			SNew(SBorder)
			.BorderImage(&Style.PanelBrush)
			.Padding(FMargin(18.f, 12.f, 22.f, 12.f))
			[
				MakeTopLeft()
			]
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 34.f))
		[
			MakeOvertimeBanner()
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(36.f, 32.f))
		[
			MakeTaskPanel()
		]

		// Crosshair dot
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(6.f)
			.HeightOverride(6.f)
			[
				SNew(SImage)
				.Image(&Style.DotBrush)
				.ColorAndOpacity_Lambda([this]()
				{
					const UUOInteractionComponent* Interaction = GetInteraction();
					const bool bActionable = Interaction && Interaction->GetFocusedStation() && Interaction->GetCurrentOption().bAvailable;
					return FSlateColor(bActionable ? FUOUIStyle::AccentBright() : FLinearColor(1.f, 1.f, 1.f, 0.8f));
				})
			]
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 150.f, 0.f, 0.f))
		[
			MakePrompt()
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 300.f, 0.f, 0.f))
		[
			MakeSecondaryPrompt()
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 44.f))
		[
			MakeCarriedChip()
		]

		// The killer's private panel
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 36.f, 74.f))
		[
			MakeKillerPanel()
		]

		// Voice chat indicator (M7 hook; inert in single player)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(44.f, 0.f, 0.f, 12.f))
		[
			MakeMicIndicator()
		]

		// Activity feed
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(44.f, 0.f, 0.f, 40.f))
		[
			SAssignNew(FeedRows, SVerticalBox)
		]

		// Personal tally
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 44.f, 40.f))
		[
			SNew(STextBlock)
			.Font(FUOUIStyle::Font("Bold", 13, 40))
			.ColorAndOpacity(FUOUIStyle::Text())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			.Text_Lambda([this]()
			{
				const AUOGameState* GameState = GetGameState();
				const AUOPlayerState* Me = GetEmployee();
				if (!GameState || !Me)
				{
					return FText::GetEmpty();
				}
				static const FNumberFormattingOptions OneDecimal = FNumberFormattingOptions().SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
				const float Fair = FUOStarRating::ComputeFairShare(FUOTaskBoard::CountTotal(GameState->GetTasks()), GameState->CountAliveEmployees());
				const FText Tally = FText::Format(LOCTEXT("Tally", "YOUR WORK  {0}  ·  FAIR SHARE  {1}"), FText::AsNumber(Me->GetCompletedCount()), FText::AsNumber(Fair, &OneDecimal));
				if (Me->GetPinnedTraceIds().Num() > 0)
				{
					return FText::Format(LOCTEXT("TallyPins", "{0}  ·  EVIDENCE  {1}/{2}"), Tally, FText::AsNumber(Me->GetPinnedTraceIds().Num()), FText::AsNumber(FUOKillerTuning::MaxPins));
				}
				return Tally;
			})
		]
	];
}

const AUOGameState* SUOHUDWidget::GetGameState() const
{
	const APlayerController* PC = Owner.Get();
	return PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
}

const AUOPlayerState* SUOHUDWidget::GetEmployee() const
{
	const APlayerController* PC = Owner.Get();
	return PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
}

const UUOInteractionComponent* SUOHUDWidget::GetInteraction() const
{
	const APlayerController* PC = Owner.Get();
	const AUOCharacter* Character = PC ? Cast<AUOCharacter>(PC->GetPawn()) : nullptr;
	return Character ? Character->GetInteraction() : nullptr;
}

bool SUOHUDWidget::IsOvertime() const
{
	const AUOGameState* GameState = GetGameState();
	return GameState && GameState->GetPhase() == EUODayPhase::Overtime;
}

float SUOHUDWidget::GetOvertimePulse() const
{
	return 0.5f + 0.5f * FMath::Sin(Time * 2.2f);
}

void SUOHUDWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Time += InDeltaTime;

	const AUOGameState* GameState = GetGameState();
	if (!GameState)
	{
		return;
	}

	// Rebuild rows only when something visible changed
	FString Signature;
	for (const FUOTaskInstance& Task : GameState->GetTasks())
	{
		const int32 Redo = Task.SubObjectives.FilterByPredicate([](const FUOSubObjective& Sub) { return Sub.bSabotaged && !Sub.IsDone(); }).Num();
		Signature += FString::Printf(TEXT("%d:%d/%d/%d;"), Task.Id, Task.CountDone(), Task.SubObjectives.Num(), Redo);
	}
	if (Signature != TaskSignature)
	{
		TaskSignature = Signature;
		RebuildTasks(*GameState);
	}

	if (GameState->GetEvidenceLog().Num() != FeedCount)
	{
		FeedCount = GameState->GetEvidenceLog().Num();
		RebuildFeed(*GameState);
	}

	UpdateNameTags();
}

void SUOHUDWidget::UpdateNameTags()
{
	using namespace UOHUDPrivate;

	NameTags.Reset();
	APlayerController* PC = Owner.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UONameTagOcclusion), false, PC->GetPawn());

	for (TActorIterator<AUOAIEmployee> It(World); It; ++It)
	{
		const AUOAIEmployee* Coworker = *It;
		const AUOPlayerState* Employee = Coworker->GetPlayerState<AUOPlayerState>();
		if (!Employee || !Employee->IsAlive() || Coworker->IsHidden())
		{
			continue;
		}

		const FVector Head = Coworker->GetActorLocation() + FVector(0.f, 0.f, NameTagHeadOffset);
		const float Distance = FVector::Dist(CameraLocation, Head);
		if (Distance > NameTagMaxDistance)
		{
			continue;
		}

		// Hidden behind walls and furniture
		FHitResult Hit;
		Params.ClearIgnoredSourceObjects();
		Params.AddIgnoredActor(PC->GetPawn());
		Params.AddIgnoredActor(Coworker);
		if (World->LineTraceSingleByChannel(Hit, CameraLocation, Head, ECC_Visibility, Params))
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Head, ScreenPosition, false))
		{
			continue;
		}

		FNameTag& NewTag = NameTags.AddDefaulted_GetRef();
		NewTag.Position = ScreenPosition;
		const AUOPlayerState* Me = GetEmployee();
		NewTag.bAccomplice = Me && Me->GetKillerView().AccompliceIds.Contains(Employee->GetEmployeeId());
		NewTag.Name = Employee->GetDisplayName();
		if (NewTag.bAccomplice)
		{
			NewTag.Name += LOCTEXT("TagAccomplice", "  · ACCOMPLICE").ToString();
		}
		if (Employee->IsOnProbation())
		{
			NewTag.Name += LOCTEXT("TagProbation", "  · PROBATION").ToString();
		}
		NewTag.Color = Employee->GetEmployeeColor();
		NewTag.Alpha = 1.f - FMath::GetMappedRangeValueClamped(FVector2D(NameTagFadeStart, NameTagMaxDistance), FVector2D(0.f, 1.f), Distance);
		NewTag.Distance = Distance;
	}

	// Declutter: nearest coworkers keep their spot, farther tags that would overlap move up
	const FSlateFontInfo Font = FUOUIStyle::Font("Bold", 11, 40);
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	NameTags.Sort([](const FNameTag& A, const FNameTag& B) { return A.Distance < B.Distance; });
	TArray<FSlateRect> Placed;
	for (FNameTag& Entry : NameTags)
	{
		Entry.TextSize = FVector2f(Measure->Measure(Entry.Name, Font));
		const float Width = Entry.TextSize.X + 40.f;
		const float Height = Entry.TextSize.Y + 10.f;
		for (int32 Attempt = 0; Attempt < 6; ++Attempt)
		{
			const FSlateRect Rect(Entry.Position.X - Width * 0.5f, Entry.Position.Y - Height, Entry.Position.X + Width * 0.5f, Entry.Position.Y);
			const bool bOverlaps = Placed.ContainsByPredicate([&Rect](const FSlateRect& Other) { return FSlateRect::DoRectanglesIntersect(Rect, Other); });
			if (!bOverlaps)
			{
				Placed.Add(Rect);
				break;
			}
			Entry.Position.Y -= Height + 3.f;
		}
	}
}

int32 SUOHUDWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Name tags first, so every HUD panel draws on top of them
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const FSlateFontInfo Font = FUOUIStyle::Font("Bold", 11, 40);

	for (const FNameTag& Label : NameTags)
	{
		const FVector2f TextSize = Label.TextSize;
		const FVector2f Padding(9.f, 4.f);
		const float DotSize = 7.f;
		const FVector2f BoxSize(TextSize.X + Padding.X * 2.f + DotSize + 6.f, TextSize.Y + Padding.Y * 2.f);
		const FVector2f BoxOrigin = FVector2f(Label.Position) - FVector2f(BoxSize.X * 0.5f, BoxSize.Y);

		// MakeBox does not apply the brush's own tint: pass it explicitly (a fellow killer's tag is red, for killers only)
		FLinearColor PanelTint = Label.bAccomplice ? FUOUIStyle::Accent() * FLinearColor(1.f, 1.f, 1.f, 0.9f) : Style.PanelBrush.GetTint(InWidgetStyle);
		PanelTint.A *= Label.Alpha;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxOrigin)),
			&Style.PanelBrush, ESlateDrawEffect::None, PanelTint);

		FLinearColor DotColor = Label.Color;
		DotColor.A = Label.Alpha;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(DotSize, DotSize), FSlateLayoutTransform(BoxOrigin + FVector2f(Padding.X, (BoxSize.Y - DotSize) * 0.5f))),
			&Style.DotBrush, ESlateDrawEffect::None, DotColor);

		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(BoxOrigin + FVector2f(Padding.X + DotSize + 6.f, Padding.Y))),
			Label.Name, Font, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Label.Alpha));
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
}

TSharedRef<SWidget> SUOHUDWidget::MakeTopLeft()
{
	using namespace UOHUDPrivate;

	const FUOUIStyle& Style = FUOUIStyle::Get();

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const AUOGameState* GameState = GetGameState();
				return GameState ? FText::FromString(GameState->GetCompanyName().ToUpper()) : FText::GetEmpty();
			})
			.Font(FUOUIStyle::Font("Bold", 15, 180))
			.ColorAndOpacity(FUOUIStyle::Text())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AUOGameState* GameState = GetGameState();
					return GameState ? FText::FromString(GameState->GetClockText()) : FText::GetEmpty();
				})
				.Font(FUOUIStyle::Font("Bold", 40))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(IsOvertime() ? FUOUIStyle::AccentBright() : FLinearColor::White); })
				.ShadowOffset(FVector2D(1.f, 2.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(14.f, 6.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					// Your secret role (only you see it)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage_Lambda([this]() { return IsKiller() ? &FUOUIStyle::Get().PillAccentBrush : &FUOUIStyle::Get().CardBrush; })
						.Padding(FMargin(8.f, 2.f))
						.Visibility_Lambda([this]()
						{
							const AUOPlayerState* Me = GetEmployee();
							return Me && Me->GetKnownRole() != EUORole::Unknown ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.ToolTipText(LOCTEXT("RoleTip", "Your role. Nobody else can see this."))
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return IsKiller() ? LOCTEXT("RoleKiller", "SABOTEUR") : LOCTEXT("RoleEmployee", "EMPLOYEE"); })
							.Font(FUOUIStyle::Label(10.f))
							.ColorAndOpacity(FLinearColor::White)
						]
					]

					// Promotion shields shown (they stack; each real one cancels one expulsion; a killer knows which are forged)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(&Style.CardBrush)
						.Padding(FMargin(6.f, 2.f))
						.Visibility_Lambda([this]()
						{
							const AUOPlayerState* Me = GetEmployee();
							return Me && Me->HasShield() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(SUOIcon).Type(EUOIconType::Shield).Size(13.f).Color(FUOUIStyle::Gold())
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									const AUOPlayerState* Me = GetEmployee();
									const int32 Shown = Me ? Me->GetDisplayedShieldCount() : 0;
									const int32 Fake = Me ? FMath::Min(Shown, Me->GetFakeShieldCount()) : 0;
									return Fake > 0
										? FText::Format(LOCTEXT("ShieldChipFake", "SHIELD ×{0} ({1} fake)"), FText::AsNumber(Shown), FText::AsNumber(Fake))
										: FText::Format(LOCTEXT("ShieldChip", "SHIELD ×{0}"), FText::AsNumber(Shown));
								})
								.Font(FUOUIStyle::Label(9.f)).ColorAndOpacity(FUOUIStyle::Gold())
							]
						]
					]

					// Public: saved at a meeting by a boss question
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(&Style.CardBrush)
						.BorderBackgroundColor(FUOUIStyle::Warning())
						.Padding(FMargin(6.f, 2.f))
						.ToolTipText(LOCTEXT("ProbationTip", "On probation: 2 real stars at the next review, or you are fired."))
						.Visibility_Lambda([this]()
						{
							const AUOPlayerState* Me = GetEmployee();
							return Me && Me->IsOnProbation() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							SNew(STextBlock).Text(LOCTEXT("ProbationChip", "ON PROBATION")).Font(FUOUIStyle::Label(9.f)).ColorAndOpacity(FUOUIStyle::Warning())
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
					SNew(SBorder)
					.BorderImage_Lambda([this]() { return IsOvertime() ? &FUOUIStyle::Get().PillAccentBrush : &FUOUIStyle::Get().CardBrush; })
					.Padding(FMargin(8.f, 2.f))
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							const AUOGameState* GameState = GetGameState();
							if (!GameState)
							{
								return FText::GetEmpty();
							}
							return FText::Format(LOCTEXT("DayPhase", "{0}  ·  {1}"), FUODayCalendar::FormatDayName(GameState->GetDayNumber()).ToUpper(), UOText::GetPhaseName(GameState->GetPhase()).ToUpper());
						})
						.Font(FUOUIStyle::Label(10.f))
					]
					]

					// Hardcore overtime weekend badge
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(&Style.PillAccentBrush)
						.BorderBackgroundColor(FUOUIStyle::Bad())
						.Padding(FMargin(8.f, 2.f))
						.Visibility_Lambda([this]()
						{
							const AUOGameState* GameState = GetGameState();
							return GameState && FUODayDifficulty::IsHardcoreDay(GameState->GetDayNumber()) ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.ToolTipText(LOCTEXT("HardcoreTip", "Overtime weekend: harder tasks, a shorter shift and a stricter boss. Monday is back to normal."))
						[
							SNew(STextBlock)
							.Text(FUODayCalendar::GetHardcoreLabel())
							.Font(FUOUIStyle::Label(10.f))
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const AUOGameState* GameState = GetGameState();
						if (!GameState)
						{
							return FText::GetEmpty();
						}
						if (GameState->GetPhase() == EUODayPhase::Overtime)
						{
							const float HardStop = GameState->GetShiftLengthSeconds() * (1.f + FUODayClock::OvertimeCapFraction);
							return FText::Format(LOCTEXT("ClosesIn", "Building closes in {0}"), FText::FromString(UOHUDPrivate::FormatCountdown(HardStop - GameState->GetShiftElapsedSeconds())));
						}
						return FText::Format(LOCTEXT("ShiftEndsIn", "Shift ends in {0}"),
							FText::FromString(UOHUDPrivate::FormatCountdown(GameState->GetShiftLengthSeconds() - GameState->GetShiftElapsedSeconds())));
					})
					.Font(FUOUIStyle::Body(12.f))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(IsOvertime() ? FUOUIStyle::AccentBright() : FLinearColor(0.8f, 0.8f, 0.82f)); })
					.ShadowOffset(FVector2D(1.f, 1.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
				]
			]
		];
}

TSharedRef<SWidget> SUOHUDWidget::MakeTaskPanel()
{
	const FUOUIStyle& Style = FUOUIStyle::Get();

	return SNew(SBox)
		.WidthOverride(UOHUDPrivate::TaskPanelWidth)
		[
			SNew(SBorder)
			.BorderImage(&Style.PanelBrush)
			.Padding(FMargin(18.f, 14.f, 18.f, 10.f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 10.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TasksHeader", "TODAY'S TASKS"))
						.Font(FUOUIStyle::Label(10.f))
						.ColorAndOpacity(FUOUIStyle::AccentBright())
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							const AUOGameState* GameState = GetGameState();
							if (!GameState)
							{
								return FText::GetEmpty();
							}
							return FText::Format(LOCTEXT("TasksCount", "{0} / {1}"), FText::AsNumber(FUOTaskBoard::CountDone(GameState->GetTasks())), FText::AsNumber(FUOTaskBoard::CountTotal(GameState->GetTasks())));
						})
						.Font(FUOUIStyle::Font("Bold", 11, 80))
						.ColorAndOpacity(FUOUIStyle::Text())
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(TaskRows, SVerticalBox)
				]
			]
		];
}

void SUOHUDWidget::RebuildTasks(const AUOGameState& GameState)
{
	if (!TaskRows.IsValid())
	{
		return;
	}
	TaskRows->ClearChildren();

	const FUOUIStyle& Style = FUOUIStyle::Get();
	for (const FUOTaskInstance& Task : GameState.GetTasks())
	{
		const int32 Done = Task.CountDone();
		const int32 Total = Task.SubObjectives.Num();
		const bool bComplete = Task.IsComplete();
		const int32 Redo = Task.SubObjectives.FilterByPredicate([](const FUOSubObjective& Sub) { return Sub.bSabotaged && !Sub.IsDone(); }).Num();
		const float Fraction = Total > 0 ? static_cast<float>(Done) / Total : 0.f;

		TaskRows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 9.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SUOIcon)
					.Type(bComplete ? EUOIconType::Check : EUOIconType::Ring)
					.Size(16.f)
					.Thickness(bComplete ? 2.5f : 2.f)
					.Progress(Fraction)
					.Color(bComplete ? FUOUIStyle::Good() : FUOUIStyle::Text())
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Task.DisplayName)
					.Font(FUOUIStyle::Body(13.f))
					.ColorAndOpacity(bComplete ? FUOUIStyle::TextDim() : FUOUIStyle::Text())
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]

				// A corrupted result has to be redone
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("TaskRedo", "{0} REDO"), FText::AsNumber(Redo)))
					.Font(FUOUIStyle::Label(9.f))
					.ColorAndOpacity(FUOUIStyle::Bad())
					.Visibility(Redo > 0 ? EVisibility::Visible : EVisibility::Collapsed)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("TaskProgress", "{0}/{1}"), FText::AsNumber(Done), FText::AsNumber(Total)))
					.Font(FUOUIStyle::Font("Bold", 12))
					.ColorAndOpacity(bComplete ? FUOUIStyle::Good() : FUOUIStyle::TextDim())
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(26.f, 5.f, 0.f, 0.f)
			[
				SNew(SBox)
				.HeightOverride(3.f)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SNew(SImage).Image(&Style.BarBackBrush)
					]

					+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(FMath::Max(Fraction, 0.0001f))
						[
							SNew(SImage)
							.Image(&Style.BarFillBrush)
							.ColorAndOpacity(bComplete ? FUOUIStyle::Good() : FUOUIStyle::Accent())
							.Visibility(Done > 0 ? EVisibility::Visible : EVisibility::Hidden)
						]

						+ SHorizontalBox::Slot()
						.FillWidth(FMath::Max(1.f - Fraction, 0.0001f))
						[
							SNullWidget::NullWidget
						]
					]
				]
			]
		];
	}
}

void SUOHUDWidget::RebuildFeed(const AUOGameState& GameState)
{
	if (!FeedRows.IsValid() || FeedRows->NumSlots() > 0)
	{
		return;
	}

	// Fixed rows, newest at the bottom; each row reads its entry every frame
	for (int32 Row = 0; Row < UOHUDPrivate::FeedLines; ++Row)
	{
		const int32 Age = UOHUDPrivate::FeedLines - 1 - Row;
		auto Entry = [this, Age]() -> const FUOEvidenceEntry*
		{
			const AUOGameState* State = GetGameState();
			const TArray<FUOEvidenceEntry>* Log = State ? &State->GetEvidenceLog() : nullptr;
			const int32 Index = Log ? Log->Num() - 1 - Age : INDEX_NONE;
			return Log && Log->IsValidIndex(Index) ? &(*Log)[Index] : nullptr;
		};

		FeedRows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Entry]()
				{
					const FUOEvidenceEntry* Item = Entry();
					return Item ? FText::FromString(Item->ClockTime) : FText::GetEmpty();
				})
				.Font(FUOUIStyle::Font("Bold", 11))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.55f - 0.1f * Age))
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text_Lambda([this, Entry]()
				{
					const FUOEvidenceEntry* Item = Entry();
					if (!Item)
					{
						return FText::GetEmpty();
					}
					switch (Item->Action)
					{
					case EUOEvidenceAction::PickedUpItem:
						return FText::Format(LOCTEXT("FeedPickup", "{0} took {1}"), FText::FromString(Item->EmployeeName), UOText::GetItemName(Item->Item).ToLower());
					case EUOEvidenceAction::DroppedItem:
						return FText::Format(LOCTEXT("FeedDrop", "{0} put back {1}"), FText::FromString(Item->EmployeeName), UOText::GetItemName(Item->Item).ToLower());
					default:
						return FText::Format(LOCTEXT("FeedDone", "{0} finished work at {1}"), FText::FromString(Item->EmployeeName), GetStationLabel(Item->StationId));
					}
				})
				.Font(FUOUIStyle::Body(12.f))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.9f - 0.15f * Age))
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
			]
		];
	}
}

TSharedRef<SWidget> SUOHUDWidget::MakePrompt()
{
	const FUOUIStyle& Style = FUOUIStyle::Get();

	return SNew(SBorder)
		.BorderImage(&Style.PanelBrush)
		.Padding(FMargin(14.f, 10.f, 20.f, 10.f))
		.Visibility_Lambda([this]()
		{
			const UUOInteractionComponent* Interaction = GetInteraction();
			return Interaction && Interaction->GetFocusedStation() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)

			// [E] key with a hold ring around it
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(46.f)
				.HeightOverride(46.f)
				.Visibility_Lambda([this]()
				{
					const UUOInteractionComponent* Interaction = GetInteraction();
					return Interaction && Interaction->GetCurrentOption().bAvailable ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						FUOUIStyle::MakeKeyCap(LOCTEXT("KeyE", "E"), 28.f)
					]

					+ SOverlay::Slot()
					[
						SNew(SUOIcon)
						.Type(EUOIconType::Ring)
						.Size(46.f)
						.Thickness(3.f)
						.Color_Lambda([this]()
						{
							const UUOInteractionComponent* Interaction = GetInteraction();
							const bool bHold = Interaction && Interaction->GetCurrentOption().HoldSeconds > 0.f;
							return bHold ? FUOUIStyle::AccentBright() : FLinearColor(1.f, 1.f, 1.f, 0.f);
						})
						.Progress_Lambda([this]()
						{
							const UUOInteractionComponent* Interaction = GetInteraction();
							return Interaction ? Interaction->GetHoldProgress() : 0.f;
						})
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						const AUOInteractableStation* Station = Interaction ? Interaction->GetFocusedStation() : nullptr;
						return Station ? Station->GetDisplayName().ToUpper() : FText::GetEmpty();
					})
					.Font(FUOUIStyle::Label(9.f))
					.ColorAndOpacity(FUOUIStyle::TextDim())
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						if (!Interaction)
						{
							return FText::GetEmpty();
						}
						const FUOInteractionOption& Option = Interaction->GetCurrentOption();
						if (!Option.bAvailable)
						{
							return Option.BlockedReason;
						}
						if (Interaction->IsHolding())
						{
							return FText::Format(LOCTEXT("Working", "{0}…"), Option.Prompt);
						}
						return Option.HoldSeconds > 0.f
							? FText::Format(LOCTEXT("HoldPrompt", "Hold to {0}"), Option.Prompt.ToLower())
							: Option.Prompt;
					})
					.Font_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						const bool bAvailable = Interaction && Interaction->GetCurrentOption().bAvailable;
						return bAvailable ? FUOUIStyle::Font("Bold", 17) : FUOUIStyle::Body(14.f);
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						const bool bAvailable = Interaction && Interaction->GetCurrentOption().bAvailable;
						return FSlateColor(bAvailable ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.72f));
					})
				]

			]
		];
}

TSharedRef<SWidget> SUOHUDWidget::MakeMicIndicator()
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	auto IsMuted = [this]()
	{
		const AUOPlayerController* PC = Cast<AUOPlayerController>(Owner.Get());
		return !PC || PC->IsMicMuted();
	};
	return SNew(SBorder)
		.BorderImage(&Style.CardBrush)
		.Padding(FMargin(8.f, 3.f))
		.ToolTipText(LOCTEXT("MicTip", "Voice chat (multiplayer). M mutes / unmutes; the mode is in Settings."))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SUOIcon).Type(EUOIconType::Mic).Size(16.f).Thickness(1.6f).Color(FUOUIStyle::Good())
					.Visibility_Lambda([IsMuted]() { return IsMuted() ? EVisibility::Collapsed : EVisibility::Visible; })
				]
				+ SOverlay::Slot()
				[
					SNew(SUOIcon).Type(EUOIconType::MicOff).Size(16.f).Thickness(1.6f).Color(FUOUIStyle::AccentBright())
					.Visibility_Lambda([IsMuted]() { return IsMuted() ? EVisibility::Visible : EVisibility::Collapsed; })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FUOUIStyle::Label(8.f))
				.ColorAndOpacity(FUOUIStyle::TextDim())
				.Text_Lambda([IsMuted]()
				{
					if (IsMuted())
					{
						return LOCTEXT("MicMutedLabel", "MIC MUTED  [M]");
					}
					return UUOGameUserSettings::Get()->GetVoiceMode() == EUOVoiceMode::OpenMic ? LOCTEXT("MicOpenLabel", "OPEN MIC  [M]") : LOCTEXT("MicPttLabel", "PUSH-TO-TALK  [M]");
				})
			]
		];
}

bool SUOHUDWidget::IsKiller() const
{
	const AUOPlayerState* Me = GetEmployee();
	return Me && Me->GetKnownRole() == EUORole::Killer;
}

TSharedRef<SWidget> SUOHUDWidget::MakeSecondaryPrompt()
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	return SNew(SBorder)
		.BorderImage(&Style.PanelBrush)
		.BorderBackgroundColor(FLinearColor(1.f, 0.6f, 0.6f))
		.Padding(FMargin(12.f, 8.f, 18.f, 8.f))
		.Visibility_Lambda([this]()
		{
			const UUOInteractionComponent* Interaction = GetInteraction();
			return IsKiller() && Interaction && Interaction->GetSecondaryOption().Kind != EUOInteractionKind::None ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(40.f)
				.HeightOverride(40.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[FUOUIStyle::MakeKeyCap(LOCTEXT("KeyF", "F"), 24.f)]
					+ SOverlay::Slot()
					[
						SNew(SUOIcon)
						.Type(EUOIconType::Ring)
						.Size(40.f)
						.Thickness(3.f)
						.Color_Lambda([this]()
						{
							const UUOInteractionComponent* Interaction = GetInteraction();
							return Interaction && Interaction->IsSecondaryHolding() ? FUOUIStyle::AccentBright() : FLinearColor(1.f, 1.f, 1.f, 0.f);
						})
						.Progress_Lambda([this]()
						{
							const UUOInteractionComponent* Interaction = GetInteraction();
							return Interaction ? Interaction->GetSecondaryProgress() : 0.f;
						})
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						return Interaction && Interaction->GetSecondaryOption().Kind == EUOInteractionKind::Kill ? LOCTEXT("SecKill", "DISMISSAL") : LOCTEXT("SecSabotage", "SABOTAGE");
					})
					.Font(FUOUIStyle::Label(9.f))
					.ColorAndOpacity(FUOUIStyle::AccentBright())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						if (!Interaction)
						{
							return FText::GetEmpty();
						}
						const FUOInteractionOption& Option = Interaction->GetSecondaryOption();
						if (!Option.bAvailable)
						{
							return FText::Format(LOCTEXT("SecBlocked", "{0}: {1}"), Option.Prompt, Option.BlockedReason);
						}
						return FText::Format(LOCTEXT("SecHold", "Hold to {0}"), Option.Prompt.ToLower());
					})
					.Font_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						return Interaction && Interaction->GetSecondaryOption().bAvailable ? FUOUIStyle::Font("Bold", 15) : FUOUIStyle::Body(13.f);
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const UUOInteractionComponent* Interaction = GetInteraction();
						return FSlateColor(Interaction && Interaction->GetSecondaryOption().bAvailable ? FLinearColor::White : FLinearColor(0.75f, 0.7f, 0.7f));
					})
				]
			]
		];
}

TSharedRef<SWidget> SUOHUDWidget::MakeKillerPanel()
{
	const FUOUIStyle& Style = FUOUIStyle::Get();
	TSharedRef<SVerticalBox> Cooldowns = SNew(SVerticalBox);
	for (int32 Index = 0; Index < static_cast<int32>(EUOSabotageType::Count); ++Index)
	{
		const EUOSabotageType Type = static_cast<EUOSabotageType>(Index);
		Cooldowns->AddSlot()
		.AutoHeight()
		.Padding(0.f, 1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(FUOTraceText::GetSabotageName(Type)).Font(FUOUIStyle::Body(12.f)).ColorAndOpacity(FUOUIStyle::Text())
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(FUOUIStyle::Font("Bold", 12))
				.Text_Lambda([this, Index]()
				{
					const AUOPlayerState* Me = GetEmployee();
					const AUOGameState* GameState = GetGameState();
					if (!Me || !GameState || !Me->GetKillerView().ReadyAt.IsValidIndex(Index))
					{
						return FText::GetEmpty();
					}
					const float Left = Me->GetKillerView().ReadyAt[Index] - GameState->GetShiftElapsedSeconds();
					return Left > 0.f ? FText::Format(LOCTEXT("CooldownLeft", "{0}s"), FText::AsNumber(FMath::CeilToInt(Left))) : LOCTEXT("Ready", "READY");
				})
				.ColorAndOpacity_Lambda([this, Index]()
				{
					const AUOPlayerState* Me = GetEmployee();
					const AUOGameState* GameState = GetGameState();
					const bool bReady = Me && GameState && Me->GetKillerView().ReadyAt.IsValidIndex(Index) && Me->GetKillerView().ReadyAt[Index] <= GameState->GetShiftElapsedSeconds();
					return FSlateColor(bReady ? FUOUIStyle::Good() : FUOUIStyle::TextDim());
				})
			]
		];
	}

	return SNew(SBox)
		.WidthOverride(300.f)
		.Visibility_Lambda([this]()
		{
			const AUOPlayerState* Me = GetEmployee();
			const AUOGameState* GameState = GetGameState();
			return IsKiller() && Me && Me->IsAlive() && GameState ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SBorder)
			.BorderImage(&Style.PanelBrush)
			.Padding(FMargin(16.f, 12.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Text(LOCTEXT("KillerPanel", "SABOTEUR")).Font(FUOUIStyle::Label(10.f)).ColorAndOpacity(FUOUIStyle::AccentBright())
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Font(FUOUIStyle::Label(9.f))
						.ColorAndOpacity(FUOUIStyle::TextDim())
						.Text_Lambda([this]()
						{
							const AUOPlayerState* Me = GetEmployee();
							return FText::Format(LOCTEXT("KillsMatch", "DISMISSALS {0}"), FText::AsNumber(Me ? Me->GetKillerView().KillsThisMatch : 0));
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("Heat", "HEAT")).Font(FUOUIStyle::Label(9.f)).ColorAndOpacity(FUOUIStyle::TextDim())
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(6.f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(&Style.BarBackBrush)]
							+ SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(TAttribute<float>::CreateLambda([this]()
								{
									const AUOPlayerState* Me = GetEmployee();
									return Me ? FMath::Max(0.0001f, Me->GetKillerView().Heat / FMath::Max(1.f, Me->GetKillerView().MaxHeat)) : 0.0001f;
								}))
								[
									SNew(SImage).Image(&Style.BarFillBrush).ColorAndOpacity(FUOUIStyle::AccentBright())
								]
								+ SHorizontalBox::Slot()
								.FillWidth(TAttribute<float>::CreateLambda([this]()
								{
									const AUOPlayerState* Me = GetEmployee();
									return Me ? FMath::Max(0.0001f, 1.f - Me->GetKillerView().Heat / FMath::Max(1.f, Me->GetKillerView().MaxHeat)) : 1.f;
								}))
								[
									SNullWidget::NullWidget
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					Cooldowns
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FUOUIStyle::Font("Bold", 12))
					.ColorAndOpacity(FUOUIStyle::Warning())
					.Visibility_Lambda([this]()
					{
						const AUOPlayerState* Me = GetEmployee();
						return Me && Me->GetKillerView().bKillEnabled ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Text_Lambda([this]()
					{
						const AUOPlayerState* Me = GetEmployee();
						const AUOGameState* GameState = GetGameState();
						if (!Me || !GameState)
						{
							return FText::GetEmpty();
						}
						const FUOKillerPrivateView& View = Me->GetKillerView();
						const float Left = View.KillReadyAt - GameState->GetShiftElapsedSeconds();
						return Left > 0.f ? FText::Format(LOCTEXT("KillCooldown", "Dismissal: {0}s"), FText::AsNumber(FMath::CeilToInt(Left))) : LOCTEXT("KillReady", "Dismissal: ready (no witnesses)");
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FUOUIStyle::Font("Bold", 11))
					.ColorAndOpacity(FUOUIStyle::AccentBright())
					.WrapTextAt(268.f)
					.Visibility_Lambda([this]()
					{
						const AUOPlayerState* Me = GetEmployee();
						return Me && Me->GetKillerView().AccompliceIds.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Text_Lambda([this]()
					{
						const AUOPlayerState* Me = GetEmployee();
						const AUOGameState* GameState = GetGameState();
						FString Names;
						if (Me && GameState)
						{
							for (const int32 Id : Me->GetKillerView().AccompliceIds)
							{
								const AUOPlayerState* Other = GameState->FindEmployee(Id);
								if (Other)
								{
									Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Other->GetDisplayName() + (Other->IsAlive() ? TEXT("") : TEXT(" (gone)"));
								}
							}
						}
						return FText::Format(LOCTEXT("Accomplices", "Accomplice: {0}"), FText::FromString(Names));
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KillerHint", "Your work is real and counts. Hold F on a machine, desk PC, camera or plant to sabotage. Every sabotage and dismissal leaves a trace."))
					.Font(FUOUIStyle::Body(11.f))
					.ColorAndOpacity(FUOUIStyle::TextDim())
					.WrapTextAt(268.f)
				]
			]
		];
}

TSharedRef<SWidget> SUOHUDWidget::MakeCarriedChip()
{
	const FUOUIStyle& Style = FUOUIStyle::Get();

	return SNew(SBorder)
		.BorderImage(&Style.PanelBrush)
		.Padding(FMargin(14.f, 9.f))
		.Visibility_Lambda([this]()
		{
			const AUOPlayerState* Me = GetEmployee();
			return Me && Me->IsCarryingItem() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(14.f)
				.HeightOverride(18.f)
				[
					SNew(SBorder)
					.BorderImage(&Style.KeyCapBrush)
					.BorderBackgroundColor_Lambda([this]()
					{
						const AUOPlayerState* Me = GetEmployee();
						return FSlateColor(UOHUDPrivate::ItemColor(Me ? Me->GetCarriedItem() : EUOItemType::None));
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 18.f, 0.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Carrying", "CARRYING"))
					.Font(FUOUIStyle::Label(8.f))
					.ColorAndOpacity(FUOUIStyle::TextDim())
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const AUOPlayerState* Me = GetEmployee();
						return Me ? UOText::GetItemName(Me->GetCarriedItem()) : FText::GetEmpty();
					})
					.Font(FUOUIStyle::Font("Bold", 14))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				FUOUIStyle::MakeKeyCap(LOCTEXT("KeyQ", "Q"), 22.f)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Drop", "Put back"))
				.Font(FUOUIStyle::Body(12.f))
				.ColorAndOpacity(FUOUIStyle::TextDim())
			]
		];
}

TSharedRef<SWidget> SUOHUDWidget::MakeOvertimeBanner()
{
	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return IsOvertime() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(&FUOUIStyle::Get().PillAccentBrush)
			.BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.65f + 0.35f * GetOvertimePulse())); })
			.Padding(FMargin(22.f, 7.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OvertimeBanner", "UNPAID OVERTIME"))
				.Font(FUOUIStyle::Font("Bold", 20, 320))
				.ColorAndOpacity(FLinearColor::White)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OvertimeHint", "The shift is over. The work is not. Nobody is getting paid for this."))
			.Font(FUOUIStyle::Body(13.f))
			.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.85f))
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
		];
}

TSharedRef<SWidget> SUOHUDWidget::MakeEdgeGlow(bool bHorizontalBand, bool bFromStart, float Thickness)
{
	constexpr int32 Strips = 16;
	const FUOUIStyle& Style = FUOUIStyle::Get();

	auto StripColor = [this, bFromStart](int32 Index)
	{
		return [this, Index, bFromStart]()
		{
			if (!IsOvertime())
			{
				return FSlateColor(FLinearColor::Transparent);
			}
			const float T = static_cast<float>(Index) / (Strips - 1);
			const float Edge = FMath::Pow(bFromStart ? 1.f - T : T, 2.2f);
			FLinearColor Color = FUOUIStyle::Accent();
			Color.A = Edge * (0.22f + 0.2f * GetOvertimePulse());
			return FSlateColor(Color);
		};
	};

	if (bHorizontalBand)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 Index = 0; Index < Strips; ++Index)
		{
			Box->AddSlot().FillHeight(1.f)[SNew(SImage).Image(&Style.WhiteBrush).ColorAndOpacity_Lambda(StripColor(Index))];
		}
		return SNew(SBox).HeightOverride(Thickness)[Box];
	}

	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Strips; ++Index)
	{
		Box->AddSlot().FillWidth(1.f)[SNew(SImage).Image(&Style.WhiteBrush).ColorAndOpacity_Lambda(StripColor(Index))];
	}
	return SNew(SBox).WidthOverride(Thickness)[Box];
}

FText SUOHUDWidget::GetStationLabel(FName StationId) const
{
	const APlayerController* PC = Owner.Get();
	const AUOGameMode* GameMode = PC && PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	const UUOTaskManagerComponent* TaskManager = GameMode ? GameMode->GetTaskManager() : nullptr;
	const AUOInteractableStation* Station = TaskManager ? TaskManager->FindStation(StationId) : nullptr;
	return Station ? Station->GetDisplayName() : FText::FromName(StationId);
}

#undef LOCTEXT_NAMESPACE
