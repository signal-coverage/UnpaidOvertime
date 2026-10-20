// UNPAID: OVERTIME - persisted player settings.

#include "UOGameUserSettings.h"
#include "UOCharacter.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UUOGameUserSettings* UUOGameUserSettings::Get()
{
	if (UUOGameUserSettings* Settings = GEngine ? Cast<UUOGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr)
	{
		return Settings;
	}

	// Class not registered in the config: keep working with a non-persisted copy
	static TStrongObjectPtr<UUOGameUserSettings> Fallback;
	if (!Fallback.IsValid())
	{
		Fallback.Reset(NewObject<UUOGameUserSettings>(GetTransientPackage()));
	}
	return Fallback.Get();
}

void UUOGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MouseSensitivity = 1.f;
	bInvertY = false;
	FieldOfView = 90.f;
	MasterVolume = 1.f;
	MusicVolume = 0.8f;
	SfxVolume = 1.f;
	AmbienceVolume = 1.f;
	UiVolume = 0.8f;
	VoiceMode = EUOVoiceMode::PushToTalk;
}

FUOVolumeLevels UUOGameUserSettings::GetVolumeLevels() const
{
	FUOVolumeLevels Levels;
	Levels.Master = MasterVolume;
	Levels.Music = MusicVolume;
	Levels.Sfx = SfxVolume;
	Levels.Ambience = AmbienceVolume;
	Levels.Ui = UiVolume;
	return Levels;
}

void UUOGameUserSettings::ApplyGameplaySettings(UWorld* World) const
{
	if (!World)
	{
		return;
	}

	UUOAudioSubsystem::ApplyVolumes(World, GetVolumeLevels());

	for (TActorIterator<AUOCharacter> It(World); It; ++It)
	{
		if (It->IsLocallyControlled())
		{
			It->ApplyUserSettings();
		}
	}
}
