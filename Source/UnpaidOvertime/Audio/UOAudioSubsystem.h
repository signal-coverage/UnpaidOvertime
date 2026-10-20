// UNPAID: OVERTIME - per-world audio director: sound playback by id, music, ambience moods, day-flow stingers, volumes.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Math/RandomStream.h"
#include "UOCoreTypes.h"
#include "UOAudioRules.h"
#include "UOAudioSubsystem.generated.h"

class UUOSoundSet;
class USoundBase;
class UAudioComponent;
class AUOGameState;

/** Player volume levels, 0..1 each (category levels are scaled by Master) */
struct FUOVolumeLevels
{
	float Master = 1.f;
	float Music = 1.f;
	float Sfx = 1.f;
	float Ambience = 1.f;
	float Ui = 1.f;
};

/**
 *  Cosmetic and local: it reads the replicated game state and never decides gameplay. Owns
 *   - playback by sound id (random variant, no immediate repeats) for gameplay code (footsteps, interactions);
 *   - music: menu muzak, silence in the shift (plus the break room radio), the dark loop in overtime/evaluation;
 *   - the HVAC room-tone bed, the overtime drone and creaks, the fluorescent hum flicker (level emitters tagged
 *     UOAudio.Hum / UOAudio.Murmur / UOAudio.Radio, placed by Scripts/Editor/uo_audio.py);
 *   - day-flow sounds (shift bell, overtime stinger + power down, task chimes, evaluation reveal);
 *   - the player's volume settings, applied as sound class overrides on the sound set's mix.
 *  Console: uo.AudioDebug 1 logs every sound played and a periodic status (non-Shipping).
 */
UCLASS(Config=Game)
class UUOAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Overtime mood fade, matched to the game mode's light fade */
	static constexpr float OvertimeBlendSeconds = 4.f;

	static UUOAudioSubsystem* Get(const UObject* WorldContextObject);

	/** Loads the sound set once per process (rooted) and hands the UI sounds to the Slate style */
	static UUOSoundSet* GetSoundSet();

	/** Pushes volume levels to the sound mix (falls back to the device volume without a sound set) */
	static void ApplyVolumes(UWorld* World, const FUOVolumeLevels& Levels);

	/** uo.AudioDebug > 0 (always false in Shipping) */
	static bool IsDebugEnabled();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** A random variant of the id (avoids repeating the previous pick); nullptr if unknown */
	USoundBase* PickSound(FName Id);

	/** 3D one-shot or loop at a location (attenuation comes from the wave). Loops must be stopped by the caller. */
	UAudioComponent* PlayAtLocation(FName Id, const FVector& Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	/** Non-spatialized one-shot (stingers, chimes, local feedback) */
	UAudioComponent* Play2D(FName Id, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

protected:

	/** Soft path of the UUOSoundSet asset ([/Script/UnpaidOvertime.UOAudioSubsystem] in DefaultGame.ini) */
	UPROPERTY(Config)
	FSoftObjectPath SoundSetPath;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MenuMusic;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DarkMusic;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RoomTone;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Drone;

	/** A level ambient emitter whose volume/pitch this subsystem modulates */
	struct FEmitter
	{
		TWeakObjectPtr<UAudioComponent> Audio;
		float BaseVolume = 1.f;
		float BasePitch = 1.f;
		float FlickerTimer = 0.f;
		float Flicker = 1.f;
	};
	TArray<FEmitter> HumEmitters;
	TArray<FEmitter> MurmurEmitters;
	TWeakObjectPtr<UAudioComponent> Radio;
	int32 AmbientEmitterCount = 0;

	struct FPendingCue
	{
		float Time = 0.f;
		FName Sound;
	};
	TArray<FPendingCue> PendingCues;
	float CueClock = 0.f;

	TMap<FName, int32> LastVariant;
	FRandomStream Random;

	EUODayPhase LastPhase = EUODayPhase::PreGame;
	bool bPhaseKnown = false;
	EUOMusicTrack CurrentTrack = EUOMusicTrack::None;
	bool bRadioOn = false;
	float MoodBlend = 0.f;
	float DroneLevel = 0.f;
	float CreakTimer = 0.f;
	int32 LastTeamDone = 0;
	int32 LastMyDone = 0;
	float StatusTimer = 0.f;
	bool bMixPushed = false;

	void FindLevelEmitters();

	/** Restarts every looping level emitter at a random point (and detunes the hums slightly) so identical loops
	 *  never play in phase (comb filtering, one loud summed hum). Runs on the first tick, after auto-activation. */
	void DecorrelateEmitters();
	void HandlePhaseChanged(EUODayPhase OldPhase, EUODayPhase NewPhase, const AUOGameState* GameState);
	void SetMusic(EUOMusicTrack Track);
	void SetRadio(bool bOn);
	void UpdateMood(float DeltaTime, EUODayPhase Phase, const AUOGameState* GameState);
	void UpdateTaskProgress(const AUOGameState* GameState);
	void UpdateCreaks(float DeltaTime, EUODayPhase Phase);
	void UpdatePendingCues(float DeltaTime);
	void ScheduleEvaluationReveal(const AUOGameState* GameState);
	void LogStatus() const;

	UAudioComponent* CreateLoop2D(FName Id, bool bUISound);
	bool GetListener(FVector& OutLocation, FVector& OutForward) const;
	class AUOPlayerState* GetLocalEmployee() const;
};
