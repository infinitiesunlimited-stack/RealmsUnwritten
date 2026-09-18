#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative relationship: this person possesses a meaningful acquired capability
 * associated with this skill type, together with any accumulated practice recorded for it.
 *
 * The pair (PersonId, SkillTypeId) is the identity of the relationship. There is no
 * separate typed identifier and no occupancy of the person record. Absence of a pair means
 * only that no such relationship is currently recorded, not that the person is biologically
 * unable to act.
 *
 * AccumulatedPractice is abstract developmental history, not hours, XP, proficiency, or
 * occupation duration. Proficiency, knowledge, occupation, credentials, and CurrentWork
 * remain separate concerns. See Docs/Systems/PERSON_CAPABILITY.md.
 */
struct FPersonCapabilityRecord
{
	/** Person who possesses the capability. Must resolve. */
	FPersonId PersonId;

	/** Skill type possessed. Must resolve. Distinct from CurrentWork and from occupation. */
	FSkillTypeId SkillTypeId;

	/**
	 * Persistent normalized meaningful developmental practice for this acquired capability.
	 *
	 * Zero until quantified. The unit is abstract: it is not hours, days, task counts, XP,
	 * or proficiency. Practice only increases; disuse does not subtract from it.
	 */
	uint32 AccumulatedPractice = 0;
};
