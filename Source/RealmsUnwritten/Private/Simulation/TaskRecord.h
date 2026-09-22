#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one particular occurrence of an objective.
 *
 * A task type is the reusable definition, Shape Beam. A task instance is one occurrence of
 * it, the shaping of one particular roof beam. The definition's authored key and display
 * name stay on FTaskTypeRecord and are deliberately not copied here, so the definition has
 * exactly one owner.
 *
 * Existence means only that this objective occurrence exists, has its own identity, and is
 * of the referenced type. It does not mean the objective is available, assigned, current,
 * active, located, executable, complete, successful, failed, known to anyone, or being
 * performed. Nothing here names a person, place, target, or time, and there is no
 * lifecycle or outcome state: those are later systems, and their absence is the point.
 *
 * This is plain registry-owned simulation data. It does not depend on an Actor, Component,
 * animation, navigation, behavior tree, loaded map, or tick, so a task survives the local
 * detail of its execution not being simulated.
 */
struct FTaskRecord
{
	/** Stable identity assigned at creation. Unique within one registry, not across runs. */
	FTaskId Id;

	/** The task type this is an occurrence of. Always resolvable; never a copy of its record. */
	FTaskTypeId TaskTypeId;
};
