# Goods and Inventory

## Document status

- **Role:** Implementation record for the Prototype 0.1C goods and inventory foundation
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (0.1A) and `SETTLEMENT_PROPERTY_RESIDENCE.md` (0.1B), neither of which is superseded
- **Extended by:** `PHYSICAL_SITE_INVENTORY_LOCATION.md` (Prototype 0.1D), which locates inventories at physical sites and retires the holderless-inventory waiver; `WORK_LABOR.md` (Prototype 0.1E), which adds work types as a second definition family; and `SKILLS_KNOWLEDGE.md` (Prototype 0.1F), which adds skill types as a third. Work and Skill Type registration do not mutate goods.
- **Scope:** Good types, inventories, quantities, conserved transfer, and the goods audit trail
- **Describes:** Only what exists in the repository today
- **Revision:** Updated after Prototype 0.1D: inventories carry an explicit location; `AddGoods` and `TransferGoods` require a resolvable physical site; the 0.1C holderless-inventory waiver is retired

## Purpose

This slice begins `PROTOTYPE_0_1_SCOPE.md` step 3 ("goods and custody") by giving physical goods somewhere to be. It implements the state layer behind the `GAME_VISION.md` principle that goods have a place and a path — originally the custody half of that place. Prototype 0.1D supplies the location half: an inventory holding goods must sit at a physical site. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`.

It answers four questions, all by stable identifier, with no world, Actor, or map involved:

| Question | Answer path |
|---|---|
| What kinds of good exist? | `FGoodTypeRecord`, created at runtime under a durable authored key |
| How much of a good does an inventory hold? | `GetQuantity(InventoryId, GoodTypeId)` |
| What does an inventory hold in total? | `FInventoryRecord::Entries` |
| How do goods move without being created or destroyed? | `TransferGoods`, which conserves quantity exactly |
| Where did quantity come from, and where did it go? | The goods audit trail, appended to by `AddGoods` and `RemoveGoods` |

The point of the slice is what it makes *impossible*: there is no `Settlement.Wood`, no `Household.Wheat`, no `Property.Firewood`. A quantity of a good exists only inside an explicit inventory, which is what DC-04 demands when it forbids global resource counters as the authoritative store.

Nothing hauls, produces, farms, consumes, prices, reserves, or routes anything. Those are later slices.

## Authoritative simulation boundary

Unchanged from 0.1A and 0.1B: all state added here is plain C++ data owned by `FSimulationRegistry`, which is not a `UObject`, `AActor`, or subsystem, has no `Tick`, requires no loaded map, and is not exposed to Blueprint. The `static_assert(!std::is_base_of_v<UObject, FSimulationRegistry>)` in the 0.1A test file still guards it, and every test in this slice constructs its registry on the stack.

Two boundary points are specific to goods:

- **An inventory is not a container Actor, a chest, a storage widget, or a warehouse mesh.** It is a custody boundary identified by `FInventoryId`.
- **A good type is not an item asset and not a C++ enum.** Wheat, flour, bread, and firewood are created at runtime under authored keys like anything else, so authored content can define good types later without a new enum per resource. The test suite creates them as test data; production code names no specific good.

The one thing this slice records *about the past* rather than the present is the goods audit trail, which exists because DC-04 requires creation and destruction of goods to be auditable in development builds. It is diagnostic state, not gameplay state.

## Module layout

Files added by this slice, in the accepted module layout:

```text
Source/RealmsUnwritten/Private/Simulation/GoodTypeRecord.h
Source/RealmsUnwritten/Private/Simulation/InventoryRecord.h
Source/RealmsUnwritten/Private/Simulation/GoodsAuditRecord.h
Source/RealmsUnwritten/Private/Tests/GoodsInventoryTests.cpp
```

Files extended: `SimulationIds.h`, `SimulationRegistry.{h,cpp}`. **`RealmsUnwritten.Build.cs` is unchanged**, no module was added, and no 0.1A or 0.1B record type was modified.

## Entity ID types

`SimulationIds.h` gains two tags and two aliases on the existing template:

```cpp
using FGoodTypeId  = TSimulationId<FGoodTypeIdTag>;
using FInventoryId = TSimulationId<FInventoryIdTag>;
```

They inherit every property documented in 0.1A: phantom-typed, `explicit` raw-value construction that grants no capability, value `0` meaning "no entity", allocation by the registry only, hashable, and never a name. `ToString` yields `GoodType#3` / `Inventory#7`.

Type safety needed no new mechanism. The accepted `TMutuallyDistinct` predicate covers later families by being handed a longer list. Prototype 0.1F currently asserts nine families including `FSkillTypeId`.

### Good type identity: durable key, runtime handle

The independent review found that a dense insertion-order `FGoodTypeId` is acceptable as a runtime lookup handle but cannot serve as the durable identity of a definition. The architectural decision that followed was to separate the two roles explicitly, which is what the implementation now does:

```text
AuthoredKey (FName, e.g. Goods.Wheat)     durable definition identity
        |
        |  FindGoodTypeIdByKey
        v
FGoodTypeId (dense runtime handle)        valid within one registry
        |
        |  stored in
        v
FInventoryEntry                           runtime inventory contents
```

| | Authored key | Runtime handle |
|---|---|---|
| Type | `FName` | `FGoodTypeId` |
| Chosen by | Whoever defines the good | The registry |
| Depends on creation order | No | Yes |
| Unique | Enforced; duplicates rejected | Enforced by slot allocation |
| Role | Definition identity; what content and a future save format refer to | Cheap handle for lookup, comparison, and storage in inventory entries |
| Durable across runs | Intended to be | No |

`FGoodTypeId` keeps exactly the semantics it had: a dense, slot-derived, phantom-typed handle, non-interchangeable with the other five families, valid for the lifetime of one registry. Nothing about inventory entry storage changed. What changed is that the handle is no longer claimed to *be* the definition's identity.

Why an `FName` for the key. It is the smallest representation in the engine that is a stable, comparable, content-authored name rather than a position: cheap to compare and copy, already interned, trivially readable in a diagnostic, and the natural key for content to name a definition with. The alternatives were a `FString` (heavier, no cheap equality, and invites case and whitespace ambiguity), an `FGuid` (durable but unreadable and not authorable by hand, and its real advantage is uniqueness across independently generated data sets, which no content pipeline yet exists to need), and a second `TSimulationId` family (which would have repeated the original problem, since any registry-allocated integer depends on allocation order).

