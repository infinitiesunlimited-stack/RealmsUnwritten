#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "Simulation/HouseholdRecord.h"
#include "Simulation/PersonRecord.h"
#include "Simulation/SimulationIds.h"

/**
 * Values accepted when creating a person. A new person starts alive and with no household.
 *
 * AgeYears must be zero or greater; CreatePerson rejects anything else.
 */
struct FPersonCreationParams
{
	FString GivenName;

	FString FamilyName;

	int32 AgeYears = 0;
};

/** Outcome of an authoritative household-membership operation. */
enum class EHouseholdMembershipResult : uint8
{
	/** Membership changed; both sides are consistent. */
	Success,

	/** The person already belonged to the requested household; no state changed. */
	AlreadyMember,

	/** The person identifier does not resolve to a record; no state changed. */
	UnknownPerson,

	/** The household identifier does not resolve to a record; no state changed. */
	UnknownHousehold,

	/** The person does not belong to the household named by the request; no state changed. */
	NotAMember
};

/**
 * Authoritative owner of person and household records.
 *
 * The registry is plain C++: no UObject, no Actor, no tick, no loaded map, and no
 * Blueprint exposure. Anything that needs a record resolves it by stable identifier
 * through this type; nothing scans the world.
 *
 * Reads return snapshots by value, so no caller ever holds an address into registry
 * storage. Membership is mutated only through AddPersonToHousehold and
 * RemovePersonFromHousehold, which both funnel into one private transition. Callers
 * therefore cannot edit a record directly, and cannot leave the two sides of a
 * person/household relationship disagreeing.
 *
 * Storage is a dense array per entity family, with the identifier value acting as the
 * slot number plus one. Lookup range-checks the unsigned identifier value against the
 * record count and is otherwise an index read, so it stays O(1) and every identifier
 * value that was never allocated fails safely. Entity removal is not supported in
 * Prototype 0.1A; adding it requires an explicit identifier allocator with generations
 * or tombstones (see Docs/Systems/SIMULATION_FOUNDATION.md).
 */
class FSimulationRegistry
{
public:
	/**
	 * Creates a living person with no household and returns its stable identifier.
	 *
	 * Returns an invalid identifier when the parameters are rejected; the only rejection
	 * rule in this slice is a negative AgeYears. A rejected creation is atomic: it adds no
	 * record, allocates no identifier, and leaves the registry unchanged.
	 */
	FPersonId CreatePerson(const FPersonCreationParams& Params);

	/** Creates an empty household and returns its stable identifier. */
	FHouseholdId CreateHousehold(const FString& Name);

	/** Whether the identifier resolves to a person record. Safe for any identifier value. */
	bool ContainsPerson(FPersonId PersonId) const;

	/** Whether the identifier resolves to a household record. Safe for any identifier value. */
	bool ContainsHousehold(FHouseholdId HouseholdId) const;

	/**
	 * Reads a person record, or returns an unset optional for an identifier that does not
	 * resolve.
	 *
	 * The result is a copy taken at the time of the call. It is not a live view, it does not
	 * observe later changes, and it cannot be invalidated by later registry operations.
	 * Callers that need current state call again; callers that need to change state use the
	 * membership operations.
	 */
	TOptional<FPersonRecord> FindPerson(FPersonId PersonId) const;

	/**
	 * Reads a household record, or returns an unset optional for an identifier that does not
	 * resolve. The result is a copy with the same contract as FindPerson, including a copy of
	 * the member list.
	 */
	TOptional<FHouseholdRecord> FindHousehold(FHouseholdId HouseholdId) const;

	/** Number of person records held. Derived from storage; not an authoritative population. */
	int32 GetPersonCount() const { return PersonRecords.Num(); }

	/** Number of household records held. Derived from storage. */
	int32 GetHouseholdCount() const { return HouseholdRecords.Num(); }

	/**
	 * Makes the person a member of the household, detaching them from any household they
	 * currently belong to so that they never belong to two at once.
	 */
	EHouseholdMembershipResult AddPersonToHousehold(FPersonId PersonId, FHouseholdId HouseholdId);

	/**
	 * Removes the person from the named household, clearing both sides of the relationship.
	 * The household must be the one the person actually belongs to.
	 */
	EHouseholdMembershipResult RemovePersonFromHousehold(FPersonId PersonId, FHouseholdId HouseholdId);

	/**
	 * Checks every stored relationship and reports the first inconsistency found.
	 * Intended for tests and development diagnostics; it is not part of normal operation.
	 */
	bool ValidateInvariants(FString& OutFailureDescription) const;

private:
	/**
	 * Internal record resolution. The returned address is valid only until the next creation
	 * call, so it never leaves the registry.
	 */
	const FPersonRecord* ResolvePerson(FPersonId PersonId) const;

	const FHouseholdRecord* ResolveHousehold(FHouseholdId HouseholdId) const;

	FPersonRecord* ResolvePersonMutable(FPersonId PersonId);

	FHouseholdRecord* ResolveHouseholdMutable(FHouseholdId HouseholdId);

	/** The single authoritative membership transition. Updates both sides or neither. */
	void SetPersonHousehold(FPersonRecord& PersonRecord, FHouseholdId NewHouseholdId);

	TArray<FPersonRecord> PersonRecords;

	TArray<FHouseholdRecord> HouseholdRecords;
};
