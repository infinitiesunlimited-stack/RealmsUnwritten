#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "Simulation/CurrentWork.h"
#include "Simulation/GoodTypeRecord.h"
#include "Simulation/GoodsAuditRecord.h"
#include "Simulation/HouseholdRecord.h"
#include "Simulation/InventoryRecord.h"
#include "Simulation/PersonRecord.h"
#include "Simulation/PhysicalSiteRecord.h"
#include "Simulation/PropertyRecord.h"
#include "Simulation/SettlementRecord.h"
#include "Simulation/SimulationIds.h"
#include "Simulation/SkillTypeRecord.h"
#include "Simulation/WorkTypeRecord.h"

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
 * Outcome of changing where an inventory physically is.
 *
 * Placing an inventory at a site, moving it between sites, and taking its place away are all
 * authoritative state changes. None of them is a journey: nothing is carried, no time passes,
 * and no route is required or checked.
 */
enum class EInventoryLocationResult : uint8
{
	/** The inventory's location changed; both sides are consistent. */
	Success,

	/** The inventory was already at the requested site; no state changed. */
	AlreadyAtSite,

	/** The inventory identifier does not resolve to a record; no state changed. */
	UnknownInventory,

	/** The physical site identifier does not resolve to a record; no state changed. */
	UnknownSite,

	/** The inventory is already nowhere, so there was no location to remove. No state changed. */
	NotLocated,

	/**
	 * The inventory holds goods, which may not be left without a place. Move it to another
	 * site, or empty it first. No state changed.
	 */
	InventoryNotEmpty
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

	/**
	 * The inventory is nowhere, so the goods would have come into existence with no place to
	 * be. Give the inventory a location first. No state changed.
	 */
	InventoryNotLocated,

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

	/** The source inventory is nowhere, so there is no place for goods to leave. No state changed. */
	SourceInventoryNotLocated,

	/** The destination inventory is nowhere, so there is no place for goods to arrive. No state changed. */
	DestinationInventoryNotLocated,

	/** The source does not hold that much of the good type; no state changed. */
	InsufficientQuantity,

	/** The destination cannot hold that much more without exceeding MaxGoodQuantity. */
	Overflow
};

/**
 * Outcome of changing a person's exclusive current work commitment.
 *
 * Assigning work, replacing it, and taking it away are all authoritative state changes.
 * None of them is physical presence: nothing walks, no time passes, and no route is
 * required or checked. The person is committed to work targeted at a site, not proven to
 * be at that site.
 */
enum class EPersonWorkResult : uint8
{
	/** The person's current work changed. */
	Success,

	/** The person already has exactly this work type at this site; no state changed. */
	AlreadyAssigned,

	/** The person identifier does not resolve to a record; no state changed. */
	UnknownPerson,

	/** The work type identifier does not resolve to a record; no state changed. */
	UnknownWorkType,

	/** The physical site identifier does not resolve to a record; no state changed. */
	UnknownSite,

	/** The person already has no work, so there was nothing to remove. No state changed. */
	NotAssigned
};

