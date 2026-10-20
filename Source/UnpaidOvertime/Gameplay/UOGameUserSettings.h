// UNPAID: OVERTIME - persisted player settings (display/quality from the engine, plus gameplay options).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "UOAudioSubsystem.h"
#include "UOGameUserSettings.generated.h"

/** Voice chat (multiplayer is M7: in single player this is stored and shown, and does nothing else) */
UENUM(BlueprintType)
enum class EUOVoiceMode : uint8
{
	PushToTalk,
	OpenMic,
	Muted
};

/**
 *  Saved in GameUserSettings.ini. Registered through [/Script/Engine.Engine] GameUserSettingsClassName.
 *  Display mode, resolution and the scalability preset come from the engine base class.
 */
UCLASS(Config=GameUserSettings, ConfigDoNotCheckDefaults)
class UUOGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:

	static constexpr float MinMouseSensitivity = 0.2f;
	static constexpr float MaxMouseSensitivity = 3.f;
	static constexpr float MinFieldOfView = 70.f;
	static constexpr float MaxFieldOfView = 110.f;

	/** The project's settings object (falls back to a transient default if the class is not registered) */
	static UUOGameUserSettings* Get();

	float GetMouseSensitivity() const { return MouseSensitivity; }
	bool IsInvertY() const { return bInvertY; }
	float GetFieldOfView() const { return FieldOfView; }
	float GetMasterVolume() const { return MasterVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetSfxVolume() const { return SfxVolume; }
	float GetAmbienceVolume() const { return AmbienceVolume; }
	float GetUiVolume() const { return UiVolume; }
	EUOVoiceMode GetVoiceMode() const { return VoiceMode; }
	void SetVoiceMode(EUOVoiceMode InMode) { VoiceMode = InMode; }

	/** All volume sliders as one struct for the audio subsystem */
	FUOVolumeLevels GetVolumeLevels() const;

	void SetMouseSensitivity(float Value) { MouseSensitivity = FMath::Clamp(Value, MinMouseSensitivity, MaxMouseSensitivity); }
	void SetInvertY(bool bValue) { bInvertY = bValue; }
	void SetFieldOfView(float Value) { FieldOfView = FMath::Clamp(Value, MinFieldOfView, MaxFieldOfView); }
	void SetMasterVolume(float Value) { MasterVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetMusicVolume(float Value) { MusicVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetSfxVolume(float Value) { SfxVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetAmbienceVolume(float Value) { AmbienceVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetUiVolume(float Value) { UiVolume = FMath::Clamp(Value, 0.f, 1.f); }

	/** Pushes volumes to the sound mix and FOV to local first-person characters */
	void ApplyGameplaySettings(UWorld* World) const;

	virtual void SetToDefaults() override;

protected:

	UPROPERTY(Config)
	float MouseSensitivity = 1.f;

	UPROPERTY(Config)
	bool bInvertY = false;

	UPROPERTY(Config)
	float FieldOfView = 90.f;

	UPROPERTY(Config)
	float MasterVolume = 1.f;

	UPROPERTY(Config)
	float MusicVolume = 0.8f;

	UPROPERTY(Config)
	float SfxVolume = 1.f;

	UPROPERTY(Config)
	float AmbienceVolume = 1.f;

	UPROPERTY(Config)
	float UiVolume = 0.8f;

	UPROPERTY(Config)
	EUOVoiceMode VoiceMode = EUOVoiceMode::PushToTalk;
};
