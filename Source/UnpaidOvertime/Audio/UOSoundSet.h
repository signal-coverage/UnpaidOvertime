// UNPAID: OVERTIME - data asset listing every game sound by id (built by Scripts/Editor/uo_audio.py).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UOSoundSet.generated.h"

class USoundBase;
class USoundClass;
class USoundMix;

/** One sound id and its interchangeable variants (picked at random on playback) */
USTRUCT(BlueprintType)
struct FUOSoundEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound")
	TArray<TObjectPtr<USoundBase>> Variants;
};

/**
 *  Code refers to sounds by id (UOSoundId::*), never by asset path. Per-wave volume, looping, sound class and
 *  attenuation are baked into the waves by the pipeline, so playback code only chooses what and where.
 *  Loaded through the soft path in [/Script/UnpaidOvertime.UOAudioSubsystem] SoundSetPath (DefaultGame.ini).
 */
UCLASS(BlueprintType)
class UUOSoundSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sounds")
	TArray<FUOSoundEntry> Entries;

	/** Mix that carries the player's volume settings as class overrides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundMix> VolumeMix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundClass> MasterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundClass> MusicClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundClass> SfxClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundClass> AmbienceClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundClass> UiClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mix")
	TObjectPtr<USoundClass> VoiceClass;

	/** Entry for an id, nullptr if missing */
	const FUOSoundEntry* FindEntry(FName Id) const;

	/** Total number of waves across entries */
	int32 CountVariants() const;
};
