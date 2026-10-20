// UNPAID: OVERTIME - per-world audio director.

#include "UOAudioSubsystem.h"
#include "UODayDifficulty.h"
#include "UOSoundSet.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOTaskBoard.h"
#include "UOUIStyle.h"
#include "UOGameUserSettings.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "UnpaidOvertime.h"

namespace UOAudioPrivate
{
#if !UE_BUILD_SHIPPING
	static TAutoConsoleVariable<int32> CVarAudioDebug(
		TEXT("uo.AudioDebug"),
		0,
		TEXT("UNPAID: OVERTIME audio debug. 1 = log every sound played (id, wave, where) and an audio status every 5 s."),
		ECVF_Default);
#endif

	const FName HumTag(TEXT("UOAudio.Hum"));
	const FName MurmurTag(TEXT("UOAudio.Murmur"));
	const FName RadioTag(TEXT("UOAudio.Radio"));
	const FName AudioTag(TEXT("UOAudio"));

	constexpr float MusicFadeSeconds = 2.5f;
	constexpr float RoomToneOvertimeScale = 0.45f;
	constexpr float HumOvertimeVolume = 0.55f;
	constexpr float HumOvertimePitch = 0.93f;
	constexpr float MurmurOvertimeVolume = 0.2f;
	constexpr float FirstCreakMin = 6.f;
	constexpr float FirstCreakMax = 12.f;
	constexpr float CreakMinDistance = 700.f;
	constexpr float CreakMaxDistance = 1500.f;
	constexpr float StatusInterval = 5.f;

	FString Describe(const UAudioComponent* Audio)
	{
		if (!Audio)
		{
			return TEXT("none");
		}
		return FString::Printf(TEXT("%s playing=%d vol=%.2f"), Audio->Sound ? *Audio->Sound->GetName() : TEXT("?"), Audio->IsPlaying() ? 1 : 0, Audio->VolumeMultiplier);
	}
}

UUOAudioSubsystem* UUOAudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UUOAudioSubsystem>() : nullptr;
}

bool UUOAudioSubsystem::IsDebugEnabled()
{
#if !UE_BUILD_SHIPPING
	return UOAudioPrivate::CVarAudioDebug.GetValueOnGameThread() > 0;
#else
	return false;
#endif
}

UUOSoundSet* UUOAudioSubsystem::GetSoundSet()
{
	static TWeakObjectPtr<UUOSoundSet> Cached;
	static bool bWarned = false;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const FSoftObjectPath& Path = GetDefault<UUOAudioSubsystem>()->SoundSetPath;
	UUOSoundSet* Set = Path.IsValid() ? Cast<UUOSoundSet>(Path.TryLoad()) : nullptr;
	if (!Set)
	{
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("UOAudio: sound set '%s' not found; the game runs silent (build it with Scripts\\BuildArt.bat audio)."), *Path.ToString());
		}
		return nullptr;
	}

	// Lives for the whole session: Slate styles keep raw pointers to the UI sounds
	Set->AddToRoot();
	Cached = Set;

	auto UISound = [Set](FName Id)
	{
		FSlateSound Sound;
		const FUOSoundEntry* Entry = Set->FindEntry(Id);
		if (Entry && Entry->Variants.Num() > 0 && Entry->Variants[0])
		{
			Sound.SetResourceObject(Entry->Variants[0]);
		}
		return Sound;
	};

	FUOUISounds UISounds;
	UISounds.Hover = UISound(UOSoundId::UIHover);
	UISounds.Click = UISound(UOSoundId::UIClick);
	UISounds.Back = UISound(UOSoundId::UIBack);
	UISounds.Toggle = UISound(UOSoundId::UIToggle);
	UISounds.SliderTick = UISound(UOSoundId::UISliderTick);
	UISounds.Error = UISound(UOSoundId::UIError);
	UISounds.MenuOpen = UISound(UOSoundId::UIMenuOpen);
	UISounds.MenuClose = UISound(UOSoundId::UIMenuClose);
	FUOUIStyle::SetSounds(UISounds);

	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: sound set %s loaded: %d ids, %d waves, mix %s."), *Set->GetName(), Set->Entries.Num(), Set->CountVariants(),
		Set->VolumeMix ? *Set->VolumeMix->GetName() : TEXT("missing"));
	return Set;
}

