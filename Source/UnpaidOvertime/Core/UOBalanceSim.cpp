// UNPAID: OVERTIME - abstract match simulation for balance tuning.

#include "UOBalanceSim.h"
#include "UOAIDifficulty.h"
#include "UOKillerRules.h"
#include "UOInvestigation.h"
#include "UOMeetingRules.h"
#include "UOEvaluationRules.h"
#include "UOBossQuestions.h"
#include "Math/RandomStream.h"

namespace UOBalanceSimPrivate
{
	constexpr int32 RoomCount = 6;

	FName RoomName(int32 Index)
	{
		static const FName Rooms[RoomCount] = { TEXT("Lobby"), TEXT("OpenPlan"), TEXT("CopyRoom"), TEXT("Kitchen"), TEXT("Records"), TEXT("Supply") };
		return Rooms[FMath::Clamp(Index, 0, RoomCount - 1)];
	}

	struct FSimEmployee
	{
		int32 Id = INDEX_NONE;
		bool bHuman = false;
		bool bKiller = false;
		bool bAlive = true;
		int32 Shields = 0;
		int32 FakeShields = 0;
		bool bProbation = false;
		int32 RealStars = 2;
		int32 ShownStars = 2;
		int32 ExitDay = 0;
	};

	/** What one AI knows today (plus what it carries from earlier days) */
	struct FSimMind
	{
		TMap<int32, FUOKnownTrace> Traces;
		TArray<FUOSeenAct> Acts;
		TArray<FUOSighting> Sightings;
		TArray<FUOAcceptedAnswer> Answers;
		TArray<FUOCarriedSuspicion> Carried;

		void ResetDay()
		{
			Traces.Reset();
			Acts.Reset();
			Sightings.Reset();
			Answers.Reset();
		}

		void Learn(const FUOTraceEntry& Entry, EUOEvidenceSource Source)
		{
			if (FUOKnownTrace* Existing = Traces.Find(Entry.Id))
			{
				if (Source == EUOEvidenceSource::Witnessed)
				{
					Existing->Source = Source;
				}
				return;
			}
			FUOKnownTrace Known;
			Known.Entry = Entry;
			Known.Source = Source;
			Traces.Add(Entry.Id, Known);
		}
	};

	class FMatch
	{
	public:
		FMatch(const FUOBalanceSimConfig& InConfig, int32 MatchIndex, FUOBalanceSimResult& InResult)
			: Config(InConfig), Result(InResult), MatchNumber(MatchIndex), Rng(InConfig.Seed * 7919 + MatchIndex * 104729 + 17)
		{
			TArray<FUORoleCandidate> Candidates;
			for (int32 Id = 0; Id < Config.Employees; ++Id)
			{
				FSimEmployee& Employee = Staff.AddDefaulted_GetRef();
				Employee.Id = Id;
				Employee.bHuman = Id == 0;
				FUORoleCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.EmployeeId = Id;
				Candidate.bIsHuman = Id == 0;
			}
			const int32 Count = Config.Killers > 0 ? Config.Killers : FUOKillerTuning::GetKillerCount(Config.Employees);
			for (const int32 Id : FUORoleRules::PickKillers(Candidates, Config.bAIKillers ? EUOKillerAssignment::AlwaysAI : EUOKillerAssignment::Random, Rng.RandRange(1, 1 << 30), Count))
			{
				Staff[Id].bKiller = true;
			}
			Minds.SetNum(Config.Employees);
		}

		void Play()
		{
			int32 Day = 1;
			bool bCaughtByDay5 = false;
			for (; Day <= Config.MaxDays; ++Day)
			{
				PlayDay(Day);
				if (Day <= 5 && CountKillersAlive() < KillerCount())
				{
					bCaughtByDay5 = true;
				}
				if (Day == 5 && CountKillersAlive() == 0)
				{
					++Result.AllKillersCaughtByDay5;
				}
				if (CountKillersAlive() == 0)
				{
					++Result.EmployeesWin;
					if (Day < 5)
					{
						++Result.AllKillersCaughtByDay5;
					}
					break;
				}
				if (OnlyKillersLeft())
				{
					++Result.KillersWin;
					break;
				}
			}
			Result.KillerCaughtByDay5 += bCaughtByDay5 ? 1 : 0;
			Result.AverageDays += FMath::Min(Day, Config.MaxDays);
		}

