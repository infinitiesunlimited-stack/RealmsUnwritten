#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Whether a person is currently a living participant in the simulation.
 *
 * The constitution requires every person to carry an explicit life-state. Prototype 0.1A
 * creates living people only; mortality, death dates, and causes are deferred.
 */
enum class EPersonLifeState : uint8
{
	Alive,
	Deceased
};

/**
 * Authoritative record for one persistent human being.
 *
 * This record is plain simulation data. It is not an Actor, Pawn, Character, or UObject,
 * it requires no loaded map, and it does not tick. Names are display identity only;
 * FPersonId is the sole identity. Records are owned exclusively by FSimulationRegistry,
 * which is also the only writer of HouseholdId.
 */
struct FPersonRecord
{
	/** Stable identity assigned at creation. */
	FPersonId Id;

	FString GivenName;

	FString FamilyName;

	/**
	 * Age in whole years at creation time, used as the prototype age basis. Never negative:
	 * person creation rejects a negative age rather than storing one.
	 *
	 * Prototype 0.1A simplification: an authoritative calendar does not exist yet, so a
	 * birth date cannot be recorded. This field is replaced by a birth date once the
	 * Time and Calendar system owns simulation time.
	 */
	int32 AgeYears = 0;

	EPersonLifeState LifeState = EPersonLifeState::Alive;

	/** Primary household, or an invalid identifier when the person belongs to none. */
	FHouseholdId HouseholdId;
};
