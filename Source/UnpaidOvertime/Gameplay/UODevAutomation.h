// UNPAID: OVERTIME - dev-only scripted runs for automated in-game verification (compiled out of Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "UOKillerTypes.h"

class AUOPlayerController;

/**
 *  Command line switches (non-Shipping builds only):
 *    -UOAutoStart=<Company>   skip the menus and start a day with that company name
 *    -UOShots                 screenshot tour: main menu, settings, setup, gameplay, interaction, carried item,
 *                             pause, overtime and evaluation, then quit. Implies a company name ("Acme Synergy").
 *    -UOTasks=<4..8>          tasks per day for the scripted day (default 8)
 *    -UOAIDifficulty=<Easy|Medium|Hard>  base AI difficulty for the auto-started match (default Medium)
 *    -UOAutoPolicy=<policy>   plays the human automatically, day after day: works the shift and answers every
 *                             evaluation prompt. win = works hard, answers right, votes out the weakest;
 *                             lose = does nothing (fired on day 1); random = seeded mix (some days 1 star, some 3).
 *    -UOAutoDays=<N>          with a policy: stop after N days in total (default 3), starting a new hire on game over
 *    -UOEvalTour              auto start + policy (default random) + screenshots of every evaluation screen
 *                             (review, firings, offers, boss questions, AI answers, cut vote, outcome, next-day HUD,
 *                             game over / victory) to Saved/Screenshots/UOAuto/Eval/
 *    -UORole=<Killer|Employee> the human's role for the auto-started match (Killer = AlwaysHuman, Employee = AlwaysAI)
 *    -UOKillerCanKill=1       the optional kill rule is on for the auto-started match
 *    -UOM4=<tour>              M4 tour (implies -UOAutoPolicy=win unless given), screenshots to UOAuto/M4/:
 *                             employee = AI killer; the human fixes a jam, reads terminals, pins, presents them at
 *                               the meeting and votes (first for someone else, then reassigns to a killer);
 *                             killer = the human sabotages (all 5 kinds) and picks its fake rating (-UOFakeStars);
 *                             kill = AI killer with dismissals on (never the human): emergency meeting, body + letter;
 *                             fakeone = an AI killer shows 1 star and its wrong defense answer is accepted in public;
 *                             forgery = an AI killer shows 3 stars (forged shield), is voted out, the forgery shows;
 *                             duelrigged = the human and an AI killer tie: rock-paper-scissors (rigged), saving throw;
 *                             duelfair = two AI employees tie: a fair duel, then the loser's saving throw;
 *                             takeover = the human is a killer; the meetings vote out the employees until only
 *                               killers are left ("the saboteurs take over the company").
 *                             The scripted human may peek at the server audit to pick true evidence (dev only, logged).
 *    -UOEmployees=<4..8>      team size of the auto-started match (killers = floor(size / 3))
 *    -UOKillers=<N>           force the killer count
 *    -UOFakeStars=<1..3>      the stars a killer shows (AI killers, and the scripted human killer)
 *    -UOKillerAnswersWrong=1  AI killers' always-accepted answers are wrong on purpose
 *    -UOMeetingPolicy=<p>     AI meeting votes: tie:<a>:<b> | expel:<who> | abstain, where a/b/who are an employee id or
 *                             human / killer / innocent / innocent2 (dev peek); the scripted human balances the tie
 *    -UOSavingAnswer=<right|wrong>  saving-throw answers (AIs and the scripted human)
 *  Screenshots go to <Project>/Saved/Screenshots/UOAuto/. Pair with -ExecCmds="uo.ShiftSecondsOverride 70".
 */
class FUODevAutomation : public TSharedFromThis<FUODevAutomation>
{
public:

	/** Null unless a dev switch is on the command line (and always null in Shipping) */
	static TSharedPtr<FUODevAutomation> CreateFromCommandLine(AUOPlayerController* Owner);

	void Tick(float DeltaSeconds);

private:

	struct FStep
	{
		FString Name;
		/** Seconds to wait after the previous step finished */
		float Delay = 0.f;
		/** Returns true when the step is done (false = poll again next tick) */
		TFunction<bool()> Run;
		/** Give up waiting after this many seconds (0 = never) */
		float Timeout = 0.f;
	};

	TWeakObjectPtr<AUOPlayerController> Owner;
	TArray<FStep> Steps;
	int32 StepIndex = 0;
	float StepClock = 0.f;
	float WaitClock = 0.f;
	FString CompanyName;
	int32 TasksPerDay = 8;
	FString ShotDirectory;