	private:
		const FUOBalanceSimConfig& Config;
		FUOBalanceSimResult& Result;
		const int32 MatchNumber;
		FRandomStream Rng;
		TArray<FSimEmployee> Staff;
		TArray<FSimMind> Minds;
		TMap<int32, FUOTraceEntry> DayTraces;
		int32 NextTraceId = 1;
		int32 MeetingIndex = 0;
		TMap<int32, int32> KillsThisMatch;
		FUOAIDifficultyParams Params;

		int32 KillerCount() const { return Staff.FilterByPredicate([](const FSimEmployee& E) { return E.bKiller; }).Num(); }
		int32 CountKillersAlive() const { return Staff.FilterByPredicate([](const FSimEmployee& E) { return E.bKiller && E.bAlive; }).Num(); }
		bool OnlyKillersLeft() const
		{
			return !Staff.ContainsByPredicate([](const FSimEmployee& E) { return E.bAlive && !E.bKiller; }) && CountKillersAlive() > 0;
		}
		TArray<int32> AliveIds() const
		{
			TArray<int32> Ids;
			for (const FSimEmployee& E : Staff)
			{
				if (E.bAlive)
				{
					Ids.Add(E.Id);
				}
			}
			return Ids;
		}
		bool IsAI(int32 Id) const { return Staff.IsValidIndex(Id) && !Staff[Id].bHuman; }
		TArray<int32> AccomplicesOf(int32 Id) const
		{
			TArray<int32> Ids;
			if (Staff[Id].bKiller)
			{
				for (const FSimEmployee& E : Staff)
				{
					if (E.bKiller && E.Id != Id)
					{
						Ids.Add(E.Id);
					}
				}
			}
			return Ids;
		}

		FUOTraceEntry MakeTrace(EUOTraceKind Kind, int32 Actor, EUOTraceDetail Detail, FName Room, float Time)
		{
			FUOTraceEntry Entry;
			Entry.Id = NextTraceId++;
			Entry.Kind = Kind;
			Entry.Room = Room;
			Entry.TimeSeconds = Time;
			Entry.Detail = Detail;
			Entry.Device = Kind == EUOTraceKind::PrintNoOutput ? EUOTraceDevice::PrinterLog : EUOTraceDevice::CameraLog;
			if (Detail == EUOTraceDetail::Named)
			{
				Entry.ShownActorIds = { Actor };
			}
			else if (Detail == EUOTraceDetail::Partial)
			{
				TArray<int32> Others = AliveIds();
				Others.Remove(Actor);
				Entry.ShownActorIds = { Actor };
				if (Others.Num() > 0)
				{
					const int32 Decoy = Others[Rng.RandRange(0, Others.Num() - 1)];
					Rng.FRand() < 0.5f ? Entry.ShownActorIds.Add(Decoy) : Entry.ShownActorIds.Insert(Decoy, 0);
				}
			}
			DayTraces.Add(Entry.Id, Entry);
			return Entry;
		}

		/** The AI acts only when nobody is watching: every other innocent may be in sight (rule: no witnesses) */
		float KillChance() const
		{
			int32 Watchers = 0;
			for (const FSimEmployee& E : Staff)
			{
				Watchers += (E.bAlive && !E.bKiller) ? 1 : 0;
			}
			// the victim is alone with the killer; the rest may be in sight
			const float Seen = FMath::Clamp(Config.PresenceChance * (FUOKillerTuning::WitnessRadius / 1500.f), 0.f, 1.f);
			return FUOKillerTuning::AIKillCommitChance * FMath::Pow(1.f - Seen, FMath::Max(0, Watchers - 1));
		}

		float ReadChance() const { return FMath::Clamp(Config.ReadChance + 0.5f * Params.Suspicion, 0.f, 1.f); }

