// UNPAID: OVERTIME - the boss's questions: generated from the real office and the day's events, always fair.

#include "UOBossQuestions.h"
#include "UODayDifficulty.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "UOBossQuestions"

namespace UOBossPrivate
{
	struct FKindName
	{
		const TCHAR* Tag;
		EUOPropKind Kind;
	};

	const FKindName KindNames[] = {
		{ TEXT("Plant"), EUOPropKind::Plant },
		{ TEXT("Printer"), EUOPropKind::Printer },
		{ TEXT("CoffeeMaker"), EUOPropKind::CoffeeMaker },
		{ TEXT("Fridge"), EUOPropKind::Fridge },
		{ TEXT("Microwave"), EUOPropKind::Microwave },
		{ TEXT("FilingCabinet"), EUOPropKind::FilingCabinet },
		{ TEXT("Chair"), EUOPropKind::Chair },
		{ TEXT("Poster"), EUOPropKind::Poster },
		{ TEXT("Clock"), EUOPropKind::Clock },
		{ TEXT("Mug"), EUOPropKind::Mug },
		{ TEXT("RecycleBin"), EUOPropKind::RecycleBin },
		{ TEXT("WaterCooler"), EUOPropKind::WaterCooler },
		{ TEXT("VendingMachine"), EUOPropKind::VendingMachine },
		{ TEXT("Sofa"), EUOPropKind::Sofa },
		{ TEXT("Whiteboard"), EUOPropKind::Whiteboard }
	};

	/** Distractor pools (real values from the level are added to these at generation time) */
	const TCHAR* const ColorPool[] = { TEXT("Red"), TEXT("Blue"), TEXT("Green"), TEXT("Yellow"), TEXT("White"), TEXT("Black"), TEXT("Silver"), TEXT("Gray"), TEXT("Orange"), TEXT("Purple") };
	const TCHAR* const PosterPool[] = { TEXT("SYNERGY"), TEXT("OVERTIME"), TEXT("TEAMWORK"), TEXT("SMILE"), TEXT("TRUST"), TEXT("EXCELLENCE"), TEXT("PERSEVERANCE") };
	const TCHAR* const ShapePool[] = { TEXT("Square"), TEXT("Round"), TEXT("Oval"), TEXT("Hexagonal") };
	const TCHAR* const CompanyPool[] = { TEXT("Synergex Group"), TEXT("Paperclip Partners"), TEXT("Quarterly Results Inc."), TEXT("Stapleton & Sons"), TEXT("Cubicle Dynamics") };

	template <typename T>
	void Shuffle(TArray<T>& Items, FRandomStream& Rng)
	{
		for (int32 Index = Items.Num() - 1; Index > 0; --Index)
		{
			Items.Swap(Index, Rng.RandRange(0, Index));
		}
	}

	FString JoinRooms(const TArray<FName>& Rooms)
	{
		FString Result;
		for (const FName& Room : Rooms)
		{
			Result += (Result.IsEmpty() ? TEXT("") : TEXT(", ")) + Room.ToString();
		}
		return Result;
	}

	/**
	 *  Fills the options: the correct one plus distractors in the given order (skipping empties and duplicates),
	 *  up to NumOptions, then shuffles. Fails below the 3-option minimum.
	 */
	bool BuildOptions(FUOBossQuestion& Question, const FText& Correct, const TArray<FText>& Distractors, int32 NumOptions, FRandomStream& Rng)
	{
		TArray<FText> Options = { Correct };
		TSet<FString> Seen = { Correct.ToString() };
		for (const FText& Distractor : Distractors)
		{
			if (Options.Num() >= NumOptions)
			{
				break;
			}
			const FString Key = Distractor.ToString();
			if (Distractor.IsEmpty() || Seen.Contains(Key))
			{
				continue;
			}
			Seen.Add(Key);
			Options.Add(Distractor);
		}
		if (Options.Num() < FUOBossQuestionGenerator::MinOptions)
		{
			return false;
		}

		Shuffle(Options, Rng);
		Question.Options = Options;
		Question.CorrectIndex = Options.IndexOfByPredicate([&Correct](const FText& Option) { return Option.ToString() == Correct.ToString(); });
		Question.bValid = Question.CorrectIndex != INDEX_NONE;
		return Question.bValid;
	}

	TArray<FText> ValuePoolTexts(EUOPropAttribute Attribute, const TArray<FName>& Exclude, FRandomStream& Rng)
	{
		TArray<FName> Pool;
		auto AddPool = [&Pool](const TCHAR* const* Values, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Pool.AddUnique(FName(Values[Index]));
			}
		};
		switch (Attribute)
		{
		case EUOPropAttribute::Color:	AddPool(ColorPool, UE_ARRAY_COUNT(ColorPool)); break;
		case EUOPropAttribute::Text:	AddPool(PosterPool, UE_ARRAY_COUNT(PosterPool)); break;
		case EUOPropAttribute::Shape:	AddPool(ShapePool, UE_ARRAY_COUNT(ShapePool)); break;
		}
		Pool.RemoveAll([&Exclude](const FName& Value) { return Exclude.Contains(Value); });
		Shuffle(Pool, Rng);