/**
 * Authoritative owner of person, household, settlement, property, good type, inventory,
 * physical site, work type, and skill type records.
 *
 * The registry is plain C++: no UObject, no Actor, no tick, no loaded map, and no
 * Blueprint exposure. Anything that needs a record resolves it by stable identifier
 * through this type; nothing scans the world.
 *
 * Reads return snapshots by value, so no caller ever holds an address into registry
 * storage. Each two-sided relationship is mutated only through its own pair of public
 * operations, each funnelling into a single private transition: person/household
 * membership, household/settlement location, household/property residence, and
 * inventory/site location. Current work is one-sided: it lives on the person and names a
 * site without the site listing workers, because assignment is not occupancy. Callers
 * therefore cannot edit a record directly, and cannot leave either side of a two-sided
 * relationship disagreeing with the other.
 *
 * Inventory contents are one-sided rather than a relationship, so they are mutated through
 * AddGoods, RemoveGoods, and TransferGoods. No caller receives a mutable inventory, so the
 * rules that quantities stay positive and that a good type appears at most once per
 * inventory cannot be bypassed.
 *
 * Goods are also located, not merely held. An inventory holding anything is at exactly one
 * resolvable physical site, so every quantity in the registry has a place: goods may only be
 * created in a located inventory, may only move between located inventories, and an
 * inventory cannot be made placeless while it holds anything.
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
	 * Creates a physical site on the given property, holding no inventories.
	 *
	 * The purpose key classifies what the site is for and must be supplied; it is not an
	 * identity, so several sites may share one. The display name is display data and is not
	 * validated, as elsewhere in the registry.
	 *
	 * Returns an invalid identifier when the property does not resolve or the purpose key is
	 * None. A rejected creation is atomic: it adds no record, allocates no identifier, and
	 * leaves the property's site list unchanged.
	 */
	FPhysicalSiteId CreatePhysicalSite(FPropertyId PropertyId, FName PurposeKey, const FString& DisplayName);

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

	/**
	 * Creates a work type under a durable authored key and returns its runtime handle.
	 *
	 * The authored key is the work type's definition identity and must be supplied by the
	 * caller: `None` is rejected, and so is a key already in use among work types. Good-type
	 * keys are a separate namespace. The display name is display data only, so two work
	 * types with different authored keys may share one.
	 *
	 * Returns an invalid handle, having changed nothing and consumed no runtime handle, when
	 * the authored key is None or already taken.
	 */
	FWorkTypeId CreateWorkType(FName AuthoredKey, const FString& DisplayName);

	/**
	 * Creates a skill type under a durable authored key and returns its runtime handle.
	 *
	 * The authored key is the skill type's definition identity. None and duplicate skill-type
	 * keys are rejected before allocation. Good-type and work-type keys are independent
	 * namespaces. The display name is display data only and need not be unique.
	 */
	FSkillTypeId CreateSkillType(FName AuthoredKey, const FString& DisplayName);

	/** Creates an empty inventory, holding no goods and located nowhere. */
	FInventoryId CreateInventory();

	/** Whether the identifier resolves to a person record. Safe for any identifier value. */
	bool ContainsPerson(FPersonId PersonId) const;

	/** Whether the identifier resolves to a household record. Safe for any identifier value. */
	bool ContainsHousehold(FHouseholdId HouseholdId) const;

	/** Whether the identifier resolves to a settlement record. Safe for any identifier value. */
	bool ContainsSettlement(FSettlementId SettlementId) const;

	/** Whether the identifier resolves to a property record. Safe for any identifier value. */
	bool ContainsProperty(FPropertyId PropertyId) const;

	/** Whether the identifier resolves to a physical site record. Safe for any identifier value. */
	bool ContainsPhysicalSite(FPhysicalSiteId PhysicalSiteId) const;

	/** Whether the identifier resolves to a good type record. Safe for any identifier value. */
	bool ContainsGoodType(FGoodTypeId GoodTypeId) const;

	/** Whether the identifier resolves to a work type record. Safe for any identifier value. */
	bool ContainsWorkType(FWorkTypeId WorkTypeId) const;

	/** Whether the identifier resolves to a skill type record. Safe for any identifier value. */
	bool ContainsSkillType(FSkillTypeId SkillTypeId) const;

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
	 * Reads a physical site record, or returns an unset optional for an identifier that does
	 * not resolve. The result is a copy with the same contract as FindPerson, including a copy
	 * of the inventory list.
	 */
	TOptional<FPhysicalSiteRecord> FindPhysicalSite(FPhysicalSiteId PhysicalSiteId) const;

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
	 * Reads a work type record, or returns an unset optional for an identifier that does not
	 * resolve. The result is a copy with the same contract as FindPerson.
	 */
	TOptional<FWorkTypeRecord> FindWorkType(FWorkTypeId WorkTypeId) const;

	/**
	 * Resolves a durable authored key to the runtime handle it currently maps to among work
	 * types, or returns an unset optional when no work type carries that key.
	 *
	 * Good-type keys are not consulted. The display name plays no part in resolution.
	 */
	TOptional<FWorkTypeId> FindWorkTypeIdByKey(FName AuthoredKey) const;

	/**
	 * Reads a skill type record, or returns an unset optional when the identifier does not
	 * resolve. The result is a detached copy and cannot mutate registry storage.
	 */
	TOptional<FSkillTypeRecord> FindSkillType(FSkillTypeId SkillTypeId) const;

	/**
	 * Resolves a durable authored key within the independent skill-type namespace.
	 * Display names and keys registered by other definition families are not consulted.
	 */
	TOptional<FSkillTypeId> FindSkillTypeIdByKey(FName AuthoredKey) const;

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

	/** Number of physical site records held. Derived from storage. */
	int32 GetPhysicalSiteCount() const { return PhysicalSiteRecords.Num(); }

	/** Number of good type records held. Derived from storage. */
	int32 GetGoodTypeCount() const { return GoodTypeRecords.Num(); }

	/** Number of work type records held. Derived from storage. */
	int32 GetWorkTypeCount() const { return WorkTypeRecords.Num(); }

	/** Number of skill type records held. Derived from storage. */
	int32 GetSkillTypeCount() const { return SkillTypeRecords.Num(); }

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
	 * Places the inventory at the physical site, removing it from any site it is currently at
	 * so that it is never at two at once.
	 *
	 * This is the authoritative statement of where an inventory is, not an act of carrying.
	 * Nothing is loaded, nothing travels, no distance or route is considered, and the goods
	 * inside simply go where the inventory goes. Physical hauling will be built from carriers,
	 * loading, travel, and unloading, and will use this only to record the result.
	 */
	EInventoryLocationResult AssignInventoryToSite(FInventoryId InventoryId, FPhysicalSiteId PhysicalSiteId);

	/**
	 * Takes away the inventory's location, leaving it nowhere, and removes it from its site's
	 * list.
	 *
	 * Permitted only while the inventory is empty. An inventory holding goods is rejected with
	 * InventoryNotEmpty, because goods may not exist without a place: to move them, assign the
	 * inventory to another site instead.
	 */
	EInventoryLocationResult RemoveInventoryLocation(FInventoryId InventoryId);

	/**
	 * Makes this the person's exclusive current work, replacing any commitment they already
	 * have so that they never hold two at once.
	 *
	 * This is the authoritative statement of what the person is assigned to do, not an act
	 * of travelling. Nothing walks, no presence is recorded, no distance or route is
	 * considered, and the person is not proven to be at the site. Household membership,
	 * settlement, and site purpose are not required and not checked. Goods are not created,
	 * moved, or destroyed.
	 */
	EPersonWorkResult AssignPersonWork(FPersonId PersonId, FWorkTypeId WorkTypeId, FPhysicalSiteId PhysicalSiteId);

	/**
	 * Takes away the person's current work, leaving it nowhere.
	 *
	 * Permitted only while the person currently has work. A person with none is rejected
	 * with NotAssigned. No site worker list is updated, because sites do not list workers.
	 */
	EPersonWorkResult RemovePersonWork(FPersonId PersonId);

	/**
	 * Creates a positive quantity of a good type inside an inventory, creating the entry if
	 * the inventory held none of it.
	 *
	 * This is a creation boundary: the quantity did not exist before the call. It is the
	 * low-level mutation that production, harvest, and scenario seeding will eventually call.
	 * The caller must say why, and one creation audit record is appended on success.
	 * Rejected requests leave the inventory untouched and append nothing.
	 *
	 * The inventory must already have a resolvable physical location. Goods cannot be brought
	 * into existence nowhere, and no site is created or assigned on the caller's behalf.
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
	 * inventories differ, both are located, the source holds enough, and the destination has
	 * headroom. Any failure leaves *both* inventories exactly as they were. A transfer to the
	 * same inventory is rejected with SameInventory; it is never a silent no-op.
	 *
	 * A transfer creates and destroys nothing, so it takes no reason and appends no audit
	 * record. It does not route through AddGoods or RemoveGoods: it applies the same
	 * internal quantity mutations those operations use, so no future creation or destruction
	 * policy can reject half of an already validated transfer.
	 *
	 * This moves goods between two places without simulating the journey between them. It is
	 * not transportation: no carrier, capacity, route, distance, or time is involved, and two
	 * arbitrarily distant sites can exchange goods instantly. Physical movement will be built
	 * as carrier loading, travel, and unloading, and will express itself through operations
	 * like this one rather than replacing them.
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

	const FPhysicalSiteRecord* ResolvePhysicalSite(FPhysicalSiteId PhysicalSiteId) const;

	const FGoodTypeRecord* ResolveGoodType(FGoodTypeId GoodTypeId) const;

	const FWorkTypeRecord* ResolveWorkType(FWorkTypeId WorkTypeId) const;

	const FSkillTypeRecord* ResolveSkillType(FSkillTypeId SkillTypeId) const;

	const FInventoryRecord* ResolveInventory(FInventoryId InventoryId) const;

	FPersonRecord* ResolvePersonMutable(FPersonId PersonId);

	FHouseholdRecord* ResolveHouseholdMutable(FHouseholdId HouseholdId);

	FSettlementRecord* ResolveSettlementMutable(FSettlementId SettlementId);

	FPropertyRecord* ResolvePropertyMutable(FPropertyId PropertyId);

	FPhysicalSiteRecord* ResolvePhysicalSiteMutable(FPhysicalSiteId PhysicalSiteId);

	FInventoryRecord* ResolveInventoryMutable(FInventoryId InventoryId);

	/** The single authoritative person/household membership transition. Both sides or neither. */
	void SetPersonHousehold(FPersonRecord& PersonRecord, FHouseholdId NewHouseholdId);

	/** The single authoritative household/settlement transition. Both sides or neither. */
	void SetHouseholdSettlement(FHouseholdRecord& HouseholdRecord, FSettlementId NewSettlementId);

	/** The single authoritative household/property residence transition. Both sides or neither. */
	void SetHouseholdResidence(FHouseholdRecord& HouseholdRecord, FPropertyId NewPropertyId);

	/**
	 * The single authoritative inventory/site location transition. Both sides or neither.
	 *
	 * Every location change goes through here, including the removal of a location, so an
	 * inventory can never be listed by a site it does not claim or claim a site that does not
	 * list it.
	 */
	void SetInventoryLocation(FInventoryRecord& InventoryRecord, FInventoryLocation NewLocation);

	/**
	 * The single authoritative person current-work write. Assignment is one-sided: the
	 * person names a site, and the site does not list workers, because commitment is not
	 * occupancy.
	 */
	void SetPersonWork(FPersonRecord& PersonRecord, const FCurrentWork& NewWork);

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

	/**
	 * Whether the inventory has a place that resolves, which is what goods require in order
	 * to exist. Shared by AddGoods and TransferGoods so that both mean exactly the same thing
	 * by "located".
	 */
	bool IsInventoryLocated(const FInventoryRecord& InventoryRecord) const;

	/** Appends one creation or destruction audit record. Only AddGoods and RemoveGoods call it. */
	void RecordGoodsAudit(
		EGoodsAuditAction Action, FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason);

	bool ValidatePersonRecords(FString& OutFailureDescription) const;

	bool ValidateHouseholdRecords(FString& OutFailureDescription) const;

	bool ValidateSettlementRecords(FString& OutFailureDescription) const;

	bool ValidatePropertyRecords(FString& OutFailureDescription) const;

	bool ValidatePhysicalSiteRecords(FString& OutFailureDescription) const;

	bool ValidateGoodTypeRecords(FString& OutFailureDescription) const;

	bool ValidateWorkTypeRecords(FString& OutFailureDescription) const;

	bool ValidateSkillTypeRecords(FString& OutFailureDescription) const;

	bool ValidateInventoryRecords(FString& OutFailureDescription) const;

	TArray<FPersonRecord> PersonRecords;

	TArray<FHouseholdRecord> HouseholdRecords;

	TArray<FSettlementRecord> SettlementRecords;

	TArray<FPropertyRecord> PropertyRecords;

	TArray<FPhysicalSiteRecord> PhysicalSiteRecords;

	TArray<FGoodTypeRecord> GoodTypeRecords;

	TArray<FWorkTypeRecord> WorkTypeRecords;

	TArray<FSkillTypeRecord> SkillTypeRecords;

	TArray<FInventoryRecord> InventoryRecords;

	TArray<FGoodsAuditRecord> GoodsAuditRecords;

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Test-only access to private storage, so that invariant detection can be proven against
	 * deliberately corrupted state.
	 *
	 * This exists because ValidateInvariants is worth nothing unless something demonstrates
	 * that it fails when state is wrong, and the public operations correctly make invalid
	 * state unreachable. It is compiled out of shipping builds, is declared only in
	 * Private/Tests/SimulationRegistryTestAccess.h, and adds no production mutation path: the
	 * alternative would have been a public API for corrupting the registry, which would be
	 * far worse.
	 */
	friend struct FSimulationRegistryTestAccess;
#endif
};
