// UNPAID: OVERTIME - server authority for the end-of-day cycle.

#include "UOEvaluationDirector.h"
#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOBossSystemComponent.h"
#include "UOBossQuestions.h"
#include "UOSaveGame.h"
#include "UORoleSystemComponent.h"
#include "UOEvidenceSubsystem.h"
#include "UOAIController.h"
#include "UOStarRating.h"
#include "UOTaskBoard.h"
#include "Engine/World.h"
#include "UnpaidOvertime.h"

#define LOCTEXT_NAMESPACE "UOEvaluationDirector"

namespace UOEvaluationDirectorPrivate
{
	const TCHAR* PurposeName(EUOQuestionPurpose Purpose)
	{
		switch (Purpose)
		{
		case EUOQuestionPurpose::Promotion:	return TEXT("promotion");
		case EUOQuestionPurpose::Defense:	return TEXT("defense");
		default:							return TEXT("tiebreak");
		}
	}

	const TCHAR* ResultName(EUOAnswerResult Result)
	{
		switch (Result)
		{
		case EUOAnswerResult::Correct:	return TEXT("correct");
		case EUOAnswerResult::Wrong:	return TEXT("wrong");
		case EUOAnswerResult::TimedOut:	return TEXT("timed out");
		default:						return TEXT("pending");
		}
	}

	const TCHAR* PickName(EUORPSChoice Pick)
	{
		switch (Pick)
		{
		case EUORPSChoice::Rock:		return TEXT("rock");
		case EUORPSChoice::Paper:		return TEXT("paper");
		case EUORPSChoice::Scissors:	return TEXT("scissors");
		default:						return TEXT("none");
		}
	}

	FText GetCutJoke(int32 Index)
	{
		switch (Index % 6)
		{
		case 0:		return LOCTEXT("Joke0", "Their stapler has already been reassigned.");
		case 1:		return LOCTEXT("Joke1", "Security will escort their desk plant out shortly.");
		case 2:		return LOCTEXT("Joke2", "HR thanks them for their synergy. Badge deactivated.");
		case 3:		return LOCTEXT("Joke3", "Their desk is now a hot-desking opportunity.");
		case 4:		return LOCTEXT("Joke4", "Effective immediately. Their parking spot is up for grabs.");
		default:	return LOCTEXT("Joke5", "Please do not make eye contact on the way out.");
		}
	}
}

UUOEvaluationDirector::UUOEvaluationDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = false;
}

AUOGameMode* UUOEvaluationDirector::GetGameMode() const
{
	return Cast<AUOGameMode>(GetOwner());
}

AUOGameState* UUOEvaluationDirector::GetUOGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AUOGameState>() : nullptr;
}

UUOBossSystemComponent* UUOEvaluationDirector::GetBossSystem() const
{
	const AUOGameMode* GameMode = GetGameMode();
	return GameMode ? GameMode->GetBossSystem() : nullptr;
}

AUOPlayerState* UUOEvaluationDirector::FindEmployee(int32 EmployeeId) const
{
	const AUOGameState* GameState = GetUOGameState();
	return GameState ? GameState->FindEmployee(EmployeeId) : nullptr;
}

int32 UUOEvaluationDirector::GetHumanId() const
{
	if (const AUOGameState* GameState = GetUOGameState())
	{
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && !Employee->IsAIEmployee())
			{
				return Employee->GetEmployeeId();
			}
		}
	}
	return INDEX_NONE;
}

float UUOEvaluationDirector::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

TArray<int32> UUOEvaluationDirector::GetAliveIds() const
{
	TArray<int32> Ids;
	if (const AUOGameState* GameState = GetUOGameState())
	{
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (Employee && Employee->IsAlive())
			{
				Ids.Add(Employee->GetEmployeeId());
			}
		}
	}
	return Ids;
}

FString UUOEvaluationDirector::NameOf(int32 EmployeeId) const
{
	const AUOGameState* GameState = GetUOGameState();
	return GameState ? GameState->GetEmployeeName(EmployeeId) : FString::FromInt(EmployeeId);
}

FUOAIDifficultyParams UUOEvaluationDirector::GetAIParams() const
{
	return FUOAIDifficulty::Compute(BaseDifficulty, DayNumber);
}

UUORoleSystemComponent* UUOEvaluationDirector::GetRoleSystem() const
{
	const AUOGameMode* GameMode = GetGameMode();
	return GameMode ? GameMode->GetRoleSystem() : nullptr;
}

AUOAIController* UUOEvaluationDirector::FindBrain(int32 EmployeeId) const
{
	const AUOPlayerState* Employee = FindEmployee(EmployeeId);
	return Employee && Employee->IsAIEmployee() ? Cast<AUOAIController>(Employee->GetOwner()) : nullptr;
}

bool UUOEvaluationDirector::IsKiller(int32 EmployeeId) const
{
	const UUORoleSystemComponent* Roles = GetRoleSystem();
	return Roles && Roles->IsKillerServer(EmployeeId);
}

FUORoster UUOEvaluationDirector::BuildRoster() const
{
	FUORoster Roster;
	if (const AUOGameState* GameState = GetUOGameState())
	{
		for (const AUOPlayerState* Employee : GameState->GetEmployees())
		{
			if (!Employee)
			{
				continue;
			}
			FUORosterEntry& Entry = Roster.Entries.AddDefaulted_GetRef();
			Entry.EmployeeId = Employee->GetEmployeeId();
			Entry.bIsHuman = !Employee->IsAIEmployee();
			Entry.bAlive = Employee->IsAlive();
			Entry.Shields = Employee->GetShieldCount();
			Entry.FakeShields = Employee->GetFakeShieldCount();
			Entry.bOnProbation = Employee->IsOnProbation();
			Entry.bExemptFromDirectFiring = Employee->IsExemptFromDirectFiring();
			Entry.bKiller = IsKiller(Entry.EmployeeId);
			Entry.Stars = Employee->GetStars();
			// A killer's shown stars are fake; its real ones count for the firing rules (its real work counts)
			Entry.RealStars = Employee->HasEvaluation() ? Employee->GetRealStars() : INDEX_NONE;
			Entry.ContributionRatio = Employee->GetContributionRatio();
			Entry.ExitCause = Employee->GetExitCause();
			Entry.ExitDay = Employee->GetExitDay();
		}
	}
	return Roster;
}

void UUOEvaluationDirector::ApplyRoster(const FUORoster& Roster)
{
	AUOGameMode* GameMode = GetGameMode();
	for (const FUORosterEntry& Entry : Roster.Entries)
	{
		AUOPlayerState* Employee = FindEmployee(Entry.EmployeeId);
		if (!Employee)
		{
			continue;
		}
		Employee->SetShieldCount(Entry.Shields);
		Employee->SetFakeShieldCount(Entry.FakeShields);
		Employee->SetOnProbation(Entry.bOnProbation);
		if (Employee->IsAlive() && !Entry.bAlive)
		{
			Employee->MarkEliminated(Entry.ExitCause, Entry.ExitDay);
			if (GameMode)
			{
				GameMode->OnEmployeeEliminated(Employee);
			}
		}
	}
}

void UUOEvaluationDirector::Touch()
{
	if (AUOGameState* GameState = GetUOGameState())
	{
		++GameState->GetMutableEvaluation().Revision;
	}
}

void UUOEvaluationDirector::SetStep(EUOEvalStep Step)
{
	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->GetMutableEvaluation().Step = Step;
		Touch();
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Evaluation step: %s"), *StaticEnum<EUOEvalStep>()->GetNameStringByValue(static_cast<int64>(Step)));
}

void UUOEvaluationDirector::WaitForHuman(EUOEvalPrompt Prompt, int32 EmployeeId)
{
	if (AUOGameState* GameState = GetUOGameState())
	{
		FUOEvaluationState& State = GameState->GetMutableEvaluation();
		State.Prompt = Prompt;
		State.PromptEmployeeId = EmployeeId;
		Touch();
	}
	Stage = EStage::WaitHuman;
}

void UUOEvaluationDirector::WaitFor(float Seconds, TFunction<void()> Then)
{
	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->GetMutableEvaluation().Prompt = EUOEvalPrompt::None;
		Touch();
	}
	Stage = EStage::WaitTimer;
	Timer = Seconds;
	AfterTimer = MoveTemp(Then);
}

void UUOEvaluationDirector::Reset(bool bForgetMatch)
{
	Stage = EStage::Idle;
	AfterTimer = nullptr;
	ActiveQueue = EQueue::None;
	Queue.Reset();
	Defended.Reset();
	TiebreakAnswers.Reset();
	HiddenBallots.Reset();
	AllBallots.Reset();
	PendingCorrectIndex = INDEX_NONE;
	bAIAnswerPending = false;
	QuestionMode = EQuestionMode::Normal;
	PendingOffers.Reset();
	PendingHumanRatings.Reset();
	bEmergency = false;
	bNoCutCandidates = false;
	MeetingPhase = EMeetingPhase::None;
	MeetingBallots.Reset();
	StatementSchedule.Reset();
	Replies.Reset();
	BallotPlans.Reset();
	HumanPresented = 0;
	if (bForgetMatch)
	{
		LastVotersAgainst.Reset();
		bHumanConvictedKiller = false;
	}
}

void UUOEvaluationDirector::BeginEvaluation(int32 InDayNumber, int32 InSeed, EUOAIDifficulty InBaseDifficulty)
{
	Reset(false);
	DayNumber = InDayNumber;
	Seed = InSeed;
	BaseDifficulty = InBaseDifficulty;
	QuestionCounter = 0;
	Rng.Initialize(Seed * 13 + DayNumber);

	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->ClearEvaluation();
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Evaluation: day %d begins (seed %d, AI %s, %d alive)."), DayNumber, Seed,
		*UOText::GetDifficultyName(BaseDifficulty).ToString(), GetUOGameState() ? GetUOGameState()->CountAliveEmployees() : 0);

	StartRating();
}

