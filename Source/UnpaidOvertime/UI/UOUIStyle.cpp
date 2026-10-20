// UNPAID: OVERTIME - shared Slate look.

#include "UOUIStyle.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "UOAudioSubsystem.h"
#include "UnpaidOvertime.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace UOUIStylePrivate
{
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		FLinearColor Color(FColor(R, G, B));
		Color.A = A;
		return Color;
	}

	FSlateRoundedBoxBrush Rounded(const FLinearColor& Fill, float Radius)
	{
		return FSlateRoundedBoxBrush(Fill, Radius);
	}

	FSlateRoundedBoxBrush RoundedOutlined(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float Width)
	{
		FSlateRoundedBoxBrush Brush(Fill, Radius);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), Width);
		return Brush;
	}

	FButtonStyle MakeButtonStyle(const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed, const FLinearColor& Outline, float Radius)
	{
		FButtonStyle Style;
		Style.SetNormal(RoundedOutlined(Normal, Radius, Outline, 1.f));
		Style.SetHovered(RoundedOutlined(Hovered, Radius, Hovered, 1.f));
		Style.SetPressed(RoundedOutlined(Pressed, Radius, Pressed, 1.f));
		Style.SetDisabled(RoundedOutlined(FLinearColor(Normal.R, Normal.G, Normal.B, Normal.A * 0.35f), Radius, FLinearColor(1.f, 1.f, 1.f, 0.05f), 1.f));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		Style.SetNormalForeground(FSlateColor(FLinearColor::White));
		Style.SetHoveredForeground(FSlateColor(FLinearColor::White));
		Style.SetPressedForeground(FSlateColor(FLinearColor::White));
		Style.SetDisabledForeground(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.35f)));
		return Style;
	}
}

FLinearColor FUOUIStyle::Accent()		{ return UOUIStylePrivate::Srgb(0xC8, 0x10, 0x2E); }
FLinearColor FUOUIStyle::AccentBright()	{ return UOUIStylePrivate::Srgb(0xE8, 0x2A, 0x48); }
FLinearColor FUOUIStyle::Text()			{ return FLinearColor(0.92f, 0.92f, 0.93f); }
FLinearColor FUOUIStyle::TextDim()		{ return FLinearColor(0.42f, 0.43f, 0.46f); }
FLinearColor FUOUIStyle::TextFaint()	{ return FLinearColor(0.2f, 0.21f, 0.23f); }
FLinearColor FUOUIStyle::Good()			{ return UOUIStylePrivate::Srgb(0x5C, 0xC9, 0x7B); }
FLinearColor FUOUIStyle::Warning()		{ return UOUIStylePrivate::Srgb(0xF0, 0xA2, 0x3B); }
FLinearColor FUOUIStyle::Bad()			{ return UOUIStylePrivate::Srgb(0xFF, 0x4D, 0x5E); }
FLinearColor FUOUIStyle::Gold()			{ return UOUIStylePrivate::Srgb(0xF5, 0xC5, 0x42); }
FLinearColor FUOUIStyle::PanelColor()	{ return FLinearColor(0.004f, 0.0045f, 0.006f, 0.82f); }

FSlateFontInfo FUOUIStyle::Font(const ANSICHAR* Typeface, float Size, int32 LetterSpacing)
{
	FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Info.LetterSpacing = LetterSpacing;
	return Info;
}

FSlateFontInfo FUOUIStyle::Title(float Size)	{ return Font("Bold", Size, 40); }
FSlateFontInfo FUOUIStyle::Heading(float Size)	{ return Font("Bold", Size, 60); }
FSlateFontInfo FUOUIStyle::Body(float Size)		{ return Font("Regular", Size); }
FSlateFontInfo FUOUIStyle::Label(float Size)	{ return Font("Bold", Size, 160); }

FUOUIStyle& FUOUIStyle::GetMutable()
{
	static FUOUIStyle Instance;
	return Instance;
}

const FUOUIStyle& FUOUIStyle::Get()
{
	return GetMutable();
}