		TArray<FText> Texts;
		for (const FName& Value : Pool)
		{
			Texts.Add(FUOBossQuestionGenerator::GetValueText(Attribute, Value));
		}
		return Texts;
	}

	/** Display names of rooms other than Correct (facts' rooms first, then every known room), shuffled */
	TArray<FText> OtherRoomTexts(const FUOBossKnowledge& Knowledge, FName Correct, FRandomStream& Rng)
	{
		TArray<FName> Rooms = Knowledge.Facts.GetRooms();
		Rooms.Remove(Correct);
		Shuffle(Rooms, Rng);
		TArray<FName> Extra = FUORooms::GetKnownRooms();
		Extra.RemoveAll([&Rooms, Correct](const FName& Room) { return Room == Correct || Rooms.Contains(Room); });
		Shuffle(Extra, Rng);
		Rooms.Append(Extra);

		TArray<FText> Texts;
		for (const FName& Room : Rooms)
		{
			Texts.Add(FUORooms::GetDisplayName(Room));
		}
		return Texts;
	}

	FText GetWorkQuestion(EUOStationType Station, bool bWithTime)
	{
		switch (Station)
		{
		case EUOStationType::CoffeeMaker:
			return bWithTime ? LOCTEXT("WhoCoffeeAt", "Who refilled a coffee maker in the {0} at {1}?") : LOCTEXT("WhoCoffee", "Who refilled a coffee maker in the {0}?");
		case EUOStationType::Printer:
			return bWithTime ? LOCTEXT("WhoPrinterAt", "Who replaced a printer's ink cartridge in the {0} at {1}?") : LOCTEXT("WhoPrinter", "Who replaced a printer's ink cartridge in the {0}?");
		case EUOStationType::FilingCabinet:
			return bWithTime ? LOCTEXT("WhoFileAt", "Who filed a report in the {0} at {1}?") : LOCTEXT("WhoFile", "Who filed a report in the {0}?");
		default:
			return bWithTime ? LOCTEXT("WhoWorkAt", "Who finished a task in the {0} at {1}?") : LOCTEXT("WhoWork", "Who finished a task in the {0}?");
		}
	}

	// ~ Static categories

	void AddPropCounts(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		for (const FName& Room : Knowledge.Presence.GetVisitedRooms(AskedId))
		{
			for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EUOPropKind::Count); ++KindIndex)
			{
				const EUOPropKind Kind = static_cast<EUOPropKind>(KindIndex);
				const int32 Count = Knowledge.Facts.Count(Room, Kind);
				const int32 MaxCount = Tuning.bAllowHardFacts ? FUOBossQuestionGenerator::MaxAskedCountHard : FUOBossQuestionGenerator::MaxAskedCount;
				if (Count <= 0 || Count > MaxCount || !FUOPropKinds::IsCountable(Kind) || (FUOPropKinds::IsHardToCount(Kind) && !Tuning.bAllowHardFacts))
				{
					continue;
				}

				const TArray<int32> Offsets = Tuning.bCloseDistractors ? TArray<int32>({ -1, 1, 2, -2, 3 }) : TArray<int32>({ 2, -2, 4, 1, -1, 3 });
				TArray<FText> Distractors;
				for (const int32 Offset : Offsets)
				{
					if (Count + Offset >= 0)
					{
						Distractors.Add(FText::AsNumber(Count + Offset));
					}
				}

				FUOBossQuestion Question;
				Question.Category = EUOBossQuestionCategory::PropCount;
				Question.SubjectRoom = Room;
				Question.Prompt = FText::Format(LOCTEXT("HowMany", "How many {0} are there in the {1}?"), FUOPropKinds::GetPluralName(Kind), FUORooms::GetDisplayName(Room));
				Question.Rationale = FString::Printf(TEXT("PropCount %s/%s = %d; asked employee %d entered %s"),
					*Room.ToString(), KindNames[KindIndex].Tag, Count, AskedId, *Room.ToString());
				if (BuildOptions(Question, FText::AsNumber(Count), Distractors, Tuning.NumOptions, Rng))
				{
					Out.Add(Question);
				}
			}
		}
	}

	void AddPosters(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		for (const FName& Room : Knowledge.Presence.GetVisitedRooms(AskedId))
		{
			const TArray<FName> Texts = Knowledge.Facts.GetValues(Room, EUOPropKind::Poster, EUOPropAttribute::Text);
			if (Texts.Num() == 0)
			{
				continue;
			}
			const FName Answer = Texts[Rng.RandRange(0, Texts.Num() - 1)];

			FUOBossQuestion Question;
			Question.Category = EUOBossQuestionCategory::PosterInRoom;
			Question.SubjectRoom = Room;
			Question.Prompt = FText::Format(LOCTEXT("WhichPoster", "Which of these posters hangs in the {0}?"), FUORooms::GetDisplayName(Room));
			Question.Rationale = FString::Printf(TEXT("PosterInRoom %s has '%s'; asked employee %d entered %s"), *Room.ToString(), *Answer.ToString(), AskedId, *Room.ToString());
			if (BuildOptions(Question, FUOBossQuestionGenerator::GetValueText(EUOPropAttribute::Text, Answer), ValuePoolTexts(EUOPropAttribute::Text, Texts, Rng), Tuning.NumOptions, Rng))
			{
				Out.Add(Question);
			}
		}
	}

	void AddColors(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		for (const FName& Room : Knowledge.Presence.GetVisitedRooms(AskedId))
		{
			for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EUOPropKind::Count); ++KindIndex)
			{
				const EUOPropKind Kind = static_cast<EUOPropKind>(KindIndex);
				if (!FUOPropKinds::HasAskableColor(Kind))
				{
					continue;
				}
				// Only unambiguous colors: every instance in the room shares it
				const TArray<FName> Colors = Knowledge.Facts.GetValues(Room, Kind, EUOPropAttribute::Color);
				if (Colors.Num() != 1)
				{
					continue;
				}

				const int32 Count = Knowledge.Facts.Count(Room, Kind);
				FUOBossQuestion Question;
				Question.Category = EUOBossQuestionCategory::PropColor;
				Question.SubjectRoom = Room;
				Question.Prompt = Count == 1
					? FText::Format(LOCTEXT("ColorOne", "What color is the {0} in the {1}?"), FUOPropKinds::GetSingularName(Kind), FUORooms::GetDisplayName(Room))
					: FText::Format(LOCTEXT("ColorMany", "What color are the {0} in the {1}?"), FUOPropKinds::GetPluralName(Kind), FUORooms::GetDisplayName(Room));
				Question.Rationale = FString::Printf(TEXT("PropColor %s/%s = %s (x%d); asked employee %d entered %s"),
					*Room.ToString(), KindNames[KindIndex].Tag, *Colors[0].ToString(), Count, AskedId, *Room.ToString());
				if (BuildOptions(Question, FUOBossQuestionGenerator::GetValueText(EUOPropAttribute::Color, Colors[0]), ValuePoolTexts(EUOPropAttribute::Color, Colors, Rng), Tuning.NumOptions, Rng))
				{
					Out.Add(Question);
				}
			}
		}
	}

	void AddClockShapes(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		for (const FName& Room : Knowledge.Presence.GetVisitedRooms(AskedId))
		{
			const TArray<FName> Shapes = Knowledge.Facts.GetValues(Room, EUOPropKind::Clock, EUOPropAttribute::Shape);
			if (Shapes.Num() != 1)
			{
				continue;
			}
			FUOBossQuestion Question;
			Question.Category = EUOBossQuestionCategory::ClockShape;
			Question.SubjectRoom = Room;
			Question.Prompt = FText::Format(LOCTEXT("ClockShape", "What shape is the wall clock in the {0}?"), FUORooms::GetDisplayName(Room));
			Question.Rationale = FString::Printf(TEXT("ClockShape %s = %s; asked employee %d entered %s"), *Room.ToString(), *Shapes[0].ToString(), AskedId, *Room.ToString());
			if (BuildOptions(Question, FUOBossQuestionGenerator::GetValueText(EUOPropAttribute::Shape, Shapes[0]), ValuePoolTexts(EUOPropAttribute::Shape, Shapes, Rng), Tuning.NumOptions, Rng))
			{
				Out.Add(Question);
			}
		}
	}

	void AddWhichRoomHas(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EUOPropKind::Count); ++KindIndex)
		{
			const EUOPropKind Kind = static_cast<EUOPropKind>(KindIndex);
			const TArray<FName> Rooms = Knowledge.Facts.GetRoomsWith(Kind);
			const bool bMoved = Knowledge.Moves.ContainsByPredicate([Kind](const FUOPropMove& Move) { return Move.Kind == Kind; });
			if (bMoved || Rooms.Num() != 1 || !Knowledge.Presence.HasVisited(AskedId, Rooms[0]))
			{
				continue;
			}

			const FName Room = Rooms[0];
			const int32 Count = Knowledge.Facts.Count(Room, Kind);
			FUOBossQuestion Question;
			Question.Category = EUOBossQuestionCategory::WhichRoomHas;
			Question.SubjectRoom = Room;
			Question.Prompt = Count == 1
				? FText::Format(LOCTEXT("WhichRoomOne", "Which room has the {0}?"), FUOPropKinds::GetSingularName(Kind))
				: FText::Format(LOCTEXT("WhichRoomMany", "Which room has the {0}?"), FUOPropKinds::GetPluralName(Kind));
			Question.Rationale = FString::Printf(TEXT("WhichRoomHas %s only in %s; asked employee %d entered %s"), KindNames[KindIndex].Tag, *Room.ToString(), AskedId, *Room.ToString());
			if (BuildOptions(Question, FUORooms::GetDisplayName(Room), OtherRoomTexts(Knowledge, Room, Rng), Tuning.NumOptions, Rng))
			{
				Out.Add(Question);
			}
		}
	}

	void AddStaticCandidates(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		AddPropCounts(Out, Knowledge, AskedId, Tuning, Rng);
		AddPosters(Out, Knowledge, AskedId, Tuning, Rng);
		AddColors(Out, Knowledge, AskedId, Tuning, Rng);
		AddClockShapes(Out, Knowledge, AskedId, Tuning, Rng);
		AddWhichRoomHas(Out, Knowledge, AskedId, Tuning, Rng);
	}

	// ~ Event categories

	TArray<FText> OtherEmployeeNames(const FUOBossKnowledge& Knowledge, int32 ExcludeA, int32 ExcludeB, FRandomStream& Rng)
	{
		TArray<FText> Names;
		for (const TPair<int32, FString>& Employee : Knowledge.Employees)
		{
			if (Employee.Key != ExcludeA && Employee.Key != ExcludeB)
			{
				Names.Add(FText::FromString(Employee.Value));
			}
		}
		Shuffle(Names, Rng);
		return Names;
	}

	void AddWhoDidWork(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		for (const FUOBossEvent& Event : Knowledge.Events)
		{
			if (Event.Kind != EUOBossEventKind::CompletedWork || !FUOBossQuestionGenerator::IsEventWitnessed(Knowledge, AskedId, Event))
			{
				continue;
			}

			auto IsSameWorkByOther = [&Event](const FUOBossEvent& Other)
			{
				return Other.Kind == EUOBossEventKind::CompletedWork && Other.Room == Event.Room && Other.StationType == Event.StationType && Other.ActorId != Event.ActorId;
			};
			const bool bAmbiguous = Knowledge.Events.ContainsByPredicate(IsSameWorkByOther);
			const bool bWithTime = bAmbiguous || Tuning.bTimeQualifiedEvents;
			// Even with the time, someone else doing the same thing in the same minute makes it unanswerable
			if (bWithTime && Knowledge.Events.ContainsByPredicate([&](const FUOBossEvent& Other) { return IsSameWorkByOther(Other) && Other.ClockTime == Event.ClockTime; }))
			{
				continue;
			}
			// One question per actor/room/station (and time when needed)
			const bool bDuplicate = Out.ContainsByPredicate([&Event, bWithTime](const FUOBossQuestion& Existing)
			{
				return Existing.Category == EUOBossQuestionCategory::WhoDidWork && Existing.Rationale.Contains(FString::Printf(TEXT("key=%d/%s/%d/%s"),
					Event.ActorId, *Event.Room.ToString(), static_cast<int32>(Event.StationType), bWithTime ? *Event.ClockTime : TEXT("-")));
			});
			if (bDuplicate)
			{
				continue;
			}

			FUOBossQuestion Question;
			Question.Category = EUOBossQuestionCategory::WhoDidWork;
			Question.SubjectRoom = Event.Room;
			Question.SubjectTime = Event.Time;
			Question.Prompt = FText::Format(GetWorkQuestion(Event.StationType, bWithTime), FUORooms::GetDisplayName(Event.Room), FText::FromString(Event.ClockTime));
			Question.Rationale = FString::Printf(TEXT("WhoDidWork: %s completed work at a %s in %s at %s (t=%.1f); asked employee %d was in %s then%s key=%d/%s/%d/%s"),
				*Event.ActorName, *UOText::GetStationTypeName(Event.StationType).ToString(), *Event.Room.ToString(), *Event.ClockTime, Event.Time, AskedId,
				*Event.Room.ToString(), bAmbiguous ? TEXT(" (time names it: others did the same there)") : TEXT(""),
				Event.ActorId, *Event.Room.ToString(), static_cast<int32>(Event.StationType), bWithTime ? *Event.ClockTime : TEXT("-"));
			if (BuildOptions(Question, FText::FromString(Event.ActorName), OtherEmployeeNames(Knowledge, Event.ActorId, AskedId, Rng), Tuning.NumOptions, Rng))
			{
				Out.Add(Question);
			}
		}
	}

	void AddWhatPickedUp(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		TSet<FString> Done;
		for (const FUOBossEvent& Event : Knowledge.Events)
		{
			if (Event.Kind != EUOBossEventKind::PickedUpItem || !FUOBossQuestionGenerator::IsEventWitnessed(Knowledge, AskedId, Event))
			{
				continue;
			}
			const FString Key = FString::Printf(TEXT("%d/%s"), Event.ActorId, *Event.Room.ToString());
			if (Done.Contains(Key))
			{
				continue;
			}
			Done.Add(Key);

			// Unambiguous only: everything this actor picked up in this room was the same item
			const bool bMixed = Knowledge.Events.ContainsByPredicate([&Event](const FUOBossEvent& Other)
			{
				return Other.Kind == EUOBossEventKind::PickedUpItem && Other.ActorId == Event.ActorId && Other.Room == Event.Room && Other.Item != Event.Item;
			});
			if (bMixed && !Tuning.bTimeQualifiedEvents)
			{
				continue;
			}
			const bool bWithTime = bMixed || Tuning.bTimeQualifiedEvents;

			TArray<FText> Distractors;
			Distractors.Add(UOText::GetItemName(Event.Item == EUOItemType::Report ? EUOItemType::InkCartridge : EUOItemType::Report));
			TArray<FText> Extra = { LOCTEXT("ItemStapler", "Stapler"), LOCTEXT("ItemFilters", "Coffee filters"), LOCTEXT("ItemNothing", "Nothing") };
			Shuffle(Extra, Rng);
			Distractors.Append(Extra);

			FUOBossQuestion Question;
			Question.Category = EUOBossQuestionCategory::WhatPickedUp;
			Question.SubjectRoom = Event.Room;
			Question.SubjectTime = Event.Time;
			Question.Prompt = bWithTime
				? FText::Format(LOCTEXT("PickedUpAt", "What did {0} pick up in the {1} at {2}?"), FText::FromString(Event.ActorName), FUORooms::GetDisplayName(Event.Room), FText::FromString(Event.ClockTime))
				: FText::Format(LOCTEXT("PickedUp", "What did {0} pick up in the {1}?"), FText::FromString(Event.ActorName), FUORooms::GetDisplayName(Event.Room));
			Question.Rationale = FString::Printf(TEXT("WhatPickedUp: %s picked up %s in %s at %s (t=%.1f); asked employee %d was in %s then"),
				*Event.ActorName, *UOText::GetItemName(Event.Item).ToString(), *Event.Room.ToString(), *Event.ClockTime, Event.Time, AskedId, *Event.Room.ToString());
			if (BuildOptions(Question, UOText::GetItemName(Event.Item), Distractors, Tuning.NumOptions, Rng))
			{
				Out.Add(Question);
			}
		}
	}

	void AddFirstCompletion(TArray<FUOBossQuestion>& Out, const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		const FUOBossEvent* First = nullptr;
		for (const FUOBossEvent& Event : Knowledge.Events)
		{
			if (Event.Kind == EUOBossEventKind::CompletedWork && !Event.Room.IsNone() && (!First || Event.Time < First->Time))
			{
				First = &Event;
			}
		}
		// Seen (or done) by the asked employee
		if (!First || !Knowledge.Presence.WasIn(AskedId, First->Room, First->Time))
		{
			return;
		}

		FUOBossQuestion Question;
		Question.Category = EUOBossQuestionCategory::FirstCompletionRoom;
		Question.SubjectRoom = First->Room;
		Question.SubjectTime = First->Time;
		Question.Prompt = LOCTEXT("FirstCompletion", "Where was the first piece of work finished today?");
		Question.Rationale = FString::Printf(TEXT("FirstCompletionRoom: %s by %s at %s (t=%.1f); asked employee %d was in %s then"),
			*First->Room.ToString(), *First->ActorName, *First->ClockTime, First->Time, AskedId, *First->Room.ToString());
		if (BuildOptions(Question, FUORooms::GetDisplayName(First->Room), OtherRoomTexts(Knowledge, First->Room, Rng), Tuning.NumOptions, Rng))
		{
			Out.Add(Question);
		}
	}

	FUOBossQuestion MakeCompanyQuestion(const FUOBossKnowledge& Knowledge, int32 AskedId, const FUOBossTuning& Tuning, FRandomStream& Rng)
	{
		TArray<FText> Distractors;
		for (const TCHAR* Name : CompanyPool)
		{
			if (!Knowledge.CompanyName.Equals(Name, ESearchCase::IgnoreCase))
			{
				Distractors.Add(FText::FromString(Name));
			}
		}
		Shuffle(Distractors, Rng);

		FUOBossQuestion Question;
		Question.Category = EUOBossQuestionCategory::CompanyName;
		Question.Prompt = LOCTEXT("CompanyQuestion", "Quick one. What is the name of the company you work for?");
		Question.Rationale = FString::Printf(TEXT("fallback: no fair room fact for employee %d; the company name is on the lobby sign and the HUD"), AskedId);
		const FString Company = Knowledge.CompanyName.IsEmpty() ? FString(TEXT("Acme")) : Knowledge.CompanyName;
		BuildOptions(Question, FText::FromString(Company), Distractors, Tuning.NumOptions, Rng);
		return Question;
	}
}

