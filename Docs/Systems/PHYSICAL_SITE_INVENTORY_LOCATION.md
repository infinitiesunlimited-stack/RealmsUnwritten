# Physical Site and Inventory Location

## Document status

- **Role:** Implementation record for the Prototype 0.1D physical-site and inventory-location foundation
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (0.1A), `SETTLEMENT_PROPERTY_RESIDENCE.md` (0.1B), and `GOODS_INVENTORY.md` (0.1C), none of which is superseded
- **Scope:** Stationary physical sites, the Property → Site relationship, and explicit inventory location
- **Describes:** Only what exists in the repository today
- **Retires:** The Prototype 0.1C holderless-inventory waiver recorded in `GOODS_INVENTORY.md`

## Purpose

This slice answers one new authoritative question:

**Where does an inventory physically exist?**

It introduces a stationary place — a physical site on a property — and an explicit location value on every inventory. Goods already had custody (they live in an inventory). They now also have a place, which is what DC-04 requires when it says physical goods must have quantities, locations, and custody.

The motivating structure is a late-medieval holding that can contain several distinct places without any of them being a building:

```text
Property
 ├── household storage site  → inventory
 ├── barn site               → inventory
 └── open yard site          → inventory
```

A site does not imply a building. A building may eventually provide several sites. Those are later slices.

## Authoritative simulation boundary

Unchanged: all state added here is plain C++ data owned by `FSimulationRegistry`. No `UObject`, `AActor`, subsystem, tick, loaded map, or Blueprint exposure.

A physical site is not a building, a room, a container, a warehouse, a field, a coordinate, an Unreal Actor, or a storage-capacity provider. It is a stationary simulation location at which physical things may exist.

An inventory's location is not ownership, not a journey, and not an environmental description.

## What a physical site is not

| Concern | Answered by | Status in 0.1D |
|---|---|---|
| Where is it? | Physical site | Implemented |
| What goods are present? | Inventory | Implemented in 0.1C, now located |
| What conditions exist there? | Future storage / environment | Deferred |
| Who owns or has a claim on it? | Future ownership / claims | Deferred |
| How do goods travel between places? | Future carrier / loading / travel / unloading | Deferred |

Ownership is explicitly separate. Locating an inventory at a site does not make the site, the property, or any household the owner of the goods. Capacity, weight, volume, weather, fire, theft, claims, rent, tithe, lots, reservations, buildings, carts, roads, fields, and coordinates are all absent.

## Entity ID types

`SimulationIds.h` gains one tag and alias:

```cpp
using FPhysicalSiteId = TSimulationId<FPhysicalSiteIdTag>;
```

The accepted `TMutuallyDistinct` predicate now covers seven families:

```cpp
static_assert(
    SimulationIdContract::TMutuallyDistinct<
        FPersonId, FHouseholdId, FSettlementId, FPropertyId,
        FGoodTypeId, FInventoryId, FPhysicalSiteId>::value,
    "Simulation identifier families must never be interchangeable with one another.");
```

`ToString` yields `PhysicalSite#4`. Value `0` never resolves. The raw-value constructor grants no capability.

## Physical site representation

`Private/Simulation/PhysicalSiteRecord.h`:

| Field | Type | Role |
|---|---|---|
| `Id` | `FPhysicalSiteId` | Runtime identity assigned at creation |
| `PropertyId` | `FPropertyId` | Authoritative forward relationship. Always valid. |
| `PurposeKey` | `FName` | Classifying metadata for the instantiated site. Never `None`. |
| `DisplayName` | `FString` | Presentation only. Never identity. |
| `Inventories` | `TArray<FInventoryId>` | Registry-maintained reverse index over inventory location |

A property may contain zero, one, or many sites. A site belongs to exactly one property. This slice has no operation that moves a site between properties.

### PurposeKey

`PurposeKey` is descriptive classification, not durable definition identity in the sense of `FGoodTypeRecord::AuthoredKey`. Several sites may share one purpose key:

```text
Site #4  PurposeKey = Test.BarnStorage
Site #9  PurposeKey = Test.BarnStorage
```

That is valid: two barn-like places, two identities. There is no `EPhysicalSiteType` enum, no authored site-definition catalogue, and no production-code purpose key. Tests use keys such as `Test.BarnStorage` as test data only.

Display names are not unique and are not validated, matching every other display string in the registry.

