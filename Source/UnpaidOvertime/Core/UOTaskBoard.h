// UNPAID: OVERTIME - pure operations over the day's task list (claims, completion, queries).

#pragma once

#include "CoreMinimal.h"
#include "UOTaskTypes.h"

/**
 *  Stateless helpers that mutate or query a task list.
 *  The authoritative list lives in the game state; the task manager calls these on the server.
 */
struct FUOTaskBoard
{
	static FUOSubObjective* FindSubObjective(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId);
	static const FUOSubObjective* FindSubObjective(const TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId);

	static const FUOTaskInstance* FindTask(const TArray<FUOTaskInstance>& Tasks, int32 TaskId);

	/** Claims an open sub-objective. Re-claiming your own claim succeeds. */
	static bool TryClaim(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId, int32 EmployeeId);

	/** Releases a claim held by the employee */
	static bool ReleaseClaim(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId, int32 EmployeeId);

	/** Releases every pending claim of an employee (e.g. when they are eliminated). Returns how many were released. */
	static int32 ReleaseAllClaims(TArray<FUOTaskInstance>& Tasks, int32 EmployeeId);

	/** Marks a sub-objective done, crediting the employee. Any claim is released. */
	static bool Complete(TArray<FUOTaskInstance>& Tasks, int32 SubObjectiveId, int32 EmployeeId, float TimeSeconds);

	static int32 CountTotal(const TArray<FUOTaskInstance>& Tasks);
	static int32 CountDone(const TArray<FUOTaskInstance>& Tasks);
	static int32 CountCompletedBy(const TArray<FUOTaskInstance>& Tasks, int32 EmployeeId);
	static bool AreAllDone(const TArray<FUOTaskInstance>& Tasks);

	/** Not-done sub-objectives that need the given item delivered */
	static int32 CountPendingRequiringItem(const TArray<FUOTaskInstance>& Tasks, EUOItemType ItemType);

	/** Pending sub-objectives at a station */
	static int32 CountPendingAtStation(const TArray<FUOTaskInstance>& Tasks, FName StationId);

	/**
	 *  Best pending sub-objective at a station for an employee:
	 *  own claim first, then open ones, then ones claimed by others (anyone may finish claimed work).
	 */
	static const FUOSubObjective* FindBestAtStation(const TArray<FUOTaskInstance>& Tasks, FName StationId, int32 EmployeeId);

	/** The sub-objective currently claimed by an employee, if any */
	static const FUOSubObjective* FindClaimOf(const TArray<FUOTaskInstance>& Tasks, int32 EmployeeId);
};