Why it is stable relative to insertion order: the key is supplied by the caller and stored verbatim, and no part of the registry derives, rewrites, or renumbers it. Creating the same set of good types in a different order gives each key a different runtime handle but leaves every key mapping to the same definition — which is asserted directly by building two registries in opposite orders in `Goods.AuthoredKeyIdentity`.

This satisfies `HIGH_LEVEL_ARCHITECTURE.md`'s requirement that "definitions such as `Wheat`, `Bread`, or a building archetype use separate definition IDs from runtime entity IDs" in the stronger sense: the definition identity is now a different kind of thing from an entity identifier, not merely a differently tagged one.

**No persistence exists.** The authored key is the identity a save format or content catalogue *would* record, but nothing serializes it, no data asset defines one, no catalogue loads one, and no migration reads one. This slice establishes the identity boundary only. The examples `Goods.Wheat`, `Goods.Flour`, and `Goods.Bread` appear exclusively in tests; production code contains no good type key.

## Good type representation

`Private/Simulation/GoodTypeRecord.h`:

| Field | Type | Justification |
|---|---|---|
| `AuthoredKey` | `FName` | Durable definition identity, per the architectural decision above. Data model "stable definition ID"; constitution integrity rule 1. Never `None`, never duplicated. |
| `Id` | `FGoodTypeId` | Runtime handle, used by inventory entries. Dense and cheap; not durable. |
| `Name` | `FString` | Display name. Identity for nothing: two good types with different authored keys may share a display name, asserted directly in the tests. |

Two identifiers on one record is one more than the previous revision had, and the redundancy is deliberate: it is what allows a compact runtime handle in every inventory entry without that handle being mistaken for the definition's identity.

No other field is required by anything in this slice, so none was added. The data model's resource definition eventually carries unit of measure and precision rules, category, density and volume, quality dimensions, perishability, tags, and allowed process references. Every one of them is either forbidden by the assignment or unusable without a system that does not exist (units, quality, spoilage, production). Adding any of them now would create an unowned field, which `PROTOTYPE_0_1_SCOPE.md` forbids: "An out-of-scope system may be represented by a narrow interface or data field. It must not be partially built without a scope amendment."

## Inventory representation

`Private/Simulation/InventoryRecord.h`:

| Field | Type | Justification |
|---|---|---|
| `Id` | `FInventoryId` | Stable identity. Data model "stable inventory ID". |
| `Location` | `FInventoryLocation` | Added by Prototype 0.1D. `None` or `PhysicalSite`. Populated inventories must be located. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`. |
| `Entries` | `TArray<FInventoryEntry>` | Data model "contained lot references **or equivalent compact lot records**". |

`FInventoryEntry` is a `FGoodTypeId` and an `int32` quantity. An entry exists only while the inventory holds a positive amount of that good type.

### Why an entry array rather than a TMap

`TMap<FGoodTypeId, int32>` was the main alternative and would have made duplicate entries unrepresentable. The entry array was chosen for three reasons:

1. **Deterministic order.** Entry order is insertion order, and removal uses order-preserving `RemoveAt`. `TMap` iteration order depends on hashing and on the history of insertions and removals, which is a poor foundation for the serialization, state checksums, and diagnostic summaries that `HIGH_LEVEL_ARCHITECTURE.md` expects. Determinism was worth more here than the duplicate-prevention that `TMap` would have given for free.
2. **Compactness at the expected size.** Prototype inventories hold a handful of good types. A contiguous array of two 8-byte entries is the layout the architecture asks for ("structure high-volume authoritative records for batch iteration and compact storage"), and it matches the dense-array approach already accepted for entity storage.
3. **Consistency with the accepted design.** 0.1A and 0.1B already maintain list consistency through a single mutation path plus an invariant check. Duplicate entries are representable in an array, so they are prevented by the mutation operations and detected by `ValidateInvariants` — the same contract used for household member lists and settlement index lists.

The cost is that entry lookup is linear in the number of *distinct good types in that one inventory*, not in the number of goods or inventories in the world. That is recorded under future scaling concerns with the measurement that would change the decision.

Entry order is deterministic but deliberately **not part of the contract**. No test asserts a quantity by entry position; quantities are read through `GetQuantity`, and the two places a test inspects `Entries[0]` do so only after asserting the list has exactly one element.

## Quantity representation

Quantities are **`int32`**, in abstract integral simulation units.

- **Integral, not floating-point.** Physical goods are counted deterministically; floating-point accumulation would make conservation approximate, and an exact conservation guarantee is the whole point of `TransferGoods`.
- **Signed, deliberately.** The mutation API must be able to *receive* a negative request and reject it. With an unsigned parameter, a caller passing `-1` would silently arrive as a huge positive quantity at the API boundary, which is the same class of hazard as the identifier narrowing corrected in 0.1A. Signed parameters make "reject a negative quantity" expressible and testable, including `MIN_int32`.
- **32-bit, as the smallest appropriate width.** `MaxGoodQuantity` is `MAX_int32`, roughly 2.1 billion units of one good in one inventory, which is far beyond anything the prototype's scenario can reach. `int64` would double the entry size for headroom no system needs yet.

Widening the quantity type later is a deliberate change with real reach, not the one-line edit an earlier revision of this document claimed. It would affect the stored entry type and `MaxGoodQuantity`, the public query and mutation contracts (`GetQuantity`, `AddGoods`, `RemoveGoods`, `TransferGoods`, and the audit record) and therefore every caller, the boundary tests that assert behaviour at the maximum, and — once persistence exists — the serialized representation and its migration. The overflow checks themselves are written as headroom comparisons and would survive unchanged, which is the one part that is genuinely cheap. `int32` is not being changed now.

**One quantity unit is an abstract integral simulation unit.** It is not a kilogram, a litre, a loaf, or a piece. Its physical interpretation belongs to the resource definition's unit-of-measure and precision rules, which arrive with authored content. No unit conversion exists, and no operation assumes a unit; this slice deliberately does not pretend that a unit of wheat and a unit of firewood are commensurable.

Stored quantities are always strictly positive. Zero is not stored: reaching zero removes the entry, so "holds none" and "has no entry" are the same state and cannot disagree.

## Registry operations

`FSimulationRegistry` was extended, not replaced, and as of Prototype 0.1D owns seven entity families. Physical site storage is documented in `PHYSICAL_SITE_INVENTORY_LOCATION.md`.

```cpp
TArray<FPersonRecord>     PersonRecords;
TArray<FHouseholdRecord>  HouseholdRecords;
TArray<FSettlementRecord> SettlementRecords;
TArray<FPropertyRecord>     PropertyRecords;
TArray<FPhysicalSiteRecord> PhysicalSiteRecords;
TArray<FGoodTypeRecord>     GoodTypeRecords;
TArray<FInventoryRecord>  InventoryRecords;