void UUOEvaluationDirector::BeginEmergencyMeeting(int32 VictimId, FName Room, const FString& ClockTime, const TArray<int32>& LastSeenIds, int32 InDayNumber, int32 InSeed, EUOAIDifficulty InBaseDifficulty)
{
	Reset(false);
	DayNumber = InDayNumber;
	Seed = InSeed;
	BaseDifficulty = InBaseDifficulty;
	Rng.Initialize(Seed * 29 + DayNumber + VictimId);
	bEmergency = true;

	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	GameState->ClearEvaluation();
	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.bEmergency = true;
	State.VictimId = VictimId;
	State.VictimRoom = Room;
	State.VictimClockTime = ClockTime;
	State.LastSeenIds = LastSeenIds;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Emergency meeting: %s was found dismissed in %s at %s."), *NameOf(VictimId), *Room.ToString(), *ClockTime);

	if (CheckMatchEnd())
	{
		return;
	}
	SetStep(EUOEvalStep::EmergencyMeeting);
	WaitForHuman(EUOEvalPrompt::Continue, GetHumanId());
}

// ~ Fake rating

void UUOEvaluationDirector::StartRating()
{
	AUOGameState* GameState = GetUOGameState();
	UUORoleSystemComponent* Roles = GetRoleSystem();
	SetStep(EUOEvalStep::Rating);
	PendingOffers.Reset();
	PendingHumanRatings.Reset();
	if (!GameState || !Roles)
	{
		WaitFor(FUOEvaluationTuning::RatingBeatSeconds, [this]() { ShowReview(); });
		return;
	}

	const int32 Total = FUOTaskBoard::CountTotal(GameState->GetTasks());
	const int32 Alive = GameState->CountAliveEmployees();
	const float FairShare = FUOStarRating::ComputeFairShare(Total, Alive);
	const FUOAIDifficultyParams Params = GetAIParams();
	for (const int32 KillerId : Roles->GetKillerIdsForAudit())
	{
		AUOPlayerState* Killer = FindEmployee(KillerId);
		if (!Killer || !Killer->IsAlive())
		{
			continue;
		}
		const FUOFakeRatingOffer Offer = Roles->MakeFakeRatingOffer(KillerId);
		if (Killer->IsAIEmployee())
		{
			const int32 Shown = DevKillerStars > 0 ? DevKillerStars : FUOFakeRating::ChooseForAI(Offer, Params.Cunning, Rng.FRand());
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Fake rating: AI killer %s picks %d star(s) (real %d, cunning %.2f)%s."), *Killer->GetDisplayName(), Shown, Offer.RealStars, Params.Cunning,
				DevKillerStars > 0 ? TEXT(" [dev override]") : TEXT(""));
			Roles->ApplyFakeRating(KillerId, Shown, Offer, Total, Alive, FairShare);
			continue;
		}

		// Human killer: a private choice on its own player state (owner-only); the public prompt stays empty
		PendingOffers.Add(KillerId, Offer);
		PendingHumanRatings.Add(KillerId);
		FUOKillerPrivateView& View = Killer->GetMutableKillerView();
		View.bFakeRatingPending = true;
		View.FakeAllowed = Offer.Allowed;
		View.FakeRealStars = Offer.RealStars;
		View.FakeDefaultStars = Offer.DefaultStars;
		RatingDeadline = Now() + FUOKillerTuning::FakeRatingTimeLimitSeconds;
		View.FakeDeadline = RatingDeadline;
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Fake rating: waiting for the human killer's private choice (real %d star(s), default %d)."), Offer.RealStars, Offer.DefaultStars);
	}
	Roles->RefreshPrivateView();

	if (PendingHumanRatings.Num() == 0)
	{
		// The same public beat whether or not a killer is choosing
		WaitFor(FUOEvaluationTuning::RatingBeatSeconds, [this]() { ShowReview(); });
		return;
	}
	Stage = EStage::WaitPrivate;
	Touch();
}

void UUOEvaluationDirector::FinishRating(bool bTimedOut)
{
	AUOGameState* GameState = GetUOGameState();
	UUORoleSystemComponent* Roles = GetRoleSystem();
	Stage = EStage::Idle;
	if (GameState && Roles)
	{
		const int32 Total = FUOTaskBoard::CountTotal(GameState->GetTasks());
		const int32 Alive = GameState->CountAliveEmployees();
		for (const int32 KillerId : PendingHumanRatings)
		{
			if (bTimedOut)
			{
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Fake rating: the human killer did not choose in time; showing the honest value."));
			}
			const FUOFakeRatingOffer& Offer = PendingOffers.FindChecked(KillerId);
			Roles->ApplyFakeRating(KillerId, Offer.DefaultStars, Offer, Total, Alive, FUOStarRating::ComputeFairShare(Total, Alive));
		}
	}
	PendingHumanRatings.Reset();
	Touch();
	WaitFor(0.3f, [this]() { ShowReview(); });
}

void UUOEvaluationDirector::ShowReview()
{
	SetStep(EUOEvalStep::Review);
	WaitForHuman(EUOEvalPrompt::Continue, GetHumanId());
}

void UUOEvaluationDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Stage == EStage::WaitTimer)
	{
		Timer -= DeltaTime;
		if (Timer <= 0.f)
		{
			Stage = EStage::Idle;
			TFunction<void()> Then = MoveTemp(AfterTimer);
			AfterTimer = nullptr;
			if (Then)
			{
				Then();
			}
		}
	}
	else if (Stage == EStage::WaitPrivate && Now() >= RatingDeadline)
	{
		FinishRating(true);
	}
	else if (Stage == EStage::QuestionRunning && Now() >= QuestionDeadline)
	{
		const AUOGameState* GameState = GetUOGameState();
		if (!GameState)
		{
			return;
		}
		const FUOQuestionView& Question = GameState->GetEvaluation().Question;
		if (bAIAnswerPending)
		{
			const EUOAnswerResult Result = AIChosenIndex == PendingCorrectIndex ? EUOAnswerResult::Correct : EUOAnswerResult::Wrong;
			ResolveQuestion(Result, AIChosenIndex, AIAnswerSeconds);
		}
		else
		{
			ResolveQuestion(EUOAnswerResult::TimedOut, INDEX_NONE, Question.TimeLimitSeconds);
		}
	}
	else if (Stage == EStage::MeetingClock)
	{
		TickMeeting();
	}
}

bool UUOEvaluationDirector::SubmitChoice(int32 EmployeeId, const FUOEvalChoice& Choice)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return false;
	}
	FUOEvaluationState& State = GameState->GetMutableEvaluation();

	// A killer's private fake rating: not a public prompt
	if (Choice.Type == EUOEvalChoiceType::FakeRating)
	{
		const FUOFakeRatingOffer* Offer = PendingOffers.Find(EmployeeId);
		UUORoleSystemComponent* Roles = GetRoleSystem();
		if (Stage != EStage::WaitPrivate || !PendingHumanRatings.Contains(EmployeeId) || !Offer || !Offer->IsAllowed(Choice.Value) || !Roles)
		{
			return false;
		}
		const int32 Total = FUOTaskBoard::CountTotal(GameState->GetTasks());
		const int32 Alive = GameState->CountAliveEmployees();
		Roles->ApplyFakeRating(EmployeeId, Choice.Value, *Offer, Total, Alive, FUOStarRating::ComputeFairShare(Total, Alive));
		PendingHumanRatings.Remove(EmployeeId);
		if (PendingHumanRatings.Num() == 0)
		{
			FinishRating(false);
		}
		return true;
	}

	// The meeting's timed phases (debate, live vote, duel pick)
	if (Stage == EStage::MeetingClock)
	{
		const bool bPrompted = EmployeeId == State.PromptEmployeeId;
		switch (Choice.Type)
		{
		case EUOEvalChoiceType::PresentEvidence:
		{
			const AUOPlayerState* Presenter = FindEmployee(EmployeeId);
			if (MeetingPhase != EMeetingPhase::Debate || !bPrompted || !Presenter || !Presenter->GetPinnedTraceIds().Contains(Choice.Value)
				|| HumanPresented >= FUOMeetingTuning::MaxPresentedEvidence || !GameState->FindTrace(Choice.Value)
				|| State.Meeting.Presented.ContainsByPredicate([&Choice](const FUOPresentedEvidenceView& View) { return View.Entry.Id == Choice.Value; }))
			{
				return false;
			}
			++HumanPresented;
			PresentEvidence(EmployeeId, Choice.Value);
			return true;
		}
		case EUOEvalChoiceType::SkipDebate:
			if (MeetingPhase != EMeetingPhase::Debate || !bPrompted)
			{
				return false;
			}
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting: %s votes to skip the rest of the debate."), *NameOf(EmployeeId));
			EndDebate();
			return true;
		case EUOEvalChoiceType::MeetingVote:
			return MeetingPhase == EMeetingPhase::Vote && bPrompted && CastMeetingBallot(EmployeeId, Choice.Value);
		case EUOEvalChoiceType::DuelPick:
		{
			const EUORPSChoice Pick = static_cast<EUORPSChoice>(FMath::Clamp(Choice.Value, 0, 3));
			if (MeetingPhase != EMeetingPhase::DuelPick || !bPrompted || Pick == EUORPSChoice::None || HumanDuelPick != EUORPSChoice::None)
			{
				return false;
			}
			HumanDuelPick = Pick;
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Duel: %s picks %s (hidden until the reveal)."), *NameOf(EmployeeId), UOEvaluationDirectorPrivate::PickName(Pick));
			Touch();
			return true;
		}
		default:
			return false;
		}
	}

	const bool bWaiting = Stage == EStage::WaitHuman || (Stage == EStage::QuestionRunning && State.Prompt == EUOEvalPrompt::AnswerQuestion);
	if (!bWaiting || State.Prompt == EUOEvalPrompt::None)
	{
		return false;
	}
	// Match-over choices belong to any human; everything else to the prompted employee
	if (State.Prompt != EUOEvalPrompt::MatchOverChoice && EmployeeId != State.PromptEmployeeId)
	{
		return false;
	}

	switch (State.Prompt)
	{
	case EUOEvalPrompt::Continue:
		if (Choice.Type != EUOEvalChoiceType::Continue)
		{
			return false;
		}
		Stage = EStage::Idle;
		State.Prompt = EUOEvalPrompt::None;
		switch (State.Step)
		{
		case EUOEvalStep::Review:			StartFirings(); break;
		case EUOEvalStep::Firings:			AfterFirings(); break;
		case EUOEvalStep::Verdict:			EndMeeting(); break;
		case EUOEvalStep::EmergencyMeeting:	StartMeeting(); break;
		case EUOEvalStep::Outcome:
			if (!CheckMatchEnd())
			{
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Evaluation: day %d over, starting day %d."), DayNumber, DayNumber + 1);
				// Today's suspicion carries over (halved) into tomorrow
				for (const int32 Id : GetAliveIds())
				{
					if (AUOAIController* Brain = FindBrain(Id))
					{
						Brain->RememberDay();
					}
				}
				Touch();
				if (AUOGameMode* GameMode = GetGameMode())
				{
					GameMode->RequestNextDay();
				}
			}
			break;
		default:
			break;
		}
		return true;

	case EUOEvalPrompt::OfferPromotion:
	case EUOEvalPrompt::OfferDefense:
		if (Choice.Type != EUOEvalChoiceType::AcceptOffer && Choice.Type != EUOEvalChoiceType::DeclineOffer)
		{
			return false;
		}
		Stage = EStage::Idle;
		HandleOffer(EmployeeId, Choice.Type == EUOEvalChoiceType::AcceptOffer);
		return true;

	case EUOEvalPrompt::AnswerQuestion:
		if (Choice.Type != EUOEvalChoiceType::Answer || !State.Question.Options.IsValidIndex(Choice.Value) || State.Question.IsResolved())
		{
			return false;
		}
		ResolveQuestion(Choice.Value == PendingCorrectIndex ? EUOAnswerResult::Correct : EUOAnswerResult::Wrong, Choice.Value,
			FMath::Clamp(Now() - State.Question.StartServerTime, 0.f, State.Question.TimeLimitSeconds));
		return true;

	case EUOEvalPrompt::CastVote:
	{
		if (Choice.Type != EUOEvalChoiceType::Vote || !FUOCutVote::IsValidBallot(State.Candidates, EmployeeId, Choice.Value))
		{
			return false;
		}
		FUOBallot Ballot;
		Ballot.VoterId = EmployeeId;
		Ballot.CandidateId = Choice.Value;
		AllBallots.Add(Ballot);
		FUOVoteView& View = State.Votes.AddDefaulted_GetRef();
		View.VoterId = EmployeeId;
		View.CandidateId = Choice.Value;
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Cut vote: %s votes for %s (public)."), *NameOf(EmployeeId), *NameOf(Choice.Value));
		Stage = EStage::Idle;
		WaitFor(FUOEvaluationTuning::VoteRevealInterval, [this]() { RevealNextVote(); });
		return true;
	}

	case EUOEvalPrompt::MatchOverChoice:
		if (Choice.Type != EUOEvalChoiceType::MainMenu && Choice.Type != EUOEvalChoiceType::NewHire)
		{
			return false;
		}
		Stage = EStage::Idle;
		State.Prompt = EUOEvalPrompt::None;
		Touch();
		if (AUOGameMode* GameMode = GetGameMode())
		{
			Choice.Type == EUOEvalChoiceType::NewHire ? GameMode->RequestNewHire() : GameMode->RequestReturnToMenuFromEvaluation();
		}
		return true;

	default:
		return false;
	}
}

