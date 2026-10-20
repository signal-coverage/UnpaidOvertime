// UNPAID: OVERTIME - shared Slate look: colors, fonts, brushes and widget factories (no binary UI assets).

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Sound/SlateSound.h"

class SWidget;

/** UI sounds; empty until the audio subsystem loads the sound set (an empty sound plays nothing) */
struct FUOUISounds
{
	FSlateSound Hover;
	FSlateSound Click;
	FSlateSound Back;
	FSlateSound Toggle;
	FSlateSound SliderTick;
	FSlateSound Error;
	FSlateSound MenuOpen;
	FSlateSound MenuClose;
};

/**
 *  One visual language for every screen: dark translucent panels, severance-red accent, Roboto.
 *  Brushes and styles live in a function-local singleton so widgets can keep raw pointers to them.
 */
class FUOUIStyle
{
public:

	static const FUOUIStyle& Get();

	// ~ Colors (linear)
	static FLinearColor Accent();
	static FLinearColor AccentBright();
	static FLinearColor Text();
	static FLinearColor TextDim();
	static FLinearColor TextFaint();
	static FLinearColor Good();
	static FLinearColor Warning();
	static FLinearColor Bad();
	static FLinearColor Gold();
	static FLinearColor PanelColor();

	// ~ Fonts (Roboto from the engine)
	static FSlateFontInfo Font(const ANSICHAR* Typeface, float Size, int32 LetterSpacing = 0);
	static FSlateFontInfo Title(float Size = 64.f);
	static FSlateFontInfo Heading(float Size = 20.f);
	static FSlateFontInfo Body(float Size = 14.f);
	static FSlateFontInfo Label(float Size = 11.f);

	// ~ Brushes
	FSlateRoundedBoxBrush PanelBrush;
	FSlateRoundedBoxBrush PanelSolidBrush;
	FSlateRoundedBoxBrush CardBrush;
	FSlateRoundedBoxBrush KeyCapBrush;
	FSlateRoundedBoxBrush PillAccentBrush;
	FSlateRoundedBoxBrush DotBrush;
	FSlateRoundedBoxBrush BarBackBrush;
	FSlateRoundedBoxBrush BarFillBrush;
	FSlateRoundedBoxBrush FieldBrush;
	FSlateColorBrush WhiteBrush;
	FSlateColorBrush DividerBrush;

	// ~ Widget styles
	FButtonStyle PrimaryButton;
	FButtonStyle SecondaryButton;
	FButtonStyle ChoiceButton;
	FButtonStyle ChoiceButtonSelected;
	FEditableTextBoxStyle TextBoxStyle;
	FSliderStyle SliderStyle;
	FCheckBoxStyle CheckBoxStyle;

	// ~ Sounds

	FUOUISounds Sounds;

	/** Installs the UI sounds into the button/check box styles. Call before menus are built (widgets copy them). */
	static void SetSounds(const FUOUISounds& InSounds);

	/** Plays a UI sound through Slate (keeps playing while the game is paused) */
	static void PlaySound(const FSlateSound& Sound);

	// ~ Factories

	/** Big menu entry (main / pause menu): label left-aligned, red bar on hover */
	static TSharedRef<SWidget> MakeMenuButton(const FText& Label, FOnClicked OnClicked, TAttribute<bool> IsEnabled = true);

	/** Regular button; primary = red filled */
	static TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, bool bPrimary, TAttribute<bool> IsEnabled = true, float MinWidth = 0.f);

	/** Toggle-style choice button */
	static TSharedRef<SWidget> MakeChoice(const FText& Label, TFunction<bool()> IsSelected, TFunction<void()> OnSelect);

	/** Uppercase section label */
	static TSharedRef<SWidget> MakeSectionLabel(const FText& Label);

	/** "[E]"-style keyboard key cap */
	static TSharedRef<SWidget> MakeKeyCap(const FText& Key, float Size = 30.f);

	/** Thin horizontal rule */
	static TSharedRef<SWidget> MakeDivider();

	/** Dark side gradient made of strips (Slate has no gradient brush without textures) */
	static TSharedRef<SWidget> MakeSideShade(float Width, float MaxAlpha, bool bFromLeft);

private:

	FUOUIStyle();

	static FUOUIStyle& GetMutable();
};