void UUOAudioSubsystem::ApplyVolumes(UWorld* World, const FUOVolumeLevels& Levels)
{
	if (!World)
	{
		return;
	}

	const UUOSoundSet* Set = GetSoundSet();
	FAudioDeviceHandle AudioDevice = World->GetAudioDevice();
	if (!Set || !Set->VolumeMix)
	{
		if (AudioDevice)
		{
			AudioDevice->SetTransientPrimaryVolume(Levels.Master);
		}
		return;
	}

	if (AudioDevice)
	{
		AudioDevice->SetTransientPrimaryVolume(1.f);
	}

	// Leaf classes get Master x category, so nothing depends on how parent/child adjusters combine
	auto Override = [World, Set](USoundClass* Class, float Volume)
	{
		if (Class)
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Set->VolumeMix, Class, FMath::Clamp(Volume, 0.f, 1.f), 1.f, 0.15f, false);
		}
	};
	Override(Set->MusicClass, Levels.Master * Levels.Music);
	Override(Set->SfxClass, Levels.Master * Levels.Sfx);
	Override(Set->AmbienceClass, Levels.Master * Levels.Ambience);
	Override(Set->UiClass, Levels.Master * Levels.Ui);
	Override(Set->VoiceClass, Levels.Master * Levels.Sfx);

	UUOAudioSubsystem* Subsystem = World->GetSubsystem<UUOAudioSubsystem>();
	if (!Subsystem || !Subsystem->bMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(World, Set->VolumeMix);
		if (Subsystem)
		{
			Subsystem->bMixPushed = true;
		}
	}

	UE_LOG(LogUnpaidOvertime, Verbose, TEXT("UOAudio: volumes master %.2f music %.2f sfx %.2f ambience %.2f ui %.2f."), Levels.Master, Levels.Music, Levels.Sfx, Levels.Ambience, Levels.Ui);
}

bool UUOAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer) || IsRunningDedicatedServer())
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UUOAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Random.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	// Load early: the UI style needs its sounds before the first menu is built
	GetSoundSet();
}

void UUOAudioSubsystem::Deinitialize()
{
	if (bMixPushed)
	{
		if (const UUOSoundSet* Set = GetSoundSet())
		{
			UGameplayStatics::PopSoundMixModifier(GetWorld(), Set->VolumeMix);
		}
		bMixPushed = false;
	}
	Super::Deinitialize();
}

void UUOAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const FUOVolumeLevels Levels = UUOGameUserSettings::Get()->GetVolumeLevels();
	ApplyVolumes(&InWorld, Levels);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: volume mix %s (master %.2f music %.2f sfx %.2f ambience %.2f ui %.2f)."),
		bMixPushed ? TEXT("pushed") : TEXT("not available"), Levels.Master, Levels.Music, Levels.Sfx, Levels.Ambience, Levels.Ui);
	FindLevelEmitters();

	RoomTone = CreateLoop2D(UOSoundId::RoomTone, false);
	if (RoomTone)
	{
		RoomTone->FadeIn(2.f);
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: world audio ready (room tone %s, %d level emitters: %d hum, %d murmur, radio %s)."),
		RoomTone ? TEXT("on") : TEXT("missing"), AmbientEmitterCount, HumEmitters.Num(), MurmurEmitters.Num(), Radio.IsValid() ? TEXT("yes") : TEXT("no"));
}

TStatId UUOAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUOAudioSubsystem, STATGROUP_Tickables);
}

void UUOAudioSubsystem::FindLevelEmitters()
{
	using namespace UOAudioPrivate;

	HumEmitters.Reset();
	MurmurEmitters.Reset();
	Radio.Reset();
	AmbientEmitterCount = 0;

	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		UAudioComponent* Audio = It->GetAudioComponent();
		if (!Audio || !It->Tags.Contains(AudioTag))
		{
			continue;
		}
		++AmbientEmitterCount;

		FEmitter Emitter;
		Emitter.Audio = Audio;
		Emitter.BaseVolume = Audio->VolumeMultiplier;
		if (It->Tags.Contains(HumTag))
		{
			HumEmitters.Add(Emitter);
		}
		else if (It->Tags.Contains(MurmurTag))
		{
			MurmurEmitters.Add(Emitter);
		}
		else if (It->Tags.Contains(RadioTag))
		{
			Radio = Audio;
			// Small speaker: band-limited
			Audio->SetLowPassFilterEnabled(true);
			Audio->SetLowPassFilterFrequency(3200.f);
			Audio->SetHighPassFilterEnabled(true);
			Audio->SetHighPassFilterFrequency(380.f);
		}
	}
}

