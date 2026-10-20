// UNPAID: OVERTIME - balance targets (rule 9), measured with the abstract match simulation.
// 1 human (passive: works, abstains, never presents) + AIs. Product target: the killers win about 40% of the matches
// (band 35-45%, Easy AIs 30-50%) in 4/6 employees with kills off and on, and at least 15% of the meetings that expel
// someone expel an innocent. Longer runs: console command `uo.BalanceSim 5000` (see UOBalanceSim.cpp).

#include "UOTestFlags.h"
#include "UOBalanceSim.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOBalanceSimTest, "UnpaidOvertime.Core.Balance.MeetingSimulation", UO_CORE_TEST_FLAGS)
bool FUOBalanceSimTest::RunTest(const FString& Parameters)
{
	const TArray<FUOBalanceSetup> Setups = FUOBalanceSim::GetStandardSetups(1500);
	for (const FUOBalanceSetup& Setup : Setups)
	{
		const FUOBalanceSimResult Result = FUOBalanceSim::Run(Setup.Config);
		AddInfo(Setup.Name + TEXT(": ") + Result.ToString());
		TestTrue(FString::Printf(TEXT("%s: killers win %.0f-%.0f%% of matches (got %.1f%%)"), *Setup.Name, Setup.MinKillerWin * 100.f, Setup.MaxKillerWin * 100.f, Result.KillersWinRate() * 100.f),
			Result.KillersWinRate() >= Setup.MinKillerWin && Result.KillersWinRate() <= Setup.MaxKillerWin);
		TestTrue(FString::Printf(TEXT("%s: innocents wrongly expelled in at least 15%% of expelling meetings (got %.1f%%)"), *Setup.Name, Result.WrongfulRate() * 100.f),
			Result.WrongfulRate() >= 0.15f);
		if (Setup.Config.Employees == 4 && !Setup.Config.bKillerCanKill && Setup.Config.Difficulty == EUOAIDifficulty::Medium)
		{
			TestTrue(TEXT("Some duels are rigged"), Result.RiggedDuels > 0);
		}
	}

	FUOBalanceSimConfig Small = Setups[0].Config;
	Small.Matches = 200;
	TestEqual(TEXT("Deterministic"), FUOBalanceSim::Run(Small).KillersWin, FUOBalanceSim::Run(Small).KillersWin);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