// ~ Rooms and prop kinds

FText FUORooms::GetDisplayName(FName RoomId)
{
	static const TMap<FName, FText> Names = {
		{ FName(TEXT("Lobby")), LOCTEXT("RoomLobby", "Lobby") },
		{ FName(TEXT("OpenPlan")), LOCTEXT("RoomOpenPlan", "Open plan") },
		{ FName(TEXT("Manager")), LOCTEXT("RoomManager", "Manager's office") },
		{ FName(TEXT("CopyRoom")), LOCTEXT("RoomCopyRoom", "Copy room") },
		{ FName(TEXT("Corridor")), LOCTEXT("RoomCorridor", "Corridor") },
		{ FName(TEXT("Supply")), LOCTEXT("RoomSupply", "Supply room") },
		{ FName(TEXT("Records")), LOCTEXT("RoomRecords", "Records") },
		{ FName(TEXT("Kitchen")), LOCTEXT("RoomKitchen", "Break room") }
	};
	if (const FText* Found = Names.Find(RoomId))
	{
		return *Found;
	}
	return FText::FromName(RoomId);
}

const TArray<FName>& FUORooms::GetKnownRooms()
{
	static const TArray<FName> Rooms = {
		FName(TEXT("Lobby")), FName(TEXT("OpenPlan")), FName(TEXT("Manager")), FName(TEXT("CopyRoom")),
		FName(TEXT("Corridor")), FName(TEXT("Supply")), FName(TEXT("Records")), FName(TEXT("Kitchen"))
	};
	return Rooms;
}