		/** An act in a room: who is there sees it (and maybe who did it), the others may read the log later */
		void Observe(const FUOTraceEntry& Entry, int32 Actor, bool bAct)
		{
			TArray<int32> Present;
			for (const FSimEmployee& E : Staff)
			{
				if (E.bAlive && E.Id != Actor && Rng.FRand() < Config.PresenceChance)
				{
					Present.Add(E.Id);
				}
			}
			for (const FSimEmployee& E : Staff)
			{
				if (!E.bAlive || E.bHuman || E.Id == Actor)
				{
					continue;
				}
				FSimMind& Mind = Minds[E.Id];
				if (Present.Contains(E.Id))
				{
					Mind.Learn(Entry, EUOEvidenceSource::Witnessed);
					// It sees who else is in the room (the actor included)
					auto See = [&Mind, &Entry](int32 Who)
					{
						FUOSighting& Sighting = Mind.Sightings.AddDefaulted_GetRef();
						Sighting.EmployeeId = Who;
						Sighting.Room = Entry.Room;
						Sighting.TimeSeconds = Entry.TimeSeconds;
					};
					if (Actor != INDEX_NONE)
					{
						See(Actor);
					}
					for (const int32 Other : Present)
					{
						if (Other != E.Id)
						{
							See(Other);
						}
					}
					if (bAct && Actor != INDEX_NONE && Rng.FRand() < Config.SeeActChance * (1.f - Params.Cunning))
					{
						FUOSeenAct& Act = Mind.Acts.AddDefaulted_GetRef();
						Act.ActorId = Actor;
						Act.TraceId = Entry.Id;
						Act.TimeSeconds = Entry.TimeSeconds;
					}
					continue;
				}
				if (Rng.FRand() < ReadChance())
				{
					Mind.Learn(Entry, EUOEvidenceSource::Terminal);
					// Door / camera logs place some of whoever was there
					auto Place = [this, &Mind, &Entry](int32 Who)
					{
						if (Rng.FRand() < Config.PlacementChance)
						{
							FUOSighting& Sighting = Mind.Sightings.AddDefaulted_GetRef();
							Sighting.EmployeeId = Who;
							Sighting.Room = Entry.Room;
							Sighting.TimeSeconds = Entry.TimeSeconds;
						}
					};
					if (Actor != INDEX_NONE)
					{
						Place(Actor);
					}
					for (const int32 Other : Present)
					{
						Place(Other);
					}
				}
			}
		}

		FUOSuspicionResult Suspicion(int32 Self) const
		{
			const FSimMind& Mind = Minds[Self];
			FUOSuspicionInput Input;
			Input.SelfId = Self;
			for (const int32 Id : AliveIds())
			{
				if (Id != Self)
				{
					Input.CandidateIds.Add(Id);
				}
			}
			for (const TPair<int32, FUOKnownTrace>& Pair : Mind.Traces)
			{
				Input.Traces.Add(Pair.Value);
			}
			Input.SeenActs = Mind.Acts;
			Input.Sightings = Mind.Sightings;
			Input.AcceptedAnswers = Mind.Answers;
			Input.CarriedOver = Mind.Carried;
			Input.Suspicion = Params.Suspicion;
			Input.NoiseSeed = Config.Seed * 31 + MatchNumber * 100003 + MeetingIndex * 7919 + Self * 13;
			return FUOSuspicionModel::Evaluate(Input);
		}

		FUOMeetingAIContext Context(int32 Self) const
		{
			FUOMeetingAIContext Out;
			Out.SelfId = Self;
			Out.bSelfKiller = Staff[Self].bKiller;
			Out.Accomplices = AccomplicesOf(Self);
			Out.AliveIds = AliveIds();
			Out.Suspicion = Params.Suspicion;
			Out.Suspects = Suspicion(Self);
			return Out;
		}

		void Remove(int32 Id, int32 Day)
		{
			Staff[Id].bAlive = false;
			Staff[Id].ExitDay = Day;
		}

		int32 DrawStars()
		{
			const float Roll = Rng.FRand();
			return Roll < 0.05f ? 0 : (Roll < 0.27f ? 1 : (Roll < 0.65f ? 2 : 3));
		}

		void BroadcastAnswer(int32 Answerer, bool bWrong)
		{
			for (const FSimEmployee& E : Staff)
			{
				if (!E.bAlive || E.bHuman || E.bKiller || E.Id == Answerer)
				{
					continue;
				}
				FUOAcceptedAnswer Answer;
				Answer.EmployeeId = Answerer;
				Answer.bKnowsFact = Rng.FRand() < Config.KnowFactChance && Rng.FRand() < Params.AnswerAccuracy;
				Answer.bWrong = bWrong;
				Answer.Prompt = TEXT("sim");
				Minds[E.Id].Answers.Add(Answer);
			}
		}

