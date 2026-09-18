#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative relationship: this person possesses a meaningful acquired capability
 * associated with this skill type.
 *
 * The pair (PersonId, SkillTypeId) is the identity of the relationship. There is no
 * separate typed identifier, no magnitude, and no occupancy of the person record. Absence
 * of a pair means only that no such relationship is currently recorded, not that the person
 * is biologically unable to act.
 *
 * Proficiency, experience, knowledge, occupation, credentials, and CurrentWork are separate
 * concerns and are not represented here. See Docs/Systems/PERSON_CAPABILITY.md.
 */
struct FPersonCapabilityRecord
{
	/** Person who possesses the capability. Must resolve. */
	FPersonId PersonId;

	/** Skill type possessed. Must resolve. Distinct from CurrentWork and from occupation. */
	FSkillTypeId SkillTypeId;
};