bool FUOPropKinds::Parse(const FString& Name, EUOPropKind& OutKind)
{
	for (const UOBossPrivate::FKindName& Entry : UOBossPrivate::KindNames)
	{
		if (Name.Equals(Entry.Tag, ESearchCase::IgnoreCase))
		{
			OutKind = Entry.Kind;
			return true;
		}
	}
	return false;
}

FText FUOPropKinds::GetSingularName(EUOPropKind Kind)
{
	switch (Kind)
	{
	case EUOPropKind::Plant:			return LOCTEXT("PlantOne", "plant");
	case EUOPropKind::Printer:			return LOCTEXT("PrinterOne", "printer");
	case EUOPropKind::CoffeeMaker:		return LOCTEXT("CoffeeMakerOne", "coffee brewer");
	case EUOPropKind::Fridge:			return LOCTEXT("FridgeOne", "fridge");
	case EUOPropKind::Microwave:		return LOCTEXT("MicrowaveOne", "microwave");
	case EUOPropKind::FilingCabinet:	return LOCTEXT("FilingCabinetOne", "filing cabinet");
	case EUOPropKind::Chair:			return LOCTEXT("ChairOne", "chair");
	case EUOPropKind::Poster:			return LOCTEXT("PosterOne", "poster");
	case EUOPropKind::Clock:			return LOCTEXT("ClockOne", "wall clock");
	case EUOPropKind::Mug:				return LOCTEXT("MugOne", "mug");
	case EUOPropKind::RecycleBin:		return LOCTEXT("RecycleBinOne", "recycling bin");
	case EUOPropKind::WaterCooler:		return LOCTEXT("WaterCoolerOne", "water cooler");
	case EUOPropKind::VendingMachine:	return LOCTEXT("VendingMachineOne", "vending machine");
	case EUOPropKind::Sofa:				return LOCTEXT("SofaOne", "sofa");
	case EUOPropKind::Whiteboard:		return LOCTEXT("WhiteboardOne", "whiteboard");
	default:							return FText::GetEmpty();
	}
}