void FUOUIStyle::SetSounds(const FUOUISounds& InSounds)
{
	FUOUIStyle& Style = GetMutable();
	Style.Sounds = InSounds;

	for (FButtonStyle* Button : { &Style.PrimaryButton, &Style.SecondaryButton })
	{
		Button->SetHoveredSound(InSounds.Hover);
		Button->SetPressedSound(InSounds.Click);
	}
	for (FButtonStyle* Button : { &Style.ChoiceButton, &Style.ChoiceButtonSelected })
	{
		Button->SetHoveredSound(InSounds.Hover);
		Button->SetPressedSound(InSounds.Toggle);
	}
	Style.CheckBoxStyle.SetCheckedSound(InSounds.Toggle);
	Style.CheckBoxStyle.SetUncheckedSound(InSounds.Toggle);
	Style.CheckBoxStyle.SetHoveredSound(InSounds.Hover);
}

void FUOUIStyle::PlaySound(const FSlateSound& Sound)
{
	const UObject* Resource = Sound.GetResourceObject();
	if (FSlateApplication::IsInitialized() && Resource)
	{
		FSlateApplication::Get().PlaySound(Sound);
		if (UUOAudioSubsystem::IsDebugEnabled())
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: ui %s"), *Resource->GetName());
		}
	}
}

FUOUIStyle::FUOUIStyle()
	: PanelBrush(UOUIStylePrivate::RoundedOutlined(FUOUIStyle::PanelColor(), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.06f), 1.f))
	, PanelSolidBrush(UOUIStylePrivate::RoundedOutlined(FLinearColor(0.006f, 0.0065f, 0.009f, 0.96f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.07f), 1.f))
	, CardBrush(UOUIStylePrivate::Rounded(FLinearColor(1.f, 1.f, 1.f, 0.035f), 4.f))
	, KeyCapBrush(UOUIStylePrivate::RoundedOutlined(FLinearColor(0.92f, 0.92f, 0.93f, 1.f), 4.f, FLinearColor(1.f, 1.f, 1.f, 1.f), 1.f))
	, PillAccentBrush(UOUIStylePrivate::Rounded(FUOUIStyle::Accent(), 3.f))
	, DotBrush(UOUIStylePrivate::Rounded(FLinearColor::White, 4.f))
	, BarBackBrush(UOUIStylePrivate::Rounded(FLinearColor(1.f, 1.f, 1.f, 0.08f), 2.f))
	, BarFillBrush(UOUIStylePrivate::Rounded(FLinearColor::White, 2.f))
	, FieldBrush(UOUIStylePrivate::RoundedOutlined(FLinearColor(1.f, 1.f, 1.f, 0.05f), 4.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), 1.f))
	, WhiteBrush(FLinearColor::White)
	, DividerBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f))
{
	using namespace UOUIStylePrivate;

	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);
	PrimaryButton = MakeButtonStyle(Accent(), AccentBright(), Srgb(0x9A, 0x0C, 0x24), Accent(), 4.f);
	SecondaryButton = MakeButtonStyle(FLinearColor(1.f, 1.f, 1.f, 0.05f), FLinearColor(1.f, 1.f, 1.f, 0.14f), FLinearColor(1.f, 1.f, 1.f, 0.2f), FLinearColor(1.f, 1.f, 1.f, 0.14f), 4.f);
	ChoiceButton = MakeButtonStyle(FLinearColor(1.f, 1.f, 1.f, 0.04f), FLinearColor(1.f, 1.f, 1.f, 0.12f), FLinearColor(1.f, 1.f, 1.f, 0.18f), FLinearColor(1.f, 1.f, 1.f, 0.1f), 4.f);
	ChoiceButtonSelected = MakeButtonStyle(FLinearColor(Accent().R, Accent().G, Accent().B, 0.85f), AccentBright(), Accent(), AccentBright(), 4.f);

	// Text box: dark field, accent outline when focused
	TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	TextBoxStyle.SetBackgroundImageNormal(RoundedOutlined(FLinearColor(1.f, 1.f, 1.f, 0.05f), 4.f, FLinearColor(1.f, 1.f, 1.f, 0.16f), 1.f));
	TextBoxStyle.SetBackgroundImageHovered(RoundedOutlined(FLinearColor(1.f, 1.f, 1.f, 0.08f), 4.f, FLinearColor(1.f, 1.f, 1.f, 0.3f), 1.f));
	TextBoxStyle.SetBackgroundImageFocused(RoundedOutlined(FLinearColor(1.f, 1.f, 1.f, 0.08f), 4.f, Accent(), 1.5f));
	TextBoxStyle.SetBackgroundImageReadOnly(RoundedOutlined(FLinearColor(1.f, 1.f, 1.f, 0.02f), 4.f, FLinearColor(1.f, 1.f, 1.f, 0.08f), 1.f));
	TextBoxStyle.SetForegroundColor(FSlateColor(Text()));
	TextBoxStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor::White));
	TextBoxStyle.SetPadding(FMargin(12.f, 9.f));
	TextBoxStyle.SetFont(Body(17.f));

	SliderStyle = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
	SliderStyle.SetNormalBarImage(Rounded(FLinearColor(1.f, 1.f, 1.f, 0.14f), 2.f));
	SliderStyle.SetHoveredBarImage(Rounded(FLinearColor(1.f, 1.f, 1.f, 0.22f), 2.f));
	SliderStyle.SetNormalThumbImage(FSlateRoundedBoxBrush(Accent(), 7.f, FVector2f(14.f, 14.f)));
	SliderStyle.SetHoveredThumbImage(FSlateRoundedBoxBrush(AccentBright(), 7.f, FVector2f(14.f, 14.f)));
	SliderStyle.SetBarThickness(4.f);

	CheckBoxStyle = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox");
	CheckBoxStyle.SetUncheckedImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.05f), 3.f, FVector2f(18.f, 18.f)));
	CheckBoxStyle.SetUncheckedHoveredImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.15f), 3.f, FVector2f(18.f, 18.f)));
	CheckBoxStyle.SetUncheckedPressedImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.2f), 3.f, FVector2f(18.f, 18.f)));
	CheckBoxStyle.SetCheckedImage(FSlateRoundedBoxBrush(Accent(), 3.f, FVector2f(18.f, 18.f)));
	CheckBoxStyle.SetCheckedHoveredImage(FSlateRoundedBoxBrush(AccentBright(), 3.f, FVector2f(18.f, 18.f)));
	CheckBoxStyle.SetCheckedPressedImage(FSlateRoundedBoxBrush(Accent(), 3.f, FVector2f(18.f, 18.f)));
	(void)Clear;
}