TArray<FGoodsAuditRecord> GoodsAuditRecords;  // diagnostic, not an entity family
```

Every accepted principle is preserved: one dense array per family, identifier value equals slot plus one, the same unsigned-before-narrowing range check resolves all identifier types, lookup is a range check plus an index read, no Actor dependency, no `UObject` per entity, no per-entity tick, no world scans, no stored totals, and no public pointer or mutable reference into storage.

```cpp
FGoodTypeId  CreateGoodType(FName AuthoredKey, const FString& DisplayName);
FInventoryId CreateInventory();

bool ContainsGoodType(FGoodTypeId GoodTypeId) const;
bool ContainsInventory(FInventoryId InventoryId) const;

TOptional<FGoodTypeRecord>  FindGoodType(FGoodTypeId GoodTypeId) const;    // snapshot copy
TOptional<FInventoryRecord> FindInventory(FInventoryId InventoryId) const; // snapshot copy
TOptional<FGoodTypeId>      FindGoodTypeIdByKey(FName AuthoredKey) const;  // key -> handle

TOptional<int32> GetQuantity(FInventoryId InventoryId, FGoodTypeId GoodTypeId) const;

int32 GetGoodTypeCount() const;   // derived from storage
int32 GetInventoryCount() const;  // derived from storage

EAddGoodsResult      AddGoods(FInventoryId, FGoodTypeId, int32 Quantity, FName Reason);
ERemoveGoodsResult   RemoveGoods(FInventoryId, FGoodTypeId, int32 Quantity, FName Reason);
ETransferGoodsResult TransferGoods(FInventoryId Source, FInventoryId Destination, FGoodTypeId, int32 Quantity);