FText FUOPropKinds::GetPluralName(EUOPropKind Kind)
{
	switch (Kind)
	{
	case EUOPropKind::Plant:			return LOCTEXT("PlantMany", "plants");
	case EUOPropKind::Printer:			return LOCTEXT("PrinterMany", "printers");
	case EUOPropKind::CoffeeMaker:		return LOCTEXT("CoffeeMakerMany", "coffee brewers");
	case EUOPropKind::Fridge:			return LOCTEXT("FridgeMany", "fridges");
	case EUOPropKind::Microwave:		return LOCTEXT("MicrowaveMany", "microwaves");
	case EUOPropKind::FilingCabinet:	return LOCTEXT("FilingCabinetMany", "filing cabinets");
	case EUOPropKind::Chair:			return LOCTEXT("ChairMany", "chairs");
	case EUOPropKind::Poster:			return LOCTEXT("PosterMany", "posters");
	case EUOPropKind::Clock:			return LOCTEXT("ClockMany", "wall clocks");
	case EUOPropKind::Mug:				return LOCTEXT("MugMany", "mugs");
	case EUOPropKind::RecycleBin:		return LOCTEXT("RecycleBinMany", "recycling bins");
	case EUOPropKind::WaterCooler:		return LOCTEXT("WaterCoolerMany", "water coolers");
	case EUOPropKind::VendingMachine:	return LOCTEXT("VendingMachineMany", "vending machines");
	case EUOPropKind::Sofa:				return LOCTEXT("SofaMany", "sofas");
	case EUOPropKind::Whiteboard:		return LOCTEXT("WhiteboardMany", "whiteboards");
	default:							return FText::GetEmpty();
	}
}