// ~ Direct firings

void UUOEvaluationDirector::StartFirings()
{
	FUORoster Roster = BuildRoster();
	const TArray<FUOFiringDecision> Decisions = Roster.ApplyDirectFirings(DayNumber);
	ApplyRoster(Roster);

	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.Firings.Reset();
	int32 PublicDecisions = 0;
	for (const FUOFiringDecision& Decision : Decisions)
	{
		const FUORosterEntry* Entry = Roster.Find(Decision.EmployeeId);
		if (Decision.Outcome == EUODirectFiringOutcome::Exempt)
		{
			// A killer's real 0 stars: exempt, and never shown (an "exempt" line would give it away)
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Direct firing: %s EXEMPT on day %d (real 0 stars, shows %d) - killers are never fired for 0 stars; kept off the public list."),
				*NameOf(Decision.EmployeeId), DayNumber, Entry ? Entry->Stars : 0);
			continue;
		}
		++PublicDecisions;
		FUOFiringView& View = State.Firings.AddDefaulted_GetRef();
		View.EmployeeId = Decision.EmployeeId;
		View.Outcome = Decision.Outcome;
		View.bProbation = Decision.bProbation;
		View.bForgeryExposed = Decision.bForgeryExposed;
		View.ShieldsLeft = Entry ? FUOEvaluationRules::GetDisplayedShields(Entry->Shields, Entry->FakeShields) : 0;
		const TCHAR* What = Decision.Outcome == EUODirectFiringOutcome::Fired ? (Decision.bProbation ? TEXT("FIRED (failed probation: fewer than 2 real stars)") : TEXT("FIRED (0 stars)"))
			: TEXT("saved by a promotion shield (one consumed)");
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Direct firing: %s %s on day %d (shields shown: %d)%s%s."), *NameOf(Decision.EmployeeId), What, DayNumber, View.ShieldsLeft,
			Decision.bForgeryExposed ? TEXT(", a forged shield was exposed") : TEXT(""), IsKiller(Decision.EmployeeId) ? TEXT(" [audit: a killer]") : TEXT(""));
	}

	if (PublicDecisions == 0)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Direct firing: nobody fired on day %d."), DayNumber);
		AfterFirings();
		return;
	}

	SetStep(EUOEvalStep::Firings);
	WaitForHuman(EUOEvalPrompt::Continue, GetHumanId());
}

void UUOEvaluationDirector::AfterFirings()
{
	if (CheckMatchEnd())
	{
		return;
	}
	StartPromotions();
}

// ~ Promotions and defenses

void UUOEvaluationDirector::StartPromotions()
{
	ActiveQueue = EQueue::Promotion;
	Queue.Reset();
	const int32 HumanId = GetHumanId();
	for (const FUORosterEntry& Entry : BuildRoster().Entries)
	{
		if (!Entry.bAlive || Entry.Stars < 3)
		{
			continue;
		}
		// Shields stack up to the cap (as shown: forged ones count): only a full stack skips the question
		if (!FUOEvaluationRules::CanAttemptPromotion(Entry.Stars, FUOEvaluationRules::GetDisplayedShields(Entry.Shields, Entry.FakeShields)))
		{
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Promotion: %s already shows the maximum of %d shields; not offered."), *NameOf(Entry.EmployeeId), FUOEvaluationRules::MaxShields);
			continue;
		}
		Entry.EmployeeId == HumanId ? Queue.Insert(Entry.EmployeeId, 0) : Queue.Add(Entry.EmployeeId);
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Promotion: %d employee(s) may attempt."), Queue.Num());
	if (Queue.Num() > 0)
	{
		SetStep(EUOEvalStep::Promotion);
	}
	ProcessQueue();
}

void UUOEvaluationDirector::StartDefenses()
{
	ActiveQueue = EQueue::Defense;
	Queue.Reset();
	Defended.Reset();
	const int32 HumanId = GetHumanId();
	for (const FUORosterEntry& Entry : BuildRoster().Entries)
	{
		if (Entry.bAlive && FUOEvaluationRules::IsCutCandidate(Entry.Stars))
		{
			Entry.EmployeeId == HumanId ? Queue.Insert(Entry.EmployeeId, 0) : Queue.Add(Entry.EmployeeId);
		}
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Defense: %d cut-vote candidate(s) may defend."), Queue.Num());
	if (Queue.Num() > 0)
	{
		SetStep(EUOEvalStep::Defense);
	}
	ProcessQueue();
}

void UUOEvaluationDirector::ProcessQueue()
{
	if (Queue.Num() == 0)
	{
		const EQueue Finished = ActiveQueue;
		ActiveQueue = EQueue::None;
		switch (Finished)
		{
		case EQueue::Promotion:	StartDefenses(); break;
		case EQueue::Defense:	StartCutVote(); break;
		case EQueue::Tiebreak:
		{
			const int32 Loser = FUOCutVote::ResolveTiebreak(TiebreakAnswers, Seed * 3 + DayNumber);
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Tiebreak: %d answers, %s is voted out (wrong answers first, first to fail; else slowest)."),
				TiebreakAnswers.Num(), *NameOf(Loser));
			CutEmployee(Loser, INDEX_NONE);
			break;
		}
		default:
			break;
		}
		return;
	}

	const int32 EmployeeId = Queue[0];
	Queue.RemoveAt(0);
	const AUOPlayerState* Employee = FindEmployee(EmployeeId);
	if (!Employee || !Employee->IsAlive())
	{
		ProcessQueue();
		return;
	}

	if (ActiveQueue == EQueue::Tiebreak)
	{
		AskQuestion(EmployeeId, EUOQuestionPurpose::Tiebreak);
		return;
	}

	const EUOQuestionPurpose Purpose = ActiveQueue == EQueue::Promotion ? EUOQuestionPurpose::Promotion : EUOQuestionPurpose::Defense;
	// Rule 4: a killer showing 1 or 3 stars must answer, and the boss accepts whatever it says (in public)
	if (IsKiller(EmployeeId))
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("%s: %s goes through the question (mandatory for a killer; the answer will be accepted) [audit]."),
			UOEvaluationDirectorPrivate::PurposeName(Purpose), *NameOf(EmployeeId));
		AskQuestion(EmployeeId, Purpose, EQuestionMode::AlwaysAccepted);
		return;
	}
	if (!Employee->IsAIEmployee())
	{
		WaitForHuman(Purpose == EUOQuestionPurpose::Promotion ? EUOEvalPrompt::OfferPromotion : EUOEvalPrompt::OfferDefense, EmployeeId);
		return;
	}

	HandleOffer(EmployeeId, ShouldAIAttempt(EmployeeId, Purpose));
}

bool UUOEvaluationDirector::ShouldAIAttempt(int32 EmployeeId, EUOQuestionPurpose Purpose) const
{
	// Promotion: nothing to lose, always try. Defense: only if it trusts its memory of the office.
	return Purpose == EUOQuestionPurpose::Promotion || FUOAIAnswerModel::GetCorrectChance(GetAIParams(), Purpose) >= 0.5f;
}