int32 GetGoodsAuditRecordCount() const;
TOptional<FGoodsAuditRecord> GetGoodsAuditRecord(int32 RecordIndex) const;  // snapshot copy
```

`CreateGoodType` now requires the authored key, and `AddGoods` and `RemoveGoods` now require a reason. Prototype 0.1D additionally requires a resolvable physical location for successful `AddGoods` and for both ends of a successful `TransferGoods`. Transfers still neither take a reason nor append an audit record.

`ValidateInvariants` keeps its signature. No 0.1A or 0.1B operation changed name, signature, or behaviour.

`CreateInventory` still takes nothing and cannot fail: it creates an empty inventory located nowhere. Location is assigned afterwards. `CreateGoodType` can fail, and does so the way 0.1B's `CreateProperty` does: it returns an invalid handle, having stored nothing.

### Good type creation and resolution

Creation requires an authored key and rejects two cases, both validated before anything is written:

| Case | Result |
|---|---|
| Authored key is `None` | Invalid handle; nothing stored |
| Authored key already in use | Invalid handle; nothing stored |
| Duplicate display name, distinct key | **Allowed**; a new good type with its own handle |

A rejected creation consumes no runtime handle, which the tests prove by rejecting a duplicate and then confirming the next successful creation receives the handle the duplicate would have taken. Nothing is left half-registered and no gap appears in the handle sequence.

`FindGoodTypeIdByKey` resolves a key to the current runtime handle, and returns an unset optional for an unknown key or for `None`. It consults only authored keys: a display name passed to it resolves to nothing, which is asserted directly. Resolution is a scan of the good type records, which is the simplest implementation that cannot disagree with the records themselves — a separate key-to-handle index would be a second copy of the same relationship, needing its own consistency invariant, for a container holding a handful of entries. That trade is recorded under scaling concerns, not baked in.

## Mutation result behaviour

Three operation-specific enums, following the accepted style in which rejections are reported rather than swallowed and only `Success` changes state:

| Enum | Values |
|---|---|
| `EAddGoodsResult` | `Success`, `UnknownInventory`, `UnknownGoodType`, `InvalidQuantity`, `InvalidReason`, `InventoryNotLocated`, `Overflow` |
| `ERemoveGoodsResult` | `Success`, `UnknownInventory`, `UnknownGoodType`, `InvalidQuantity`, `InvalidReason`, `InsufficientQuantity` |
| `ETransferGoodsResult` | `Success`, `UnknownSourceInventory`, `UnknownDestinationInventory`, `UnknownGoodType`, `InvalidQuantity`, `SameInventory`, `SourceInventoryNotLocated`, `DestinationInventoryNotLocated`, `InsufficientQuantity`, `Overflow` |

Each enum contains exactly the outcomes its own operation can produce, so a caller's `switch` has no unreachable cases: adding cannot be insufficient, removing cannot overflow, transfer has no reason to reject, and only transfer has two inventories to confuse. Transfer distinguishes an unknown *source* from an unknown *destination*, because "which end was wrong" is the first thing a caller needs to know.

Naming follows the accepted convention: `Unknown*` when an identifier does not resolve, `Invalid*` when a value fails validation.

## Add and remove semantics

`AddGoods` requires a resolvable inventory, a resolvable good type, a strictly positive quantity, a reason, a resolvable physical location, and enough headroom below `MaxGoodQuantity`. An unlocated inventory is rejected with `InventoryNotLocated`: no goods are created, no site is auto-assigned, and no audit record is appended. It creates the entry when the inventory held none of that good, and accumulates into the existing entry otherwise — never creating a second entry for the same good type.

`RemoveGoods` requires a resolvable inventory, a resolvable good type, a strictly positive quantity, a reason, and a holding of at least that much. An inventory holding none of a good type has no entry at all, which is reported as `InsufficientQuantity` rather than as a separate "no entry" outcome, because the caller's situation is identical. When the remaining quantity reaches zero the entry is removed rather than stored as a zero.

Both validate fully before writing anything, so a rejected request leaves the inventory byte-for-byte as it was and appends nothing to the audit trail. On success, each appends exactly one audit record.

### These two are the integration boundary

`AddGoods` creates simulation quantity out of nothing and `RemoveGoods` destroys it. That is deliberate and is the one place in this slice where conservation does not apply, because these are the low-level controlled mutations that future systems will call:

| Future caller | Operation |
|---|---|
| Scenario initialization and seeding | `AddGoods` |
| Harvest completion | `AddGoods` |
| Production output | `AddGoods` |
| Production input consumption | `RemoveGoods` |
| Household consumption of food | `RemoveGoods` |
| Spoilage, loss, and destruction | `RemoveGoods` |
| Import and trade arrival | `AddGoods` |

None of those systems exists. Each will supply its own reason when it arrives, and the audit trail described next is where those reasons are recorded.

## Goods audit trail

DC-04 requires that "creation, transformation, loss, spoilage, consumption, and destruction MUST be auditable in development builds". Creation and destruction are the two of those that this slice can perform, so both are audited. The review declined to waive this, and the mechanism below is the smallest thing that satisfies it.

`FGoodsAuditRecord` carries five fields and nothing else:

| Field | Type | Meaning |
|---|---|---|
| `Action` | `EGoodsAuditAction` | `Created` or `Destroyed` |
| `InventoryId` | `FInventoryId` | Which inventory's contents changed |
| `GoodTypeId` | `FGoodTypeId` | Which good type |
| `Quantity` | `int32` | How much, always positive |
| `Reason` | `FName` | Why, supplied by the caller |

It carries no calendar or world time (there is no simulation clock to ask), no lot, no recipe, no worker, no Actor, no owner, and no value. It is not event sourcing, not a logging framework, and not a save format; `UE_LOG` text is not involved, because a reviewer asked for something tests can inspect.

### The reason contract

A caller must say why. An `FName` was chosen for the same reasons as the authored key — cheap, interned, comparable in a test, readable in a diagnostic — and because it leaves callers free to say something meaningful later without production code enumerating a fixed list of future systems now. No reason value is defined in production code; the tests use `Test.Seed`, `Test.Harvest`, and `Test.Consume` as test data.

`None` is rejected with `InvalidReason`, before any state changes. Authoritative quantity therefore cannot come into existence or vanish anonymously, which is the property that makes the trail worth keeping: an unexplained entry is impossible rather than merely discouraged.

Where a reason sits in the validation order is deliberate. It is checked after the identifiers and the quantity but before the state-dependent overflow and sufficiency checks, so a caller with a malformed request hears about the malformed request first.

### What is and is not audited

| Operation | Outcome | Audit effect |
|---|---|---|
| `AddGoods` | Success | Exactly one `Created` record |
| `AddGoods` | Any rejection | None |
| `RemoveGoods` | Success | Exactly one `Destroyed` record |
| `RemoveGoods` | Any rejection | None |
| `TransferGoods` | Success | **None** — quantity moved, nothing was created or destroyed |
| `TransferGoods` | Any rejection | None |

A transfer appending audit records would be a lie about what happened: the total is unchanged, so nothing was created and nothing destroyed. Recording one would also make the trail useless for its actual purpose, which is explaining why the world's total quantity of a good is not what it was.

### Read access

```cpp
int32 GetGoodsAuditRecordCount() const;
TOptional<FGoodsAuditRecord> GetGoodsAuditRecord(int32 RecordIndex) const;
```

Records read oldest first, by position, and an index outside the range returns an unset optional rather than asserting — including a negative one. Reads are copies, following the same contract as every other read in the registry, so no caller receives a reference into audit storage and the record of what happened cannot be edited after the fact. A test asserts exactly that, holding a record copy across enough further audited mutations to reallocate the audit array repeatedly, then confirming the copy is intact and that editing it changes no stored record.

Deliberately absent: filtering, querying by inventory or good type, aggregation, trimming, and clearing. None is needed to prove the property, and a query surface built before a real consumer exists would be guesswork.

## Transfer semantics

`TransferGoods(Source, Destination, GoodType, Quantity)` validates every condition before writing anything, in this exact order:

1. source inventory resolves, else `UnknownSourceInventory`
2. destination inventory resolves, else `UnknownDestinationInventory`
3. good type resolves, else `UnknownGoodType`
4. quantity is strictly positive, else `InvalidQuantity`
5. the two inventories differ, else `SameInventory`
6. the source is located, else `SourceInventoryNotLocated`
7. the destination is located, else `DestinationInventoryNotLocated`
8. the source holds at least the quantity, else `InsufficientQuantity`
9. the destination has headroom below `MaxGoodQuantity`, else `Overflow`

The order is part of the contract and is tested. Identifier and value validation precede the structural check, location precedes the quantity checks, and the structural check precedes the state-dependent checks — so a same-inventory transfer of an unknown good type reports the unknown good type, while a same-inventory transfer that *would also* have been insufficient reports `SameInventory`. A caller learns about the thing it can fix first.

`TransferGoods` is not transportation. Prototype 0.1D records that distinction in `PHYSICAL_SITE_INVENTORY_LOCATION.md`: this operation moves quantity between two custody boundaries without simulating a journey.

### The internal mutation boundary

The previous revision implemented a transfer by calling the public `RemoveGoods` and `AddGoods`. The review confirmed that was atomic as written but identified it as future fragility: any policy those public operations later acquire — an audit requirement being the immediate example — could reject half of an already validated transfer. It was right, and the boundary has been restructured.

There are now two private primitives, `ApplyGoodsAddition` and `ApplyGoodsRemoval`, which are the only code in the registry that writes an entry list:

```text
AddGoods       = validate + ApplyGoodsAddition + append Created audit
RemoveGoods    = validate + ApplyGoodsRemoval  + append Destroyed audit
TransferGoods  = validate + ApplyGoodsRemoval  + ApplyGoodsAddition   (no audit)
```

The primitives express *movement of quantity into or out of one inventory*, which is creation, destruction, or half a transfer depending entirely on who calls them. Their callers decide which it was. They assume every precondition has already been validated, assert it, take no reason, and **cannot fail** — which is precisely what makes a transfer's two halves safe to apply in sequence.

Transfer atomicity after the restructuring rests on three facts: all conditions are checked before the first write; the primitives have no failure path, so neither can refuse after the other has run; and the inventories are provably distinct by step 5, so applying one cannot invalidate the other's precondition. Each record is resolved immediately before its own mutation, so no address is held across a write. A failed transfer leaves **both** inventories untouched, because nothing is written until the last check passes.

Keeping the entry-creation and zero-entry-removal rules inside the two primitives also means the halves of a transfer cannot drift from the semantics of a direct add or remove — the benefit the old delegation had, kept without the coupling. This is a deliberately small change: no transaction framework, no journal, no rollback, two private functions.

## Same-inventory behaviour

A transfer whose source and destination are the same inventory is **rejected with `SameInventory`**. It is never a silent no-op.

The assignment permitted either choice. Rejection was chosen because it matches the accepted style established in 0.1A and 0.1B, where a meaningless or ambiguous request is named rather than absorbed (`AlreadyMember`, `AlreadyResident`, `StillResident`, `NotResident`). A transfer to oneself is almost certainly a caller bug in a logistics system — a route that resolved both ends to the same store — and reporting it lets that bug surface at the call site instead of appearing as a successful move that moved nothing. It also keeps the conservation argument trivial: the successful path always has two distinct inventories.

Either way, goods are never duplicated or destroyed by such a call. The tests assert the quantity and the entry count are identical afterwards.

## Conservation guarantee

For any successful `TransferGoods` of Q units of good G:

```text
before:  Source(G) + Destination(G) = N
after:   Source(G) + Destination(G) = N
```

with `Source(G)` reduced by exactly Q and `Destination(G)` increased by exactly Q. No rounding is involved, because quantities are integral. No transfer can create or destroy quantity, and no failed transfer changes either side.

The headless scenario test proves this across four good types and five inventories: it records each good type's total before six transfers and asserts each total is identical afterwards, then asserts every individual inventory's holding, so conservation cannot be satisfied by goods ending up in the wrong place. It then checks the same property from the other direction, asserting that the audit trail did not grow — if any transfer had created or destroyed quantity, the trail would say so.

Conservation is a property of transfer only. `AddGoods` and `RemoveGoods` intentionally break it, which is exactly why they are the operations that must explain themselves.

## Invariant validation

`ValidateInvariants` gained two record-family passes and preserves every accepted 0.1A and 0.1B check unchanged. The new detections are:

- a good type record whose runtime handle does not match its registry slot,
- a good type record with no authored key,
- two good type records sharing an authored key,
- an inventory record whose identifier does not match its registry slot,
- more than one entry for the same good type in one inventory,
- an entry whose good type does not resolve,
- a stored quantity of zero or less.

Prototype 0.1D extends this list with physical-site and inventory-location checks, documented in `PHYSICAL_SITE_INVENTORY_LOCATION.md`. The checks above remain in force.

The two authored-key checks came with the identity split: a definition identity that is missing or ambiguous is exactly as broken as a mismatched slot, and a duplicate key would make `FindGoodTypeIdByKey` return whichever record came first. Duplicate keys are compared against earlier slots only, so the first duplicate is reported once rather than twice.

On quantities "outside the valid representation": quantities are `int32` and the per-entry maximum *is* that type's maximum, so every value a record can physically store is representable, and the only invalid range is zero and below — which the check above covers exactly. There is no reachable upper-bound violation to detect, because the mutation operations refuse to create one.

It remains diagnostic and never repairs: the function has no non-const path. The mutation operations are what prevent invalid state.

### Proving the detectors work

An invariant checker that has never failed is an untested branch. The public operations correctly make every one of these states unreachable, so the tests reach private storage through a single test-only hook:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FSimulationRegistryTestAccess;
#endif
```