## Property → site relationship

`FPropertyRecord` gains:

```cpp
TArray<FPhysicalSiteId> PhysicalSites;
```

`FPhysicalSiteRecord::PropertyId` is the authoritative forward statement. `Property.PhysicalSites` is controlled denormalization, the same pattern as `Settlement.Properties` over `Property.SettlementId`. Both directions are written by `CreatePhysicalSite` and checked by `ValidateInvariants`. They can be rebuilt from the site records.

A new property starts with an empty site list. Occupancy (`ResidentHouseholdId`) is unchanged and remains distinct from sites: who lives on a property is not where goods sit.

## Inventory location representation

`Private/Simulation/InventoryLocation.h`:

```cpp
enum class EInventoryLocationKind : uint8
{
    None,
    PhysicalSite
};

struct FInventoryLocation
{
    static FInventoryLocation Nowhere();
    static FInventoryLocation AtPhysicalSite(FPhysicalSiteId PhysicalSiteId);

    bool IsLocated() const;

    EInventoryLocationKind Kind = EInventoryLocationKind::None;
    FPhysicalSiteId PhysicalSiteId;
};
```

`FInventoryRecord` is now:

```text
Id
Location
Entries
```

Authoritative registry mutation paths construct and install locations through `AssignInventoryToSite`, `RemoveInventoryLocation`, and the factory functions `Nowhere()` / `AtPhysicalSite()`. Those paths do not independently assign a mismatched kind and identifier. The fields themselves are public: callers may construct or locally edit a detached `FInventoryLocation`, including a snapshot returned by `FindInventory`. Doing so does not mutate authoritative registry state, because registry storage is private and snapshots are copies. `ValidateInvariants` is the backstop if authoritative storage is ever corrupted into a `None` location that names a site, or an unsupported kind.

### Why this remains extensible without speculative variants

The location is an explicit tagged domain value, not a raw `FPhysicalSiteId` and not an untyped `HolderId`. The only supported kinds are `None` and `PhysicalSite`.

A future mobile holder — a person, a cart, a pack animal, a ship — becomes a new named kind with its own typed identifier field, decided when that holder type is actually built. Adding that kind does not require turning the location into a generic identifier bag, a variant/container abstraction, or an inheritance hierarchy of speculative holders. Those alternatives are not present as placeholders.

Stationary property and settlement location are derived:

```text
Inventory → PhysicalSite → Property → Settlement
```

None of those IDs is duplicated onto the inventory, so they cannot disagree with the site.

## None versus PhysicalSite

| Inventory state | Location | Entries | Valid? |
|---|---|---|---|
| Newly created | `None` | empty | Yes |
| Assigned to a site, empty | `PhysicalSite` | empty | Yes |
| Assigned to a site, holding goods | `PhysicalSite` | non-empty | Yes |
| Nowhere, holding goods | `None` | non-empty | **Invalid** |

`CreateInventory` still creates an empty, unlocated inventory. That is construction order: a site can be created, then an inventory, then the two joined. Goods cannot be created until the join has happened.

The retired 0.1C waiver allowed populated inventories with no place. That exception is closed.

## Site ↔ inventory relationship

Authoritative: `Inventory.Location` → physical site.

Controlled reverse: `PhysicalSite.Inventories` → inventory IDs.

If inventory #10 is at site #4, site #4 lists inventory #10 exactly once. If site #4 lists inventory #10, inventory #10 resolves and reports site #4. An inventory is listed by at most one site. A site may contain several inventories; one-site/one-inventory is deliberately not enforced. Two storage points at the same barn are two inventories at one site.

Both directions are written by one private transition, `SetInventoryLocation`, including the removal of a location.

## Registry operations

`FSimulationRegistry` now owns a seventh entity family, `PhysicalSiteRecords`.

```cpp
FPhysicalSiteId CreatePhysicalSite(FPropertyId PropertyId, FName PurposeKey, const FString& DisplayName);

bool ContainsPhysicalSite(FPhysicalSiteId PhysicalSiteId) const;
TOptional<FPhysicalSiteRecord> FindPhysicalSite(FPhysicalSiteId PhysicalSiteId) const; // snapshot copy
int32 GetPhysicalSiteCount() const;

EInventoryLocationResult AssignInventoryToSite(FInventoryId InventoryId, FPhysicalSiteId PhysicalSiteId);
EInventoryLocationResult RemoveInventoryLocation(FInventoryId InventoryId);
```

