// UNPAID: OVERTIME - pre-match setup screen (Slate, no binary UI assets).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UOMatchConfig.h"

class SEditableTextBox;

/** Asks the authority to create the company and start the day; returns the server-side validation result */
DECLARE_DELEGATE_RetVal_OneParam(FUOConfigValidationResult, FOnUOCreateCompany, const FUOMatchConfig& /*Config*/);

/**
 *  Company name (required, validated live), tasks per day, day length and AI difficulty.
 *  The Create button stays disabled until the configuration is valid. The widget only collects input:
 *  the game mode validates again and owns every rule.
 */
class SUOMatchSetupWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOMatchSetupWidget) {}
		SLATE_EVENT(FOnUOCreateCompany, OnCreateCompany)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Widget that should receive keyboard focus when the screen opens */
	TSharedPtr<SWidget> GetInitialFocusTarget() const;

	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** Types a company name as if the user did (dev automation) */
	void SetCompanyName(const FString& Name);

	/** Presses Create (dev automation); returns the authority's answer */
	FUOConfigValidationResult SubmitForAutomation();

	FUOMatchConfig& GetMutableConfig() { return Config; }

private:

	FUOMatchConfig Config;
	FOnUOCreateCompany OnCreateCompany;
	FSimpleDelegate OnBack;

	/** Error returned by the authority on the last Create attempt */
	FText AuthorityError;

	bool bNameTouched = false;

	TSharedPtr<SEditableTextBox> NameBox;

	TSharedRef<SWidget> MakeTaskStepper();

	void HandleNameChanged(const FText& NewText);
	void HandleNameCommitted(const FText& NewText, ETextCommit::Type CommitType);

	FText GetValidationText() const;
	FSlateColor GetValidationColor() const;

	bool IsCreateEnabled() const;
	FReply HandleCreateClicked();
};