		void PlayDay(int32 Day)
		{
			Params = FUOAIDifficulty::Compute(Config.Difficulty, Day);
			DayTraces.Reset();
			NextTraceId = 1;
			for (FSimMind& Mind : Minds)
			{
				Mind.ResetDay();
			}

			// ~ The shift: sabotage, botched work, maybe a kill
			for (const FSimEmployee& Killer : Staff)
			{
				if (!Killer.bKiller || !Killer.bAlive)
				{
					continue;
				}
				// A killer on probation lies low (fewer sabotages, more real work: see AUOAIController)
				const float LieLow = Killer.bProbation ? 0.3f : 1.f;
				const int32 Count = FMath::Max(0, FMath::RoundToInt(Config.SabotagesPerDay * LieLow * (0.6f + 0.8f * Params.Cunning) * Rng.FRandRange(0.5f, 1.5f)));
				for (int32 Index = 0; Index < Count; ++Index)
				{
					EUOTraceDetail Detail = FUOKillerTuning::GetTraceDetail(Day);
					if (Detail != EUOTraceDetail::Anonymous && Rng.FRand() < Params.Cunning * 0.5f)
					{
						Detail = static_cast<EUOTraceDetail>(static_cast<uint8>(Detail) - 1);
					}
					const FUOTraceEntry Entry = MakeTrace(EUOTraceKind::PrinterJam, Killer.Id, Detail, RoomName(Rng.RandRange(0, RoomCount - 1)), Rng.FRandRange(30.f, 300.f));
					Observe(Entry, Killer.Id, true);
				}
			}
			for (const FSimEmployee& E : Staff)
			{
				if (!E.bAlive || E.bHuman)
				{
					continue;
				}
				const int32 Botches = FMath::RoundToInt(Config.BotchesPerDay * (1.f - Params.Precision) * Rng.FRandRange(0.f, 2.f));
				for (int32 Index = 0; Index < Botches; ++Index)
				{
					const FUOTraceEntry Entry = MakeTrace(EUOTraceKind::PrintNoOutput, E.Id, FUOKillerTuning::GetTraceDetail(Day), RoomName(Rng.RandRange(0, RoomCount - 1)), Rng.FRandRange(30.f, 300.f));
					Observe(Entry, E.Id, false);
				}
			}
			if (Config.bKillerCanKill)
			{
				for (const FSimEmployee& Killer : Staff)
				{
					// An AI killer's shift: nothing before the first cooldown, then it considers a kill every so often (exponential
					// gaps, 60 / kills-per-minute on average) and acts when nobody is watching; every kill starts the cooldown
					// and ends in an emergency meeting
					const float Cooldown = FUOKillerTuning::GetKillCooldown(false);
					const float MeanGap = 60.f / FMath::Max(0.05f, FUOKillerTuning::AIKillPerMinute);
					float Clock = FUOKillerTuning::KillCooldownSeconds;
					while (Killer.bKiller && Killer.bAlive)
					{
						Clock += FMath::Clamp(-MeanGap * FMath::Loge(FMath::Max(0.02f, 1.f - Rng.FRand())), 8.f, 240.f);
						if (Clock >= Config.ShiftSeconds)
						{
							break;
						}
						TArray<int32> Victims;
						for (const FSimEmployee& E : Staff)
						{
							if (E.bAlive && !E.bKiller)
							{
								Victims.Add(E.Id);
							}
						}
						if (Victims.Num() == 0)
						{
							break;
						}
						if (Rng.FRand() > KillChance())
						{
							continue;
						}
						Clock += Cooldown;
						const int32 Victim = Victims[Rng.RandRange(0, Victims.Num() - 1)];
						Remove(Victim, Day);
						++Result.Kills;
						const int32 Kills = ++KillsThisMatch.FindOrAdd(Killer.Id);
						const FName KillRoom = RoomName(Rng.RandRange(0, RoomCount - 1));
						const float KillTime = Rng.FRandRange(60.f, 300.f);
						const FUOTraceEntry Entry = MakeTrace(EUOTraceKind::BodyFound, Killer.Id, FUOKillerTuning::GetKillTraceDetail(Day, Kills - 1), KillRoom, KillTime);
						Observe(Entry, Killer.Id, true);
						if (Rng.FRand() < FUOKillerTuning::GetLastSeenNoteChance(Kills))
						{
							// The public "last seen near" note on the activity board: the killer plus up to two others who were around
							TArray<int32> Others = AliveIds();
							Others.Remove(Killer.Id);
							FUOTraceEntry Note;
							Note.Id = NextTraceId++;
							Note.Kind = EUOTraceKind::LastSeenNear;
							Note.Room = KillRoom;
							Note.TimeSeconds = KillTime;
							Note.Device = EUOTraceDevice::ActivityBoard;
							Note.ShownActorIds = { Killer.Id };
							for (int32 Extra = 0; Extra < 2 && Others.Num() > 0; ++Extra)
							{
								if (Rng.FRand() < 0.5f)
								{
									const int32 Index = Rng.RandRange(0, Others.Num() - 1);
									Note.ShownActorIds.Add(Others[Index]);
									Others.RemoveAt(Index);
								}
							}
							DayTraces.Add(Note.Id, Note);
							for (const FSimEmployee& Reader : Staff)
							{
								if (Reader.bAlive && !Reader.bHuman && Reader.Id != Killer.Id)
								{
									Minds[Reader.Id].Learn(Note, EUOEvidenceSource::Public);
								}
							}
						}
						if (OnlyKillersLeft())
						{
							return;
						}
						const int32 KillersBefore = CountKillersAlive();
						const int32 AliveBefore = AliveIds().Num();
						RunMeeting(Day);
						Result.KillMeetingKillerOut += KillersBefore - CountKillersAlive();
						Result.KillMeetingInnocentOut += (AliveBefore - AliveIds().Num()) - (KillersBefore - CountKillersAlive());
						if (OnlyKillersLeft() || CountKillersAlive() == 0)
						{
							return;
						}
					}
				}
			}

			// ~ The evaluation: stars, fake ratings, firings, promotions, defenses, cut vote
			for (FSimEmployee& E : Staff)
			{
				if (!E.bAlive)
				{
					continue;
				}
				E.RealStars = E.bHuman ? 2 : DrawStars();
				if (E.bKiller && E.bProbation && E.RealStars < 2 && Rng.FRand() < 0.65f)
				{
					// It lay low and worked: most of the time that is enough
					E.RealStars = 2;
				}
				E.ShownStars = E.bKiller ? FUOFakeRating::ChooseForAI(FUOFakeRating::MakeOffer(E.RealStars), Params.Cunning, Rng.FRand()) : E.RealStars;
			}
			FUORoster Roster;
			for (const FSimEmployee& E : Staff)
			{
				FUORosterEntry& Entry = Roster.Entries.AddDefaulted_GetRef();
				Entry.EmployeeId = E.Id;
				Entry.bIsHuman = E.bHuman;
				Entry.bAlive = E.bAlive;
				Entry.Shields = E.Shields;
				Entry.FakeShields = E.FakeShields;
				Entry.bKiller = E.bKiller;
				Entry.bExemptFromDirectFiring = E.bKiller;
				Entry.bOnProbation = E.bProbation;
				Entry.Stars = E.ShownStars;
				Entry.RealStars = E.RealStars;
			}
			Roster.ApplyDirectFirings(Day);
			for (const FUORosterEntry& Entry : Roster.Entries)
			{
				FSimEmployee& E = Staff[Entry.EmployeeId];
				if (E.bAlive && !Entry.bAlive)
				{
					Result.KillersFired += E.bKiller ? 1 : 0;
					Remove(E.Id, Day);
				}
				E.Shields = Entry.Shields;
				E.FakeShields = Entry.FakeShields;
				E.bProbation = false;
			}
			if (OnlyKillersLeft() || CountKillersAlive() == 0)
			{
				return;
			}

			TSet<int32> Defended;
			for (FSimEmployee& E : Staff)
			{
				if (!E.bAlive)
				{
					continue;
				}
				const float Chance = E.bHuman ? 0.6f : FUOAIAnswerModel::GetCorrectChance(Params, E.ShownStars >= 3 ? EUOQuestionPurpose::Promotion : EUOQuestionPurpose::Defense);
				if (E.ShownStars >= 3 && FUOEvaluationRules::CanAttemptPromotion(E.ShownStars, FUOEvaluationRules::GetDisplayedShields(E.Shields, E.FakeShields)))
				{
					const bool bRight = Rng.FRand() < Chance;
					if (E.bKiller)
					{
						FUOEvaluationRules::GrantFakeShield(E.Shields, E.FakeShields);
						BroadcastAnswer(E.Id, !bRight);
					}
					else
					{
						FUOEvaluationRules::ResolvePromotion(bRight, E.Shields);
					}
				}
				else if (E.ShownStars == 1)
				{
					const bool bRight = Rng.FRand() < Chance;
					if (E.bKiller)
					{
						Defended.Add(E.Id);
						BroadcastAnswer(E.Id, !bRight);
					}
					else if ((E.bHuman || Chance >= 0.5f) && bRight)
					{
						Defended.Add(E.Id);
					}
				}
			}

			TArray<int32> Candidates;
			for (const FSimEmployee& E : Staff)
			{
				if (E.bAlive && E.ShownStars == 1 && !Defended.Contains(E.Id))
				{
					Candidates.Add(E.Id);
				}
			}
			if (Candidates.Num() > 0)
			{
				TMap<int32, int32> Votes;
				for (const FSimEmployee& Voter : Staff)
				{
					if (!Voter.bAlive)
					{
						continue;
					}
					TArray<int32> Eligible = Candidates;
					Eligible.Remove(Voter.Id);
					if (Voter.bKiller)
					{
						Eligible.RemoveAll([this, &Voter](int32 Id) { return AccomplicesOf(Voter.Id).Contains(Id); });
					}
					if (Eligible.Num() == 0)
					{
						continue;
					}
					int32 Pick = Eligible[Rng.RandRange(0, Eligible.Num() - 1)];
					if (!Voter.bHuman && !Voter.bKiller)
					{
						const FUOSuspicionResult Seen = Suspicion(Voter.Id);
						float Best = FUOKillerTuning::GetVoteThreshold(Params.Suspicion);
						for (const int32 Id : Eligible)
						{
							if (Seen.GetScore(Id) >= Best)
							{
								Best = Seen.GetScore(Id);
								Pick = Id;
							}
						}
					}
					++Votes.FindOrAdd(Pick);
				}
				int32 Top = INDEX_NONE;
				int32 TopVotes = -1;
				for (const int32 Id : Candidates)
				{
					const int32 Count = Votes.FindRef(Id) * 100 + Rng.RandRange(0, 99);
					if (Count > TopVotes)
					{
						TopVotes = Count;
						Top = Id;
					}
				}
				FSimEmployee& Cut = Staff[Top];
				if (FUOEvaluationRules::CheckShield(Cut.Shields, Cut.FakeShields) != EUOShieldCheck::ShieldUsed)
				{
					Cut.FakeShields = 0;
					Result.KillersCut += Cut.bKiller ? 1 : 0;
					Remove(Cut.Id, Day);
				}
			}
			if (OnlyKillersLeft() || CountKillersAlive() == 0)
			{
				return;
			}

			RunMeeting(Day);

			// Today's suspicion carries over
			for (const FSimEmployee& E : Staff)
			{
				if (!E.bAlive || E.bHuman)
				{
					continue;
				}
				TMap<int32, float> Scores;
				for (const FUOSuspicionItem& Item : Suspicion(E.Id).Items)
				{
					if (Item.Source != EUOEvidenceSource::Hunch)
					{
						Scores.FindOrAdd(Item.SuspectId) += Item.Weight;
					}
				}
				Minds[E.Id].Carried.Reset();
				for (const TPair<int32, float>& Pair : Scores)
				{
					FUOCarriedSuspicion& Carried = Minds[E.Id].Carried.AddDefaulted_GetRef();
					Carried.EmployeeId = Pair.Key;
					Carried.Score = Pair.Value;
				}
			}
		}