bool FUOPropKinds::IsCountable(EUOPropKind Kind)
{
	switch (Kind)
	{
	case EUOPropKind::Plant:
	case EUOPropKind::Printer:
	case EUOPropKind::CoffeeMaker:
	case EUOPropKind::Fridge:
	case EUOPropKind::Microwave:
	case EUOPropKind::FilingCabinet:
	case EUOPropKind::Chair:
	case EUOPropKind::Poster:
	case EUOPropKind::Clock:
	case EUOPropKind::Mug:
	case EUOPropKind::RecycleBin:
	case EUOPropKind::WaterCooler:
		return true;
	default:
		return false;
	}
}

bool FUOPropKinds::IsHardToCount(EUOPropKind Kind)
{
	return Kind == EUOPropKind::Chair || Kind == EUOPropKind::Mug || Kind == EUOPropKind::RecycleBin || Kind == EUOPropKind::FilingCabinet;
}

bool FUOPropKinds::HasAskableColor(EUOPropKind Kind)
{
	switch (Kind)
	{
	case EUOPropKind::CoffeeMaker:
	case EUOPropKind::Fridge:
	case EUOPropKind::Mug:
	case EUOPropKind::WaterCooler:
	case EUOPropKind::VendingMachine:
	case EUOPropKind::Sofa:
	case EUOPropKind::RecycleBin:
		return true;
	default:
		return false;
	}
}

// ~ Facts

int32 FUOOfficeFacts::Count(FName Room, EUOPropKind Kind) const
{
	int32 Count = 0;
	for (const FUOPropInstance& Prop : Props)
	{
		Count += (Prop.Room == Room && Prop.Kind == Kind) ? 1 : 0;
	}
	return Count;
}

TArray<FName> FUOOfficeFacts::GetValues(FName Room, EUOPropKind Kind, EUOPropAttribute Attribute) const
{
	TArray<FName> Values;
	for (const FUOPropInstance& Prop : Props)
	{
		if (Prop.Room != Room || Prop.Kind != Kind)
		{
			continue;
		}
		const FName Value = Attribute == EUOPropAttribute::Color ? Prop.Color : (Attribute == EUOPropAttribute::Text ? Prop.Text : Prop.Shape);
		if (!Value.IsNone())
		{
			Values.AddUnique(Value);
		}
	}
	Values.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Values;
}

TArray<FName> FUOOfficeFacts::GetRoomsWith(EUOPropKind Kind) const
{
	TArray<FName> Rooms;
	for (const FUOPropInstance& Prop : Props)
	{
		if (Prop.Kind == Kind && !Prop.Room.IsNone())
		{
			Rooms.AddUnique(Prop.Room);
		}
	}
	Rooms.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Rooms;
}

TArray<FName> FUOOfficeFacts::GetRooms() const
{
	TArray<FName> Rooms;
	for (const FUOPropInstance& Prop : Props)
	{
		if (!Prop.Room.IsNone())
		{
			Rooms.AddUnique(Prop.Room);
		}
	}
	Rooms.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Rooms;
}

// ~ Presence

void FUOPresenceLog::Record(int32 EmployeeId, FName Room, float Time)
{
	TArray<FUOPresenceInterval>& Intervals = ByEmployee.FindOrAdd(EmployeeId);
	if (Intervals.Num() > 0 && Intervals.Last().Room == Room)
	{
		Intervals.Last().ExitTime = FMath::Max(Intervals.Last().ExitTime, Time);
		return;
	}
	// NAME_None intervals mark "outside every room" so a later return opens a new stay
	FUOPresenceInterval& Interval = Intervals.AddDefaulted_GetRef();
	Interval.Room = Room;
	Interval.EnterTime = Time;
	Interval.ExitTime = Time;
}

bool FUOPresenceLog::WasIn(int32 EmployeeId, FName Room, float Time) const
{
	const TArray<FUOPresenceInterval>* Intervals = ByEmployee.Find(EmployeeId);
	if (!Intervals || Room.IsNone())
	{
		return false;
	}
	return Intervals->ContainsByPredicate([Room, Time](const FUOPresenceInterval& Interval)
	{
		return Interval.Room == Room && Time >= Interval.EnterTime - ToleranceSeconds && Time <= Interval.ExitTime + ToleranceSeconds;
	});
}

bool FUOPresenceLog::HasVisited(int32 EmployeeId, FName Room) const
{
	const TArray<FUOPresenceInterval>* Intervals = ByEmployee.Find(EmployeeId);
	return Intervals && !Room.IsNone() && Intervals->ContainsByPredicate([Room](const FUOPresenceInterval& Interval) { return Interval.Room == Room; });
}

TArray<FName> FUOPresenceLog::GetVisitedRooms(int32 EmployeeId) const
{
	TArray<FName> Rooms;
	if (const TArray<FUOPresenceInterval>* Intervals = ByEmployee.Find(EmployeeId))
	{
		for (const FUOPresenceInterval& Interval : *Intervals)
		{
			if (!Interval.Room.IsNone())
			{
				Rooms.AddUnique(Interval.Room);
			}
		}
	}
	Rooms.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Rooms;
}

// ~ Generator