TSharedRef<SWidget> FUOUIStyle::MakeMenuButton(const FText& Label, FOnClicked OnClicked, TAttribute<bool> IsEnabled)
{
	TSharedPtr<SButton> Button;
	TSharedRef<SWidget> Result =
		SAssignNew(Button, SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.HoveredSoundOverride(Get().Sounds.Hover)
		.PressedSoundOverride(Get().Sounds.Click)
		.ContentPadding(FMargin(0.f))
		.OnClicked(OnClicked)
		.IsEnabled(IsEnabled)
		.Cursor(EMouseCursor::Hand);

	TWeakPtr<SButton> WeakButton = Button;
	Button->SetContent(
		SNew(SBox)
		.MinDesiredWidth(340.f)
		.Padding(FMargin(0.f, 6.f))
		[
			SNew(SHorizontalBox)

			// Red marker that grows on hover
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SBox)
				.HeightOverride(26.f)
				.WidthOverride_Lambda([WeakButton]()
				{
					const TSharedPtr<SButton> Pinned = WeakButton.Pin();
					return FOptionalSize(Pinned && Pinned->IsHovered() ? 6.f : 2.f);
				})
				[
					SNew(SImage)
					.Image(&Get().WhiteBrush)
					.ColorAndOpacity_Lambda([WeakButton]()
					{
						const TSharedPtr<SButton> Pinned = WeakButton.Pin();
						return FSlateColor(Pinned && Pinned->IsHovered() ? AccentBright() : FLinearColor(1.f, 1.f, 1.f, 0.18f));
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label.ToUpper())
				.Font(Font("Bold", 24, 220))
				.ColorAndOpacity_Lambda([WeakButton]()
				{
					const TSharedPtr<SButton> Pinned = WeakButton.Pin();
					if (Pinned && !Pinned->IsEnabled())
					{
						return FSlateColor(TextFaint());
					}
					return FSlateColor(Pinned && Pinned->IsHovered() ? FLinearColor::White : FLinearColor(0.78f, 0.78f, 0.8f));
				})
			]
		]);
	return Result;
}

TSharedRef<SWidget> FUOUIStyle::MakeButton(const FText& Label, FOnClicked OnClicked, bool bPrimary, TAttribute<bool> IsEnabled, float MinWidth)
{
	return SNew(SBox)
		.MinDesiredWidth(MinWidth)
		[
			SNew(SButton)
			.ButtonStyle(bPrimary ? &Get().PrimaryButton : &Get().SecondaryButton)
			.ContentPadding(FMargin(26.f, 11.f))
			.HAlign(HAlign_Center)
			.OnClicked(OnClicked)
			.IsEnabled(IsEnabled)
			.Cursor(EMouseCursor::Hand)
			[
				SNew(STextBlock)
				.Text(Label.ToUpper())
				.Font(Font("Bold", 13, 140))
			]
		];
}

TSharedRef<SWidget> FUOUIStyle::MakeChoice(const FText& Label, TFunction<bool()> IsSelected, TFunction<void()> OnSelect)
{
	// The button style is not an attribute, so the selected state is an accent layer inside the button
	return SNew(SButton)
		.ButtonStyle(&Get().ChoiceButton)
		.ContentPadding(FMargin(0.f))
		.Cursor(EMouseCursor::Hand)
		.OnClicked_Lambda([OnSelect]()
		{
			OnSelect();
			return FReply::Handled();
		})
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&Get().PillAccentBrush)
				.Visibility_Lambda([IsSelected]() { return IsSelected() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10.f, 8.f))
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Font("Bold", 12, 40))
				.ColorAndOpacity_Lambda([IsSelected]() { return FSlateColor(IsSelected() ? FLinearColor::White : FLinearColor(0.75f, 0.75f, 0.78f)); })
			]
		];
}