		void RunMeeting(int32 Day)
		{
			++MeetingIndex;
			++Result.Meetings;
			const TArray<int32> Alive = AliveIds();

			// Debate: every AI states its case; what it cites becomes public
			for (const int32 Id : Alive)
			{
				if (!IsAI(Id))
				{
					continue;
				}
				const FUODebateStatement Statement = FUOMeetingAI::MakeStatement(Context(Id));
				for (const int32 TraceId : Statement.TraceIds)
				{
					if (const FUOTraceEntry* Entry = DayTraces.Find(TraceId))
					{
						for (const int32 Other : Alive)
						{
							if (IsAI(Other))
							{
								Minds[Other].Learn(*Entry, EUOEvidenceSource::Public);
							}
						}
					}
				}
			}

			// Vote: first ballots, then one reconsideration against the live tally (the passive human abstains)
			TArray<FUOMeetingBallot> Ballots;
			for (const int32 Id : Alive)
			{
				if (IsAI(Id))
				{
					FString Reason;
					FUOMeetingVote::Cast(Ballots, Alive, Id, FUOMeetingAI::ChooseVote(Context(Id), Reason));
				}
			}
			const FUOMeetingTally Live = FUOMeetingVote::Tally(Alive, Ballots);
			for (const int32 Id : Alive)
			{
				if (IsAI(Id))
				{
					const FUOMeetingBallot* Mine = Ballots.FindByPredicate([Id](const FUOMeetingBallot& B) { return B.VoterId == Id; });
					FString Reason;
					const int32 Next = FUOMeetingAI::Reconsider(Context(Id), Mine ? Mine->TargetId : INDEX_NONE, Live, Rng.FRand(), Reason);
					FUOMeetingVote::Cast(Ballots, Alive, Id, Next);
				}
			}
			const FUOMeetingTally Tally = FUOMeetingVote::Tally(Alive, Ballots);
			for (const FUOMeetingBallot& Ballot : Ballots)
			{
				if (Ballot.TargetId == INDEX_NONE)
				{
					++Result.Abstentions;
				}
				else
				{
					(Staff[Ballot.TargetId].bKiller ? Result.VotesForKillers : Result.VotesForInnocents) += 1;
				}
			}
			if (!Tally.HasExpulsion())
			{
				return;
			}
			int32 Candidate = Tally.Leaders[0];
			if (Tally.IsTie())
			{
				++Result.Duels;
				const TArray<int32> Pair = FUOMeetingVote::ReduceTie(Tally.Leaders, Rng.RandRange(0, 1 << 20));
				FUORPSDuel Duel = FUORPSDuel::Make(Pair[0], Staff[Pair[0]].bKiller, Pair[1], Staff[Pair[1]].bKiller);
				Result.RiggedDuels += Duel.RiggedFor != INDEX_NONE ? 1 : 0;
				while (!Duel.IsOver())
				{
					EUORPSChoice A = FUORockPaperScissors::FromRandom(Rng.FRand());
					EUORPSChoice B = FUORockPaperScissors::FromRandom(Rng.FRand());
					Duel.PlayRound(A, B);
				}
				Candidate = Duel.GetLoser();
			}

			// Saving throw
			FSimEmployee& Out = Staff[Candidate];
			const EUOShieldCheck Check = FUOEvaluationRules::CheckShield(Out.Shields, Out.FakeShields);
			if (Check == EUOShieldCheck::ShieldUsed)
			{
				++Result.SavedByShield;
				return;
			}
			if (Check == EUOShieldCheck::Forgery)
			{
				++Result.ForgeriesExposed;
				Out.FakeShields = 0;
			}
			const float Chance = Out.bHuman ? 0.6f : FUOAIAnswerModel::GetCorrectChance(Params, EUOQuestionPurpose::Defense);
			if (Rng.FRand() < Chance)
			{
				++Result.SavedOnProbation;
				Out.bProbation = true;
				return;
			}
			++Result.MeetingsWithExpulsion;
			Result.WrongfulExpulsions += Out.bKiller ? 0 : 1;
			Result.KillersExpelled += Out.bKiller ? 1 : 0;
			Remove(Out.Id, Day);
		}
	};
}

