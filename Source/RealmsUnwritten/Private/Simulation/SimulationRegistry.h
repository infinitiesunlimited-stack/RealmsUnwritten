#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "Simulation/GoodTypeRecord.h"
#include "Simulation/GoodsAuditRecord.h"
#include "Simulation/HouseholdRecord.h"
#include "Simulation/InventoryRecord.h"
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
 * Outcome of adding goods to an inventory.
 *
 * Adding creates simulation quantity, which is a controlled low-level mutation rather than a
 * physical process. Production, harvest, and scenario seeding are the future callers; none
 * of them exists yet (see Docs/Systems/GOODS_INVENTORY.md).
 */
enum class EAddGoodsResult : uint8
{
	/** The inventory now holds the additional quantity, and one audit record was appended. */
	Success,

	/** The inventory identifier does not resolve to a record; no state changed. */
	UnknownInventory,

	/** The good type identifier does not resolve to a record; no state changed. */
	UnknownGoodType,

	/** The requested quantity was zero or negative; no state changed. */
	InvalidQuantity,

	/** No reason was supplied, so the creation would have been anonymous; no state changed. */
	InvalidReason,

	/** The inventory cannot hold that much of the good type without exceeding MaxGoodQuantity. */
	Overflow
};

/**
 * Outcome of removing goods from an inventory.
 *
 * Removing destroys simulation quantity, the counterpart to EAddGoodsResult. Consumption,
 * spoilage, loss, and destruction are the future callers.
 */
enum class ERemoveGoodsResult : uint8
{
	/** The inventory no longer holds the removed quantity, and one audit record was appended. */
	Success,

	/** The inventory identifier does not resolve to a record; no state changed. */
	UnknownInventory,

	/** The good type identifier does not resolve to a record; no state changed. */
	UnknownGoodType,

	/** The requested quantity was zero or negative; no state changed. */
	InvalidQuantity,

	/** No reason was supplied, so the destruction would have been anonymous; no state changed. */
	InvalidReason,

	/** The inventory does not hold that much of the good type; no state changed. */
	InsufficientQuantity
};

/**
 * Outcome of moving goods between two inventories. A successful transfer conserves total
 * quantity exactly; every failure leaves both inventories untouched.
 */
enum class ETransferGoodsResult : uint8
{
	/** The quantity moved; the total across both inventories is unchanged. */
	Success,

	/** The source inventory identifier does not resolve to a record; no state changed. */
	UnknownSourceInventory,

	/** The destination inventory identifier does not resolve to a record; no state changed. */
	UnknownDestinationInventory,

	/** The good type identifier does not resolve to a record; no state changed. */
	UnknownGoodType,

	/** The requested quantity was zero or negative; no state changed. */
	InvalidQuantity,

	/**
	 * Source and destination are the same inventory. Rejected rather than treated as a
	 * no-op, because a transfer to oneself is a caller error rather than a request. No state
	 * changed.
	 */
	SameInventory,

	/** The source does not hold that much of the good type; no state changed. */
	InsufficientQuantity,

	/** The destination cannot hold that much more without exceeding MaxGoodQuantity. */
	Overflow
};