void UUOAudioSubsystem::DecorrelateEmitters()
{
	using namespace UOAudioPrivate;

	int32 Restarted = 0;
	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		UAudioComponent* Audio = It->GetAudioComponent();
		const USoundWave* Wave = Audio ? Cast<USoundWave>(Audio->Sound) : nullptr;
		if (!Wave || !Wave->bLooping || !It->Tags.Contains(AudioTag) || It->Tags.Contains(RadioTag) || !Audio->IsPlaying())
		{
			continue;
		}
		Audio->Stop();
		Audio->Play(Random.FRandRange(0.f, FMath::Max(0.f, Wave->Duration - 0.1f)));
		++Restarted;
	}
	for (FEmitter& Emitter : HumEmitters)
	{
		Emitter.BasePitch = Random.FRandRange(0.985f, 1.015f);
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: %d looping emitters restarted at random offsets."), Restarted);
}

USoundBase* UUOAudioSubsystem::PickSound(FName Id)
{
	const UUOSoundSet* Set = GetSoundSet();
	const FUOSoundEntry* Entry = Set ? Set->FindEntry(Id) : nullptr;
	if (!Entry || Entry->Variants.Num() == 0)
	{
		if (Set && IsDebugEnabled())
		{
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("UOAudio: no sound for id %s"), *Id.ToString());
		}
		return nullptr;
	}

	int32 Index = 0;
	if (Entry->Variants.Num() > 1)
	{
		const int32* Last = LastVariant.Find(Id);
		Index = Random.RandHelper(Entry->Variants.Num() - (Last ? 1 : 0));
		if (Last && Index >= *Last)
		{
			++Index;
		}
	}
	LastVariant.Add(Id, Index);
	return Entry->Variants[Index];
}

UAudioComponent* UUOAudioSubsystem::PlayAtLocation(FName Id, const FVector& Location, float VolumeMultiplier, float PitchMultiplier)
{
	USoundBase* Sound = PickSound(Id);
	if (!Sound)
	{
		return nullptr;
	}

	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, VolumeMultiplier, PitchMultiplier);
	if (IsDebugEnabled())
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: play3d %s (%s) at %s vol %.2f pitch %.2f -> %s"), *Id.ToString(), *Sound->GetName(), *Location.ToCompactString(),
			VolumeMultiplier, PitchMultiplier, Audio ? (Audio->IsPlaying() ? TEXT("playing") : TEXT("spawned")) : TEXT("culled"));
	}
	return Audio;
}

UAudioComponent* UUOAudioSubsystem::Play2D(FName Id, float VolumeMultiplier, float PitchMultiplier)
{
	USoundBase* Sound = PickSound(Id);
	if (!Sound)
	{
		return nullptr;
	}

	UAudioComponent* Audio = UGameplayStatics::SpawnSound2D(this, Sound, VolumeMultiplier, PitchMultiplier);
	if (IsDebugEnabled())
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: play2d %s (%s) vol %.2f pitch %.2f -> %s"), *Id.ToString(), *Sound->GetName(), VolumeMultiplier, PitchMultiplier,
			Audio ? (Audio->IsPlaying() ? TEXT("playing") : TEXT("spawned")) : TEXT("failed"));
	}
	return Audio;
}

UAudioComponent* UUOAudioSubsystem::CreateLoop2D(FName Id, bool bUISound)
{
	USoundBase* Sound = PickSound(Id);
	if (!Sound)
	{
		return nullptr;
	}

	UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
	if (Audio)
	{
		// Music keeps playing under the pause menu
		Audio->bIsUISound = bUISound;
		if (IsDebugEnabled())
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: loop2d %s (%s) created"), *Id.ToString(), *Sound->GetName());
		}
	}
	return Audio;
}

void UUOAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}

	const AUOGameState* GameState = World->GetGameState<AUOGameState>();
	const EUODayPhase Phase = GameState ? GameState->GetPhase() : EUODayPhase::PreGame;

	if (!bPhaseKnown)
	{
		bPhaseKnown = true;
		LastPhase = Phase;
		DecorrelateEmitters();
		SetMusic(FUOAudioRules::SelectMusic(Phase));
		SetRadio(FUOAudioRules::IsKitchenRadioOn(Phase));
	}
	else if (Phase != LastPhase)
	{
		const EUODayPhase OldPhase = LastPhase;
		LastPhase = Phase;
		HandlePhaseChanged(OldPhase, Phase, GameState);
	}

	UpdateMood(DeltaTime, Phase, GameState);
	UpdateTaskProgress(GameState);
	UpdateCreaks(DeltaTime, Phase);
	UpdatePendingCues(DeltaTime);

	if (IsDebugEnabled())
	{
		StatusTimer += DeltaTime;
		if (StatusTimer >= UOAudioPrivate::StatusInterval)
		{
			StatusTimer = 0.f;
			LogStatus();
		}
	}
}

void UUOAudioSubsystem::HandlePhaseChanged(EUODayPhase OldPhase, EUODayPhase NewPhase, const AUOGameState* GameState)
{
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: phase %s -> %s"), *UOText::GetPhaseName(OldPhase).ToString(), *UOText::GetPhaseName(NewPhase).ToString());

	switch (NewPhase)
	{
	case EUODayPhase::Shift:
		Play2D(UOSoundId::ShiftBell);
		// The hardcore overtime weekend opens with the overtime stinger
		if (GameState && FUODayDifficulty::IsHardcoreDay(GameState->GetDayNumber()))
		{
			Play2D(UOSoundId::OvertimeStinger);
			UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: hardcore weekend stinger (day %d)"), GameState->GetDayNumber());
		}
		LastTeamDone = 0;
		LastMyDone = 0;
		break;

	case EUODayPhase::Overtime:
		Play2D(UOSoundId::OvertimeStinger);
		Play2D(UOSoundId::PowerDown, 0.9f);
		CreakTimer = Random.FRandRange(UOAudioPrivate::FirstCreakMin, UOAudioPrivate::FirstCreakMax);
		break;

	case EUODayPhase::Evaluation:
		ScheduleEvaluationReveal(GameState);
		break;

	default:
		break;
	}

	SetMusic(FUOAudioRules::SelectMusic(NewPhase));
	SetRadio(FUOAudioRules::IsKitchenRadioOn(NewPhase));
}

void UUOAudioSubsystem::SetMusic(EUOMusicTrack Track)
{
	using namespace UOAudioPrivate;

	if (Track == CurrentTrack && (Track == EUOMusicTrack::None || (Track == EUOMusicTrack::Menu ? MenuMusic : DarkMusic)))
	{
		return;
	}
	CurrentTrack = Track;

	if (Track == EUOMusicTrack::Menu && !MenuMusic)
	{
		MenuMusic = CreateLoop2D(UOSoundId::MusicMenu, true);
	}
	if (Track == EUOMusicTrack::Dark && !DarkMusic)
	{
		DarkMusic = CreateLoop2D(UOSoundId::MusicDark, true);
	}

	auto Crossfade = [](UAudioComponent* Audio, bool bOn)
	{
		if (!Audio)
		{
			return;
		}
		if (bOn && !Audio->IsPlaying())
		{
			Audio->FadeIn(MusicFadeSeconds);
		}
		else if (!bOn && Audio->IsPlaying())
		{
			Audio->FadeOut(MusicFadeSeconds, 0.f);
		}
	};
	Crossfade(MenuMusic, Track == EUOMusicTrack::Menu);
	Crossfade(DarkMusic, Track == EUOMusicTrack::Dark);

	const TCHAR* Names[] = { TEXT("none"), TEXT("menu muzak"), TEXT("dark muzak") };
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: music -> %s"), Names[static_cast<int32>(Track)]);
}