Reads follow the accepted snapshot contract. `FindPhysicalSite` copies the inventory list. Inventory location is read through `FindInventory` as part of the inventory snapshot. No public API exposes a mutable pointer into site or inventory storage.

### CreatePhysicalSite

Requires a resolvable property and a non-`None` purpose key. Display name is not validated. Failure is atomic: no record, no identifier consumed, property list unchanged. Success appends the site exactly once to `Property.PhysicalSites`. The new site holds no inventories.

### AssignInventoryToSite

Places an inventory at a site. This is an authoritative state-management primitive, **not hauling and not physical travel**. Nothing is loaded, nothing travels, no distance or route is considered. Goods inside the inventory simply go where the inventory is recorded to be. Future physical gameplay will use carrier loading, travel, and unloading, and will use this operation only to record the result.

| Outcome | Meaning |
|---|---|
| `Success` | Inventory is at the site; both directions agree |
| `AlreadyAtSite` | Already there; no state changed |
| `UnknownInventory` | Inventory does not resolve |
| `UnknownSite` | Site does not resolve |

Assigning an unlocated inventory locates it. Relocating from site A to site B removes the old reverse entry and adds the new one. The final state is exactly one valid location. Failed assignment leaves prior state unchanged. Duplicate reverse entries are not created.

A populated inventory may relocate. Goods are not created, destroyed, or left behind.

### RemoveInventoryLocation

| Inventory | Result |
|---|---|
| Empty and located | `Success`: reverse entry removed, location becomes `None` |
| Empty and already nowhere | `NotLocated` |
| Holds any goods | `InventoryNotEmpty`; nothing changes |
| Unknown | `UnknownInventory` |

A populated inventory can never be intentionally made locationless.

### AddGoods location rule

Successful `AddGoods` additionally requires a valid resolvable physical location. An empty unlocated inventory is rejected with `InventoryNotLocated`: no goods created, no site auto-created or auto-assigned, no audit record. A located inventory keeps the 0.1C creation and audit behaviour: exactly one `Created` record on success.

Failed location validation participates in the same atomic boundary as every other `AddGoods` rejection.

### RemoveGoods

Unchanged. A valid populated inventory already has a location under the new invariants. No extra policy.

### TransferGoods location rule

Both source and destination must have valid resolvable physical locations. Either end unlocated or invalidly located is rejected atomically: quantities unchanged, no audit record.

New outcomes, after `SameInventory` and before quantity checks:

| Outcome | Meaning |
|---|---|
| `SourceInventoryNotLocated` | Source is nowhere |
| `DestinationInventoryNotLocated` | Destination is nowhere |

Conservation and the rule that transfers produce no creation/destruction audit records are unchanged.

**`TransferGoods` is not transportation.** It is an authoritative inventory state operation. Two arbitrarily distant sites can exchange goods instantly. No carrier, capacity, route, distance, or time is involved. Future physical movement will be built as carrier loading, travel, and unloading rather than as arbitrary distant direct transfers.

## Retired 0.1C waiver

Prototype 0.1C was granted a time-bounded waiver:

> "Prototype 0.1C may contain holderless inventories solely as a headless foundation and test abstraction. This exception expires before any inventory participates in gameplay, production, harvesting, consumption, hauling, markets, trade, or other physical world simulation. Inventory holder/location semantics must be resolved before such integration."

Prototype 0.1D retires that waiver. The constitutional rule now in force:

- Empty inventories may temporarily have `Location = None`.
- Any inventory containing authoritative goods must have exactly one valid physical location.
- Gameplay systems may not create holderless goods.

Inventories are still not attached to a Person, Household, Building, Actor, or speculative mobile holder. Stationary location is resolved; mobile custody is not. That remaining gap is a deferred system, not a waiver against DC-04's location requirement for goods at rest.

## Snapshot semantics

`FindPhysicalSite` returns a copy, including a copy of `Inventories`. Later site creation that reallocates dense storage does not invalidate a held snapshot, and editing a snapshot does not mutate authoritative state. Inventory snapshots now include `Location` with the same contract.

## Invariant summary

`ValidateInvariants` preserves every 0.1A, 0.1B, and 0.1C check and adds:

**Physical site identity**

- Site slot / ID mismatch
- Missing purpose key

**Site → property**

