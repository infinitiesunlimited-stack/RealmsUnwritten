#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative relationship: the simulation recognizes this person as a participant in
 * this task.
 *
 * The pair (PersonId, TaskId) is the identity of the relationship. There is no separate
 * typed identifier, no occupancy of the person record, and no participant list on the
 * task. Absence of a pair means only that no such recognition is currently recorded.
 *
 * Membership is the whole meaning. The record does not assert that the person is currently
 * executing the task, is physically present, was assigned or ordered to perform it, has
 * performed any work, has contributed successfully, possesses any relevant capability, has
 * generated Practice, has advanced Task progress, or has completed anything. Assignment,
 * CurrentTask, presence, activity, work, capability, and execution remain separate
 * concerns. See Docs/Systems/TASKS.md.
 */
struct FPersonTaskParticipationRecord
{
	/** Person recognized as a participant. Must resolve. */
	FPersonId PersonId;

	/** Task the person is recognized as participating in. Must resolve. Distinct from CurrentTask. */
	FTaskId TaskId;
};