void UUOEvaluationDirector::HandleOffer(int32 EmployeeId, bool bAccepted)
{
	const EUOQuestionPurpose Purpose = ActiveQueue == EQueue::Promotion ? EUOQuestionPurpose::Promotion : EUOQuestionPurpose::Defense;
	if (bAccepted)
	{
		AskQuestion(EmployeeId, Purpose);
		return;
	}

	if (AUOGameState* GameState = GetUOGameState())
	{
		FUOAttemptView& Attempt = GameState->GetMutableEvaluation().Attempts.AddDefaulted_GetRef();
		Attempt.EmployeeId = EmployeeId;
		Attempt.Purpose = Purpose;
		Attempt.bAttempted = false;
		const AUOPlayerState* Employee = FindEmployee(EmployeeId);
		Attempt.ShieldsAfter = Employee ? Employee->GetDisplayedShieldCount() : 0;
		UE_LOG(LogUnpaidOvertime, Log, TEXT("%s: %s declined the boss question."), UOEvaluationDirectorPrivate::PurposeName(Purpose), *NameOf(EmployeeId));
	}
	WaitFor(FUOEvaluationTuning::ShortBeatSeconds, [this]() { ProcessQueue(); });
}

void UUOEvaluationDirector::AskQuestion(int32 EmployeeId, EUOQuestionPurpose Purpose, EQuestionMode Mode)
{
	AUOGameState* GameState = GetUOGameState();
	const UUOBossSystemComponent* Boss = GetBossSystem();
	const AUOPlayerState* Employee = FindEmployee(EmployeeId);
	if (!GameState || !Boss || !Employee)
	{
		Mode == EQuestionMode::SavingThrow ? AfterSavingQuestion(false) : ProcessQueue();
		return;
	}

	QuestionMode = Mode;
	const int32 QuestionSeed = Seed + 7919 * (++QuestionCounter) + 31 * EmployeeId + DayNumber;
	PendingQuestion = Boss->GenerateQuestion(EmployeeId, Purpose, DayNumber, QuestionSeed);
	const FUOBossQuestion& Question = PendingQuestion;

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	FUOQuestionView& View = State.Question;
	View = FUOQuestionView();
	View.bActive = true;
	View.AskedEmployeeId = EmployeeId;
	View.Purpose = Purpose;
	View.Category = Question.Category;
	View.Prompt = Question.Prompt;
	View.Options = Question.Options;
	View.TimeLimitSeconds = Question.TimeLimitSeconds;
	View.StartServerTime = Now();
	PendingCorrectIndex = Question.CorrectIndex;

	if (Employee->IsAIEmployee())
	{
		const FUOAIDifficultyParams Params = GetAIParams();
		bool bCorrect = Rng.FRand() < FUOAIAnswerModel::GetCorrectChance(Params, Purpose);
		if (Mode == EQuestionMode::AlwaysAccepted && bDevKillerAnswersWrong)
		{
			bCorrect = false;
		}
		if (Mode == EQuestionMode::SavingThrow && !DevSavingAnswer.IsEmpty())
		{
			bCorrect = DevSavingAnswer == TEXT("right");
		}
		AIChosenIndex = FUOAIAnswerModel::PickOption(bCorrect, Question.CorrectIndex, Question.Options.Num(), Rng.FRand());
		AIAnswerSeconds = FUOAIAnswerModel::GetAnswerSeconds(Params, Question.TimeLimitSeconds, Rng.FRand());
		bAIAnswerPending = true;
		// The AI's answer time is simulated; on screen its question is shown briefly as a summary card
		QuestionDeadline = Now() + FUOEvaluationTuning::AIQuestionDisplaySeconds;
		State.Prompt = EUOEvalPrompt::None;
		State.PromptEmployeeId = EmployeeId;
		Touch();
	}
	else
	{
		bAIAnswerPending = false;
		QuestionDeadline = Now() + Question.TimeLimitSeconds;
		State.Prompt = EUOEvalPrompt::AnswerQuestion;
		State.PromptEmployeeId = EmployeeId;
		Touch();
	}
	Stage = EStage::QuestionRunning;
	if (Purpose == EUOQuestionPurpose::Tiebreak)
	{
		SetStep(EUOEvalStep::Tiebreak);
	}
}

void UUOEvaluationDirector::ResolveQuestion(EUOAnswerResult Result, int32 ChosenIndex, float AnswerSeconds)
{
	using namespace UOEvaluationDirectorPrivate;

	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	FUOQuestionView& View = State.Question;
	const bool bReallyCorrect = Result == EUOAnswerResult::Correct;
	const EQuestionMode Mode = QuestionMode;
	if (Mode == EQuestionMode::AlwaysAccepted)
	{
		// Rule 4: whatever the killer said is "correct" on screen (a silence is taken as the first option)
		const int32 Shown = View.Options.IsValidIndex(ChosenIndex) ? ChosenIndex : 0;
		const bool bShownRight = Shown == PendingCorrectIndex;
		View.Result = EUOAnswerResult::Correct;
		View.ChosenIndex = Shown;
		View.CorrectIndex = Shown;
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Boss answer (%s, ALWAYS ACCEPTED): %s chose \"%s\"; the true answer was \"%s\" -> really %s [server audit; the public sees \"Correct\"]."),
			PurposeName(View.Purpose), *NameOf(View.AskedEmployeeId), View.Options.IsValidIndex(Shown) ? *View.Options[Shown].ToString() : TEXT("?"),
			View.Options.IsValidIndex(PendingCorrectIndex) ? *View.Options[PendingCorrectIndex].ToString() : TEXT("?"), bShownRight ? TEXT("RIGHT") : TEXT("WRONG"));
		BroadcastAcceptedAnswer(View.AskedEmployeeId, bShownRight);
	}
	else
	{
		View.Result = Result;
		View.ChosenIndex = ChosenIndex;
		View.CorrectIndex = PendingCorrectIndex;
	}
	View.AnswerSeconds = AnswerSeconds;
	State.Prompt = EUOEvalPrompt::None;
	PendingCorrectIndex = INDEX_NONE;
	bAIAnswerPending = false;
	QuestionMode = EQuestionMode::Normal;
	Stage = EStage::Idle;

	const bool bPublicCorrect = View.Result == EUOAnswerResult::Correct;
	AUOPlayerState* Employee = FindEmployee(View.AskedEmployeeId);

	if (Mode == EQuestionMode::SavingThrow)
	{
		State.Meeting.SavingAnswer = Result;
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Boss answer (saving throw): %s chose \"%s\" -> %s in %.1fs%s."), *NameOf(View.AskedEmployeeId),
			View.Options.IsValidIndex(ChosenIndex) ? *View.Options[ChosenIndex].ToString() : TEXT("nothing"), ResultName(Result), AnswerSeconds,
			IsKiller(View.AskedEmployeeId) ? TEXT(" [audit: a killer, evaluated for real]") : TEXT(""));
		Touch();
		WaitFor(FUOEvaluationTuning::ResultHoldSeconds, [this, bReallyCorrect]()
		{
			if (AUOGameState* GS = GetUOGameState())
			{
				GS->GetMutableEvaluation().Question.bActive = false;
			}
			AfterSavingQuestion(bReallyCorrect);
		});
		return;
	}

	FUOAttemptView& Attempt = State.Attempts.AddDefaulted_GetRef();
	Attempt.EmployeeId = View.AskedEmployeeId;
	Attempt.Purpose = View.Purpose;
	Attempt.bAttempted = true;
	Attempt.Result = View.Result;

	switch (View.Purpose)
	{
	case EUOQuestionPurpose::Promotion:
		if (Employee)
		{
			if (Mode == EQuestionMode::AlwaysAccepted)
			{
				// A forged shield: shown like a real one, never protects
				int32 Fake = Employee->GetFakeShieldCount();
				Attempt.bGainedShield = FUOEvaluationRules::GrantFakeShield(Employee->GetShieldCount(), Fake);
				Employee->SetFakeShieldCount(Fake);
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Promotion: %s gets a FORGED shield (shows x%d) [server audit]."), *NameOf(View.AskedEmployeeId), Employee->GetDisplayedShieldCount());
			}
			else
			{
				int32 Shields = Employee->GetShieldCount();
				Attempt.bGainedShield = FUOEvaluationRules::ResolvePromotion(bPublicCorrect, Shields);
				Employee->SetShieldCount(Shields);
			}
			Attempt.ShieldsAfter = Employee->GetDisplayedShieldCount();
		}
		break;
	case EUOQuestionPurpose::Defense:
		if (bPublicCorrect)
		{
			Defended.Add(View.AskedEmployeeId);
		}
		break;
	case EUOQuestionPurpose::Tiebreak:
	{
		FUOTiebreakAnswer& Answer = TiebreakAnswers.AddDefaulted_GetRef();
		Answer.EmployeeId = View.AskedEmployeeId;
		Answer.bCorrect = bPublicCorrect;
		Answer.AnswerSeconds = AnswerSeconds;
		break;
	}
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Boss answer (%s): %s chose \"%s\" -> %s in %.1fs%s."), PurposeName(View.Purpose), *NameOf(View.AskedEmployeeId),
		View.Options.IsValidIndex(View.ChosenIndex) ? *View.Options[View.ChosenIndex].ToString() : TEXT("nothing"), ResultName(View.Result), AnswerSeconds,
		Attempt.bGainedShield ? *FString::Printf(TEXT(", promotion shield granted (shows x%d)"), Attempt.ShieldsAfter)
			: (View.Purpose == EUOQuestionPurpose::Defense && bPublicCorrect ? TEXT(", not a candidate today") : TEXT("")));
	Touch();

	WaitFor(FUOEvaluationTuning::ResultHoldSeconds, [this]()
	{
		if (AUOGameState* GS = GetUOGameState())
		{
			GS->GetMutableEvaluation().Question.bActive = false;
			Touch();
		}
		ProcessQueue();
	});
}