TSharedRef<SWidget> FUOUIStyle::MakeSectionLabel(const FText& Label)
{
	return SNew(STextBlock)
		.Text(Label.ToUpper())
		.Font(FUOUIStyle::Label(10.f))
		.ColorAndOpacity(TextDim());
}

TSharedRef<SWidget> FUOUIStyle::MakeKeyCap(const FText& Key, float Size)
{
	return SNew(SBox)
		.MinDesiredWidth(Size)
		.HeightOverride(Size)
		[
			SNew(SBorder)
			.BorderImage(&Get().KeyCapBrush)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(Size * 0.2f, 0.f))
			[
				SNew(STextBlock)
				.Text(Key)
				.Font(Font("Bold", Size * 0.45f))
				.ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.025f))
			]
		];
}

TSharedRef<SWidget> FUOUIStyle::MakeDivider()
{
	return SNew(SBox)
		.HeightOverride(1.f)
		[
			SNew(SImage)
			.Image(&Get().DividerBrush)
		];
}

TSharedRef<SWidget> FUOUIStyle::MakeSideShade(float Width, float MaxAlpha, bool bFromLeft)
{
	constexpr int32 Strips = 96;
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Strips; ++Index)
	{
		const float T = static_cast<float>(Index) / (Strips - 1);
		const float Alpha = MaxAlpha * FMath::Pow(bFromLeft ? 1.f - T : T, 1.6f);
		Box->AddSlot()
		.FillWidth(1.f)
		[
			SNew(SImage)
			.Image(&Get().WhiteBrush)
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, Alpha))
		];
	}
	return SNew(SBox).WidthOverride(Width)[Box];
}