	void BuildAutoStart();
	void BuildShotTour();

	// ~ Scripted human (policy)

	enum class EPolicy : uint8
	{
		None,
		Win,
		Lose,
		Random
	};

	EPolicy Policy = EPolicy::None;
	FRandomStream PolicyRng;
	int32 MaxDays = 3;
	int32 DaysStarted = 0;
	int32 LastDay = 0;
	int32 WorkTarget = 0;
	float WorkCooldown = 0.f;
	float HoldClock = 0.f;
	bool bWasHolding = false;
	/** Station the scripted human just walked to (the view must update before the interaction trace) */
	TWeakObjectPtr<class AUOInteractableStation> PendingStation;
	float PendingClock = 0.f;
	bool bEvalShots = false;
	FString EvalShotDirectory;
	TSet<FString> ShotsTaken;
	int32 ActedRevision = INDEX_NONE;
	float PromptClock = 0.f;
	float ShiftClock = 0.f;
	bool bQuitting = false;

	// ~ M4 tours

	enum class EM4 : uint8
	{
		None,
		Employee,
		Killer,
		Kill,
		FakeOne,
		Forgery,
		DuelRigged,
		DuelFair,
		Takeover
	};

	EM4 M4 = EM4::None;
	FString M4ShotDirectory;
	int32 M4Step = 0;
	float M4Clock = 0.f;
	int32 M4Index = 0;
	bool bM4Busy = false;
	bool bM4Holding = false;
	TWeakObjectPtr<class AUOInteractableStation> M4Target;
	TArray<uint8> M4TerminalOrder;
	int32 M4SabotagesSeenAtTour = 0;
	bool bM4ToursDone = false;
	/** -UOSimMatchDays=N: a new hire starts once the killers are gone or after N days (balance runs) */
	int32 SimMatchDays = 0;
	bool bM4SecondPass = false;

	// ~ M4.1 meeting automation
	FString MeetingPolicy;
	FString SavingAnswer;
	int32 FakeStars = 0;
	int32 MeetingSeenRevision = INDEX_NONE;
	float MeetingClock = 0.f;
	int32 MeetingPresented = 0;
	int32 MeetingVoteStep = 0;
	int32 HumanTieTarget = INDEX_NONE;
	bool bMeetingPolicyApplied = false;

	/** Translates the symbolic policy into ids for the director, at the start of each meeting */
	void ApplyMeetingPolicy();
	int32 ResolvePolicyId(const FString& Token) const;
	void TickMeeting(float DeltaSeconds);
	TArray<int32> GetKillerIdsForDev() const;
	bool IsKillerForDev(int32 EmployeeId) const;

	void TickM4(float DeltaSeconds);
	void TickM4Employee(float DeltaSeconds);
	void TickM4Killer(float DeltaSeconds);
	void TickM4Kill(float DeltaSeconds);
	void M4Shot(const FString& Name);
	int32 GetKillerIdForDev() const;
	/** Pins entries in the open terminal that really point at the killer (dev audit peek); returns pins added */
	int32 M4PinTrueEvidence(bool bAllowPresence);
	int32 CountSabotageTraces() const;
	/** Stand at the best target of a sabotage type; true when standing there */
	bool M4StandForSabotage(EUOSabotageType Type);

	void TickPolicy(float DeltaSeconds);
	void TickWorker(float DeltaSeconds);
	void TickEvaluation(float DeltaSeconds);
	void OnDayStarted(int32 Day);
	/** Screenshot once per name (eval tour only) */
	void EvalShot(const FString& Name);
	void Quit(const FString& Why);

	void Add(const FString& Name, float Delay, TFunction<bool()> Run, float Timeout = 0.f);
	void AddShot(const FString& Name, float Delay);

	/** Walks the local pawn forward for a while (optionally jumping halfway), to exercise footstep audio */
	void AddWalk(const FString& Name, float Seconds, bool bJump);

	/** Teleports the local pawn to a station's stand point, looking at its furniture */
	bool StandAtStation(class AUOInteractableStation* Station, float BackOff = 0.f);
	class AUOInteractableStation* FindStation(TFunctionRef<bool(const class AUOInteractableStation&)> Predicate) const;
	bool HasOpenWork(const class AUOInteractableStation& Station, bool bWantItem) const;
	bool LookFrom(const FVector& Location, const FRotator& Rotation);
	bool IsPhase(uint8 Phase) const;
};
