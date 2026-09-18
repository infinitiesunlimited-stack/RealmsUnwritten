#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative definition record for one kind of capability.
 *
 * AuthoredKey is durable definition identity. Id is the dense runtime handle used within
 * one registry, and Name is display data only. No skill catalog is hard-coded: callers
 * register definitions under their own authored keys.
 *
 * This record does not describe any person's capability. Skill values, proficiency,
 * experience, progression, knowledge, occupation, training, and production effects are
 * separate concerns and are not represented here.
 */
struct FSkillTypeRecord
{
	/** Durable identity chosen by authored content. Unique among skill types and never None. */
	FName AuthoredKey;

	/** Runtime handle assigned at creation. Stable within one registry, not across runs. */
	FSkillTypeId Id;

	/** Display name for the skill type. Never an identifier. */
	FString Name;
};