`FSimulationRegistryTestAccess` is a test-only friend, declared in `Private/Tests/SimulationRegistryTestAccess.h`, compiled out of shipping builds. It adds no production mutation path — the alternative would have been a public API for corrupting the registry, which is precisely what should not exist. `Goods.InvariantDetection` then corrupts a fresh registry per case and asserts that validation fails *and* describes what it found.

## Snapshot and read contract

Unchanged from 0.1A and extended to the two new families. `FindGoodType` and `FindInventory` return `TOptional<...>` copies that cannot be invalidated by later registry operations and do not observe later changes. `ContainsGoodType` and `ContainsInventory` answer existence with no copy and are safe for any identifier value. No public API hands out an address or a mutable reference into storage, so no caller can edit an inventory's entries directly.

`GetQuantity(InventoryId, GoodTypeId)` returns `TOptional<int32>` and exists so that asking for one quantity never copies an inventory:

- **unset** when either identifier does not resolve, and
- **`0`** when the inventory resolves and simply holds none of that good type.

That distinction is the point. "No such inventory" and "none in stock" are different answers, and a caller that conflates them would report an empty warehouse for a typo'd identifier. `FindInventory` remains available for inspection and tests that want the whole entry list.

## Tests

`Private/Tests/GoodsInventoryTests.cpp`, guarded by `WITH_DEV_AUTOMATION_TESTS`. Fifteen automation tests. After Prototype 0.1D they construct a Settlement → Property → PhysicalSite chain before populating any inventory. Empty-inventory creation tests may leave the inventory unlocated.

