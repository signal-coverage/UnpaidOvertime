// UNPAID: OVERTIME - pure operations over the day's task list.

#include "UOTaskBoard.h"

FUOSubObjective* FUOTaskBoard::FindSubObjective(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId)
{
	for (FUOTaskInstance& Task : Tasks)
	{
		for (FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (Sub.Id == SubObjectiveId)
			{
				return &Sub;
			}
		}
	}
	return nullptr;
}

const FUOSubObjective* FUOTaskBoard::FindSubObjective(const TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId)
{
	return FindSubObjective(const_cast<TArray<FUOTaskInstance>&>(Tasks), SubObjectiveId);
}

const FUOTaskInstance* FUOTaskBoard::FindTask(const TArray<FUOTaskInstance>& Tasks, int32 TaskId)
{
	return Tasks.FindByPredicate([TaskId](const FUOTaskInstance& Task) { return Task.Id == TaskId; });
}

bool FUOTaskBoard::TryClaim(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId, int32 EmployeeId)
{
	FUOSubObjective* Sub = FindSubObjective(Tasks, SubObjectiveId);
	if (!Sub || Sub->IsDone())
	{
		return false;
	}

	if (Sub->State == EUOSubObjectiveState::Claimed)
	{
		return Sub->ClaimedBy == EmployeeId;
	}

	Sub->State = EUOSubObjectiveState::Claimed;
	Sub->ClaimedBy = EmployeeId;
	return true;
}

bool FUOTaskBoard::ReleaseClaim(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId, int32 EmployeeId)
{
	FUOSubObjective* Sub = FindSubObjective(Tasks, SubObjectiveId);
	if (!Sub || Sub->State != EUOSubObjectiveState::Claimed || Sub->ClaimedBy != EmployeeId)
	{
		return false;
	}

	Sub->State = EUOSubObjectiveState::Open;
	Sub->ClaimedBy = INDEX_NONE;
	return true;
}

int32 FUOTaskBoard::ReleaseAllClaims(TArray<FUOTaskInstance>& Tasks, int32 EmployeeId)
{
	int32 Released = 0;
	for (FUOTaskInstance& Task : Tasks)
	{
		for (FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (Sub.State == EUOSubObjectiveState::Claimed && Sub.ClaimedBy == EmployeeId)
			{
				Sub.State = EUOSubObjectiveState::Open;
				Sub.ClaimedBy = INDEX_NONE;
				++Released;
			}
		}
	}
	return Released;
}

bool FUOTaskBoard::Complete(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId, int32 EmployeeId, float TimeSeconds)
{
	FUOSubObjective* Sub = FindSubObjective(Tasks, SubObjectiveId);
	if (!Sub || Sub->IsDone())
	{
		return false;
	}

	Sub->State = EUOSubObjectiveState::Done;
	Sub->ClaimedBy = INDEX_NONE;
	Sub->CompletedBy = EmployeeId;
	Sub->CompletedAtTime = TimeSeconds;
	Sub->bSabotaged = false;
	return true;
}

int32 FUOTaskBoard::Uncomplete(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId)
{
	FUOSubObjective* Sub = FindSubObjective(Tasks, SubObjectiveId);
	if (!Sub || !Sub->IsDone())
	{
		return INDEX_NONE;
	}
	const int32 LostBy = Sub->CompletedBy;
	Sub->State = EUOSubObjectiveState::Open;
	Sub->CompletedBy = INDEX_NONE;
	Sub->CompletedAtTime = -1.f;
	Sub->ClaimedBy = INDEX_NONE;
	Sub->bSabotaged = true;
	return LostBy;
}

int32 FUOTaskBoard::CountTotal(const TArray<FUOTaskInstance>& Tasks)
{
	int32 Total = 0;
	for (const FUOTaskInstance& Task : Tasks)
	{
		Total += Task.SubObjectives.Num();
	}
	return Total;
}

int32 FUOTaskBoard::CountDone(const TArray<FUOTaskInstance>& Tasks)
{
	int32 Done = 0;
	for (const FUOTaskInstance& Task : Tasks)
	{
		Done += Task.CountDone();
	}
	return Done;
}

int32 FUOTaskBoard::CountCompletedBy(const TArray<FUOTaskInstance>& Tasks, int32 EmployeeId)
{
	int32 Count = 0;
	for (const FUOTaskInstance& Task : Tasks)
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			Count += (Sub.IsDone() && Sub.CompletedBy == EmployeeId) ? 1 : 0;
		}
	}
	return Count;
}

bool FUOTaskBoard::AreAllDone(const TArray<FUOTaskInstance>& Tasks)
{
	return CountDone(Tasks) == CountTotal(Tasks);
}

int32 FUOTaskBoard::CountPendingRequiringItem(const TArray<FUOTaskInstance>& Tasks, EUOItemType ItemType)
{
	int32 Count = 0;
	for (const FUOTaskInstance& Task : Tasks)
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			Count += (!Sub.IsDone() && Sub.bRequiresItem && Sub.ItemType == ItemType) ? 1 : 0;
		}
	}
	return Count;
}

int32 FUOTaskBoard::CountPendingAtStation(const TArray<FUOTaskInstance>& Tasks, FName StationId)
{
	int32 Count = 0;
	for (const FUOTaskInstance& Task : Tasks)
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			Count += (!Sub.IsDone() && Sub.StationId == StationId) ? 1 : 0;
		}
	}
	return Count;
}

const FUOSubObjective* FUOTaskBoard::FindBestAtStation(const TArray<FUOTaskInstance>& Tasks, FName StationId, int32 EmployeeId)
{
	const FUOSubObjective* OwnClaim = nullptr;
	const FUOSubObjective* FirstOpen = nullptr;
	const FUOSubObjective* OthersClaim = nullptr;

	for (const FUOTaskInstance& Task : Tasks)
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (Sub.IsDone() || Sub.StationId != StationId)
			{
				continue;
			}

			if (Sub.State == EUOSubObjectiveState::Claimed)
			{
				if (Sub.ClaimedBy == EmployeeId)
				{
					OwnClaim = OwnClaim ? OwnClaim : &Sub;
				}
				else
				{
					OthersClaim = OthersClaim ? OthersClaim : &Sub;
				}
			}
			else
			{
				FirstOpen = FirstOpen ? FirstOpen : &Sub;
			}
		}
	}

	if (OwnClaim)
	{
		return OwnClaim;
	}
	return FirstOpen ? FirstOpen : OthersClaim;
}

const FUOSubObjective* FUOTaskBoard::FindClaimOf(const TArray<FUOTaskInstance>& Tasks, int32 EmployeeId)
{
	for (const FUOTaskInstance& Task : Tasks)
	{
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			if (Sub.State == EUOSubObjectiveState::Claimed && Sub.ClaimedBy == EmployeeId)
			{
				return &Sub;
			}
		}
	}
	return nullptr;
}