- Site references an unknown property
- Property reverse list missing the site, contains an unknown site, contains a duplicate, or disagrees with `Site.PropertyId`

**Site → inventory**

- Site lists an unknown inventory
- Site lists the same inventory more than once
- Site lists an inventory whose location disagrees
- The same inventory appearing in more than one site's reverse list is detected as disagreement on at least one of those sites

**Inventory → site**

- `PhysicalSite` location references an unknown site
- Inventory says `PhysicalSite` but the site does not list it exactly once
- `None` that still names a site, or an unsupported kind

**Goods location rule**

- Any inventory containing goods while `Location = None` is invalid

It remains diagnostic and never repairs.

## Tests

`Private/Tests/PhysicalSiteInventoryLocationTests.cpp`, plus migrations of existing goods tests so every populated inventory is constructed as Settlement → Property → PhysicalSite → Inventory.

New tests:

| Test | Covers |
|---|---|
| `PhysicalSite.Creation` | Valid site; property reverse list; multiple sites per property; shared purpose keys; invalid property atomicity across extreme IDs; missing purpose key; no ID consumed; extreme site IDs fail lookup |
| `PhysicalSite.SnapshotRegression` | Held site snapshot survives 64 later sites and a later inventory assignment; editing the snapshot changes no authoritative field |
| `Inventory.LocationAssignment` | Empty inventory `None` → site, both directions agree; re-assignment is `AlreadyAtSite`; two inventories at one site; unknown inventory/site leave prior state unchanged |
| `Inventory.Relocation` | Site A → site B atomically; old reverse removed; new reverse added; goods unchanged |
| `Inventory.LocationRemoval` | Empty unassignment succeeds; populated unassignment is `InventoryNotEmpty` with goods, location, reverse list, and audit count unchanged |
| `Goods.LocationGate` | Unlocated `AddGoods` rejected with no goods and no audit; located `AddGoods` succeeds with one `Created` audit; unlocated transfer rejected with quantities and audit unchanged; located transfer conserves quantity and appends no audit |
| `PhysicalSite.InvariantDetection` | Direct detection of bad site slot ID, unknown property, duplicate property reverse site, unknown site inventory, duplicate site inventory, inventory/site disagreement, same inventory listed by two sites, populated unlocated inventory, and invalid site location reference |

Existing 0.1A, 0.1B, and 0.1C tests continue to pass after the goods-test migration. Empty-inventory creation tests still leave inventories unlocated.

## Explicit non-goals

Not implemented, and not scaffolded:

- Ownership, claims, deeds, rent, tithe
- Buildings, rooms, functional spaces, construction
- Carts, pack animals, ships, people-as-carriers
- Roads, coordinates, geometry, movement, hauling, pathfinding
- Production, farming, labor, markets
- Storage capacity, weight, volume
- Spoilage, weather, fire, theft
- Lots, reservations
- UI, Actors, Components, UObject simulation entities
- Authored site-definition catalogues
- Site reassignment between properties
- Generic `HolderId` or speculative location kinds

## Future mobile-holder extensibility

Documented, not implemented. The intended later shape is a deliberate additional kind:

```text
Inventory → PhysicalSite          (0.1D)
Inventory → mobile carrier        (later)
```

where a mobile carrier might eventually be a person, a cart, a pack animal, or a ship. Each would be a named kind with its own typed identifier. None of those IDs, kinds, or holder types exists now.

## Future scaling concerns

1. Site creation and inventory assignment use the same dense-array model as 0.1A–0.1C. Reverse lists are scanned linearly at prototype sizes.
2. `IsInventoryLocated` resolves the named site. Fine while site counts stay small.
3. Growing `PhysicalSiteRecords` copies records that themselves carry inventory arrays, the same reallocation pattern already documented for inventories.
4. Derived settlement stock still means walking inventories via sites and properties. No cached totals.

## Open architectural questions

1. **When a building exists, does it create sites or merely occupy a property that already has them?** This slice treats sites as belonging to properties, so a building can later expose, own, or sit among sites without inventories having to change holder. The choice is deferred until a building record exists.
2. **Where does mobile custody live?** Same tagged location value, new kind, when the first carrier exists. Not a generic holder field.
3. **Does environment/capacity attach to the site, the inventory, or a later storage-space record?** The site currently answers only "where". Conditions and limits should not be smuggled onto it in advance.