| Test | Covers |
|---|---|
| `Goods.GoodTypeCreation` | Valid runtime handle; two good types sharing the display name `Wheat` under different authored keys receive distinct handles; differently named ones too; lookup returns the stored authored key, handle, and display name; `0`, one past range, `MAX_int32`, `0x80000000`, and `MAX_uint32` all fail safely through `Contains` and `Find`; failed lookups create nothing |
| `Goods.AuthoredKeyIdentity` | Creation without an authored key is refused; an authored key resolves to its runtime handle; an unknown key, `None`, and a display name each resolve to nothing; a duplicate authored key is rejected atomically, stores nothing, and consumes no runtime handle (proved by the next creation taking the handle the duplicate would have had); the original record survives the rejected duplicate; duplicate display names are allowed under distinct keys; and creating the same keys in the opposite order in a second registry yields *different* handles while both keys still resolve — insertion order is not identity |
| `Goods.InventoryCreation` | Valid and distinct identifiers; a new inventory has no entries and reports zero of a known good; extreme identifiers fail safely through `Contains`, `Find`, and `GetQuantity`; a resolvable inventory with an unresolvable good type is unreadable, separating "no such good type" from "holds none" |
| `Goods.AddGoods` | A valid addition is held; repeated additions accumulate into one entry; two good types coexist without interfering; unknown inventory, default inventory identifier, unknown good type, default good type handle, zero, `-5`, `MIN_int32`, and a missing reason are each rejected, with both quantities, the entry count, and the inventory count provably unchanged afterwards |
| `Goods.RemoveGoods` | A partial removal keeps the entry; removing the remainder drops the entry and leaves the other good type's entry in place; removing a good type no longer held, more than is held, and anything from an empty inventory are all `InsufficientQuantity`; unknown inventory, unknown good type, zero, negative, `MIN_int32`, and a missing reason rejected; holdings and entry lists provably unchanged |
| `Goods.QuantityBoundaries` | `MaxGoodQuantity` is itself a valid holding; adding one more and adding the maximum again are both `Overflow` with the quantity unchanged and invariants intact; the maximum can be removed again; a transfer that would overflow the destination is rejected with **both** inventories unchanged; a transfer that exactly fills the destination to `MaxGoodQuantity` succeeds; a full destination then accepts nothing |
| `Goods.Transfer` | A transfer succeeds between two inventories; the source decreases and the destination increases by exactly the quantity; the total is conserved; a partial transfer retains the source entry; a full transfer removes it and leaves the untransferred good type's entry; a second good type transfers independently in the other direction; both totals stay conserved throughout |
| `Goods.TransferRejection` | Insufficient source, unknown source, unknown destination, and unknown good type across five extreme identifier values each, plus zero, negative, and `MIN_int32` quantities — then both holdings, the conserved total, both entry lists, and the inventory and good type counts are all asserted unchanged |
| `Goods.SameInventoryTransfer` | A same-inventory transfer is rejected with `SameInventory` and neither duplicates nor destroys goods; it still reports `SameInventory` when the quantity would also have been insufficient; zero, negative, and unknown good type are reported ahead of it, proving the documented validation order; an unknown inventory transferred to itself reports an unknown source |
| `Goods.Audit` | A new registry has audited nothing and reads nothing; a successful creation changes the quantity and appends exactly one `Created` record with the correct inventory, good type, quantity, and reason; a successful destruction appends exactly one `Destroyed` record; the earlier record is unaffected by the later one and records read oldest first; a retained record copy keeps all five of its fields across 64 further audited mutations that reallocate the audit array several times; editing that retained copy rewrites neither it nor any other stored record; and indexes past the end and below zero read nothing |
| `Goods.AuditRejection` | Four failed additions and three failed removals leave the quantity and the audit count unchanged; a missing reason is rejected with `InvalidReason` for both operations, changing no quantity, no entry list, and no audit record; three failed transfers leave both inventories and the audit trail unchanged |
| `Goods.AuditExcludesTransfer` | A partial transfer and a full one that empties the source conserve the total and append **no** audit records at all; the only record present is still the original seeding creation, unchanged; a later destruction is still audited normally, proving transfers did not simply disable auditing |
| `Goods.SnapshotRegression` | Good type and inventory snapshots are taken, then 18 good types and 18 inventories are created (reallocating both arrays) and the snapshotted inventory is added to, removed from, and transferred out of. The held snapshots still report their original authored key, handle, display name, entry count, and quantity, while the registry reports the current values. Editing a snapshot's publicly writable fields — authored key, name, identifiers, and emptying the entry array — changes no authoritative state and no key resolution |
| `Goods.InvariantDetection` | Six corruptions produced through the test-only access hook, each on its own registry, are each detected *and* described: good type handle/slot mismatch, inventory identifier/slot mismatch, a duplicate good type entry, an unresolvable good type reference, a stored quantity of zero, and a negative stored quantity. Two more cover the new identity checks: a good type with no authored key, and two sharing one. Each case asserts the registry validated cleanly beforehand where applicable |
| `Goods.Scenario.HeadlessConservation` | 4 runtime good types under authored keys and 5 inventories, seeded to 1200 wheat, 150 flour, 65 bread, and 40 firewood in seven audited creations, then six transfers along a chain including one that empties a source entry. Every good type's total is asserted identical before and after, the audit count is asserted unchanged by the transfers, every individual holding is asserted, the emptied inventory is asserted to hold no entries at all, three "goods never appeared where none were sent" checks guard against leakage between good types, and the authored keys are asserted to still resolve at the end |

Every test constructs its own registry on the stack and ends by calling `ValidateInvariants`, except `Goods.InvariantDetection`, which asserts the opposite by design. The boundary, transfer, and scenario tests call it at several intermediate points too. No test spawns an Actor, creates a `UObject`, opens a map, or requires one to be opened, which is what demonstrates that inventory state depends on neither a world nor a level.

The accepted 0.1A and 0.1B test files were not modified, and all eighteen of their tests still pass. The small test helpers (`SimulationTestFlags`, `VerifyInvariants`) are duplicated once more rather than shared, deliberately, to leave those files untouched while 0.1C is under review; extracting a shared test header remains a reasonable later cleanup.

### Commands and results

Build:

```text
Engine\Build\BatchFiles\Build.bat RealmsUnwrittenEditor Win64 Development ^
  -Project="<repo>\RealmsUnwritten.uproject" -WaitMutex
```

Result: **Succeeded**, no warnings from the new or changed code.

Tests:

```text
UnrealEditor-Cmd.exe "<repo>\RealmsUnwritten.uproject" ^
  -ExecCmds="Automation RunTests RealmsUnwritten.Simulation; Quit" ^
  -unattended -nopause -nosplash -nullrhi -NoSound -log
```