void UUOAudioSubsystem::SetRadio(bool bOn)
{
	UAudioComponent* Audio = Radio.Get();
	if (!Audio || bOn == bRadioOn)
	{
		return;
	}
	bRadioOn = bOn;
	if (bOn)
	{
		Audio->FadeIn(1.5f);
	}
	else
	{
		Audio->FadeOut(2.f, 0.f);
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: break room radio %s"), bOn ? TEXT("on") : TEXT("off"));
}

void UUOAudioSubsystem::UpdateMood(float DeltaTime, EUODayPhase Phase, const AUOGameState* GameState)
{
	using namespace UOAudioPrivate;

	// Overtime mood: follows the light fade in overtime, holds through the evaluation of an overtime day
	float Target = 0.f;
	if (Phase == EUODayPhase::Overtime && GameState)
	{
		Target = FUOAudioRules::GetOvertimeBlend(GameState->GetOvertimeSeconds(), OvertimeBlendSeconds);
	}
	else if (Phase == EUODayPhase::Evaluation && GameState && GameState->IsUnpaidOvertime())
	{
		Target = 1.f;
	}
	MoodBlend = Target;

	// Drone: in during overtime, slowly out at the evaluation
	const float DroneTarget = Phase == EUODayPhase::Overtime ? MoodBlend : 0.f;
	DroneLevel = DroneTarget > DroneLevel ? DroneTarget : FMath::FInterpConstantTo(DroneLevel, DroneTarget, DeltaTime, 1.f / 3.f);
	if (DroneLevel > 0.001f && !Drone)
	{
		Drone = CreateLoop2D(UOSoundId::OvertimeDrone, false);
		if (Drone)
		{
			Drone->SetVolumeMultiplier(0.001f);
			Drone->Play();
		}
	}
	if (Drone)
	{
		Drone->SetVolumeMultiplier(FMath::Max(0.001f, DroneLevel));
		if (DroneLevel <= 0.001f && DroneTarget <= 0.f && Drone->IsPlaying())
		{
			Drone->Stop();
		}
	}

	if (RoomTone)
	{
		RoomTone->SetVolumeMultiplier(FMath::Lerp(1.f, RoomToneOvertimeScale, MoodBlend));
	}

	// Fluorescent hum: drops in pitch and flickers with the dimmed panels
	for (FEmitter& Emitter : HumEmitters)
	{
		UAudioComponent* Audio = Emitter.Audio.Get();
		if (!Audio)
		{
			continue;
		}
		if (MoodBlend > 0.f)
		{
			Emitter.FlickerTimer -= DeltaTime;
			if (Emitter.FlickerTimer <= 0.f)
			{
				Emitter.Flicker = Random.FRand() < 0.3f ? Random.FRandRange(0.3f, 0.9f) : 1.f;
				Emitter.FlickerTimer = Random.FRandRange(0.05f, 0.6f);
			}
		}
		else
		{
			Emitter.Flicker = 1.f;
		}
		Audio->SetVolumeMultiplier(Emitter.BaseVolume * FMath::Lerp(1.f, HumOvertimeVolume * Emitter.Flicker, MoodBlend));
		Audio->SetPitchMultiplier(Emitter.BasePitch * FMath::Lerp(1.f, HumOvertimePitch, MoodBlend));
	}

	// The open plan empties out
	for (FEmitter& Emitter : MurmurEmitters)
	{
		if (UAudioComponent* Audio = Emitter.Audio.Get())
		{
			Audio->SetVolumeMultiplier(Emitter.BaseVolume * FMath::Lerp(1.f, MurmurOvertimeVolume, MoodBlend));
		}
	}
}

AUOPlayerState* UUOAudioSubsystem::GetLocalEmployee() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
}

void UUOAudioSubsystem::UpdateTaskProgress(const AUOGameState* GameState)
{
	if (!GameState || !GameState->IsWorkPhase())
	{
		return;
	}

	const int32 TeamDone = FUOTaskBoard::CountDone(GameState->GetTasks());
	const AUOPlayerState* Me = GetLocalEmployee();
	const int32 MyDone = Me ? Me->GetCompletedCount() : 0;

	// The last sub-objective ends the day: the evaluation fanfare covers it
	if (TeamDone > LastTeamDone && !FUOTaskBoard::AreAllDone(GameState->GetTasks()))
	{
		const bool bMine = MyDone > LastMyDone;
		Play2D(UOSoundId::TaskChime, bMine ? 1.f : 0.45f, bMine ? 1.f : 0.94f);
	}
	LastTeamDone = TeamDone;
	LastMyDone = MyDone;
}