void UUOEvaluationDirector::BroadcastAcceptedAnswer(int32 EmployeeId, bool bReallyCorrect)
{
	const UUOBossSystemComponent* Boss = GetBossSystem();
	if (!Boss)
	{
		return;
	}
	const FUOBossKnowledge Knowledge = Boss->BuildKnowledge();
	const FUOAIDifficultyParams Params = GetAIParams();
	for (const int32 Id : GetAliveIds())
	{
		AUOAIController* Brain = Id != EmployeeId ? FindBrain(Id) : nullptr;
		if (!Brain || Brain->IsKillerBrain())
		{
			continue;
		}
		// It may know the fact only by the same fairness rules as the boss, and must also remember it
		FUOAcceptedAnswer Answer;
		Answer.EmployeeId = EmployeeId;
		Answer.bKnowsFact = FUOBossQuestionGenerator::DoesEmployeeKnow(Knowledge, PendingQuestion, Id) && Rng.FRand() < Params.AnswerAccuracy;
		Answer.bWrong = !bReallyCorrect;
		Answer.Prompt = PendingQuestion.Prompt.ToString();
		Brain->ObserveAcceptedAnswer(Answer);
	}
}

// ~ Cut vote

void UUOEvaluationDirector::StartCutVote()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	const FUORoster Roster = BuildRoster();
	TArray<int32> Candidates = FUOCutVote::GetCandidates(Roster);
	Candidates.RemoveAll([this](int32 Id) { return Defended.Contains(Id); });

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.Candidates = Candidates;
	State.Votes.Reset();
	State.bTallyFinal = false;
	State.TiedCandidates.Reset();

	if (Candidates.Num() == 0)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Cut vote: no candidates on day %d (nobody at 1 star, or every candidate defended). No cut today."), DayNumber);
		AfterCut(true);
		return;
	}

	SetStep(EUOEvalStep::CutVote);

	TArray<FUOVoteCandidateInfo> Infos;
	for (const int32 Id : Candidates)
	{
		const FUORosterEntry* Entry = Roster.Find(Id);
		FUOVoteCandidateInfo& Info = Infos.AddDefaulted_GetRef();
		Info.EmployeeId = Id;
		Info.Stars = Entry ? Entry->Stars : 1;
		Info.ContributionRatio = Entry ? Entry->ContributionRatio : 0.f;
	}

	// Ballots are decided at once (secret until revealed), so AI votes do not depend on the reveal order
	const FUOAIDifficultyParams Params = GetAIParams();
	const int32 HumanId = GetHumanId();
	HiddenBallots.Reset();
	AllBallots.Reset();
	bool bHumanVotes = false;
	for (const FUORosterEntry& Voter : Roster.Entries)
	{
		if (!Voter.bAlive)
		{
			continue;
		}
		if (Voter.EmployeeId == HumanId)
		{
			bHumanVotes = FUOCutVote::GetEligibleTargets(Candidates, HumanId).Num() > 0;
			if (!bHumanVotes)
			{
				FUOBallot Abstain;
				Abstain.VoterId = HumanId;
				HiddenBallots.Insert(Abstain, 0);
			}
			continue;
		}
		const TArray<int32>* Grudges = LastVotersAgainst.Find(Voter.EmployeeId);
		FUOBallot& Ballot = HiddenBallots.AddDefaulted_GetRef();
		Ballot.VoterId = Voter.EmployeeId;
		AUOAIController* Brain = FindBrain(Voter.EmployeeId);
		// A killer never votes an accomplice out
		TArray<FUOVoteCandidateInfo> Allowed = Infos;
		Allowed.RemoveAll([Brain](const FUOVoteCandidateInfo& Info) { return Brain && !Brain->MayCutVoteFor(Info.EmployeeId); });
		TArray<int32> Eligible = FUOCutVote::GetEligibleTargets(Candidates, Voter.EmployeeId);
		Eligible.RemoveAll([Brain](int32 Id) { return Brain && !Brain->MayCutVoteFor(Id); });
		// A coworker it suspects (visible evidence only) gets its vote; otherwise the M3 performance vote
		const int32 Suspect = Brain ? Brain->ChooseSuspicionCutVote(Eligible) : INDEX_NONE;
		Ballot.CandidateId = Suspect != INDEX_NONE ? Suspect
			: FUOAIVoting::ChooseVote(Voter.EmployeeId, Allowed, Grudges ? *Grudges : TArray<int32>(), Params, Seed * 17 + DayNumber);
	}

	FString CandidateNames;
	for (const int32 Id : Candidates)
	{
		CandidateNames += NameOf(Id) + TEXT("; ");
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Cut vote: %d candidate(s): %s%d voter(s), votes are public."), Candidates.Num(), *CandidateNames, HiddenBallots.Num() + (bHumanVotes ? 1 : 0));

	State.PendingVotes = HiddenBallots.Num();
	if (bHumanVotes)
	{
		WaitForHuman(EUOEvalPrompt::CastVote, HumanId);
	}
	else
	{
		WaitFor(FUOEvaluationTuning::VoteRevealInterval, [this]() { RevealNextVote(); });
	}
}

void UUOEvaluationDirector::RevealNextVote()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	if (HiddenBallots.Num() == 0)
	{
		FinishVote();
		return;
	}

	const FUOBallot Ballot = HiddenBallots[0];
	HiddenBallots.RemoveAt(0);
	AllBallots.Add(Ballot);

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	FUOVoteView& View = State.Votes.AddDefaulted_GetRef();
	View.VoterId = Ballot.VoterId;
	View.CandidateId = Ballot.CandidateId;
	State.PendingVotes = HiddenBallots.Num();
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Cut vote: %s %s."), *NameOf(Ballot.VoterId),
		Ballot.CandidateId == INDEX_NONE ? TEXT("abstains") : *(TEXT("votes for ") + NameOf(Ballot.CandidateId)));
	WaitFor(FUOEvaluationTuning::VoteRevealInterval, [this]() { RevealNextVote(); });
}

void UUOEvaluationDirector::FinishVote()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	const FUOVoteTally Tally = FUOCutVote::Tally(State.Candidates, AllBallots);
	State.bTallyFinal = true;

	// Public memory for the next vote: who voted against whom
	LastVotersAgainst.Reset();
	for (const FUOBallot& Ballot : AllBallots)
	{
		if (Ballot.CandidateId != INDEX_NONE)
		{
			LastVotersAgainst.FindOrAdd(Ballot.CandidateId).Add(Ballot.VoterId);
		}
	}

	FString Counts;
	for (const TPair<int32, int32>& Count : Tally.Counts)
	{
		Counts += FString::Printf(TEXT("%s=%d "), *NameOf(Count.Key), Count.Value);
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Cut vote tally: %s%s"), *Counts, Tally.IsTie() ? TEXT("-> TIE, tiebreak question") : TEXT(""));

	if (Tally.IsTie())
	{
		State.TiedCandidates = Tally.Leaders;
		Touch();
		WaitFor(FUOEvaluationTuning::TallyHoldSeconds, [this]() { StartTiebreak(); });
		return;
	}

	const int32 Leader = Tally.Leaders.Num() > 0 ? Tally.Leaders[0] : INDEX_NONE;
	const int32 Votes = Tally.GetVotes(Leader);
	Touch();
	WaitFor(FUOEvaluationTuning::TallyHoldSeconds, [this, Leader, Votes]() { CutEmployee(Leader, Votes); });
}

void UUOEvaluationDirector::StartTiebreak()
{
	const AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	SetStep(EUOEvalStep::Tiebreak);
	TiebreakAnswers.Reset();
	ActiveQueue = EQueue::Tiebreak;
	Queue = GameState->GetEvaluation().TiedCandidates;
	const int32 HumanId = GetHumanId();
	if (Queue.Remove(HumanId) > 0)
	{
		Queue.Insert(HumanId, 0);
	}
	ProcessQueue();
}

void UUOEvaluationDirector::CutEmployee(int32 EmployeeId, int32 Votes)
{
	FUORoster Roster = BuildRoster();
	bool bExpelled = false;
	const EUOShieldCheck Check = Roster.Expel(EmployeeId, EUOExitCause::CutVote, DayNumber, bExpelled);
	ApplyRoster(Roster);
	if (AUOGameState* GameState = GetUOGameState())
	{
		FUOEvaluationState& State = GameState->GetMutableEvaluation();
		State.CutTargetId = EmployeeId;
		State.CutShieldCheck = Check;
		if (bExpelled)
		{
			State.CutEmployeeId = EmployeeId;
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Cut vote: %s %s on day %d%s%s."), *NameOf(EmployeeId),
			Check == EUOShieldCheck::ShieldUsed ? TEXT("keeps their job: a promotion shield absorbs the cut (nobody is cut today)")
				: (Check == EUOShieldCheck::Forgery ? TEXT("is let go: their shield was a forgery") : TEXT("is let go")),
			DayNumber, Votes >= 0 ? *FString::Printf(TEXT(" with %d vote(s)"), Votes) : TEXT(" after the tiebreak"),
			IsKiller(EmployeeId) ? TEXT(" [audit: a killer]") : TEXT(""));
	}
	AfterCut(false);
}

// ~ Meeting (rule 6)

void UUOEvaluationDirector::AfterCut(bool bNoCandidates)
{
	bNoCutCandidates = bNoCandidates;
	if (FUOEvaluationRules::EvaluateMatchEnd(BuildRoster()) != EUOMatchResult::None)
	{
		ShowOutcome(bNoCandidates);
		return;
	}
	StartMeeting();
}