/**
 * Authoritative owner of person, household, settlement, property, good type, and inventory
 * records.
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
 * Inventory contents are one-sided rather than a relationship, so they are mutated through
 * AddGoods, RemoveGoods, and TransferGoods. No caller receives a mutable inventory, so the
 * rules that quantities stay positive and that a good type appears at most once per
 * inventory cannot be bypassed.
 *
 * Those three operations divide into creation, destruction, and movement. The first two
 * change how much quantity exists and each leaves an audit record naming the caller's
 * reason; the third only changes where quantity is, and is audited nowhere because it
 * conserves the total.
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

	/**
	 * Creates a good type under a durable authored key and returns its runtime handle.
	 *
	 * The authored key is the good type's definition identity and must be supplied by the
	 * caller: `None` is rejected, and so is a key already in use. The display name is
	 * display data only, so two good types with different authored keys may share one.
	 *
	 * Returns an invalid handle, having changed nothing and consumed no runtime handle, when
	 * the authored key is None or already taken.
	 */
	FGoodTypeId CreateGoodType(FName AuthoredKey, const FString& DisplayName);

	/** Creates an empty inventory, holding no goods. */
	FInventoryId CreateInventory();

	/** Whether the identifier resolves to a person record. Safe for any identifier value. */
	bool ContainsPerson(FPersonId PersonId) const;

	/** Whether the identifier resolves to a household record. Safe for any identifier value. */
	bool ContainsHousehold(FHouseholdId HouseholdId) const;

	/** Whether the identifier resolves to a settlement record. Safe for any identifier value. */
	bool ContainsSettlement(FSettlementId SettlementId) const;

	/** Whether the identifier resolves to a property record. Safe for any identifier value. */
	bool ContainsProperty(FPropertyId PropertyId) const;

	/** Whether the identifier resolves to a good type record. Safe for any identifier value. */
	bool ContainsGoodType(FGoodTypeId GoodTypeId) const;

	/** Whether the identifier resolves to an inventory record. Safe for any identifier value. */
	bool ContainsInventory(FInventoryId InventoryId) const;

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

	/**
	 * Reads a good type record, or returns an unset optional for an identifier that does not
	 * resolve. The result is a copy with the same contract as FindPerson.
	 */
	TOptional<FGoodTypeRecord> FindGoodType(FGoodTypeId GoodTypeId) const;

	/**
	 * Resolves a durable authored key to the runtime handle it currently maps to, or returns
	 * an unset optional when no good type carries that key.
	 *
	 * This is the bridge between the two identities: content and diagnostics name a good
	 * type by authored key, while inventory entries store the handle. The display name plays
	 * no part in resolution.
	 */
	TOptional<FGoodTypeId> FindGoodTypeIdByKey(FName AuthoredKey) const;

	/**
	 * Reads an inventory record, or returns an unset optional for an identifier that does not
	 * resolve. The result is a copy with the same contract as FindPerson, including a copy of
	 * the entry list, and is intended for inspection and tests.
	 *
	 * Callers that want one quantity should use GetQuantity, which copies nothing.
	 */
	TOptional<FInventoryRecord> FindInventory(FInventoryId InventoryId) const;

	/**
	 * Quantity of one good type held by one inventory.
	 *
	 * Returns an unset optional when either identifier does not resolve, and `0` when the
	 * inventory resolves and simply holds none of that good. The distinction matters: "no
	 * such inventory" and "none in stock" are different answers.
	 */
	TOptional<int32> GetQuantity(FInventoryId InventoryId, FGoodTypeId GoodTypeId) const;

	/** Number of person records held. Derived from storage; not an authoritative population. */
	int32 GetPersonCount() const { return PersonRecords.Num(); }

	/** Number of household records held. Derived from storage. */
	int32 GetHouseholdCount() const { return HouseholdRecords.Num(); }

	/** Number of settlement records held. Derived from storage. */
	int32 GetSettlementCount() const { return SettlementRecords.Num(); }

	/** Number of property records held. Derived from storage. */
	int32 GetPropertyCount() const { return PropertyRecords.Num(); }

	/** Number of good type records held. Derived from storage. */
	int32 GetGoodTypeCount() const { return GoodTypeRecords.Num(); }

	/** Number of inventory records held. Derived from storage. */
	int32 GetInventoryCount() const { return InventoryRecords.Num(); }

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
	 * Creates a positive quantity of a good type inside an inventory, creating the entry if
	 * the inventory held none of it.
	 *
	 * This is a creation boundary: the quantity did not exist before the call. It is the
	 * low-level mutation that production, harvest, and scenario seeding will eventually call.
	 * The caller must say why, and one creation audit record is appended on success.
	 * Rejected requests leave the inventory untouched and append nothing.
	 */
	EAddGoodsResult AddGoods(FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason);

	/**
	 * Destroys a positive quantity of a good type held by an inventory, which must hold at
	 * least that much. When the remaining quantity reaches zero the entry is removed rather
	 * than stored as a zero.
	 *
	 * This is a destruction boundary and the counterpart to AddGoods: the quantity ceases to
	 * exist rather than moving elsewhere. The caller must say why, and one destruction audit
	 * record is appended on success. Rejected requests leave the inventory untouched and
	 * append nothing.
	 */
	ERemoveGoodsResult RemoveGoods(
		FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason);

	/**
	 * Moves a positive quantity of a good type from one inventory to another, conserving
	 * total quantity exactly.
	 *
	 * Every condition is validated before anything is written, in this order: source
	 * resolves, destination resolves, good type resolves, quantity is positive, the two
	 * inventories differ, the source holds enough, and the destination has headroom. Any
	 * failure leaves *both* inventories exactly as they were. A transfer to the same
	 * inventory is rejected with SameInventory; it is never a silent no-op.
	 *
	 * A transfer creates and destroys nothing, so it takes no reason and appends no audit
	 * record. It does not route through AddGoods or RemoveGoods: it applies the same
	 * internal quantity mutations those operations use, so no future creation or destruction
	 * policy can reject half of an already validated transfer.
	 */
	ETransferGoodsResult TransferGoods(
		FInventoryId SourceInventoryId,
		FInventoryId DestinationInventoryId,
		FGoodTypeId GoodTypeId,
		int32 Quantity);

	/** Number of goods creation and destruction audit records held. */
	int32 GetGoodsAuditRecordCount() const { return GoodsAuditRecords.Num(); }

	/**
	 * Reads one audit record by position, oldest first, or returns an unset optional for an
	 * index outside the recorded range.
	 *
	 * Copies, like every other read: no caller receives a reference into audit storage, so
	 * the record of what was created and destroyed cannot be edited after the fact.
	 */
	TOptional<FGoodsAuditRecord> GetGoodsAuditRecord(int32 RecordIndex) const;

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

	const FGoodTypeRecord* ResolveGoodType(FGoodTypeId GoodTypeId) const;

	const FInventoryRecord* ResolveInventory(FInventoryId InventoryId) const;

	FPersonRecord* ResolvePersonMutable(FPersonId PersonId);

	FHouseholdRecord* ResolveHouseholdMutable(FHouseholdId HouseholdId);

	FSettlementRecord* ResolveSettlementMutable(FSettlementId SettlementId);

	FPropertyRecord* ResolvePropertyMutable(FPropertyId PropertyId);

	FInventoryRecord* ResolveInventoryMutable(FInventoryId InventoryId);

	/** The single authoritative person/household membership transition. Both sides or neither. */
	void SetPersonHousehold(FPersonRecord& PersonRecord, FHouseholdId NewHouseholdId);

	/** The single authoritative household/settlement transition. Both sides or neither. */
	void SetHouseholdSettlement(FHouseholdRecord& HouseholdRecord, FSettlementId NewSettlementId);

	/** The single authoritative household/property residence transition. Both sides or neither. */
	void SetHouseholdResidence(FHouseholdRecord& HouseholdRecord, FPropertyId NewPropertyId);

	/**
	 * The two authoritative inventory quantity mutations, and the only code that writes an
	 * entry list.
	 *
	 * These are unaudited and deliberately so: they express *movement of quantity into or
	 * out of one inventory*, which is creation, destruction, or half a transfer depending
	 * only on who calls them. Their callers decide which it was, and AddGoods and
	 * RemoveGoods are the only ones that call it creation or destruction.
	 *
	 * Both assume every precondition has already been validated, assert as much, and cannot
	 * fail. Keeping them incapable of failure is what lets TransferGoods apply two of them
	 * with no possibility of a half transfer.
	 */
	void ApplyGoodsAddition(FInventoryRecord& InventoryRecord, FGoodTypeId GoodTypeId, int32 Quantity);

	void ApplyGoodsRemoval(FInventoryRecord& InventoryRecord, FGoodTypeId GoodTypeId, int32 Quantity);

	/** Appends one creation or destruction audit record. Only AddGoods and RemoveGoods call it. */
	void RecordGoodsAudit(
		EGoodsAuditAction Action, FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason);

	bool ValidatePersonRecords(FString& OutFailureDescription) const;

	bool ValidateHouseholdRecords(FString& OutFailureDescription) const;

	bool ValidateSettlementRecords(FString& OutFailureDescription) const;

	bool ValidatePropertyRecords(FString& OutFailureDescription) const;

	bool ValidateGoodTypeRecords(FString& OutFailureDescription) const;

	bool ValidateInventoryRecords(FString& OutFailureDescription) const;

	TArray<FPersonRecord> PersonRecords;

	TArray<FHouseholdRecord> HouseholdRecords;

	TArray<FSettlementRecord> SettlementRecords;

	TArray<FPropertyRecord> PropertyRecords;

	TArray<FGoodTypeRecord> GoodTypeRecords;

	TArray<FInventoryRecord> InventoryRecords;

	TArray<FGoodsAuditRecord> GoodsAuditRecords;

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Test-only access to private storage, so that invariant detection can be proven against
	 * deliberately corrupted state.
	 *
	 * This exists because ValidateInvariants is worth nothing unless something demonstrates
	 * that it fails when state is wrong, and the public operations correctly make invalid
	 * state unreachable. It is compiled out of shipping builds, is declared nowhere else,
	 * and adds no production mutation path: the alternative would have been a public API for
	 * corrupting the registry, which would be far worse.
	 */
	friend struct FSimulationRegistryTestAccess;
#endif
};