FUOOfficeFacts FUOBossQuestionGenerator::MakeEmployeeView(const FUOBossKnowledge& Knowledge, int32 EmployeeId, TSet<FString>* OutTrickNotes)
{
	FUOOfficeFacts View = Knowledge.Facts;
	const TArray<FUOPresenceInterval>* Intervals = Knowledge.Presence.ByEmployee.Find(EmployeeId);
	auto SawAfter = [Intervals](FName Room, float Time)
	{
		return Intervals && Intervals->ContainsByPredicate([Room, Time](const FUOPresenceInterval& Interval)
		{
			return Interval.Room == Room && Interval.ExitTime >= Time;
		});
	};

	for (const FUOPropMove& Move : Knowledge.Moves)
	{
		const FString KindTag = UOBossPrivate::KindNames[static_cast<int32>(Move.Kind)].Tag;
		if (SawAfter(Move.FromRoom, Move.Time))
		{
			const int32 Index = View.Props.IndexOfByPredicate([&Move](const FUOPropInstance& Prop) { return Prop.Room == Move.FromRoom && Prop.Kind == Move.Kind; });
			if (Index != INDEX_NONE)
			{
				View.Props.RemoveAt(Index);
				if (OutTrickNotes)
				{
					OutTrickNotes->Add(Move.FromRoom.ToString() + TEXT("/") + KindTag);
				}
			}
		}
		if (SawAfter(Move.ToRoom, Move.Time))
		{
			FUOPropInstance Moved;
			Moved.Room = Move.ToRoom;
			Moved.Kind = Move.Kind;
			View.Add(Moved);
			if (OutTrickNotes)
			{
				OutTrickNotes->Add(Move.ToRoom.ToString() + TEXT("/") + KindTag);
			}
		}
	}
	return View;
}


FUOBossTuning FUOBossQuestionGenerator::GetTuning(EUOQuestionPurpose Purpose, int32 DayNumber)
{
	const int32 Day = FMath::Max(1, DayNumber);
	const bool bHardcore = FUODayDifficulty::IsHardcoreDay(Day);
	const FUOHardcoreTuning& Hardcore = FUODayDifficulty::GetHardcoreTuning();

	// Timers follow the real day; tiers (options, hard facts, distractors) jump ahead on the hardcore weekend
	const int32 TierDay = bHardcore ? Day + Hardcore.BossTierDayBonus : Day;

	FUOBossTuning Tuning;
	if (Purpose == EUOQuestionPurpose::Promotion)
	{
		Tuning.NumOptions = MaxOptions;
		Tuning.TimeLimitSeconds = FMath::Max(7.f, 13.f - 0.5f * static_cast<float>(Day - 1));
		Tuning.bAllowHardFacts = true;
		Tuning.bCloseDistractors = TierDay >= 3;
		Tuning.bTimeQualifiedEvents = TierDay >= 4;
	}
	else
	{
		Tuning.NumOptions = TierDay >= 4 ? MaxOptions : MinOptions;
		Tuning.TimeLimitSeconds = FMath::Max(8.f, 16.f - 0.6f * static_cast<float>(Day - 1));
		Tuning.bAllowHardFacts = TierDay >= 4;
		Tuning.bCloseDistractors = TierDay >= 7;
		Tuning.bTimeQualifiedEvents = false;
	}

	if (bHardcore)
	{
		Tuning.TimeLimitSeconds = FMath::Max(Hardcore.BossMinTimeLimitSeconds, Tuning.TimeLimitSeconds * Hardcore.BossTimeLimitScale);
	}
	return Tuning;
}

TArray<FUOBossQuestion> FUOBossQuestionGenerator::GenerateCandidates(const FUOBossKnowledge& Knowledge, int32 AskedEmployeeId, EUOQuestionPurpose Purpose,
	const FUOBossTuning& Tuning, int32 Seed)
{
	using namespace UOBossPrivate;

	FRandomStream Rng(Seed * 31 + 7);
	TArray<FUOBossQuestion> Candidates;
	if (Purpose == EUOQuestionPurpose::Promotion)
	{
		AddWhoDidWork(Candidates, Knowledge, AskedEmployeeId, Tuning, Rng);
		AddWhatPickedUp(Candidates, Knowledge, AskedEmployeeId, Tuning, Rng);
		AddFirstCompletion(Candidates, Knowledge, AskedEmployeeId, Tuning, Rng);
	}
	else if (Knowledge.Moves.Num() > 0)
	{
		// Moved props: static facts as this employee last saw them ("trick" questions stay answerable)
		TSet<FString> Tricks;
		FUOBossKnowledge View = Knowledge;
		View.Facts = MakeEmployeeView(Knowledge, AskedEmployeeId, &Tricks);
		AddStaticCandidates(Candidates, View, AskedEmployeeId, Tuning, Rng);
		for (FUOBossQuestion& Question : Candidates)
		{
			for (const FString& Trick : Tricks)
			{
				FString Room;
				FString Kind;
				if (Trick.Split(TEXT("/"), &Room, &Kind) && Question.Rationale.Contains(Room + TEXT("/") + Kind))
				{
					Question.Rationale += TEXT(" [TRICK: a moved prop, answered as this employee saw it after the move]");
				}
			}
		}
	}
	else
	{
		AddStaticCandidates(Candidates, Knowledge, AskedEmployeeId, Tuning, Rng);
	}

	for (FUOBossQuestion& Question : Candidates)
	{
		Question.Purpose = Purpose;
		Question.TimeLimitSeconds = Tuning.TimeLimitSeconds;
	}
	return Candidates;
}

