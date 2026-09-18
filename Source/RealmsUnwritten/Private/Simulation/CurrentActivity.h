#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Kind of recorded current activity a person may hold.
 *
 * Exactly two kinds exist, and the list is deliberately short. An authored activity type
 * is the only recorded engagement in Prototype 0.1J. A future kind is added only when that
 * kind is actually built.
 *
 * This is not occupation, not CurrentWork, not presence, and not a task.
 */
enum class ECurrentActivityKind : uint8
{
	/** No authoritative current activity is presently recorded. */
	None,

	/** The person is recorded as engaged in one authored activity type. */
	ActivityType
};

/**
 * A person's authoritative current activity: what broad kind of behavior is recorded.
 *
 * Kind ActivityType naming a type means that behavior is currently recorded. It does not
 * mean the person is at a place, performing a specific task, practicing a skill, or
 * occupying a job. Presence, tasks, occupation, and practice are later or separate systems.
 *
 * Kind None means only that no current activity is instantiated. It does not mean the
 * person is idle, unemployed, unconscious, or physically inactive. Distant persistent
 * people may exist without moment-to-moment activity.
 *
 * The kind and the identifier belong together and are set through the factory functions.
 * A person holds at most one of these. Replacement overwrites it. There is no history.
 */
struct FCurrentActivity
{
	/** The current activity of a person for whom none is recorded. */
	static FCurrentActivity Unrecorded() { return FCurrentActivity(); }

	/** Current activity of one authored activity type. */
	static FCurrentActivity OfType(FActivityTypeId ActivityTypeId)
	{
		FCurrentActivity CurrentActivity;
		CurrentActivity.Kind = ECurrentActivityKind::ActivityType;
		CurrentActivity.ActivityTypeId = ActivityTypeId;

		return CurrentActivity;
	}

	/** Whether an authoritative current activity is recorded at all. */
	bool IsRecorded() const { return Kind != ECurrentActivityKind::None; }

	/** Which kind of current activity is recorded. */
	ECurrentActivityKind Kind = ECurrentActivityKind::None;

	/** Activity type. Valid and resolvable when Kind is ActivityType, unset otherwise. */
	FActivityTypeId ActivityTypeId;
};
