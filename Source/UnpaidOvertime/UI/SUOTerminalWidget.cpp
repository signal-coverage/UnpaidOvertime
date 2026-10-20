// UNPAID: OVERTIME - a terminal's device log on screen.

#include "SUOTerminalWidget.h"
#include "UOUIStyle.h"
#include "SUOIcon.h"
#include "UOPlayerController.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOTerminal.h"
#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "UODayCalendar.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOTerminal"

namespace UOTerminalWidgetPrivate
{
	constexpr float PanelWidth = 1180.f;
	constexpr float ListHeight = 470.f;

	/** Sabotage lines and "no output" lines stand out a little (they are what people come here for) */
	FLinearColor KindColor(EUOTraceKind Kind)
	{
		if (FUOSuspicionModel::IsSabotageTrace(Kind))
		{
			return FUOUIStyle::Bad();
		}
		if (FUOSuspicionModel::IsNoOutputTrace(Kind))
		{
			return FUOUIStyle::Warning();
		}
		return FUOUIStyle::Text();
	}
}

void SUOTerminalWidget::Construct(const FArguments& InArgs)
{
	using namespace UOTerminalWidgetPrivate;

	Owner = InArgs._Owner;
	Terminal = InArgs._Terminal;
	if (const AUOTerminal* Term = Terminal.Get())
	{
		Term->GetDevice(Device);
	}

	const FUOUIStyle& Style = FUOUIStyle::Get();
	const AUOTerminal* Term = Terminal.Get();
	const FText Kicker = Term ? Term->GetDisplayName().ToUpper() : FText::GetEmpty();

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage).Image(&Style.WhiteBrush).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(PanelWidth)
			[
				SNew(SBorder)
				.BorderImage(&Style.PanelSolidBrush)
				.Padding(FMargin(40.f, 30.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Kicker).Font(FUOUIStyle::Label(11.f)).ColorAndOpacity(FUOUIStyle::AccentBright())
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
						[
							SNew(STextBlock).Text(FUOTraceText::GetDeviceName(Device).ToUpper()).Font(FUOUIStyle::Title(34.f)).ColorAndOpacity(FLinearColor::White)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(STextBlock)
							.Font(FUOUIStyle::Font("Bold", 13, 60))
							.ColorAndOpacity(FUOUIStyle::Gold())
							.Text_Lambda([this]()
							{
								return FText::Format(LOCTEXT("Pinned", "EVIDENCE PINNED  {0} / {1}"), FText::AsNumber(CountPins()), FText::AsNumber(FUOKillerTuning::MaxPins));
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 14.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "Every line comes from the device itself, and devices only know what they saw. Pin up to 6 lines as evidence: a formal accusation needs exactly 3."))
						.Font(FUOUIStyle::Body(14.f))
						.ColorAndOpacity(FUOUIStyle::Text())
						.WrapTextAt(PanelWidth - 80.f)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						SAssignNew(FilterRow, SVerticalBox)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(ListHeight)
						[
							SNew(SBorder)
							.BorderImage(&Style.CardBrush)
							.Padding(FMargin(6.f))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(Rows, SVerticalBox)
								]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Footer", "Work goes on while you read. Red lines are incidents; amber lines are jobs that produced nothing."))
							.Font(FUOUIStyle::Body(12.f))
							.ColorAndOpacity(FUOUIStyle::TextDim())
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
						[
							FUOUIStyle::MakeKeyCap(LOCTEXT("KeyEsc", "Esc"), 26.f)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							FUOUIStyle::MakeButton(LOCTEXT("Close", "Close"), FOnClicked::CreateLambda([this]()
							{
								if (AUOPlayerController* PC = Owner.Get())
								{
									PC->CloseTerminal();
								}
								return FReply::Handled();
							}), false, true, 160.f)
						]
					]
				]
			]
		]
	];

	RebuildFilters();
	RebuildRows();
}

const AUOGameState* SUOTerminalWidget::GetGameState() const
{
	const AUOPlayerController* PC = Owner.Get();
	return PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AUOGameState>() : nullptr;
}

int32 SUOTerminalWidget::GetMyId() const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	return Me ? Me->GetEmployeeId() : INDEX_NONE;
}

FString SUOTerminalWidget::NameOf(int32 EmployeeId) const
{
	if (EmployeeId == GetMyId())
	{
		return LOCTEXT("You", "You").ToString();
	}
	const AUOGameState* GameState = GetGameState();
	return GameState ? GameState->GetEmployeeName(EmployeeId) : FString();
}

bool SUOTerminalWidget::IsPinned(int32 TraceId) const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	return Me && Me->GetPinnedTraceIds().Contains(TraceId);
}

int32 SUOTerminalWidget::CountPins() const
{
	const AUOPlayerController* PC = Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	return Me ? Me->GetPinnedTraceIds().Num() : 0;
}

TArray<const FUOTraceEntry*> SUOTerminalWidget::GetEntries() const
{
	TArray<const FUOTraceEntry*> Entries;
	if (const AUOGameState* GameState = GetGameState())
	{
		for (const FUOTraceEntry& Entry : GameState->GetTraceLog())
		{
			if (Entry.Device == Device && (Filter == INDEX_NONE || Entry.Mentions(Filter)))
			{
				Entries.Add(&Entry);
			}
		}
	}
	Algo::Reverse(Entries);
	return Entries;
}

void SUOTerminalWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const AUOGameState* GameState = GetGameState();
	if (!GameState)
	{
		return;
	}
	const AUOPlayerController* PC = Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	FString Signature = FString::Printf(TEXT("%d|%d|"), GameState->GetTraceLog().Num(), Filter);
	if (Me)
	{
		for (const int32 Id : Me->GetPinnedTraceIds())
		{
			Signature += FString::Printf(TEXT("%d,"), Id);
		}
	}
	if (Signature != BuiltSignature)
	{
		RebuildRows();
	}
}

void SUOTerminalWidget::RebuildFilters()
{
	if (!FilterRow.IsValid())
	{
		return;
	}
	FilterRow->ClearChildren();
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	Wrap->AddSlot()
	[
		SNew(SBox).MinDesiredWidth(110.f)
		[
			FUOUIStyle::MakeChoice(LOCTEXT("Everyone", "Everyone"), [this]() { return Filter == INDEX_NONE; }, [this]() { Filter = INDEX_NONE; RebuildRows(); })
		]
	];
	if (const AUOGameState* GameState = GetGameState())
	{
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (!Employee || (!Employee->IsAlive() && Employee->GetExitDay() < GameState->GetDayNumber()))
			{
				continue;
			}
			const int32 Id = Employee->GetEmployeeId();
			Wrap->AddSlot()
			[
				SNew(SBox).MinDesiredWidth(110.f)
				[
					FUOUIStyle::MakeChoice(FText::FromString(NameOf(Id)), [this, Id]() { return Filter == Id; }, [this, Id]() { Filter = Id; RebuildRows(); })
				]
			];
		}
	}
	FilterRow->AddSlot().AutoHeight()[FUOUIStyle::MakeSectionLabel(LOCTEXT("FilterLabel", "Show lines mentioning"))];
	FilterRow->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[Wrap];
}

void SUOTerminalWidget::RebuildRows()
{
	using namespace UOTerminalWidgetPrivate;

	const AUOGameState* GameState = GetGameState();
	if (!Rows.IsValid() || !GameState)
	{
		return;
	}
	const AUOPlayerController* PC = Owner.Get();
	const AUOPlayerState* Me = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
	BuiltSignature = FString::Printf(TEXT("%d|%d|"), GameState->GetTraceLog().Num(), Filter);
	if (Me)
	{
		for (const int32 Id : Me->GetPinnedTraceIds())
		{
			BuiltSignature += FString::Printf(TEXT("%d,"), Id);
		}
	}

	Rows->ClearChildren();
	const FUOUIStyle& Style = FUOUIStyle::Get();
	const TArray<const FUOTraceEntry*> Entries = GetEntries();
	if (Entries.Num() == 0)
	{
		Rows->AddSlot().AutoHeight().Padding(14.f, 12.f)
		[
			SNew(STextBlock).Text(LOCTEXT("Empty", "Nothing logged yet today.")).Font(FUOUIStyle::Body(14.f)).ColorAndOpacity(FUOUIStyle::TextDim())
		];
		return;
	}

	for (const FUOTraceEntry* Entry : Entries)
	{
		const int32 TraceId = Entry->Id;
		const bool bPinned = IsPinned(TraceId);
		const FText Line = FUOTraceText::Describe(*Entry, [this](int32 Id) { return NameOf(Id); });
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.CardBrush)
			.BorderBackgroundColor(bPinned ? FUOUIStyle::Gold() * FLinearColor(1.f, 1.f, 1.f, 0.6f) : FLinearColor::White)
			.Padding(FMargin(12.f, 7.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
				[
					SNew(SBox).WidthOverride(52.f)
					[
						SNew(STextBlock).Text(FText::FromString(Entry->ClockTime)).Font(FUOUIStyle::Font("Bold", 14)).ColorAndOpacity(FUOUIStyle::TextDim())
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Line).Font(FUOUIStyle::Body(14.f)).ColorAndOpacity(KindColor(Entry->Kind)).WrapTextAt(880.f)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(bPinned ? &Style.ChoiceButtonSelected : &Style.ChoiceButton)
					.ContentPadding(FMargin(12.f, 5.f))
					.Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([this, TraceId]()
					{
						if (AUOPlayerController* Controller = Owner.Get())
						{
							Controller->TogglePin(TraceId);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(bPinned ? LOCTEXT("Unpin", "PINNED") : LOCTEXT("Pin", "PIN"))
						.Font(FUOUIStyle::Label(10.f))
					]
				]
			]
		];
	}
}

int32 SUOTerminalWidget::PinWhere(TFunctionRef<bool(const FUOTraceEntry&)> Predicate, int32 MaxCount)
{
	int32 Pinned = 0;
	AUOPlayerController* PC = Owner.Get();
	if (!PC)
	{
		return 0;
	}
	for (const FUOTraceEntry* Entry : GetEntries())
	{
		if (Pinned >= MaxCount)
		{
			break;
		}
		if (!IsPinned(Entry->Id) && Predicate(*Entry) && PC->TogglePin(Entry->Id))
		{
			++Pinned;
		}
	}
	return Pinned;
}

#undef LOCTEXT_NAMESPACE