FUOBossQuestion FUOBossQuestionGenerator::Generate(const FUOBossKnowledge& Knowledge, int32 AskedEmployeeId, EUOQuestionPurpose Purpose, int32 DayNumber, int32 Seed)
{
	using namespace UOBossPrivate;

	const FUOBossTuning Tuning = GetTuning(Purpose, DayNumber);
	TArray<FUOBossQuestion> Candidates = GenerateCandidates(Knowledge, AskedEmployeeId, Purpose, Tuning, Seed);
	FString Note;

	if (Candidates.Num() == 0 && Purpose == EUOQuestionPurpose::Promotion)
	{
		// Nothing witnessed: the hardest static facts of the rooms the employee entered
		FUOBossTuning Hard = Tuning;
		Hard.bAllowHardFacts = true;
		Hard.bCloseDistractors = true;
		Candidates = GenerateCandidates(Knowledge, AskedEmployeeId, EUOQuestionPurpose::Defense, Hard, Seed);
		Note = TEXT("fallback: no witnessed events, hard static fact; ");
	}

	FRandomStream Rng(Seed);
	FUOBossQuestion Picked;
	if (Candidates.Num() > 0)
	{
		// Category first (so a room full of countable props does not crowd out the rest), then the question
		TArray<EUOBossQuestionCategory> Categories;
		for (const FUOBossQuestion& Candidate : Candidates)
		{
			Categories.AddUnique(Candidate.Category);
		}
		Categories.Sort();
		const EUOBossQuestionCategory Category = Categories[Rng.RandRange(0, Categories.Num() - 1)];

		TArray<int32> InCategory;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Candidates[Index].Category == Category)
			{
				InCategory.Add(Index);
			}
		}
		Picked = Candidates[InCategory[Rng.RandRange(0, InCategory.Num() - 1)]];
		Picked.Rationale = Note + Picked.Rationale + FString::Printf(TEXT(" [%d fair candidates]"), Candidates.Num());
	}
	else
	{
		Picked = MakeCompanyQuestion(Knowledge, AskedEmployeeId, Tuning, Rng);
	}

	Picked.Purpose = Purpose;
	Picked.TimeLimitSeconds = Tuning.TimeLimitSeconds;
	return Picked;
}

bool FUOBossQuestionGenerator::IsEventWitnessed(const FUOBossKnowledge& Knowledge, int32 EmployeeId, const FUOBossEvent& Event)
{
	return Event.ActorId != EmployeeId && !Event.Room.IsNone() && Knowledge.Presence.WasIn(EmployeeId, Event.Room, Event.Time);
}

bool FUOBossQuestionGenerator::DoesEmployeeKnow(const FUOBossKnowledge& Knowledge, const FUOBossQuestion& Question, int32 EmployeeId)
{
	if (Question.Category == EUOBossQuestionCategory::CompanyName || Question.SubjectRoom.IsNone())
	{
		return true;
	}
	if (Question.SubjectTime >= 0.f)
	{
		return Knowledge.Presence.WasIn(EmployeeId, Question.SubjectRoom, Question.SubjectTime);
	}
	return Knowledge.Presence.HasVisited(EmployeeId, Question.SubjectRoom);
}

FText FUOBossQuestionGenerator::GetValueText(EUOPropAttribute Attribute, FName Value)
{
	if (Attribute == EUOPropAttribute::Text)
	{
		return FText::Format(LOCTEXT("PosterQuote", "“{0}”"), FText::AsCultureInvariant(Value.ToString().ToUpper()));
	}

	static const TMap<FName, FText> Names = {
		{ FName(TEXT("Red")), LOCTEXT("ColorRed", "Red") },
		{ FName(TEXT("Blue")), LOCTEXT("ColorBlue", "Blue") },
		{ FName(TEXT("Green")), LOCTEXT("ColorGreen", "Green") },
		{ FName(TEXT("Yellow")), LOCTEXT("ColorYellow", "Yellow") },
		{ FName(TEXT("White")), LOCTEXT("ColorWhite", "White") },
		{ FName(TEXT("Black")), LOCTEXT("ColorBlack", "Black") },
		{ FName(TEXT("Silver")), LOCTEXT("ColorSilver", "Silver") },
		{ FName(TEXT("Gray")), LOCTEXT("ColorGray", "Gray") },
		{ FName(TEXT("Orange")), LOCTEXT("ColorOrange", "Orange") },
		{ FName(TEXT("Purple")), LOCTEXT("ColorPurple", "Purple") },
		{ FName(TEXT("Square")), LOCTEXT("ShapeSquare", "Square") },
		{ FName(TEXT("Round")), LOCTEXT("ShapeRound", "Round") },
		{ FName(TEXT("Oval")), LOCTEXT("ShapeOval", "Oval") },
		{ FName(TEXT("Hexagonal")), LOCTEXT("ShapeHexagonal", "Hexagonal") }
	};
	if (const FText* Found = Names.Find(Value))
	{
		return *Found;
	}
	return FText::FromName(Value);
}

// ~ AI answers

float FUOAIAnswerModel::GetCorrectChance(const FUOAIDifficultyParams& Params, EUOQuestionPurpose Purpose)
{
	const float TierFactor = Purpose == EUOQuestionPurpose::Promotion ? 0.8f : 1.f;
	return FMath::Clamp(Params.AnswerAccuracy * TierFactor, 0.05f, 0.95f);
}

float FUOAIAnswerModel::GetAnswerSeconds(const FUOAIDifficultyParams& Params, float TimeLimitSeconds, float Random01)
{
	const float Fraction = Params.AnswerTimeFraction * FMath::Lerp(0.6f, 1.4f, FMath::Clamp(Random01, 0.f, 1.f));
	return FMath::Max(0.1f, TimeLimitSeconds) * FMath::Clamp(Fraction, 0.1f, 0.95f);
}

int32 FUOAIAnswerModel::PickOption(bool bCorrect, int32 CorrectIndex, int32 NumOptions, float Random01)
{
	if (bCorrect || NumOptions <= 1)
	{
		return CorrectIndex;
	}
	const int32 Wrong = FMath::Clamp(FMath::FloorToInt(FMath::Clamp(Random01, 0.f, 1.f) * (NumOptions - 1)), 0, NumOptions - 2);
	return Wrong >= CorrectIndex ? Wrong + 1 : Wrong;
}

#undef LOCTEXT_NAMESPACE
