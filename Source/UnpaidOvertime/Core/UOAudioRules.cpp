// UNPAID: OVERTIME - pure audio decisions.

#include "UOAudioRules.h"

namespace UOAudioRulesPrivate
{
	/** Floor material name fragments that sound hard under a shoe */
	const TCHAR* HardSurfaceKeywords[] = { TEXT("Tile"), TEXT("Asphalt"), TEXT("Concrete"), TEXT("Marble"), TEXT("Stone") };

	constexpr float StrideBase = 55.f;
	constexpr float StridePerSpeed = 0.13f;
	constexpr float MinStride = 70.f;
	constexpr float MaxStride = 120.f;
}

EUOFootstepSurface FUOAudioRules::GetSurfaceForMaterial(const FString& MaterialName)
{
	for (const TCHAR* Keyword : UOAudioRulesPrivate::HardSurfaceKeywords)
	{
		if (MaterialName.Contains(Keyword, ESearchCase::IgnoreCase))
		{
			return EUOFootstepSurface::Tile;
		}
	}
	return EUOFootstepSurface::Carpet;
}

FName FUOAudioRules::GetFootstepSound(EUOFootstepSurface Surface)
{
	return Surface == EUOFootstepSurface::Tile ? UOSoundId::FootstepTile : UOSoundId::FootstepCarpet;
}

float FUOAudioRules::GetStrideLength(float Speed)
{
	using namespace UOAudioRulesPrivate;
	return FMath::Clamp(StrideBase + StridePerSpeed * FMath::Max(0.f, Speed), MinStride, MaxStride);
}

EUOMusicTrack FUOAudioRules::SelectMusic(EUODayPhase Phase)
{
	switch (Phase)
	{
	case EUODayPhase::PreGame:		return EUOMusicTrack::Menu;
	case EUODayPhase::Overtime:
	case EUODayPhase::Evaluation:
	case EUODayPhase::Meeting:		return EUOMusicTrack::Dark;
	default:						return EUOMusicTrack::None;
	}
}

bool FUOAudioRules::IsKitchenRadioOn(EUODayPhase Phase)
{
	return Phase == EUODayPhase::Shift;
}

FUOInteractionSounds FUOAudioRules::GetInteractionSounds(EUOStationType Station, EUOInteractionSoundAction Action)
{
	FUOInteractionSounds Sounds;
	Sounds.Start = UOSoundId::InteractStart;
	Sounds.Loop = UOSoundId::InteractHoldLoop;
	Sounds.Complete = UOSoundId::InteractComplete;

	if (Action == EUOInteractionSoundAction::PickUp)
	{
		// Pickups are instant presses: no hold, the source makes the noise
		Sounds.Start = NAME_None;
		Sounds.Loop = NAME_None;
		Sounds.CompleteExtra = UOSoundId::ItemPickup;
	}

	switch (Station)
	{
	case EUOStationType::Printer:
		if (Action == EUOInteractionSoundAction::Work)
		{
			Sounds.Loop = UOSoundId::PrinterWhirLoop;
			Sounds.Complete = UOSoundId::PrinterFeed;
		}
		else
		{
			Sounds.Complete = UOSoundId::PrinterCartridge;
		}
		break;

	case EUOStationType::CoffeeMaker:
		Sounds.Loop = UOSoundId::CoffeeGurgleLoop;
		Sounds.Complete = UOSoundId::CoffeeBeep;
		break;

	case EUOStationType::FilingCabinet:
		Sounds.Start = UOSoundId::DrawerOpen;
		Sounds.Complete = UOSoundId::DrawerClose;
		break;

	case EUOStationType::InboxTray:
		Sounds.Complete = UOSoundId::PaperRustle;
		break;

	case EUOStationType::SupplyCloset:
		Sounds.Complete = UOSoundId::SupplyRummage;
		break;

	default:
		break;
	}
	return Sounds;
}

float FUOAudioRules::GetOvertimeBlend(float OvertimeSeconds, float BlendSeconds)
{
	if (BlendSeconds <= 0.f)
	{
		return 1.f;
	}
	return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(OvertimeSeconds / BlendSeconds, 0.f, 1.f));
}

float FUOAudioRules::GetNextCreakDelay(float Random01)
{
	return FMath::Lerp(MinCreakDelay, MaxCreakDelay, FMath::Clamp(Random01, 0.f, 1.f));
}

TArray<FUORevealCue> FUOAudioRules::BuildEvaluationReveal(int32 Stars, EUOStarConsequence Consequence, bool bForceClosed, bool bAllTasksDone)
{
	TArray<FUORevealCue> Cues;

	float Start = RevealStartPlain;
	if (bForceClosed)
	{
		Cues.Add({ 0.f, UOSoundId::ClosingBell });
		Start = RevealStartAfterClosingBell;
	}
	else if (bAllTasksDone)
	{
		Cues.Add({ 0.f, UOSoundId::TaskAllDone });
		Start = RevealStartAfterFanfare;
	}

	const FName StarSounds[] = { UOSoundId::Star1, UOSoundId::Star2, UOSoundId::Star3 };
	const int32 Shown = FMath::Clamp(Stars, 0, static_cast<int32>(UE_ARRAY_COUNT(StarSounds)));
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		Cues.Add({ Start + Index * RevealStarInterval, StarSounds[Index] });
	}

	if (Consequence == EUOStarConsequence::Fired)
	{
		Cues.Add({ Start + Shown * RevealStarInterval, UOSoundId::Fired });
	}
	return Cues;
}