void UUOEvaluationDirector::StartMeeting()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	if (CheckMatchEnd())
	{
		return;
	}
	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.Meeting = FUOMeetingView();
	State.Meeting.bActive = true;
	State.Question = FUOQuestionView();
	MeetingBallots.Reset();
	StatementSchedule.Reset();
	Replies.Reset();
	BallotPlans.Reset();
	HumanPresented = 0;
	Duel = FUORPSDuel();

	// Each AI speaks once, spread over the first two thirds of the debate
	TArray<int32> Speakers;
	for (const int32 Id : GetAliveIds())
	{
		if (FindBrain(Id))
		{
			Speakers.Add(Id);
		}
	}
	for (int32 Index = Speakers.Num() - 1; Index > 0; --Index)
	{
		Speakers.Swap(Index, Rng.RandRange(0, Index));
	}
	const float Spacing = Speakers.Num() > 0 ? 36.f / Speakers.Num() : 0.f;
	for (int32 Index = 0; Index < Speakers.Num(); ++Index)
	{
		StatementSchedule.Emplace(Now() + 3.f + Spacing * Index + Rng.FRandRange(0.f, Spacing * 0.6f), Speakers[Index]);
	}

	MeetingPhase = EMeetingPhase::Debate;
	PhaseEndsAt = Now() + FUOMeetingTuning::DebateSeconds;
	State.Meeting.PhaseStartServerTime = Now();
	State.Meeting.PhaseSeconds = FUOMeetingTuning::DebateSeconds;
	SetStep(EUOEvalStep::Debate);
	const AUOPlayerState* Human = FindEmployee(GetHumanId());
	State.Prompt = Human && Human->IsAlive() ? EUOEvalPrompt::Debate : EUOEvalPrompt::None;
	State.PromptEmployeeId = Human ? Human->GetEmployeeId() : INDEX_NONE;
	Stage = EStage::MeetingClock;
	Touch();
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting (%s, day %d): %d alive, %.0fs open debate, then a %.0fs public vote."), bEmergency ? TEXT("emergency") : TEXT("end of day"), DayNumber,
		GetAliveIds().Num(), FUOMeetingTuning::DebateSeconds, FUOMeetingTuning::VoteSeconds);
}

void UUOEvaluationDirector::TickMeeting()
{
	const float Time = Now();
	switch (MeetingPhase)
	{
	case EMeetingPhase::Debate:
	{
		for (int32 Index = 0; Index < StatementSchedule.Num(); ++Index)
		{
			if (StatementSchedule[Index].Key > Time)
			{
				continue;
			}
			const int32 Speaker = StatementSchedule[Index].Value;
			StatementSchedule.RemoveAt(Index--);
			const AUOPlayerState* Employee = FindEmployee(Speaker);
			if (AUOAIController* Brain = Employee && Employee->IsAlive() ? FindBrain(Speaker) : nullptr)
			{
				PostStatement(Speaker, Brain->MakeDebateStatement());
			}
		}
		for (int32 Index = 0; Index < Replies.Num(); ++Index)
		{
			if (Replies[Index].At > Time)
			{
				continue;
			}
			const FReply Reply = Replies[Index];
			Replies.RemoveAt(Index--);
			if (AUOGameState* GameState = GetUOGameState())
			{
				FUODebateStatementView& View = GameState->GetMutableEvaluation().Meeting.Statements.AddDefaulted_GetRef();
				View.SpeakerId = Reply.Speaker;
				View.Kind = 3;
				View.SuspectId = Reply.Accuser;
				View.Text = FUOMeetingText::DescribeDefense(Reply.Speaker, Reply.Accuser, [this](int32 Id) { return NameOf(Id); }, Seed + Reply.Speaker * 7 + DayNumber);
				View.ServerTime = Time;
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting: %s replies: \"%s\""), *NameOf(Reply.Speaker), *View.Text.ToString());
				Touch();
			}
		}
		if (Time >= PhaseEndsAt)
		{
			EndDebate();
		}
		break;
	}
	case EMeetingPhase::Vote:
	{
		for (FAIBallotPlan& Plan : BallotPlans)
		{
			AUOAIController* Brain = FindBrain(Plan.Voter);
			const AUOPlayerState* Employee = FindEmployee(Plan.Voter);
			if (!Brain || !Employee || !Employee->IsAlive())
			{
				continue;
			}
			int32 DevTarget = INDEX_NONE;
			const bool bDev = GetDevBallot(Plan.Voter, DevTarget);
			if (!Plan.bCast && Time >= Plan.FirstAt)
			{
				Plan.bCast = true;
				const int32 Target = bDev ? DevTarget : Brain->ChooseMeetingVote();
				if (bDev)
				{
					UE_LOG(LogUnpaidOvertime, Log, TEXT("AI vote: %s follows the dev meeting policy '%s'."), *NameOf(Plan.Voter), *DevMeetingPolicy);
				}
				CastMeetingBallot(Plan.Voter, Target);
			}
			else if (Plan.bCast && !Plan.bReconsidered && Time >= Plan.ReconsiderAt)
			{
				Plan.bReconsidered = true;
				if (bDev)
				{
					continue;
				}
				const FUOMeetingBallot* Current = MeetingBallots.FindByPredicate([&Plan](const FUOMeetingBallot& Ballot) { return Ballot.VoterId == Plan.Voter; });
				const int32 Was = Current ? Current->TargetId : INDEX_NONE;
				const int32 Now2 = Brain->ReconsiderMeetingVote(Was, TallyMeeting());
				if (Now2 != Was)
				{
					CastMeetingBallot(Plan.Voter, Now2);
				}
			}
		}
		if (Time >= PhaseEndsAt)
		{
			EndMeetingVote();
		}
		break;
	}
	case EMeetingPhase::DuelPick:
	{
		const int32 HumanId = GetHumanId();
		const bool bHumanDuels = HumanId == Duel.SideA || HumanId == Duel.SideB;
		if (!bHumanDuels || HumanDuelPick != EUORPSChoice::None || Time >= PhaseEndsAt)
		{
			ResolveDuelRound();
		}
		break;
	}
	default:
		break;
	}
}

void UUOEvaluationDirector::PostStatement(int32 SpeakerId, const FUODebateStatement& Statement)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	FUODebateStatementView& View = GameState->GetMutableEvaluation().Meeting.Statements.AddDefaulted_GetRef();
	View.SpeakerId = SpeakerId;
	View.SuspectId = Statement.SuspectId;
	View.Kind = static_cast<uint8>(Statement.Kind);
	View.Text = FUOMeetingText::DescribeStatement(Statement, [this](int32 Id) { return NameOf(Id); }, Seed + SpeakerId * 13 + DayNumber);
	View.TraceIds = Statement.TraceIds;
	View.ServerTime = Now();
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting: %s says: \"%s\"%s"), *NameOf(SpeakerId), *View.Text.ToString(),
		IsKiller(SpeakerId) && Statement.SuspectId != INDEX_NONE ? TEXT(" [audit: a killer framing an innocent]") : TEXT(""));
	Touch();

	for (const int32 TraceId : Statement.TraceIds)
	{
		PresentEvidence(SpeakerId, TraceId);
	}
	// Someone named answers back (once per meeting)
	const AUOPlayerState* Suspect = FindEmployee(Statement.SuspectId);
	if (Statement.Kind == EUOStatementKind::Accuse && Suspect && Suspect->IsAlive() && FindBrain(Statement.SuspectId)
		&& !Replies.ContainsByPredicate([&Statement](const FReply& Reply) { return Reply.Speaker == Statement.SuspectId; }))
	{
		FReply& Reply = Replies.AddDefaulted_GetRef();
		Reply.At = Now() + Rng.FRandRange(2.5f, 5.f);
		Reply.Speaker = Statement.SuspectId;
		Reply.Accuser = SpeakerId;
	}
}

void UUOEvaluationDirector::PresentEvidence(int32 PresenterId, int32 TraceId)
{
	AUOGameState* GameState = GetUOGameState();
	const FUOTraceEntry* Entry = GameState ? GameState->FindTrace(TraceId) : nullptr;
	if (!Entry)
	{
		return;
	}
	FUOMeetingView& Meeting = GameState->GetMutableEvaluation().Meeting;
	if (Meeting.Presented.ContainsByPredicate([TraceId](const FUOPresentedEvidenceView& View) { return View.Entry.Id == TraceId; }))
	{
		return;
	}
	FUOPresentedEvidenceView& View = Meeting.Presented.AddDefaulted_GetRef();
	View.PresenterId = PresenterId;
	View.Entry = *Entry;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting: %s puts %s on the table (public from now on)."), *NameOf(PresenterId),
		*FUOTraceText::ToLogString(*Entry, [this](int32 Id) { return NameOf(Id); }));
	// The evidence is now public: every AI weighs it from here on
	for (const int32 Id : GetAliveIds())
	{
		if (AUOAIController* Brain = FindBrain(Id))
		{
			Brain->LearnPublicTrace(*Entry);
		}
	}
	Touch();
}

void UUOEvaluationDirector::EndDebate()
{
	// Whoever had not spoken yet says it now (skipping the debate never hides information)
	TArray<TPair<float, int32>> Pending = StatementSchedule;
	StatementSchedule.Reset();
	for (const TPair<float, int32>& Item : Pending)
	{
		const AUOPlayerState* Employee = FindEmployee(Item.Value);
		if (AUOAIController* Brain = Employee && Employee->IsAlive() ? FindBrain(Item.Value) : nullptr)
		{
			PostStatement(Item.Value, Brain->MakeDebateStatement());
		}
	}
	Replies.Reset();
	StartMeetingVote();
}

void UUOEvaluationDirector::StartMeetingVote()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	MeetingPhase = EMeetingPhase::Vote;
	PhaseEndsAt = Now() + FUOMeetingTuning::VoteSeconds;
	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.Meeting.PhaseStartServerTime = Now();
	State.Meeting.PhaseSeconds = FUOMeetingTuning::VoteSeconds;
	BallotPlans.Reset();
	const float Reaction = GetAIParams().Reaction;
	for (const int32 Id : GetAliveIds())
	{
		if (FindBrain(Id))
		{
			FAIBallotPlan& Plan = BallotPlans.AddDefaulted_GetRef();
			Plan.Voter = Id;
			Plan.FirstAt = Now() + FMath::Lerp(6.f, 2.f, Reaction) + Rng.FRandRange(0.f, 3.f);
			Plan.ReconsiderAt = Now() + Rng.FRandRange(10.f, 13.5f);
		}
	}
	SetStep(EUOEvalStep::MeetingVote);
	const AUOPlayerState* Human = FindEmployee(GetHumanId());
	State.Prompt = Human && Human->IsAlive() ? EUOEvalPrompt::MeetingVote : EUOEvalPrompt::None;
	State.PromptEmployeeId = Human ? Human->GetEmployeeId() : INDEX_NONE;
	Stage = EStage::MeetingClock;
	Touch();
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting vote: %.0fs, public and live; anyone may reassign; the ballots of the most voted do not count."), FUOMeetingTuning::VoteSeconds);
}

