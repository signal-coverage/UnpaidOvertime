// UNPAID: OVERTIME - end-of-day star rating by individual contribution.

#pragma once

#include "CoreMinimal.h"
#include "UOCoreTypes.h"

/**
 *  fairShare = totalSubObjectives / aliveEmployees, ratio = completed / fairShare.
 *  3 stars at ratio >= 1.0, 2 at >= 0.5, 1 above 0, 0 when nothing was completed.
 */
struct FUOStarRating
{
	static constexpr int32 MaxStars = 3;
	static constexpr float ThreeStarRatio = 1.f;
	static constexpr float TwoStarRatio = 0.5f;

	/** 0 when there are no alive employees or no sub-objectives */
	static float ComputeFairShare(int32 TotalSubObjectives, int32 AliveEmployees);

	/** completed / fairShare; 0 when nothing was completed; 1 when the fair share is undefined but work was done */
	static float ComputeContributionRatio(int32 Completed, int32 TotalSubObjectives, int32 AliveEmployees);

	static int32 ComputeStars(int32 Completed, int32 TotalSubObjectives, int32 AliveEmployees);

	static EUOStarConsequence GetConsequence(int32 Stars);

	/** Short label for the evaluation screen */
	static FText GetConsequenceLabel(EUOStarConsequence Consequence);
};
