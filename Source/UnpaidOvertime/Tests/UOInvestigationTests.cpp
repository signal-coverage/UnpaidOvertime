// UNPAID: OVERTIME - investigation tests: trace texts and the AI suspicion model (evidence, noise and false leads).

#include "UOTestFlags.h"
#include "UOInvestigation.h"
#include "UOKillerRules.h"
#include "UOEvaluationRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UOInvestigationTestsPrivate
{
	FUOTraceEntry Trace(int32 Id, EUOTraceKind Kind, float Time, FName Room, TArray<int32> Shown = {}, FName Station = NAME_None)
	{
		FUOTraceEntry Entry;
		Entry.Id = Id;
		Entry.Kind = Kind;
		Entry.TimeSeconds = Time;
		Entry.Room = Room;
		Entry.StationId = Station;
		Entry.ShownActorIds = Shown;
		Entry.Detail = Shown.Num() == 0 ? EUOTraceDetail::Anonymous : (Shown.Num() == 1 ? EUOTraceDetail::Named : EUOTraceDetail::Partial);
		switch (Kind)
		{
		case EUOTraceKind::WorkReported:											Entry.Device = EUOTraceDevice::ActivityBoard; break;
		case EUOTraceKind::PrintNoOutput: case EUOTraceKind::PrinterJam: case EUOTraceKind::PrintOk:	Entry.Device = EUOTraceDevice::PrinterLog; break;
		case EUOTraceKind::DoorEntry: case EUOTraceKind::StationLocked:			Entry.Device = EUOTraceDevice::DoorLog; break;
		case EUOTraceKind::CameraSighting: case EUOTraceKind::CameraOffline:		Entry.Device = EUOTraceDevice::CameraLog; break;
		default:																	Entry.Device = EUOTraceDevice::Inventory; break;
		}
		return Entry;
	}

	FUOKnownTrace Known(const FUOTraceEntry& Entry, EUOEvidenceSource Source)
	{
		FUOKnownTrace Result;
		Result.Entry = Entry;
		Result.Source = Source;
		return Result;
	}

	FUOSuspicionInput Input(int32 Self)
	{
		FUOSuspicionInput Result;
		Result.SelfId = Self;
		for (int32 Id = 0; Id < 4; ++Id)
		{
			if (Id != Self)
			{
				Result.CandidateIds.Add(Id);
			}
		}
		return Result;
	}

	FString NameOf(int32 Id)
	{
		static const TCHAR* Names[] = { TEXT("You"), TEXT("Brenda"), TEXT("Kevin"), TEXT("Doug") };
		return Id >= 0 && Id < 4 ? FString(Names[Id]) : FString(TEXT("?"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOTraceTextTest, "UnpaidOvertime.Core.Investigation.TraceText", UO_CORE_TEST_FLAGS)
bool FUOTraceTextTest::RunTest(const FString& Parameters)
{
	using namespace UOInvestigationTestsPrivate;
	auto Names = [](int32 Id) { return NameOf(Id); };

	const FString Anonymous = FUOTraceText::Describe(Trace(1, EUOTraceKind::PrinterJam, 10.f, TEXT("CopyRoom")), Names).ToString();
	for (int32 Id = 0; Id < 4; ++Id)
	{
		TestFalse(TEXT("An anonymous trace names nobody"), Anonymous.Contains(NameOf(Id)));
	}
	const FString Partial = FUOTraceText::Describe(Trace(2, EUOTraceKind::PrinterJam, 10.f, TEXT("CopyRoom"), { 3, 1 }), Names).ToString();
	TestTrue(TEXT("A partial trace names both candidates"), Partial.Contains(TEXT("Doug")) && Partial.Contains(TEXT("Brenda")));
	const FString Named = FUOTraceText::Describe(Trace(3, EUOTraceKind::FileOverwritten, 10.f, TEXT("OpenPlan"), { 3 }), Names).ToString();
	const FString LastSeen = FUOTraceText::Describe(Trace(5, EUOTraceKind::LastSeenNear, 10.f, TEXT("Kitchen"), { 3, 1 }), Names).ToString();
	TestTrue(TEXT("The last-seen note names who was around"), LastSeen.Contains(TEXT("Doug")) && LastSeen.Contains(TEXT("Brenda")));
	TestTrue(TEXT("A named trace names the actor"), Named.Contains(TEXT("Doug")));
	TestFalse(TEXT("Every kind has text"), FUOTraceText::Describe(Trace(4, EUOTraceKind::PropMissing, 1.f, TEXT("Kitchen")), Names).IsEmpty());

	EUOTraceDevice Device;
	TestTrue(TEXT("Print server shows the printer log"), FUOTraceText::GetTerminalDevice(EUOTerminalKind::PrintServer, Device) && Device == EUOTraceDevice::PrinterLog);
	TestTrue(TEXT("Desk PCs show the file history"), FUOTraceText::GetTerminalDevice(EUOTerminalKind::DeskComputer, Device) && Device == EUOTraceDevice::FileHistory);
	TestTrue(TEXT("The badge reader shows the door log"), FUOTraceText::GetTerminalDevice(EUOTerminalKind::BadgeReader, Device) && Device == EUOTraceDevice::DoorLog);
	TestTrue(TEXT("The security monitor shows the camera log"), FUOTraceText::GetTerminalDevice(EUOTerminalKind::SecurityMonitor, Device) && Device == EUOTraceDevice::CameraLog);
	TestTrue(TEXT("The inventory sheet shows the inventory"), FUOTraceText::GetTerminalDevice(EUOTerminalKind::InventorySheet, Device) && Device == EUOTraceDevice::Inventory);
	TestFalse(TEXT("Cameras are not readable"), FUOTraceText::GetTerminalDevice(EUOTerminalKind::SecurityCamera, Device));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOSuspicionModelTest, "UnpaidOvertime.Core.Investigation.SuspicionModel", UO_CORE_TEST_FLAGS)
bool FUOSuspicionModelTest::RunTest(const FString& Parameters)
{
	using namespace UOInvestigationTestsPrivate;

	// Self = Brenda (1). Doug (3) is the killer in these synthetic cases.
	{
		const FUOSuspicionResult Empty = FUOSuspicionModel::Evaluate(Input(1));
		TestEqual(TEXT("No evidence, no suspicion"), Empty.Items.Num(), 0);
	}
	{
		FUOSuspicionInput In = Input(1);
		In.Traces.Add(Known(Trace(10, EUOTraceKind::PrinterJam, 100.f, TEXT("CopyRoom"), { 3 }), EUOEvidenceSource::Terminal));
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(In);
		TestEqual(TEXT("A named sabotage trace points at its actor"), Result.GetScore(3), FUOSuspicionModel::WeightNamed);
		TestEqual(TEXT("Others are not suspected"), Result.GetScore(2), 0.f);
		TestTrue(TEXT("Each item records its source"), Result.Items.Num() == 1 && Result.Items[0].Source == EUOEvidenceSource::Terminal && Result.Items[0].TraceId == 10);
		TestTrue(TEXT("Each item explains itself"), Result.Items.Num() == 1 && !Result.Items[0].Reason.IsEmpty());
	}
	{
		FUOSuspicionInput In = Input(1);
		In.Traces.Add(Known(Trace(11, EUOTraceKind::PrinterJam, 100.f, TEXT("CopyRoom"), { 3, 2 }), EUOEvidenceSource::Terminal));
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(In);
		TestEqual(TEXT("A partial trace splits between two candidates (Doug)"), Result.GetScore(3), FUOSuspicionModel::WeightPartial);
		TestEqual(TEXT("A partial trace splits between two candidates (Kevin)"), Result.GetScore(2), FUOSuspicionModel::WeightPartial);
	}
	{
		// Anonymous jam in the copy room at 100 s; the door log puts Doug and Kevin there, the AI itself too
		FUOSuspicionInput In = Input(1);
		In.Traces.Add(Known(Trace(12, EUOTraceKind::PrinterJam, 100.f, TEXT("CopyRoom")), EUOEvidenceSource::Terminal));
		In.Traces.Add(Known(Trace(13, EUOTraceKind::DoorEntry, 90.f, TEXT("CopyRoom"), { 3 }), EUOEvidenceSource::Terminal));
		In.Traces.Add(Known(Trace(14, EUOTraceKind::DoorEntry, 95.f, TEXT("CopyRoom"), { 2 }), EUOEvidenceSource::Terminal));
		In.Traces.Add(Known(Trace(15, EUOTraceKind::DoorEntry, 99.f, TEXT("CopyRoom"), { 1 }), EUOEvidenceSource::Witnessed));
		In.Traces.Add(Known(Trace(16, EUOTraceKind::DoorEntry, 40.f, TEXT("Kitchen"), { 0 }), EUOEvidenceSource::Terminal));
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(In);
		TestEqual(TEXT("Presence splits the weight (Doug)"), Result.GetScore(3), FUOSuspicionModel::WeightPresence / 2.f);
		TestEqual(TEXT("Presence splits the weight (Kevin)"), Result.GetScore(2), FUOSuspicionModel::WeightPresence / 2.f);
		TestEqual(TEXT("Someone elsewhere is not suspected"), Result.GetScore(0), 0.f);
		TestEqual(TEXT("Never suspects itself"), Result.GetScore(1), 0.f);
	}
	{
		// Same jam, but the presence of Doug comes from its own eyes
		FUOSuspicionInput In = Input(1);
		In.Traces.Add(Known(Trace(17, EUOTraceKind::PrinterJam, 100.f, TEXT("CopyRoom")), EUOEvidenceSource::Witnessed));
		FUOSighting Sighting;
		Sighting.EmployeeId = 3;
		Sighting.Room = TEXT("CopyRoom");
		Sighting.TimeSeconds = 100.f;
		In.Sightings.Add(Sighting);
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(In);
		TestEqual(TEXT("Its own sighting places the suspect"), Result.GetScore(3), FUOSuspicionModel::WeightPresence);
	}
	{
		FUOSuspicionInput In = Input(1);
		In.Traces.Add(Known(Trace(18, EUOTraceKind::CameraOffline, 100.f, TEXT("Kitchen")), EUOEvidenceSource::Witnessed));
		FUOSeenAct Act;
		Act.ActorId = 3;
		Act.TraceId = 18;
		Act.TimeSeconds = 100.f;
		In.SeenActs.Add(Act);
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(In);
		TestTrue(TEXT("An act seen with its own eyes is strong"), Result.GetScore(3) >= FUOSuspicionModel::WeightSeenAct);
	}
	{
		// Rule 3: no fake work any more. A report plus a "0 pages" line is someone's botched attempt, not evidence
		// (for a sharp AI; a gullible one may read too much into it, see FalseLeads)
		FUOSuspicionInput In = Input(1);
		In.Traces.Add(Known(Trace(20, EUOTraceKind::WorkReported, 200.f, TEXT("CopyRoom"), { 3 }, TEXT("Printer_01")), EUOEvidenceSource::Public));
		In.Traces.Add(Known(Trace(21, EUOTraceKind::PrintNoOutput, 200.5f, TEXT("CopyRoom"), { 3 }, TEXT("Printer_01")), EUOEvidenceSource::Terminal));
		TestEqual(TEXT("No fake-work evidence for a sharp AI"), FUOSuspicionModel::Evaluate(In).GetScore(3), 0.f);
	}
	{
		FUOSuspicionInput In = Input(1);
		FUOCarriedSuspicion Yesterday;
		Yesterday.EmployeeId = 3;
		Yesterday.Score = 4.f;
		In.CarriedOver.Add(Yesterday);
		FUOCarriedSuspicion Gone;
		Gone.EmployeeId = 7;
		Gone.Score = 9.f;
		In.CarriedOver.Add(Gone);
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(In);
		TestEqual(TEXT("Yesterday's suspicion carries over, halved"), Result.GetScore(3), 4.f * FUOSuspicionModel::CarryOverFactor);
		TestEqual(TEXT("...only for current candidates"), Result.GetScore(7), 0.f);
		TestTrue(TEXT("...as memory"), Result.Items.Num() == 1 && Result.Items[0].Source == EUOEvidenceSource::Memory && Result.Items[0].TraceId == INDEX_NONE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOAcceptedAnswerTest, "UnpaidOvertime.Core.Investigation.AcceptedAnswers", UO_CORE_TEST_FLAGS)
bool FUOAcceptedAnswerTest::RunTest(const FString& Parameters)
{
	using namespace UOInvestigationTestsPrivate;

	auto Answer = [](int32 Who, bool bKnows, bool bWrong)
	{
		FUOAcceptedAnswer Result;
		Result.EmployeeId = Who;
		Result.bKnowsFact = bKnows;
		Result.bWrong = bWrong;
		Result.Prompt = TEXT("How many plants are there in the Kitchen?");
		return Result;
	};

	// A sharp AI that knows the fact
	FUOSuspicionInput Knows = Input(1);
	Knows.AcceptedAnswers.Add(Answer(3, true, true));
	const FUOSuspicionResult Caught = FUOSuspicionModel::Evaluate(Knows);
	TestEqual(TEXT("The boss accepted a wrong answer it knows: strong evidence"), Caught.GetScore(3), FUOSuspicionModel::WeightAcceptedWrong);
	TestTrue(TEXT("Logged as Public+Knowledge"), Caught.Items.Num() == 1 && Caught.Items[0].Source == EUOEvidenceSource::PublicKnowledge && Caught.Items[0].TraceId == INDEX_NONE);

	FUOSuspicionInput Right = Input(1);
	Right.AcceptedAnswers.Add(Answer(3, true, false));
	TestEqual(TEXT("A right answer is nothing"), FUOSuspicionModel::Evaluate(Right).Items.Num(), 0);

	FUOSuspicionInput Unaware = Input(1);
	Unaware.AcceptedAnswers.Add(Answer(3, false, true));
	TestEqual(TEXT("Without knowing the fact a sharp AI sees nothing"), FUOSuspicionModel::Evaluate(Unaware).Items.Num(), 0);

	// A gullible AI that does not know the fact sometimes misreads the anomaly and blames someone else
	int32 Misreads = 0;
	for (int32 Seed = 0; Seed < 400; ++Seed)
	{
		FUOSuspicionInput Gullible = Input(1);
		Gullible.Suspicion = 0.f;
		Gullible.NoiseSeed = Seed;
		Gullible.AcceptedAnswers.Add(Answer(3, false, true));
		for (const FUOSuspicionItem& Item : FUOSuspicionModel::Evaluate(Gullible).Items)
		{
			if (Item.Reason.Contains(TEXT("anomaly")))
			{
				++Misreads;
				TestNotEqual(TEXT("A misread anomaly is blamed on someone else"), Item.SuspectId, 3);
				TestNotEqual(TEXT("...never on itself"), Item.SuspectId, 1);
				TestEqual(TEXT("...as a hunch"), Item.Source, EUOEvidenceSource::Hunch);
			}
		}
	}
	TestTrue(FString::Printf(TEXT("Misreads happen about %.0f%% of the time at zero Suspicion (got %d of 400)"), FUOSuspicionModel::MisreadChance * 100.f, Misreads),
		Misreads > 400 * FUOSuspicionModel::MisreadChance * 0.75f && Misreads < 400 * FUOSuspicionModel::MisreadChance * 1.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOFalseLeadsTest, "UnpaidOvertime.Core.Investigation.FalseLeads", UO_CORE_TEST_FLAGS)
bool FUOFalseLeadsTest::RunTest(const FString& Parameters)
{
	using namespace UOInvestigationTestsPrivate;

	// Someone's botched work: a false lead for the gullible only
	auto Botched = [](float Suspicion, int32 Seed)
	{
		FUOSuspicionInput In = Input(1);
		In.Suspicion = Suspicion;
		In.NoiseSeed = Seed;
		In.Traces.Add(Known(Trace(30, EUOTraceKind::PrintNoOutput, 100.f, TEXT("CopyRoom"), { 2 }, TEXT("Printer_01")), EUOEvidenceSource::Terminal));
		return In;
	};
	float Sharp = 0.f;
	float Dull = 0.f;
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		Sharp += FUOSuspicionModel::Evaluate(Botched(1.f, Seed)).GetScore(2);
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(Botched(0.2f, Seed));
		for (const FUOSuspicionItem& Item : Result.Items)
		{
			Dull += Item.TraceId == 30 ? Item.Weight : 0.f;
		}
	}
	TestEqual(TEXT("A sharp AI ignores a botched attempt"), Sharp, 0.f);
	TestTrue(TEXT("A gullible AI reads something into it"), Dull > 0.f);

	// Hunches: never with full Suspicion, often with none; never about itself
	int32 Hunches = 0;
	for (int32 Seed = 0; Seed < 400; ++Seed)
	{
		FUOSuspicionInput Sharpest = Input(1);
		Sharpest.NoiseSeed = Seed;
		TestEqual(TEXT("Full Suspicion: no hunches"), FUOSuspicionModel::Evaluate(Sharpest).Items.Num(), 0);

		FUOSuspicionInput Gullible = Input(1);
		Gullible.Suspicion = 0.f;
		Gullible.NoiseSeed = Seed;
		for (const FUOSuspicionItem& Item : FUOSuspicionModel::Evaluate(Gullible).Items)
		{
			TestEqual(TEXT("A hunch is a hunch"), Item.Source, EUOEvidenceSource::Hunch);
			TestTrue(TEXT("About a candidate"), Item.SuspectId != 1 && Gullible.CandidateIds.Contains(Item.SuspectId));
			++Hunches;
		}
	}
	TestTrue(FString::Printf(TEXT("Hunches about %.0f%% of the time at zero Suspicion (got %d of 400)"), FUOSuspicionModel::HunchChance * 100.f, Hunches),
		Hunches > 400 * FUOSuspicionModel::HunchChance * 0.75f && Hunches < 400 * FUOSuspicionModel::HunchChance * 1.25f);

	// Noise: the same seed gives the same view; different seeds wobble evidence weights within +-WeightNoise
	FUOSuspicionInput Noisy = Input(1);
	Noisy.Suspicion = 0.f;
	Noisy.Traces.Add(Known(Trace(40, EUOTraceKind::PrinterJam, 100.f, TEXT("CopyRoom"), { 3 }), EUOEvidenceSource::Terminal));
	Noisy.NoiseSeed = 9;
	TestEqual(TEXT("Deterministic per seed"), FUOSuspicionModel::Evaluate(Noisy).GetScore(3), FUOSuspicionModel::Evaluate(Noisy).GetScore(3));
	bool bVaries = false;
	for (int32 Seed = 0; Seed < 50; ++Seed)
	{
		Noisy.NoiseSeed = Seed;
		const FUOSuspicionResult Result = FUOSuspicionModel::Evaluate(Noisy);
		for (const FUOSuspicionItem& Item : Result.Items)
		{
			if (Item.TraceId == 40)
			{
				TestTrue(TEXT("Within the noise band"), Item.Weight >= FUOSuspicionModel::WeightNamed * (1.f - FUOSuspicionModel::WeightNoise) - 0.001f
					&& Item.Weight <= FUOSuspicionModel::WeightNamed * (1.f + FUOSuspicionModel::WeightNoise) + 0.001f);
				bVaries |= !FMath::IsNearlyEqual(Item.Weight, FUOSuspicionModel::WeightNamed, 0.01f);
			}
		}
	}
	TestTrue(TEXT("A gullible AI weighs evidence unevenly"), bVaries);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
