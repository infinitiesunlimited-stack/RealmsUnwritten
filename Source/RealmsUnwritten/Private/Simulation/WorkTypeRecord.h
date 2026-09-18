#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one kind of work a person may currently be committed to.
 *
 * This is a definition-shaped record: it describes a kind of activity, not an occupation,
 * a job posting, a workplace, or a person. Quantities of labor live on people, as a single
 * current-work commitment, never here, and never as a workforce counter on a settlement or
 * household.
 *
 * A work type carries two identifiers, matching the good-type pattern:
 *
 *   AuthoredKey  durable definition identity. Chosen by whoever defines the work, never by
 *                the registry, so it does not depend on insertion order, slot position, or
 *                load order.
 *   Id           runtime handle. A dense slot-derived FWorkTypeId, cheap to store and
 *                compare, used by current-work values for the lifetime of one registry. It
 *                is not durable: the same authored key may map to a different Id in a
 *                different run.
 *
 * No work type is hard-coded. Harvest, repair, and any other kind are created at runtime
 * like any other, so authored content can define work types later without a C++ enum of
 * medieval jobs. Occupation remains a separate, unimplemented concept.
 *
 * Display names are neither identity: two work types with different authored keys may share
 * a display name, while a duplicate authored key is rejected outright. Work-type keys are a
 * separate namespace from good-type keys.
 *
 * Duration, wages, skill, tools, recipes, and process references are all part of later
 * labor systems and are deliberately absent here (see Docs/Systems/WORK_LABOR.md).
 */
struct FWorkTypeRecord
{
	/**
	 * Durable definition identity, unique across work types and independent of creation
	 * order. Never None.
	 */
	FName AuthoredKey;

	/** Runtime handle assigned at creation. Stable within one registry, not across runs. */
	FWorkTypeId Id;

	/** Display name for the work type. Never an identifier. */
	FString Name;
};