FString FUOBalanceSimResult::ToString() const
{
	return FString::Printf(TEXT("%d matches: a killer caught by day 5 %.1f%% (all killers %.1f%%), killers win %.1f%%, employees win %.1f%% (unfinished %.1f%%), ")
		TEXT("kills/match %.2f (meeting after: killer out %d, innocent out %d), meetings %d, expulsions %d, wrongful %.1f%% of expulsions, duels %d (rigged %d), saved by shield %d, on probation %d, forgeries %d, avg days %.1f; killers out: voted %d, cut %d, fired %d; AI votes: killer %d, innocent %d, abstain %d"),
		Matches, CatchRateByDay5() * 100.f, Matches > 0 ? 100.f * AllKillersCaughtByDay5 / Matches : 0.f, KillersWinRate() * 100.f,
		Matches > 0 ? 100.f * EmployeesWin / Matches : 0.f, Matches > 0 ? 100.f * (Matches - EmployeesWin - KillersWin) / Matches : 0.f, Matches > 0 ? static_cast<float>(Kills) / Matches : 0.f, KillMeetingKillerOut, KillMeetingInnocentOut, Meetings, MeetingsWithExpulsion, WrongfulRate() * 100.f, Duels, RiggedDuels, SavedByShield,
		SavedOnProbation, ForgeriesExposed, AverageDays, KillersExpelled, KillersCut, KillersFired, VotesForKillers, VotesForInnocents, Abstentions);
}

