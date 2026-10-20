// UNPAID: OVERTIME - audio rules tests (footstep surfaces, music selection, interaction sounds, reveal timeline).

#include "UOTestFlags.h"
#include "UOAudioRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAudioFootstepSurfaceTest, "UnpaidOvertime.Core.Audio.FootstepSurface", UO_CORE_TEST_FLAGS)
bool FUOAudioFootstepSurfaceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Lobby tile is tile"), FUOAudioRules::GetSurfaceForMaterial(TEXT("MI_LobbyTile")), EUOFootstepSurface::Tile);
	TestEqual(TEXT("Kitchen tile is tile"), FUOAudioRules::GetSurfaceForMaterial(TEXT("MI_KitchenTile")), EUOFootstepSurface::Tile);
	TestEqual(TEXT("Matching ignores case"), FUOAudioRules::GetSurfaceForMaterial(TEXT("mi_kitchentile_inst")), EUOFootstepSurface::Tile);
	TestEqual(TEXT("Asphalt outside sounds hard"), FUOAudioRules::GetSurfaceForMaterial(TEXT("MI_Asphalt")), EUOFootstepSurface::Tile);
	TestEqual(TEXT("Concrete sounds hard"), FUOAudioRules::GetSurfaceForMaterial(TEXT("MI_Concrete")), EUOFootstepSurface::Tile);
	TestEqual(TEXT("Carpet is carpet"), FUOAudioRules::GetSurfaceForMaterial(TEXT("MI_Carpet")), EUOFootstepSurface::Carpet);
	TestEqual(TEXT("Dark carpet is carpet"), FUOAudioRules::GetSurfaceForMaterial(TEXT("MI_Carpet_Dark")), EUOFootstepSurface::Carpet);
	TestEqual(TEXT("Unknown material defaults to carpet"), FUOAudioRules::GetSurfaceForMaterial(TEXT("BasicShapeMaterial")), EUOFootstepSurface::Carpet);
	TestEqual(TEXT("No material defaults to carpet"), FUOAudioRules::GetSurfaceForMaterial(FString()), EUOFootstepSurface::Carpet);

	TestEqual(TEXT("Carpet sound"), FUOAudioRules::GetFootstepSound(EUOFootstepSurface::Carpet), UOSoundId::FootstepCarpet);
	TestEqual(TEXT("Tile sound"), FUOAudioRules::GetFootstepSound(EUOFootstepSurface::Tile), UOSoundId::FootstepTile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAudioStrideTest, "UnpaidOvertime.Core.Audio.Stride", UO_CORE_TEST_FLAGS)
