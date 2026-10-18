// UNPAID: OVERTIME - star rating tests.

#include "UOTestFlags.h"
#include "UOStarRating.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOStarRatingThresholdsTest, "UnpaidOvertime.Core.StarRating.Thresholds", UO_CORE_TEST_FLAGS)
bool FUOStarRatingThresholdsTest::RunTest(const FString& Parameters)
{
	// 16 sub-objectives, 4 alive employees -> fair share 4
	TestEqual(TEXT("Fair share is total / alive"), FUOStarRating::ComputeFairShare(16, 4), 4.f);

	TestEqual(TEXT("100% of fair share is 3 stars"), FUOStarRating::ComputeStars(4, 16, 4), 3);
	TestEqual(TEXT("Above fair share is 3 stars"), FUOStarRating::ComputeStars(9, 16, 4), 3);
	TestEqual(TEXT("75% is 2 stars"), FUOStarRating::ComputeStars(3, 16, 4), 2);
	TestEqual(TEXT("Exactly 50% is 2 stars"), FUOStarRating::ComputeStars(2, 16, 4), 2);
	TestEqual(TEXT("25% is 1 star"), FUOStarRating::ComputeStars(1, 16, 4), 1);
	TestEqual(TEXT("Nothing done is 0 stars"), FUOStarRating::ComputeStars(0, 16, 4), 0);

	// Non-integer fair share: 15 / 4 = 3.75
	TestEqual(TEXT("2 of 3.75 (53%) is 2 stars"), FUOStarRating::ComputeStars(2, 15, 4), 2);
	TestEqual(TEXT("1 of 3.75 (27%) is 1 star"), FUOStarRating::ComputeStars(1, 15, 4), 1);
	TestEqual(TEXT("4 of 3.75 is 3 stars"), FUOStarRating::ComputeStars(4, 15, 4), 3);
	TestEqual(TEXT("3 of 3.75 (80%) is 2 stars"), FUOStarRating::ComputeStars(3, 15, 4), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOStarRatingEdgeCasesTest, "UnpaidOvertime.Core.StarRating.EdgeCases", UO_CORE_TEST_FLAGS)
bool FUOStarRatingEdgeCasesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No alive employees gives a zero fair share"), FUOStarRating::ComputeFairShare(10, 0), 0.f);
	TestEqual(TEXT("No sub-objectives gives a zero fair share"), FUOStarRating::ComputeFairShare(0, 4), 0.f);
	TestEqual(TEXT("Negative inputs give a zero fair share"), FUOStarRating::ComputeFairShare(-3, -1), 0.f);

	TestEqual(TEXT("Zero completed is always 0 stars, even with no work"), FUOStarRating::ComputeStars(0, 0, 4), 0);
	TestEqual(TEXT("Zero completed with no employees is 0 stars"), FUOStarRating::ComputeStars(0, 10, 0), 0);
	TestEqual(TEXT("Work done with an undefined fair share counts as full contribution"), FUOStarRating::ComputeStars(2, 0, 4), 3);
	TestEqual(TEXT("Work done with no alive employees counts as full contribution"), FUOStarRating::ComputeStars(2, 10, 0), 3);
	TestEqual(TEXT("Negative completed is 0 stars"), FUOStarRating::ComputeStars(-1, 10, 4), 0);
	TestEqual(TEXT("Solo employee doing everything gets 3 stars"), FUOStarRating::ComputeStars(5, 5, 1), 3);

	TestEqual(TEXT("Ratio with nothing done is 0"), FUOStarRating::ComputeContributionRatio(0, 16, 4), 0.f);
	TestEqual(TEXT("Ratio of 2 over fair share 4 is 0.5"), FUOStarRating::ComputeContributionRatio(2, 16, 4), 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOStarRatingConsequenceTest, "UnpaidOvertime.Core.StarRating.Consequences", UO_CORE_TEST_FLAGS)
bool FUOStarRatingConsequenceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("0 stars: fired"), FUOStarRating::GetConsequence(0), EUOStarConsequence::Fired);
	TestEqual(TEXT("1 star: cut-vote candidate"), FUOStarRating::GetConsequence(1), EUOStarConsequence::CutVoteCandidate);
	TestEqual(TEXT("2 stars: safe"), FUOStarRating::GetConsequence(2), EUOStarConsequence::Safe);
	TestEqual(TEXT("3 stars: promotion available"), FUOStarRating::GetConsequence(3), EUOStarConsequence::PromotionAvailable);
	TestEqual(TEXT("Out of range low clamps to fired"), FUOStarRating::GetConsequence(-2), EUOStarConsequence::Fired);
	TestEqual(TEXT("Out of range high clamps to promotion"), FUOStarRating::GetConsequence(7), EUOStarConsequence::PromotionAvailable);

	for (const EUOStarConsequence Consequence : { EUOStarConsequence::Fired, EUOStarConsequence::CutVoteCandidate, EUOStarConsequence::Safe, EUOStarConsequence::PromotionAvailable })
	{
		TestFalse(TEXT("Every consequence has a label"), FUOStarRating::GetConsequenceLabel(Consequence).IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