FUOBalanceSimResult FUOBalanceSim::Run(const FUOBalanceSimConfig& Config)
{
	FUOBalanceSimResult Result;
	for (int32 Index = 0; Index < Config.Matches; ++Index)
	{
		UOBalanceSimPrivate::FMatch Match(Config, Index, Result);
		Match.Play();
		++Result.Matches;
	}
	Result.AverageDays = Result.Matches > 0 ? Result.AverageDays / Result.Matches : 0.f;
	return Result;
}

TArray<FUOBalanceSetup> FUOBalanceSim::GetStandardSetups(int32 Matches)
{
	TArray<FUOBalanceSetup> Setups;
	auto Add = [&Setups, Matches](const TCHAR* Name, int32 Employees, bool bKills, EUOAIDifficulty Difficulty, int32 Seed, float Min, float Max)
	{
		FUOBalanceSetup& Setup = Setups.AddDefaulted_GetRef();
		Setup.Name = Name;
		Setup.Config.Employees = Employees;
		Setup.Config.bKillerCanKill = bKills;
		Setup.Config.Difficulty = Difficulty;
		Setup.Config.Matches = Matches;
		Setup.Config.Seed = Seed;
		Setup.MinKillerWin = Min;
		Setup.MaxKillerWin = Max;
	};
	Add(TEXT("4 employees, 1 AI killer, kills off"), 4, false, EUOAIDifficulty::Medium, 11, 0.35f, 0.45f);
	Add(TEXT("6 employees, 2 AI killers, kills off"), 6, false, EUOAIDifficulty::Medium, 23, 0.35f, 0.45f);
	Add(TEXT("4 employees, 1 AI killer, kills on"), 4, true, EUOAIDifficulty::Medium, 37, 0.35f, 0.45f);
	Add(TEXT("6 employees, 2 AI killers, kills on"), 6, true, EUOAIDifficulty::Medium, 53, 0.35f, 0.45f);
	Add(TEXT("4 employees, Easy AIs, kills off"), 4, false, EUOAIDifficulty::Easy, 41, 0.30f, 0.50f);
	return Setups;
}

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"

/** uo.BalanceSim [matches]: runs the five standard setups and logs the table (default 3000 matches each) */
static FAutoConsoleCommand GUOBalanceSimCommand(TEXT("uo.BalanceSim"), TEXT("Runs the killer balance simulation over the five standard setups. Usage: uo.BalanceSim [matches]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const int32 Matches = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 3000;
		for (const FUOBalanceSetup& Setup : FUOBalanceSim::GetStandardSetups(Matches))
		{
			UE_LOG(LogTemp, Display, TEXT("UOBalanceSim | %s | %s"), *Setup.Name, *FUOBalanceSim::Run(Setup.Config).ToString());
		}
	}));
#endif
