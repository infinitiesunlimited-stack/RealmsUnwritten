#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "Simulation/HouseholdRecord.h"
#include "Simulation/PersonRecord.h"
#include "Simulation/PropertyRecord.h"
#include "Simulation/SettlementRecord.h"
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

/** Outcome of an authoritative change to which settlement a household is located in. */
enum class ESettlementMembershipResult : uint8
{
	/** The household's settlement changed; both sides are consistent. */
	Success,

	/** The household was already located in the requested settlement; no state changed. */
	AlreadyMember,

	/** The household identifier does not resolve to a record; no state changed. */
	UnknownHousehold,

	/** The settlement identifier does not resolve to a record; no state changed. */
	UnknownSettlement,

	/** The household is not located in the settlement named by the request; no state changed. */
	NotAMember,

	/**
	 * The household still occupies a property, which would be left in another settlement.
	 * Remove its residence first. No state changed.
	 */
	StillResident
};

/** Outcome of an authoritative change to which property a household occupies. */
enum class EResidenceResult : uint8
{
	/** The household's residence changed; both sides are consistent. */
	Success,

	/** The household already occupied the requested property; no state changed. */
	AlreadyResident,

	/** The household identifier does not resolve to a record; no state changed. */
	UnknownHousehold,

	/** The property identifier does not resolve to a record; no state changed. */
	UnknownProperty,

	/** The household is located in no settlement, so it cannot occupy anything. No state changed. */
	HouseholdNotInSettlement,

	/** The property belongs to a different settlement than the household. No state changed. */
	SettlementMismatch,

	/** Another household already occupies the property; no state changed. */
	PropertyOccupied,

	/** The household does not occupy the property named by the request; no state changed. */
	NotResident
};

/**
 * Authoritative owner of person, household, settlement, and property records.
 *
 * The registry is plain C++: no UObject, no Actor, no tick, no loaded map, and no
 * Blueprint exposure. Anything that needs a record resolves it by stable identifier
 * through this type; nothing scans the world.
 *
 * Reads return snapshots by value, so no caller ever holds an address into registry
 * storage. Each two-sided relationship is mutated only through its own pair of public
 * operations, each funnelling into a single private transition: person/household
 * membership, household/settlement location, and household/property residence. Callers
 * therefore cannot edit a record directly, and cannot leave either side of a relationship
 * disagreeing with the other.
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

	/** Creates an empty household, in no settlement and with no residence. */
	FHouseholdId CreateHousehold(const FString& Name);

	/** Creates an empty settlement, with no properties and no households. */
	FSettlementId CreateSettlement(const FString& Name);

	/**
	 * Creates an unoccupied property belonging to the given settlement.
	 *
	 * Returns an invalid identifier when the settlement does not resolve, so a property can
	 * never reference a settlement that does not exist. A rejected creation is atomic: it
	 * adds no record, allocates no identifier, and leaves the registry unchanged.
	 */
	FPropertyId CreateProperty(FSettlementId SettlementId);

	/** Whether the identifier resolves to a person record. Safe for any identifier value. */
	bool ContainsPerson(FPersonId PersonId) const;

	/** Whether the identifier resolves to a household record. Safe for any identifier value. */
	bool ContainsHousehold(FHouseholdId HouseholdId) const;

	/** Whether the identifier resolves to a settlement record. Safe for any identifier value. */
	bool ContainsSettlement(FSettlementId SettlementId) const;

	/** Whether the identifier resolves to a property record. Safe for any identifier value. */
	bool ContainsProperty(FPropertyId PropertyId) const;

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

	/**
	 * Reads a settlement record, or returns an unset optional for an identifier that does not
	 * resolve. The result is a copy with the same contract as FindPerson, including copies of
	 * the property and household lists.
	 */
	TOptional<FSettlementRecord> FindSettlement(FSettlementId SettlementId) const;

	/**
	 * Reads a property record, or returns an unset optional for an identifier that does not
	 * resolve. The result is a copy with the same contract as FindPerson.
	 */
	TOptional<FPropertyRecord> FindProperty(FPropertyId PropertyId) const;

	/** Number of person records held. Derived from storage; not an authoritative population. */
	int32 GetPersonCount() const { return PersonRecords.Num(); }

	/** Number of household records held. Derived from storage. */
	int32 GetHouseholdCount() const { return HouseholdRecords.Num(); }

	/** Number of settlement records held. Derived from storage. */
	int32 GetSettlementCount() const { return SettlementRecords.Num(); }

	/** Number of property records held. Derived from storage. */
	int32 GetPropertyCount() const { return PropertyRecords.Num(); }

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
	 * Locates the household in the settlement, removing it from any settlement it is
	 * currently in so that it is never located in two at once.
	 *
	 * A household that still occupies a property is rejected with StillResident rather than
	 * being silently evicted: relocating is an explicit sequence of vacating, moving, and
	 * taking up a new residence. The household's people are unaffected either way.
	 */
	ESettlementMembershipResult PlaceHouseholdInSettlement(FHouseholdId HouseholdId, FSettlementId SettlementId);

	/**
	 * Removes the household from the named settlement, clearing both sides. The settlement
	 * must be the one the household is actually in, and the household must occupy no
	 * property.
	 */
	ESettlementMembershipResult RemoveHouseholdFromSettlement(FHouseholdId HouseholdId, FSettlementId SettlementId);

	/**
	 * Makes the property the household's residence, vacating any property it currently
	 * occupies so that it never occupies two at once.
	 *
	 * The property must be unoccupied and must belong to the household's own settlement.
	 */
	EResidenceResult AssignHouseholdResidence(FHouseholdId HouseholdId, FPropertyId PropertyId);

	/**
	 * Vacates the named property, clearing both sides. The property must be the one the
	 * household actually occupies. The household stays in its settlement.
	 */
	EResidenceResult RemoveHouseholdResidence(FHouseholdId HouseholdId, FPropertyId PropertyId);

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

	const FSettlementRecord* ResolveSettlement(FSettlementId SettlementId) const;

	const FPropertyRecord* ResolveProperty(FPropertyId PropertyId) const;

	FPersonRecord* ResolvePersonMutable(FPersonId PersonId);

	FHouseholdRecord* ResolveHouseholdMutable(FHouseholdId HouseholdId);

	FSettlementRecord* ResolveSettlementMutable(FSettlementId SettlementId);

	FPropertyRecord* ResolvePropertyMutable(FPropertyId PropertyId);

	/** The single authoritative person/household membership transition. Both sides or neither. */
	void SetPersonHousehold(FPersonRecord& PersonRecord, FHouseholdId NewHouseholdId);

	/** The single authoritative household/settlement transition. Both sides or neither. */
	void SetHouseholdSettlement(FHouseholdRecord& HouseholdRecord, FSettlementId NewSettlementId);

	/** The single authoritative household/property residence transition. Both sides or neither. */
	void SetHouseholdResidence(FHouseholdRecord& HouseholdRecord, FPropertyId NewPropertyId);

	bool ValidatePersonRecords(FString& OutFailureDescription) const;

	bool ValidateHouseholdRecords(FString& OutFailureDescription) const;

	bool ValidateSettlementRecords(FString& OutFailureDescription) const;

	bool ValidatePropertyRecords(FString& OutFailureDescription) const;

	TArray<FPersonRecord> PersonRecords;

	TArray<FHouseholdRecord> HouseholdRecords;

	TArray<FSettlementRecord> SettlementRecords;

	TArray<FPropertyRecord> PropertyRecords;
};
