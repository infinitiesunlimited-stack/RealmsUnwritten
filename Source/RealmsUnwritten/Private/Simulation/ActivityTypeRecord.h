#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative definition record for one kind of broad current behavior.
 *
 * AuthoredKey is durable definition identity. Id is the dense runtime handle used within
 * one registry, and Name is display data only. No activity catalog is hard-coded: callers
 * register definitions under their own authored keys.
 *
 * This record does not describe a task, occupation, work type, location, capability, or
 * event. Tasks, presence, and scheduling are later systems.
 */
struct FActivityTypeRecord
{
	/** Durable identity chosen by authored content. Unique among activity types and never None. */
	FName AuthoredKey;

	/** Runtime handle assigned at creation. Stable within one registry, not across runs. */
	FActivityTypeId Id;

	/** Display name for the activity type. Never an identifier. */
	FString Name;
};