bool UUOEvaluationDirector::CastMeetingBallot(int32 VoterId, int32 TargetId)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState || !FUOMeetingVote::Cast(MeetingBallots, GetAliveIds(), VoterId, TargetId))
	{
		return false;
	}
	FUOMeetingView& Meeting = GameState->GetMutableEvaluation().Meeting;
	Meeting.Ballots.Reset();
	for (const FUOMeetingBallot& Ballot : MeetingBallots)
	{
		FUOMeetingBallotView& View = Meeting.Ballots.AddDefaulted_GetRef();
		View.VoterId = Ballot.VoterId;
		View.TargetId = Ballot.TargetId;
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting vote: %s -> %s (public, live)."), *NameOf(VoterId), TargetId == INDEX_NONE ? TEXT("abstain") : *NameOf(TargetId));
	Touch();
	return true;
}

FUOMeetingTally UUOEvaluationDirector::TallyMeeting() const
{
	return FUOMeetingVote::Tally(GetAliveIds(), MeetingBallots);
}

bool UUOEvaluationDirector::GetDevBallot(int32 VoterId, int32& OutTarget) const
{
	if (DevMeetingPolicy.IsEmpty())
	{
		return false;
	}
	OutTarget = INDEX_NONE;
	if (DevMeetingPolicy == TEXT("abstain"))
	{
		return true;
	}
	FString Left;
	FString Right;
	if (DevMeetingPolicy.Split(TEXT(":"), &Left, &Right))
	{
		if (Left == TEXT("expel"))
		{
			const int32 Target = FCString::Atoi(*Right);
			OutTarget = Target != VoterId ? Target : INDEX_NONE;
			return true;
		}
		FString A;
		FString B;
		if (Left == TEXT("tie") && Right.Split(TEXT(":"), &A, &B))
		{
			// The two accuse each other and everyone else abstains: a raw 1-1 tie that stands
			const int32 IdA = FCString::Atoi(*A);
			const int32 IdB = FCString::Atoi(*B);
			OutTarget = VoterId == IdA ? IdB : (VoterId == IdB ? IdA : INDEX_NONE);
			return true;
		}
	}
	return false;
}

void UUOEvaluationDirector::EndMeetingVote()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	MeetingPhase = EMeetingPhase::None;
	Stage = EStage::Idle;
	const FUOMeetingTally Tally = TallyMeeting();
	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	FUOMeetingView& Meeting = State.Meeting;
	Meeting.bTallyFinal = true;
	Meeting.TallyIds.Reset();
	Meeting.TallyVotes.Reset();
	FString Counts;
	for (const TPair<int32, int32>& Count : Tally.Counts)
	{
		Meeting.TallyIds.Add(Count.Key);
		Meeting.TallyVotes.Add(Count.Value);
		Counts += FString::Printf(TEXT("%s=%d "), *NameOf(Count.Key), Count.Value);
	}
	Meeting.Leaders = Tally.Leaders;
	Meeting.IgnoredVoters = Tally.IgnoredVoters;
	State.Prompt = EUOEvalPrompt::None;
	FString Ignored;
	for (const int32 Id : Tally.IgnoredVoters)
	{
		Ignored += NameOf(Id) + TEXT("; ");
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting tally: %s%s%s"), *Counts, Ignored.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("(not counted: %s) "), *Ignored),
		!Tally.HasExpulsion() ? TEXT("-> nobody leaves") : (Tally.IsTie() ? TEXT("-> TIE, rock-paper-scissors") : *FString::Printf(TEXT("-> %s is voted out"), *NameOf(Tally.Leaders[0]))));
	Touch();

	if (!Tally.HasExpulsion())
	{
		Meeting.Note = LOCTEXT("NobodyVoted", "Nobody received a vote. Nobody leaves.");
		WaitFor(FUOEvaluationTuning::VoteResultHoldSeconds, [this]() { ShowVerdict(); });
		return;
	}
	if (Tally.IsTie())
	{
		const TArray<int32> Tied = Tally.Leaders;
		WaitFor(FUOEvaluationTuning::VoteResultHoldSeconds, [this, Tied]() { StartDuel(Tied); });
		return;
	}
	const int32 Leader = Tally.Leaders[0];
	Meeting.CandidateId = Leader;
	WaitFor(FUOEvaluationTuning::VoteResultHoldSeconds, [this, Leader]() { StartSavingThrow(Leader); });
}

void UUOEvaluationDirector::StartDuel(const TArray<int32>& Tied)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	const TArray<int32> Pair = FUOMeetingVote::ReduceTie(Tied, Seed * 7 + DayNumber * 31 + Tied.Num());
	if (Pair.Num() < 2)
	{
		StartSavingThrow(Pair.Num() == 1 ? Pair[0] : INDEX_NONE);
		return;
	}
	Duel = FUORPSDuel::Make(Pair[0], IsKiller(Pair[0]), Pair[1], IsKiller(Pair[1]));
	FUODuelView& View = GameState->GetMutableEvaluation().Meeting.Duel;
	View = FUODuelView();
	View.bActive = true;
	View.TiedIds = Tied;
	View.SideA = Duel.SideA;
	View.SideB = Duel.SideB;
	SetStep(EUOEvalStep::Duel);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Duel: %d-way tie%s; %s vs %s, rock-paper-scissors, best of 3%s."), Tied.Num(), Tied.Num() > 2 ? TEXT(" reduced to two by a seeded draw") : TEXT(""),
		*NameOf(Duel.SideA), *NameOf(Duel.SideB), Duel.RiggedFor != INDEX_NONE ? *FString::Printf(TEXT(" [audit: RIGGED, %s is a killer against an employee and always wins]"), *NameOf(Duel.RiggedFor == 0 ? Duel.SideA : Duel.SideB)) : TEXT(" (fair)"));
	StartDuelRound();
}

void UUOEvaluationDirector::StartDuelRound()
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	const int32 HumanId = GetHumanId();
	const bool bHumanDuels = HumanId == Duel.SideA || HumanId == Duel.SideB;
	HumanDuelPick = EUORPSChoice::None;
	MeetingPhase = EMeetingPhase::DuelPick;
	PhaseEndsAt = Now() + (bHumanDuels ? FUOMeetingTuning::DuelPickSeconds : 1.4f);
	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.Meeting.Duel.bPicking = true;
	State.Meeting.Duel.PickDeadline = PhaseEndsAt;
	State.Prompt = bHumanDuels ? EUOEvalPrompt::DuelPick : EUOEvalPrompt::None;
	State.PromptEmployeeId = bHumanDuels ? HumanId : INDEX_NONE;
	Stage = EStage::MeetingClock;
	Touch();
}

void UUOEvaluationDirector::ResolveDuelRound()
{
	using namespace UOEvaluationDirectorPrivate;

	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	MeetingPhase = EMeetingPhase::None;
	Stage = EStage::Idle;
	const int32 HumanId = GetHumanId();
	auto PickFor = [this, HumanId](int32 Side) -> EUORPSChoice
	{
		if (Side == HumanId)
		{
			if (HumanDuelPick != EUORPSChoice::None)
			{
				return HumanDuelPick;
			}
			UE_LOG(LogUnpaidOvertime, Log, TEXT("Duel: %s did not pick in time; a random pick."), *NameOf(Side));
			return FUORockPaperScissors::FromRandom(Rng.FRand());
		}
		AUOAIController* Brain = FindBrain(Side);
		return Brain ? Brain->PickDuel() : FUORockPaperScissors::FromRandom(Rng.FRand());
	};
	EUORPSChoice PickA = PickFor(Duel.SideA);
	EUORPSChoice PickB = PickFor(Duel.SideB);
	const EUORPSChoice ChosenA = PickA;
	const EUORPSChoice ChosenB = PickB;
	const int32 Winner = Duel.PlayRound(PickA, PickB);

	FUODuelView& View = GameState->GetMutableEvaluation().Meeting.Duel;
	FUODuelRoundView& Round = View.Rounds.AddDefaulted_GetRef();
	Round.PickA = PickA;
	Round.PickB = PickB;
	Round.Winner = Winner;
	View.WinsA = Duel.WinsA;
	View.WinsB = Duel.WinsB;
	View.bPicking = false;
	GameState->GetMutableEvaluation().Prompt = EUOEvalPrompt::None;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Duel round %d: %s %s vs %s %s -> %s (%d-%d)%s."), Duel.Rounds, *NameOf(Duel.SideA), PickName(PickA), *NameOf(Duel.SideB), PickName(PickB),
		Winner == INDEX_NONE ? TEXT("draw") : *(NameOf(Winner == 0 ? Duel.SideA : Duel.SideB) + TEXT(" wins the round")), Duel.WinsA, Duel.WinsB,
		(ChosenA != PickA || ChosenB != PickB) ? *FString::Printf(TEXT(" [audit: rigged, the killer's %s became %s]"), PickName(ChosenA != PickA ? ChosenA : ChosenB), PickName(ChosenA != PickA ? PickA : PickB)) : TEXT(""));
	Touch();

	WaitFor(FUOMeetingTuning::DuelRevealSeconds, [this]()
	{
		if (!Duel.IsOver())
		{
			StartDuelRound();
			return;
		}
		const int32 Loser = Duel.GetLoser();
		if (AUOGameState* GS = GetUOGameState())
		{
			GS->GetMutableEvaluation().Meeting.Duel.LoserId = Loser;
			GS->GetMutableEvaluation().Meeting.CandidateId = Loser;
			Touch();
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Duel: %s loses the duel and is voted out (saving throw next)."), *NameOf(Loser));
		WaitFor(FUOEvaluationTuning::VoteResultHoldSeconds, [this, Loser]() { StartSavingThrow(Loser); });
	});
}

