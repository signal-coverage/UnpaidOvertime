// UNPAID: OVERTIME - settings screen.

#include "SUOSettingsWidget.h"
#include "UOUIStyle.h"
#include "UOGameUserSettings.h"
#include "UOAudioSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UOSettings"

namespace UOSettingsPrivate
{
	const FIntPoint CommonResolutions[] = {
		FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160)
	};

	FText QualityName(int32 Level)
	{
		switch (Level)
		{
		case 0:		return LOCTEXT("QualityLow", "Low");
		case 1:		return LOCTEXT("QualityMedium", "Medium");
		case 2:		return LOCTEXT("QualityHigh", "High");
		case 3:		return LOCTEXT("QualityEpic", "Epic");
		default:	return LOCTEXT("QualityCinematic", "Cinematic");
		}
	}
}

void SUOSettingsWidget::Construct(const FArguments& InArgs)
{
	using namespace UOSettingsPrivate;

	Owner = InArgs._Owner;
	OnClose = InArgs._OnClose;

	const UUOGameUserSettings* Settings = UUOGameUserSettings::Get();
	MouseSensitivity = Settings->GetMouseSensitivity();
	bInvertY = Settings->IsInvertY();
	FieldOfView = Settings->GetFieldOfView();
	MasterVolume = Settings->GetMasterVolume();
	MusicVolume = Settings->GetMusicVolume();
	SfxVolume = Settings->GetSfxVolume();
	AmbienceVolume = Settings->GetAmbienceVolume();
	UiVolume = Settings->GetUiVolume();
	VoiceMode = static_cast<uint8>(Settings->GetVoiceMode());
	WindowMode = Settings->GetFullscreenMode();
	// -1 means a custom mix of scalability groups: show Epic until the player picks a preset
	const int32 SavedQuality = Settings->GetOverallScalabilityLevel();
	QualityLevel = SavedQuality < 0 ? 3 : FMath::Clamp(SavedQuality, 0, 4);

	// Resolutions the monitor supports, else a common list
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	for (const FIntPoint& Resolution : Supported)
	{
		if (Resolution.X >= 1280 && Resolution.Y >= 720)
		{
			Resolutions.AddUnique(Resolution);
		}
	}
	if (Resolutions.Num() == 0)
	{
		Resolutions.Append(CommonResolutions, UE_ARRAY_COUNT(CommonResolutions));
	}
	const FIntPoint Current = Settings->GetScreenResolution();
	Resolutions.AddUnique(Current);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	ResolutionIndex = FMath::Max(0, Resolutions.IndexOfByKey(Current));

	const FUOUIStyle& Style = FUOUIStyle::Get();

	auto ChoiceRow = [](TArray<TSharedRef<SWidget>> Buttons)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			Row->AddSlot()
			.FillWidth(1.f)
			.Padding(Index == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f)
			[
				Buttons[Index]
			];
		}
		return Row;
	};

	TArray<TSharedRef<SWidget>> QualityButtons;
	for (int32 Level = 0; Level <= 4; ++Level)
	{
		QualityButtons.Add(FUOUIStyle::MakeChoice(QualityName(Level), [this, Level]() { return QualityLevel == Level; }, [this, Level]() { QualityLevel = Level; }));
	}

	auto ArrowButton = [this](const FText& Label, int32 Delta)
	{
		return SNew(SBox)
			.WidthOverride(40.f)
			[
				SNew(SButton)
				.ButtonStyle(&FUOUIStyle::Get().ChoiceButton)
				.HAlign(HAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this, Delta]()
				{
					ResolutionIndex = (ResolutionIndex + Delta + Resolutions.Num()) % Resolutions.Num();
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(FUOUIStyle::Font("Bold", 14))
				]
			];
	};

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&Style.WhiteBrush)
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, InArgs._bInGame ? 0.72f : 0.55f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(800.f)
			[
				SNew(SBorder)
				.BorderImage(&Style.PanelSolidBrush)
				.Padding(FMargin(44.f, 36.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 26.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "SETTINGS"))
						.Font(FUOUIStyle::Heading(28.f))
						.ColorAndOpacity(FLinearColor::White)
					]

					// Gameplay
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("Gameplay", "Controls & view"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeRow(LOCTEXT("Sensitivity", "Mouse sensitivity"),
							MakeSlider([this]() { return MouseSensitivity; }, [this](float V) { MouseSensitivity = V; },
								UUOGameUserSettings::MinMouseSensitivity, UUOGameUserSettings::MaxMouseSensitivity,
								[this]()
								{
									static const FNumberFormattingOptions TwoDecimals = FNumberFormattingOptions().SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
									return FText::AsNumber(MouseSensitivity, &TwoDecimals);
								}))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeRow(LOCTEXT("InvertY", "Invert vertical look"),
							SNew(SCheckBox)
							.Style(&Style.CheckBoxStyle)
							.IsChecked_Lambda([this]() { return bInvertY ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bInvertY = State == ECheckBoxState::Checked; }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
					[
						MakeRow(LOCTEXT("Fov", "Field of view"),
							MakeSlider([this]() { return FieldOfView; }, [this](float V) { FieldOfView = FMath::RoundToFloat(V); },
								UUOGameUserSettings::MinFieldOfView, UUOGameUserSettings::MaxFieldOfView,
								[this]() { return FText::Format(LOCTEXT("FovValue", "{0}°"), FText::AsNumber(FMath::RoundToInt(FieldOfView))); }))
					]

					// Audio
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("Audio", "Audio"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeVolumeRow(LOCTEXT("Volume", "Master volume"), &MasterVolume)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeVolumeRow(LOCTEXT("MusicVolume", "Music"), &MusicVolume)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeVolumeRow(LOCTEXT("SfxVolume", "Effects"), &SfxVolume)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeVolumeRow(LOCTEXT("AmbienceVolume", "Ambience"), &AmbienceVolume)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeVolumeRow(LOCTEXT("UiVolume", "Interface"), &UiVolume)
					]
					// Voice chat: multiplayer (M7) only; stored now, the mute key (M) toggles it in game
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
					[
						MakeRow(LOCTEXT("VoiceChat", "Voice chat (M to mute)"),
							ChoiceRow({
								FUOUIStyle::MakeChoice(LOCTEXT("PushToTalk", "Push-to-talk"), [this]() { return VoiceMode == static_cast<uint8>(EUOVoiceMode::PushToTalk); }, [this]() { VoiceMode = static_cast<uint8>(EUOVoiceMode::PushToTalk); }),
								FUOUIStyle::MakeChoice(LOCTEXT("OpenMic", "Open mic"), [this]() { return VoiceMode == static_cast<uint8>(EUOVoiceMode::OpenMic); }, [this]() { VoiceMode = static_cast<uint8>(EUOVoiceMode::OpenMic); }),
								FUOUIStyle::MakeChoice(LOCTEXT("MicMuted", "Muted"), [this]() { return VoiceMode == static_cast<uint8>(EUOVoiceMode::Muted); }, [this]() { VoiceMode = static_cast<uint8>(EUOVoiceMode::Muted); })
							}))
					]

					// Display
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						FUOUIStyle::MakeSectionLabel(LOCTEXT("Display", "Display"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeRow(LOCTEXT("WindowMode", "Display mode"),
							ChoiceRow({
								FUOUIStyle::MakeChoice(LOCTEXT("Windowed", "Windowed"), [this]() { return WindowMode == EWindowMode::Windowed; }, [this]() { WindowMode = EWindowMode::Windowed; }),
								FUOUIStyle::MakeChoice(LOCTEXT("Borderless", "Borderless"), [this]() { return WindowMode == EWindowMode::WindowedFullscreen; }, [this]() { WindowMode = EWindowMode::WindowedFullscreen; }),
								FUOUIStyle::MakeChoice(LOCTEXT("Fullscreen", "Fullscreen"), [this]() { return WindowMode == EWindowMode::Fullscreen; }, [this]() { WindowMode = EWindowMode::Fullscreen; })
							}))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeRow(LOCTEXT("Resolution", "Resolution"),
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[ArrowButton(LOCTEXT("Prev", "<"), -1)]
							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									const FIntPoint& R = Resolutions[ResolutionIndex];
									return FText::Format(LOCTEXT("ResolutionValue", "{0} × {1}"), FText::AsNumber(R.X, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(R.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
								})
								.Font(FUOUIStyle::Font("Bold", 14))
							]
							+ SHorizontalBox::Slot().AutoWidth()[ArrowButton(LOCTEXT("Next", ">"), 1)])
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 26.f)
					[
						MakeRow(LOCTEXT("Quality", "Graphics quality"), ChoiceRow(QualityButtons))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 20.f)
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
							.Text(LOCTEXT("SavedHint", "Settings are saved to your profile."))
							.Font(FUOUIStyle::Body(12.f))
							.ColorAndOpacity(FUOUIStyle::TextDim())
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 10.f, 0.f)
						[
							FUOUIStyle::MakeButton(LOCTEXT("Back", "Back"), FOnClicked::CreateSP(this, &SUOSettingsWidget::HandleBack), false)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							FUOUIStyle::MakeButton(LOCTEXT("Apply", "Apply"), FOnClicked::CreateSP(this, &SUOSettingsWidget::HandleApply), true, true, 120.f)
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SUOSettingsWidget::MakeRow(const FText& Label, TSharedRef<SWidget> Control) const
{
	return SNew(SBox)
		.MinDesiredHeight(44.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.34f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FUOUIStyle::Body(15.f))
				.ColorAndOpacity(FUOUIStyle::Text())
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.66f)
			.VAlign(VAlign_Center)
			[
				Control
			]
		];
}

TSharedRef<SWidget> SUOSettingsWidget::MakeVolumeRow(const FText& Label, float* Value)
{
	return MakeRow(Label,
		MakeSlider([Value]() { return *Value; }, [this, Value](float V) { *Value = V; PreviewVolumes(); }, 0.f, 1.f,
			[Value]() { return FText::AsPercent(*Value); }));
}

void SUOSettingsWidget::PreviewVolumes() const
{
	FUOVolumeLevels Levels;
	Levels.Master = MasterVolume;
	Levels.Music = MusicVolume;
	Levels.Sfx = SfxVolume;
	Levels.Ambience = AmbienceVolume;
	Levels.Ui = UiVolume;
	if (const APlayerController* PC = Owner.Get())
	{
		UUOAudioSubsystem::ApplyVolumes(PC->GetWorld(), Levels);
	}
}

TSharedRef<SWidget> SUOSettingsWidget::MakeSlider(TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, TFunction<FText()> ValueText) const
{
	// A soft tick every 5 % of travel while dragging
	TSharedRef<int32> LastTickStep = MakeShared<int32>(INDEX_NONE);
	TFunction<void(float)> SetWithTick = [Set = MoveTemp(Set), LastTickStep, Min, Max](float V)
	{
		Set(V);
		const int32 Step = FMath::FloorToInt((V - Min) / FMath::Max(UE_KINDA_SMALL_NUMBER, Max - Min) * 20.f);
		if (*LastTickStep != INDEX_NONE && Step != *LastTickStep)
		{
			FUOUIStyle::PlaySound(FUOUIStyle::Get().Sounds.SliderTick);
		}
		*LastTickStep = Step;
	};

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(SSlider)
			.Style(&FUOUIStyle::Get().SliderStyle)
			.MinValue(Min)
			.MaxValue(Max)
			.Value_Lambda(MoveTemp(Get))
			.OnValueChanged_Lambda(MoveTemp(SetWithTick))
			.Cursor(EMouseCursor::Hand)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(52.f)
			[
				SNew(STextBlock)
				.Text_Lambda(MoveTemp(ValueText))
				.Font(FUOUIStyle::Font("Bold", 13))
				.Justification(ETextJustify::Right)
			]
		];
}

FReply SUOSettingsWidget::HandleApply()
{
	UUOGameUserSettings* Settings = UUOGameUserSettings::Get();
	Settings->SetMouseSensitivity(MouseSensitivity);
	Settings->SetInvertY(bInvertY);
	Settings->SetFieldOfView(FieldOfView);
	Settings->SetMasterVolume(MasterVolume);
	Settings->SetMusicVolume(MusicVolume);
	Settings->SetSfxVolume(SfxVolume);
	Settings->SetAmbienceVolume(AmbienceVolume);
	Settings->SetUiVolume(UiVolume);
	Settings->SetVoiceMode(static_cast<EUOVoiceMode>(VoiceMode));
	Settings->SetFullscreenMode(WindowMode);
	if (Resolutions.IsValidIndex(ResolutionIndex))
	{
		Settings->SetScreenResolution(Resolutions[ResolutionIndex]);
	}
	Settings->SetOverallScalabilityLevel(QualityLevel);
	Settings->ApplySettings(false);
	Settings->SaveSettings();

	if (APlayerController* PC = Owner.Get())
	{
		Settings->ApplyGameplaySettings(PC->GetWorld());
	}

	OnClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SUOSettingsWidget::HandleBack()
{
	// Drop the live volume preview
	if (APlayerController* PC = Owner.Get())
	{
		UUOAudioSubsystem::ApplyVolumes(PC->GetWorld(), UUOGameUserSettings::Get()->GetVolumeLevels());
	}
	OnClose.ExecuteIfBound();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
