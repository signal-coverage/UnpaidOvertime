// UNPAID: OVERTIME - end-of-day star rating by individual contribution.

#include "UOStarRating.h"

#define LOCTEXT_NAMESPACE "UOStarRating"

namespace UOStarRatingPrivate
{
	/** Absorbs float error so that exactly 50% / 100% land on the higher tier */
	constexpr float RatioTolerance = 1.e-4f;
}

float FUOStarRating::ComputeFairShare(int32 TotalSubObjectives, int32 AliveEmployees)
{
	if (TotalSubObjectives <= 0 || AliveEmployees <= 0)
	{
		return 0.f;
	}
	return static_cast<float>(TotalSubObjectives) / static_cast<float>(AliveEmployees);
}

float FUOStarRating::ComputeContributionRatio(int32 Completed, int32 TotalSubObjectives, int32 AliveEmployees)
{
	if (Completed <= 0)
	{
		return 0.f;
	}

	const float FairShare = ComputeFairShare(TotalSubObjectives, AliveEmployees);
	if (FairShare <= 0.f)
	{
		return ThreeStarRatio;
	}
	return static_cast<float>(Completed) / FairShare;
}

int32 FUOStarRating::ComputeStars(int32 Completed, int32 TotalSubObjectives, int32 AliveEmployees)
{
	if (Completed <= 0)
	{
		return 0;
	}

	const float Ratio = ComputeContributionRatio(Completed, TotalSubObjectives, AliveEmployees) + UOStarRatingPrivate::RatioTolerance;
	if (Ratio >= ThreeStarRatio)
	{
		return 3;
	}
	if (Ratio >= TwoStarRatio)
	{
		return 2;
	}
	return 1;
}

EUOStarConsequence FUOStarRating::GetConsequence(int32 Stars)
{
	switch (FMath::Clamp(Stars, 0, MaxStars))
	{
	case 0:		return EUOStarConsequence::Fired;
	case 1:		return EUOStarConsequence::CutVoteCandidate;
	case 2:		return EUOStarConsequence::Safe;
	default:	return EUOStarConsequence::PromotionAvailable;
	}
}

FText FUOStarRating::GetConsequenceLabel(EUOStarConsequence Consequence)
{
	switch (Consequence)
	{
	case EUOStarConsequence::Fired:					return LOCTEXT("ConsequenceFired", "Fired");
	case EUOStarConsequence::CutVoteCandidate:		return LOCTEXT("ConsequenceCutVote", "Cut-vote candidate");
	case EUOStarConsequence::Safe:					return LOCTEXT("ConsequenceSafe", "Safe");
	case EUOStarConsequence::PromotionAvailable:	return LOCTEXT("ConsequencePromotion", "Promotion available");
	default:										return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