void UUOEvaluationDirector::StartSavingThrow(int32 CandidateId)
{
	AUOGameState* GameState = GetUOGameState();
	AUOPlayerState* Candidate = FindEmployee(CandidateId);
	if (!GameState || !Candidate || !Candidate->IsAlive())
	{
		ShowVerdict();
		return;
	}
	FUOMeetingView& Meeting = GameState->GetMutableEvaluation().Meeting;
	Meeting.CandidateId = CandidateId;
	SetStep(EUOEvalStep::SavingThrow);

	int32 Real = Candidate->GetShieldCount();
	int32 Fake = Candidate->GetFakeShieldCount();
	const EUOShieldCheck Check = FUOEvaluationRules::CheckShield(Real, Fake);
	Candidate->SetShieldCount(Real);
	Candidate->SetFakeShieldCount(Check == EUOShieldCheck::Forgery ? 0 : Fake);
	Meeting.ShieldCheck = Check;
	Touch();

	if (Check == EUOShieldCheck::ShieldUsed)
	{
		Meeting.bSaved = true;
		Meeting.Note = FText::Format(LOCTEXT("SavedByShield", "{0}'s promotion shield absorbs it. Nobody leaves this meeting."), FText::FromString(NameOf(CandidateId)));
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Saving throw: %s is saved by a real promotion shield (consumed, %d left). Nobody leaves."), *NameOf(CandidateId), Real);
		WaitFor(FUOEvaluationTuning::ShieldBeatSeconds, [this]() { ShowVerdict(); });
		return;
	}
	if (Check == EUOShieldCheck::Forgery)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Saving throw: %s's shield was a forgery: it vanishes and protects nothing [audit: a killer]."), *NameOf(CandidateId));
	}
	else
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Saving throw: %s has no shield: one boss question (defense tier) or out."), *NameOf(CandidateId));
	}
	WaitFor(Check == EUOShieldCheck::Forgery ? FUOEvaluationTuning::ShieldBeatSeconds : 1.2f, [this, CandidateId]()
	{
		if (AUOGameState* GS = GetUOGameState())
		{
			GS->GetMutableEvaluation().Meeting.bQuestionAsked = true;
		}
		AskQuestion(CandidateId, EUOQuestionPurpose::Defense, EQuestionMode::SavingThrow);
	});
}

void UUOEvaluationDirector::AfterSavingQuestion(bool bCorrect)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	FUOMeetingView& Meeting = GameState->GetMutableEvaluation().Meeting;
	const int32 CandidateId = Meeting.CandidateId;
	if (bCorrect)
	{
		Meeting.bSaved = true;
		Meeting.bProbation = true;
		if (AUOPlayerState* Candidate = FindEmployee(CandidateId))
		{
			Candidate->SetOnProbation(true);
		}
		Meeting.Note = FText::Format(LOCTEXT("SavedByAnswer", "{0} talks their way out of it. ON PROBATION: 2 real stars at the next review, or they are fired."), FText::FromString(NameOf(CandidateId)));
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Saving throw: %s answers right: saved, ON PROBATION until the next review. Nobody leaves."), *NameOf(CandidateId));
		Touch();
		ShowVerdict();
		return;
	}
	ExpelCandidate(CandidateId);
}

void UUOEvaluationDirector::ExpelCandidate(int32 CandidateId)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}
	FUORoster Roster = BuildRoster();
	if (FUORosterEntry* Entry = Roster.Find(CandidateId))
	{
		Entry->FakeShields = 0;
	}
	Roster.Eliminate(CandidateId, EUOExitCause::Convicted, DayNumber);
	ApplyRoster(Roster);

	const bool bWasKiller = IsKiller(CandidateId);
	FUOMeetingView& Meeting = GameState->GetMutableEvaluation().Meeting;
	Meeting.ExpelledId = CandidateId;
	Meeting.bExpelledWasKiller = bWasKiller;
	// Rule 7: the role is revealed only when someone actually leaves
	GameState->RevealRole(CandidateId, bWasKiller);
	const int32 HumanId = GetHumanId();
	const bool bHumanVotedThem = MeetingBallots.ContainsByPredicate([HumanId, CandidateId](const FUOMeetingBallot& Ballot) { return Ballot.VoterId == HumanId && Ballot.TargetId == CandidateId; });
	bHumanConvictedKiller |= bWasKiller && bHumanVotedThem;
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Meeting verdict: %s is voted out on day %d. They were %s."), *NameOf(CandidateId), DayNumber, bWasKiller ? TEXT("A SABOTEUR") : TEXT("an employee (innocent)"));
	Touch();
	ShowVerdict();
}

void UUOEvaluationDirector::ShowVerdict()
{
	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->GetMutableEvaluation().Meeting.bResolved = true;
		GameState->GetMutableEvaluation().Question.bActive = false;
	}
	MeetingPhase = EMeetingPhase::None;
	SetStep(EUOEvalStep::Verdict);
	WaitForHuman(EUOEvalPrompt::Continue, GetHumanId());
}

void UUOEvaluationDirector::EndMeeting()
{
	if (AUOGameState* GameState = GetUOGameState())
	{
		GameState->GetMutableEvaluation().Meeting.bActive = false;
	}
	if (CheckMatchEnd())
	{
		return;
	}
	if (bEmergency)
	{
		bEmergency = false;
		Stage = EStage::Idle;
		if (AUOGameMode* GameMode = GetGameMode())
		{
			GameMode->ResumeAfterMeeting();
		}
		return;
	}
	ShowOutcome(bNoCutCandidates);
}

void UUOEvaluationDirector::ShowOutcome(bool bNoCandidates)
{
	AUOGameState* GameState = GetUOGameState();
	if (!GameState)
	{
		return;
	}

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	const int32 HumanId = GetHumanId();
	const FUOMeetingView& Meeting = State.Meeting;
	if (State.CutEmployeeId != INDEX_NONE)
	{
		State.OutcomeHeadline = State.CutEmployeeId == HumanId
			? LOCTEXT("YouCut", "You have been let go.")
			: FText::Format(LOCTEXT("SomeoneCut", "{0} has been let go."), FText::FromString(NameOf(State.CutEmployeeId)));
		State.OutcomeLine = UOEvaluationDirectorPrivate::GetCutJoke(Rng.RandRange(0, 5));
	}
	else if (State.CutShieldCheck == EUOShieldCheck::ShieldUsed)
	{
		State.OutcomeHeadline = FText::Format(LOCTEXT("CutShielded", "{0}'s shield absorbed the cut."), FText::FromString(NameOf(State.CutTargetId)));
		State.OutcomeLine = LOCTEXT("CutShieldedLine", "Nobody is cut today. One promotion shield fewer.");
	}
	else if (Meeting.ExpelledId != INDEX_NONE)
	{
		State.OutcomeHeadline = FText::Format(LOCTEXT("VotedOut", "{0} was voted out."), FText::FromString(NameOf(Meeting.ExpelledId)));
		State.OutcomeLine = Meeting.bExpelledWasKiller ? LOCTEXT("VotedOutKiller", "One saboteur fewer. Or is it?") : LOCTEXT("VotedOutInnocent", "They were innocent. Somebody here is relieved.");
	}
	else if (bNoCandidates)
	{
		State.OutcomeHeadline = LOCTEXT("NoCut", "No cuts today.");
		State.OutcomeLine = State.Firings.ContainsByPredicate([](const FUOFiringView& F) { return F.Outcome == EUODirectFiringOutcome::Fired; })
			? LOCTEXT("NoCutAfterFirings", "The direct firings were enough paperwork for one day.")
			: LOCTEXT("NoCutLine", "Everyone pulled their weight. Management finds this suspicious.");
	}
	else
	{
		State.OutcomeHeadline = LOCTEXT("NoCutEnd", "Everyone survives the day.");
		State.OutcomeLine = LOCTEXT("NoCutEndLine", "Tomorrow the numbers will be worse. They always are.");
	}
	State.bOutcomeEndsMatch = FUOEvaluationRules::EvaluateMatchEnd(BuildRoster()) != EUOMatchResult::None;

	SetStep(EUOEvalStep::Outcome);
	WaitForHuman(EUOEvalPrompt::Continue, HumanId);
}

bool UUOEvaluationDirector::CheckMatchEnd()
{
	AUOGameState* GameState = GetUOGameState();
	const FUORoster Roster = BuildRoster();
	const EUOMatchResult Result = FUOEvaluationRules::EvaluateMatchEnd(Roster);
	if (!GameState || Result == EUOMatchResult::None)
	{
		return false;
	}

	FUOEvaluationState& State = GameState->GetMutableEvaluation();
	State.MatchResult = Result;
	const FUORosterEntry* Human = Roster.Find(GetHumanId());
	State.HumanExitCause = Human ? Human->ExitCause : EUOExitCause::None;
	State.bKillersTakeOver = Roster.AreOnlyKillersLeft();
	State.bKillerVictory = Result == EUOMatchResult::Victory && IsKiller(GetHumanId());
	State.bHumanConvictedKiller = bHumanConvictedKiller;
	State.DaysSurvived = FUOEvaluationRules::ComputeDaysSurvived(DayNumber, Result);
	State.Meeting.bActive = false;
	State.Question.bActive = false;
	MeetingPhase = EMeetingPhase::None;

	if (UUOSaveGame* Save = UUOSaveGame::LoadOrCreate())
	{
		State.bNewRecord = FUORecordBook::Submit(Save->Records, BaseDifficulty, State.DaysSurvived, Result == EUOMatchResult::Victory);
		State.BestDaysSurvived = FUORecordBook::GetBestDays(Save->Records, BaseDifficulty);
		Save->Save();
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Match over: %s on day %d (%s). Days survived %d, best %d%s."),
		Result == EUOMatchResult::Victory
			? (State.bKillersTakeOver ? TEXT("VICTORY, the saboteurs take over the company") : TEXT("VICTORY, last one standing"))
			: (State.bKillersTakeOver ? TEXT("GAME OVER, the saboteurs take over the company") : TEXT("GAME OVER, the human was let go")), DayNumber,
		State.HumanExitCause == EUOExitCause::None ? TEXT("still employed") : *StaticEnum<EUOExitCause>()->GetNameStringByValue(static_cast<int64>(State.HumanExitCause)),
		State.DaysSurvived, State.BestDaysSurvived, State.bNewRecord ? TEXT(", NEW RECORD") : TEXT(""));

	SetStep(EUOEvalStep::MatchOver);
	WaitForHuman(EUOEvalPrompt::MatchOverChoice, GetHumanId());
	return true;
}

#undef LOCTEXT_NAMESPACE