Result: **33 succeeded, 0 failed, 0 with warnings**, `EXIT CODE: 0`, at acceptance of 0.1C. Prototype 0.1D migrated these tests onto located inventories and added further tests under `RealmsUnwritten.Simulation`.

## Labeled prototype exceptions

`DESIGN_CONSTITUTION.md` requires a prototype simplification to state what is simplified, why, which constitutional behaviour stays protected, what triggers replacement, and how the prototype data avoids blocking the future model. Prototype 0.1C originally recorded three such exceptions. Prototype 0.1D retired the holderless-inventory waiver; two exceptions remain active. The retired waiver is kept below as historical context and does not permit holderless populated inventories.

### 1. Inventories have no holder or location — waiver retired by Prototype 0.1D

The following waiver was explicitly approved for Prototype 0.1C:

> "Prototype 0.1C may contain holderless inventories solely as a headless foundation and test abstraction. This exception expires before any inventory participates in gameplay, production, harvesting, consumption, hauling, markets, trade, or other physical world simulation. Inventory holder/location semantics must be resolved before such integration."

Prototype 0.1D retires it. Stationary location is now an explicit `FInventoryLocation` on every inventory: empty inventories may be `None`, and any inventory containing goods must have exactly one valid `PhysicalSite`. Gameplay systems may not create holderless goods. The historical waiver text is kept here so the decision record stays intact; the rule in force is in `PHYSICAL_SITE_INVENTORY_LOCATION.md`.

Mobile holders (person, cart, pack animal, ship) remain unimplemented. That is a deferred system, not a continuation of this waiver.

### 2. Aggregate quantities rather than resource lots

- **Simplified:** An inventory holds one quantity per good type, not a collection of individually identified lots with provenance, quality, and creation time.
- **Why:** `PROTOTYPE_0_1_SCOPE.md` explicitly permits either — "goods exist as lots **or safely aggregated stacks** within specific inventories" — and `DATA_MODEL_OVERVIEW.md` requires a lot identifier only "when independent provenance is required", allowing "contained lot references **or equivalent compact lot records**". Nothing in this slice distinguishes one unit of wheat from another, so lot identity would carry no information. `HIGH_LEVEL_ARCHITECTURE.md` also lists "granularity of item lots, provenance retention, and aggregation thresholds" among its deliberately deferred decisions.
- **Still protected:** Quantity cannot be negative, totals are conserved across transfers, and quantities live in inventories rather than in counters.
- **Replacement trigger:** The first system that needs to tell two portions of the same good apart — quality, spoilage or expiry, reservations, or provenance for DC-15. Reservations in particular are named as part of scope step 3 and are not implemented here.
- **Does not block:** A lot becomes the entry grown into a record with its own identifier; the good type identity and the inventory identifier are unchanged, and aggregate quantity remains derivable by summing lots.
- **Not yet satisfied:** DC-15's provenance preference. Per-lot history does not exist; the audit trail records that quantity was created or destroyed and why, not which portion of a holding it was.

### 3. Good type definitions are allocated at runtime, not authored

- **Simplified:** A good type is created by a runtime call. Its authored key is the durable identity a definition needs, but real definitions are authored, versioned content validated on load.
- **Why:** There is no content pipeline, no data asset format, and no schema version in the project yet, and building one is out of scope. What matters architecturally now is that the definition identity exists, is separate in kind from every entity identifier, and does not depend on load order — which it is.
- **Still protected:** No good is hard-coded in production code, so no C++ enum has to grow per resource; display names are identity for nothing; authored keys are unique and validated; definitions cannot mutate runtime state, because a good type record has no behaviour at all.
- **Replacement trigger:** The first authored content, most likely resource definitions arriving with the production slice, which also brings unit of measure and precision rules.
- **Does not block:** The registry becomes a loader of definitions rather than an allocator of them, and the authored key is what it loads them by. Inventory entries keep referring to a good type by runtime handle either way.
- **Not claimed:** Nothing is persisted. No save format, data asset, catalogue, or migration exists or is implied by the presence of the key.

## Known limitations

1. **Empty inventories may be unlocated; populated inventories may not.** Prototype 0.1D retired the holderless waiver. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`. Mobile holders remain unimplemented.
2. **No lots, provenance, quality, or reservations.** Two units of the same good are indistinguishable, and nothing can be reserved before collection, which scope step 3 will eventually require.
3. **The audit trail is minimal and unbounded.** It records action, inventory, good type, quantity, and reason, and nothing else: no time, because no clock exists; no lot; no actor. It is never trimmed, cleared, or persisted, so a long-running session grows it without limit. It covers creation and destruction only — DC-04 also names transformation and spoilage, which no system can yet perform.
4. **No persistence of any kind.** The authored key is the identity a save format would record, but nothing serializes it, and no record carries a schema version.
5. **No capacity.** An inventory accepts any good type up to `MaxGoodQuantity` each. Weight, volume, per-type restrictions, and destination rejection do not exist, so the "destination inventory is full" failure case from `PROTOTYPE_0_1_SCOPE.md` cannot yet be reproduced except as arithmetic overflow.
6. **No units of measure.** A quantity unit is abstract, and quantities of different good types are not commensurable. Nothing converts, weighs, or aggregates across good types, and the totals summed in tests are per good type for exactly that reason.
7. **Nothing consumes or produces.** The slice has state and movement but no process, so goods can only be seeded, moved, and destroyed by direct call.
8. **No removal of good types or inventories.** As in 0.1A and 0.1B, nothing can be deleted, so a good type referenced by an entry can never dangle, and an authored key can never be freed and reused. Deletion requires the identifier allocator with generations or tombstones already recorded as a 0.1A limitation, and `ValidateInvariants` would need to cover the dangling-entry case that is currently unreachable.
9. **Entry lookup and authored key resolution are both linear** — the first in the number of distinct good types within one inventory, the second in the number of good types. Fine at the handful this slice creates; see scaling concerns.
10. **Snapshots can go stale**, and an inventory snapshot copies its entry array. Both are deliberate properties of the accepted read contract.
11. **No owner and no command boundary.** Nothing constructs the registry outside tests, and callers would invoke methods directly.
12. **No measured performance data.** The largest test holds 4 good types and 5 inventories. No benchmark has been run and no scalability claim is made beyond the structural observations below.

## Deferred mobile-holder model

Stationary location was resolved in Prototype 0.1D: `Inventory.Location` is a tagged value whose only located kind is `PhysicalSite`. The remaining open question is mobile custody — a person carrying goods, a cart, a pack animal, a ship.

That future kind belongs on the same tagged location value, with its own typed identifier, when the first carrier exists. It must not be a generic `HolderId`. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`.