void UUOAudioSubsystem::UpdateCreaks(float DeltaTime, EUODayPhase Phase)
{
	using namespace UOAudioPrivate;

	if (Phase != EUODayPhase::Overtime)
	{
		return;
	}

	CreakTimer -= DeltaTime;
	if (CreakTimer > 0.f)
	{
		return;
	}
	CreakTimer = FUOAudioRules::GetNextCreakDelay(Random.FRand());

	FVector Listener;
	FVector Forward;
	if (!GetListener(Listener, Forward))
	{
		return;
	}

	// Somewhere off to the side or behind, up in the ceiling or walls
	const float Yaw = Random.FRandRange(60.f, 300.f);
	const FVector Direction = Forward.GetSafeNormal2D().RotateAngleAxis(Yaw, FVector::UpVector);
	const FVector Location = Listener + Direction * Random.FRandRange(CreakMinDistance, CreakMaxDistance) + FVector(0.f, 0.f, Random.FRandRange(40.f, 140.f));
	PlayAtLocation(UOSoundId::Creak, Location, 1.f, Random.FRandRange(0.9f, 1.05f));
}

bool UUOAudioSubsystem::GetListener(FVector& OutLocation, FVector& OutForward) const
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	FVector Right;
	PC->GetAudioListenerPosition(OutLocation, OutForward, Right);
	return true;
}

void UUOAudioSubsystem::ScheduleEvaluationReveal(const AUOGameState* GameState)
{
	const AUOPlayerState* Me = GetLocalEmployee();
	if (!GameState || !Me)
	{
		return;
	}

	const bool bForceClosed = GameState->WasDayForceClosed();
	const bool bAllDone = FUOTaskBoard::AreAllDone(GameState->GetTasks());
	CueClock = 0.f;
	PendingCues.Reset();
	for (const FUORevealCue& Cue : FUOAudioRules::BuildEvaluationReveal(Me->GetStars(), Me->GetConsequence(), bForceClosed, bAllDone))
	{
		PendingCues.Add({ Cue.Delay, Cue.Sound });
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio: evaluation reveal scheduled (%d cues, %d stars, force-closed %d, all done %d)."),
		PendingCues.Num(), Me->GetStars(), bForceClosed ? 1 : 0, bAllDone ? 1 : 0);
}

void UUOAudioSubsystem::UpdatePendingCues(float DeltaTime)
{
	if (PendingCues.Num() == 0)
	{
		return;
	}

	CueClock += DeltaTime;
	for (int32 Index = 0; Index < PendingCues.Num();)
	{
		if (PendingCues[Index].Time <= CueClock)
		{
			Play2D(PendingCues[Index].Sound);
			PendingCues.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
}

void UUOAudioSubsystem::LogStatus() const
{
	using namespace UOAudioPrivate;

	int32 HumPlaying = 0;
	for (const FEmitter& Emitter : HumEmitters)
	{
		HumPlaying += Emitter.Audio.IsValid() && Emitter.Audio->IsPlaying() ? 1 : 0;
	}

	int32 AmbientPlaying = 0;
	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		const UAudioComponent* Audio = It->GetAudioComponent();
		AmbientPlaying += Audio && Audio->IsPlaying() ? 1 : 0;
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("UOAudio status: phase %s mood %.2f | menu music [%s] | dark music [%s] | room tone [%s] | drone [%s] level %.2f | ambient emitters playing %d/%d (hum %d/%d) | radio [%s]"),
		*UOText::GetPhaseName(LastPhase).ToString(), MoodBlend, *Describe(MenuMusic), *Describe(DarkMusic), *Describe(RoomTone), *Describe(Drone), DroneLevel,
		AmbientPlaying, AmbientEmitterCount, HumPlaying, HumEmitters.Num(), *Describe(Radio.Get()));
}
