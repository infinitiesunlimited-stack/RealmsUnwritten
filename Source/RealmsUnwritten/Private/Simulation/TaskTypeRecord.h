#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative definition record for one specific reusable kind of objective or action.
 *
 * AuthoredKey is durable definition identity. Id is the dense runtime handle used within
 * one registry, and Name is display data only. No task catalog is hard-coded: callers
 * register definitions under their own authored keys.
 *
 * A task type is what kind of objective this is, not an attempt at it. It names no person,
 * place, target, tool, material, output, time, or progress, and it is not a task instance.
 * Task types may be far narrower than skill types: Shape Beam is a task type, while
 * Carpentry is the skill type.
 */
struct FTaskTypeRecord
{
	/** Durable identity chosen by authored content. Unique among task types and never None. */
	FName AuthoredKey;

	/** Runtime handle assigned at creation. Stable within one registry, not across runs. */
	FTaskTypeId Id;

	/** Display name for the task type. Never an identifier. */
	FString Name;
};
