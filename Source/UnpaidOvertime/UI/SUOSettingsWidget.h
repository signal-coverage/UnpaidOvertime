// UNPAID: OVERTIME - settings screen: gameplay, audio, display and quality (persisted via UUOGameUserSettings).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GenericPlatform/GenericWindow.h"

class APlayerController;

class SUOSettingsWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUOSettingsWidget)
		: _bInGame(false)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
		/** Opened from the pause menu (darker backdrop, gameplay keeps running underneath) */
		SLATE_ARGUMENT(bool, bInGame)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	TWeakObjectPtr<APlayerController> Owner;
	FSimpleDelegate OnClose;

	// Pending values (written on Apply)
	float MouseSensitivity = 1.f;
	bool bInvertY = false;
	uint8 VoiceMode = 0;
	float FieldOfView = 90.f;
	float MasterVolume = 1.f;
	float MusicVolume = 1.f;
	float SfxVolume = 1.f;
	float AmbienceVolume = 1.f;
	float UiVolume = 1.f;
	EWindowMode::Type WindowMode = EWindowMode::Windowed;
	TArray<FIntPoint> Resolutions;
	int32 ResolutionIndex = 0;
	int32 QualityLevel = 3;

	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Control) const;
	TSharedRef<SWidget> MakeSlider(TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, TFunction<FText()> ValueText) const;

	/** Volume row: 0..100 % slider that previews the mix live */
	TSharedRef<SWidget> MakeVolumeRow(const FText& Label, float* Value);

	/** Hears the pending volumes right away (reverted by Back) */
	void PreviewVolumes() const;

	FReply HandleApply();
	FReply HandleBack();
};