`CreateInventory` still takes nothing and creates an empty unlocated inventory. Assigning a site is a separate operation. Introducing a required location at creation time, or a mobile kind, will still affect creation, loading/migration, validation, and relationship APIs.

## Future scaling concerns

Recorded rather than solved, per `AI_DEVELOPMENT_RULES.md`.

1. **No measurement yet.** Everything here is structural observation. The synthetic harness `DEVELOPMENT_ROADMAP.md` requires before Prototype 0.1 closes — thousands of people and "substantially more goods lots" — has not been built, and goods are precisely where that harness will matter most, since the architecture expects far more lots than people.
2. **Entry lookup is linear per inventory.** Every add, remove, transfer, and quantity query scans one inventory's entries. At a handful of good types this beats hashing. The measurement that would change the decision is the distinct-good-type count per inventory: a market or central warehouse holding dozens would favour a sorted array with binary search (keeping determinism) or a `TMap` (giving up ordered iteration). Not to be changed without that measurement.
3. **Authored key resolution is linear in the number of good types.** `FindGoodTypeIdByKey` scans the good type records, and is also what `CreateGoodType` uses to reject duplicates, making creation quadratic in the number of good types. At the handful this slice creates that is irrelevant; with a few hundred authored resource definitions loaded at startup it would still be trivial, and with several thousand it would want an `FName`-to-handle index. The index is cheap to add — the reason it is not here is that it would be a second copy of a relationship the records already express, requiring its own consistency invariant, and content is expected to resolve keys once at load and hold handles thereafter.
4. **The audit trail grows without bound.** Every creation and destruction appends a record, nothing trims or clears them, and a long-running simulation with real production and consumption would append continuously. This is acceptable for a headless foundation whose only callers are tests, and is exactly the kind of thing the data model's retention tiers ("permanent landmark history, summarized ordinary history, and discardable diagnostics") exist to settle. It must be settled before production and consumption run at volume, and the trail is discardable diagnostics rather than landmark history.
5. **Inventory snapshot cost.** `FindInventory` copies the entry array, which is why `GetQuantity` exists and copies nothing. Read paths that want a single number should use it; bulk aggregation across many inventories belongs in a purpose-built read model rather than a loop of snapshots.
6. **Per-good-type totals are derived, deliberately.** There is no cached "total wheat" anywhere, which is what DC-04 and integrity rule 8 require. Computing a settlement's stock will therefore mean visiting its inventories. When that becomes hot it should become a rebuildable index with clear invalidation — the data model already calls such caches rebuildable derived data — and not an authoritative counter.
7. **Invariant validation walks every entry** and builds a `TSet` per inventory. It is a test and diagnostic facility at prototype sizes, not a runtime operation, and should be sampled or made incremental before ever running inside a live loop.
8. **Quantity width.** `int32` per entry is chosen for compactness, and about 2.1 billion units of one good in one inventory is far beyond prototype reach. Widening it is not a local change, as set out under quantity representation above: it would affect the stored quantity on `FInventoryEntry`, `MaxGoodQuantity`, the public query and mutation contracts (`GetQuantity`, `AddGoods`, `RemoveGoods`, `TransferGoods`) and therefore every caller, the quantity field on `FGoodsAuditRecord`, the boundary tests that assert behaviour at the maximum, and — once persistence exists — the serialized representation and its migration. Only the overflow checks come free, since they are written as headroom comparisons and would not need restructuring.
9. **Dense arrays reallocate.** Same as 0.1A and 0.1B: growth copies records, and an inventory record carries an array, so growing the inventory array moves those arrays. Reserving at scenario load is the direction when a loader exists. No correctness implication under the snapshot contract.
10. **Single-threaded and non-reentrant**, unchanged.
11. **What this slice already avoids.** No Actor or `UObject` per good type, inventory, or quantity; no tick; no world scan for any lookup; no display-name-based identity and no name-keyed storage, since an authored key is a definition's identity while a display name identifies nothing; no global or per-settlement resource counter; no floating-point authoritative quantity; no stored zero quantities; and no public API that can hand out a dangling or mutable reference.

## Open architectural questions

Raised for review, not decided here. The earlier open questions — who owns the registry, the module split, and the four from 0.1B — all still stand. This slice adds:

1. **Definition identifiers — answered for good types, and reused for work and skill types.** The review's decision split the two roles: an `FName` authored key is the durable definition identity, and a typed runtime handle is the cheap in-registry reference. Prototype 0.1E (`WORK_LABOR.md`) applies that shape to `FWorkTypeRecord`; Prototype 0.1F (`SKILLS_KNOWLEDGE.md`) applies it to `FSkillTypeRecord`. Crops, processes, building archetypes, and occupations remain later definition families. Authored-key versioning rules are still open.
2. **Where does mobile custody live?** Stationary location is answered in 0.1D. Mobile holders remain a later tagged kind on `FInventoryLocation`, not a generic holder field. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`.
3. **When do aggregate stacks become lots, and what triggers the split?** The architecture lists aggregation thresholds as deferred. A concrete answer is needed before reservations, because a reservation against an aggregate quantity and a reservation against a lot are different designs.
4. **Should the goods audit trail eventually move to the domain-event system?** A minimal trail now lives in the registry, which is the smallest thing that satisfies DC-04 for this slice. Once domain events, a simulation clock, and history retention exist, creation and destruction of goods are plausibly events rather than a private array — and the reason `FName` becomes a typed cause carrying its originating system. That migration, along with retention policy and whether the trail is ever persisted, should be decided before production and consumption run at volume.
5. **Should reasons be validated against something?** Today any non-`None` `FName` is accepted, so a caller can supply a meaningless one. A registered vocabulary of causes would catch typos and make the trail groupable, but defining one before real callers exist would be guesswork. Revisit when the first production or consumption system supplies reasons.