bool FUOAudioStrideTest::RunTest(const FString& Parameters)
{
	const float AIWalk = FUOAudioRules::GetStrideLength(240.f);
	const float PlayerWalk = FUOAudioRules::GetStrideLength(420.f);
	TestTrue(TEXT("AI walking stride is a normal step (75..95 cm)"), AIWalk >= 75.f && AIWalk <= 95.f);
	TestTrue(TEXT("Player stride is longer (100..120 cm)"), PlayerWalk >= 100.f && PlayerWalk <= 120.f);
	TestTrue(TEXT("Faster means longer strides"), PlayerWalk > AIWalk);
	TestEqual(TEXT("Standing still clamps to the minimum stride"), FUOAudioRules::GetStrideLength(0.f), 70.f);
	TestEqual(TEXT("Very fast clamps to the maximum stride"), FUOAudioRules::GetStrideLength(5000.f), 120.f);
	TestEqual(TEXT("Negative speed clamps to the minimum stride"), FUOAudioRules::GetStrideLength(-50.f), 70.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAudioMusicTest, "UnpaidOvertime.Core.Audio.Music", UO_CORE_TEST_FLAGS)
bool FUOAudioMusicTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Pre-game (main menu) plays the muzak"), FUOAudioRules::SelectMusic(EUODayPhase::PreGame), EUOMusicTrack::Menu);
	TestEqual(TEXT("The shift has no score"), FUOAudioRules::SelectMusic(EUODayPhase::Shift), EUOMusicTrack::None);
	TestEqual(TEXT("Overtime turns dark"), FUOAudioRules::SelectMusic(EUODayPhase::Overtime), EUOMusicTrack::Dark);
	TestEqual(TEXT("Evaluation stays dark"), FUOAudioRules::SelectMusic(EUODayPhase::Evaluation), EUOMusicTrack::Dark);
	TestEqual(TEXT("Emergency meetings are dark"), FUOAudioRules::SelectMusic(EUODayPhase::Meeting), EUOMusicTrack::Dark);
	TestFalse(TEXT("Radio off in meetings"), FUOAudioRules::IsKitchenRadioOn(EUODayPhase::Meeting));

	TestFalse(TEXT("Radio off in the menu"), FUOAudioRules::IsKitchenRadioOn(EUODayPhase::PreGame));
	TestTrue(TEXT("Radio on during the shift"), FUOAudioRules::IsKitchenRadioOn(EUODayPhase::Shift));
	TestFalse(TEXT("Radio off in overtime"), FUOAudioRules::IsKitchenRadioOn(EUODayPhase::Overtime));
	TestFalse(TEXT("Radio off in evaluation"), FUOAudioRules::IsKitchenRadioOn(EUODayPhase::Evaluation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAudioInteractionSoundsTest, "UnpaidOvertime.Core.Audio.InteractionSounds", UO_CORE_TEST_FLAGS)
bool FUOAudioInteractionSoundsTest::RunTest(const FString& Parameters)
{
	using A = EUOInteractionSoundAction;

	const FUOInteractionSounds Ink = FUOAudioRules::GetInteractionSounds(EUOStationType::Printer, A::Deliver);
	TestEqual(TEXT("Delivering ink loops the generic hold"), Ink.Loop, UOSoundId::InteractHoldLoop);
	TestEqual(TEXT("Delivering ink ends with the cartridge click"), Ink.Complete, UOSoundId::PrinterCartridge);

	const FUOInteractionSounds Print = FUOAudioRules::GetInteractionSounds(EUOStationType::Printer, A::Work);
	TestEqual(TEXT("Printer work whirs"), Print.Loop, UOSoundId::PrinterWhirLoop);
	TestEqual(TEXT("Printer work ends with a paper feed"), Print.Complete, UOSoundId::PrinterFeed);
	TestEqual(TEXT("Printer work starts with the generic click"), Print.Start, UOSoundId::InteractStart);

	const FUOInteractionSounds Coffee = FUOAudioRules::GetInteractionSounds(EUOStationType::CoffeeMaker, A::Work);
	TestEqual(TEXT("Coffee gurgles"), Coffee.Loop, UOSoundId::CoffeeGurgleLoop);
	TestEqual(TEXT("Coffee beeps when done"), Coffee.Complete, UOSoundId::CoffeeBeep);

	const FUOInteractionSounds File = FUOAudioRules::GetInteractionSounds(EUOStationType::FilingCabinet, A::Deliver);
	TestEqual(TEXT("Filing opens a drawer"), File.Start, UOSoundId::DrawerOpen);
	TestEqual(TEXT("Filing closes the drawer"), File.Complete, UOSoundId::DrawerClose);
	TestEqual(TEXT("Filing work also uses the drawer"), FUOAudioRules::GetInteractionSounds(EUOStationType::FilingCabinet, A::Work).Start, UOSoundId::DrawerOpen);

	const FUOInteractionSounds Report = FUOAudioRules::GetInteractionSounds(EUOStationType::InboxTray, A::PickUp);
	TestEqual(TEXT("Taking a report rustles paper"), Report.Complete, UOSoundId::PaperRustle);
	TestEqual(TEXT("Taking a report layers the pickup"), Report.CompleteExtra, UOSoundId::ItemPickup);
	TestEqual(TEXT("Instant pickups have no hold loop"), Report.Loop, NAME_None);

	const FUOInteractionSounds Supply = FUOAudioRules::GetInteractionSounds(EUOStationType::SupplyCloset, A::PickUp);
	TestEqual(TEXT("The supply closet rummages"), Supply.Complete, UOSoundId::SupplyRummage);
	TestEqual(TEXT("Supply pickups layer the pickup"), Supply.CompleteExtra, UOSoundId::ItemPickup);

	const FUOInteractionSounds InboxWork = FUOAudioRules::GetInteractionSounds(EUOStationType::InboxTray, A::Work);
	TestEqual(TEXT("Inbox work rustles paper"), InboxWork.Complete, UOSoundId::PaperRustle);
	TestEqual(TEXT("Inbox work holds with the generic loop"), InboxWork.Loop, UOSoundId::InteractHoldLoop);
	TestEqual(TEXT("Only pickups layer the pickup sound"), InboxWork.CompleteExtra, NAME_None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAudioOvertimeTest, "UnpaidOvertime.Core.Audio.Overtime", UO_CORE_TEST_FLAGS)
bool FUOAudioOvertimeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Blend starts at 0"), FUOAudioRules::GetOvertimeBlend(0.f, 4.f), 0.f);
	TestEqual(TEXT("Blend is halfway at half time"), FUOAudioRules::GetOvertimeBlend(2.f, 4.f), 0.5f);
	TestEqual(TEXT("Blend is full at the end"), FUOAudioRules::GetOvertimeBlend(4.f, 4.f), 1.f);
	TestEqual(TEXT("Blend stays full afterwards"), FUOAudioRules::GetOvertimeBlend(30.f, 4.f), 1.f);
	TestEqual(TEXT("Negative time is no blend"), FUOAudioRules::GetOvertimeBlend(-1.f, 4.f), 0.f);
	TestEqual(TEXT("Zero blend time is immediate"), FUOAudioRules::GetOvertimeBlend(0.f, 0.f), 1.f);
	TestTrue(TEXT("Blend eases in (smoothstep)"), FUOAudioRules::GetOvertimeBlend(1.f, 4.f) < 0.25f);

	TestEqual(TEXT("Creak delay minimum"), FUOAudioRules::GetNextCreakDelay(0.f), FUOAudioRules::MinCreakDelay);
	TestEqual(TEXT("Creak delay maximum"), FUOAudioRules::GetNextCreakDelay(1.f), FUOAudioRules::MaxCreakDelay);
	TestEqual(TEXT("Creak delay midpoint"), FUOAudioRules::GetNextCreakDelay(0.5f), 27.5f);
	TestEqual(TEXT("Out-of-range randoms are clamped"), FUOAudioRules::GetNextCreakDelay(7.f), FUOAudioRules::MaxCreakDelay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAudioRevealTest, "UnpaidOvertime.Core.Audio.EvaluationReveal", UO_CORE_TEST_FLAGS)
bool FUOAudioRevealTest::RunTest(const FString& Parameters)
{
	// Three stars after finishing everything: fanfare, then three rising pings
	{
		const TArray<FUORevealCue> Cues = FUOAudioRules::BuildEvaluationReveal(3, EUOStarConsequence::PromotionAvailable, false, true);
		if (TestEqual(TEXT("Fanfare + 3 stars"), Cues.Num(), 4))
		{
			TestEqual(TEXT("Opens with the fanfare"), Cues[0].Sound, UOSoundId::TaskAllDone);
			TestEqual(TEXT("Fanfare plays immediately"), Cues[0].Delay, 0.f);
			TestEqual(TEXT("First star"), Cues[1].Sound, UOSoundId::Star1);
			TestEqual(TEXT("Second star"), Cues[2].Sound, UOSoundId::Star2);
			TestEqual(TEXT("Third star"), Cues[3].Sound, UOSoundId::Star3);
			TestEqual(TEXT("Stars wait for the fanfare"), Cues[1].Delay, FUOAudioRules::RevealStartAfterFanfare);
			TestEqual(TEXT("Stars are evenly spaced"), Cues[2].Delay - Cues[1].Delay, FUOAudioRules::RevealStarInterval);
		}
	}

	// Force-closed day, one star: closing bell, one star, no thud
	{
		const TArray<FUORevealCue> Cues = FUOAudioRules::BuildEvaluationReveal(1, EUOStarConsequence::CutVoteCandidate, true, false);
		if (TestEqual(TEXT("Bell + 1 star"), Cues.Num(), 2))
		{
			TestEqual(TEXT("Opens with the closing bell"), Cues[0].Sound, UOSoundId::ClosingBell);
			TestEqual(TEXT("Star after the bell"), Cues[1].Delay, FUOAudioRules::RevealStartAfterClosingBell);
		}
	}

	// Fired with nothing done on a force-closed day: bell, then the thud
	{
		const TArray<FUORevealCue> Cues = FUOAudioRules::BuildEvaluationReveal(0, EUOStarConsequence::Fired, true, false);
		if (TestEqual(TEXT("Bell + thud"), Cues.Num(), 2))
		{
			TestEqual(TEXT("Thud last"), Cues[1].Sound, UOSoundId::Fired);
			TestTrue(TEXT("Thud after the bell"), Cues[1].Delay >= FUOAudioRules::RevealStartAfterClosingBell);
		}
	}

	// Plain end (no bell, no fanfare): stars start shortly after
	{
		const TArray<FUORevealCue> Cues = FUOAudioRules::BuildEvaluationReveal(2, EUOStarConsequence::Safe, false, false);
		if (TestEqual(TEXT("2 stars only"), Cues.Num(), 2))
		{
			TestEqual(TEXT("First star at the plain start"), Cues[0].Delay, FUOAudioRules::RevealStartPlain);
			TestEqual(TEXT("Second star"), Cues[1].Sound, UOSoundId::Star2);
		}
	}

	// Stars are clamped to 0..3
	TestEqual(TEXT("More than 3 stars still pings 3 times"), FUOAudioRules::BuildEvaluationReveal(9, EUOStarConsequence::PromotionAvailable, false, false).Num(), 3);
	TestEqual(TEXT("Negative stars ping nothing"), FUOAudioRules::BuildEvaluationReveal(-2, EUOStarConsequence::Safe, false, false).Num(), 0);
	return true;
}

#endif
