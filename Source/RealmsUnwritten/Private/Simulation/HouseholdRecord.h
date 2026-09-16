#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one persistent domestic unit.
 *
 * Membership is stored as stable person identifiers, never as pointers or names. The
 * member list is maintained exclusively by FSimulationRegistry so that it can never
 * disagree with FPersonRecord::HouseholdId. Household population is derived from this
 * list; no population total is stored anywhere.
 */
struct FHouseholdRecord
{
	/** Stable identity assigned at creation. */
	FHouseholdId Id;

	/** Display name for the household, such as a family name. Never an identifier. */
	FString Name;

	/** Current members. Contains no duplicates and no unresolvable identifiers. */
	TArray<FPersonId> Members;
};
