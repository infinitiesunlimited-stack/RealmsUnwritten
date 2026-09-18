# Settlement, Property and Residence

## Document status

- **Role:** Implementation record for the Prototype 0.1B settlement, property, and residence foundation
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (accepted Prototype 0.1A), which is not superseded
- **Extended by:** `GOODS_INVENTORY.md` (Prototype 0.1C), `PHYSICAL_SITE_INVENTORY_LOCATION.md` (Prototype 0.1D), and `WORK_LABOR.md` (Prototype 0.1E). Prototype 0.1D adds a reverse `PhysicalSites` list on the property record; occupancy, settlement membership, and residence behaviour recorded here are unchanged. Current work belongs to the person, not the household.
- **Scope:** Settlements, properties, household settlement membership, and household residence
- **Describes:** Only what exists in the repository today

## Purpose

This slice continues `PROTOTYPE_0_1_SCOPE.md` step 2 ("place and population") by adding the authoritative relationships that 0.1A deliberately left out. It answers six questions, all by stable identifier and with no world, Actor, or map involved:

| Question | Answer path |
|---|---|
| Which settlement is a household part of? | `FHouseholdRecord::SettlementId` |
| Which property is a household's residence? | `FHouseholdRecord::ResidenceId` |
| Who lives on a property? | `FPropertyRecord::ResidentHouseholdId` |
| Which properties belong to a settlement? | `FSettlementRecord::Properties` |
| Which properties are occupied? | `FPropertyRecord::ResidentHouseholdId` is valid |
| Who lives in a settlement? | `FSettlementRecord::Households`, then each household's `Members` |

Everything in 0.1A remains as accepted. People still reach a settlement only through their household; no person record gained a settlement or residence field.

## Authoritative simulation boundary

The boundary is the one established in 0.1A and is unchanged: all state added here is plain C++ data owned by `FSimulationRegistry`, which is not a `UObject`, `AActor`, or subsystem, has no `Tick`, requires no loaded map, and is not exposed to Blueprint. The `static_assert(!std::is_base_of_v<UObject, FSimulationRegistry>)` in the 0.1A test file still guards that.

Two boundary points matter specifically for this slice, because it is the first one dealing with *places*:

- **A settlement is not a map, a region volume, or an Actor.** It has no extent, no transform, and no spatial query. It is a grouping identity that properties and households reference.
- **A property is not a building, a lot, or a mesh.** It has no geometry, area, address, or improvements. `DATA_MODEL_OVERVIEW.md` is explicit that "property geometry is authoritative domain/spatial data; a rendered fence is not" — this slice implements neither the geometry nor the fence, only the identity and the relationships.

This is what DC-12 and the design principle in the assignment require: a household exists because the simulation says so, not because a house was placed, and a property exists as a holding before anything is ever built on it.

## Module layout

Files added by this slice, in the module layout accepted by the 0.1A correction pass:

```text
Source/RealmsUnwritten/Private/Simulation/SettlementRecord.h
Source/RealmsUnwritten/Private/Simulation/PropertyRecord.h
Source/RealmsUnwritten/Private/Tests/SettlementPropertyResidenceTests.cpp
```

Files extended: `SimulationIds.h`, `HouseholdRecord.h`, `SimulationRegistry.{h,cpp}`. **`RealmsUnwritten.Build.cs` is unchanged**, and no module was added.

## Entity IDs

`SimulationIds.h` gains two tags and two aliases on the existing template:

```cpp
using FSettlementId = TSimulationId<FSettlementIdTag>;
using FPropertyId   = TSimulationId<FPropertyIdTag>;
```

They inherit every property documented in 0.1A: phantom-typed, `explicit` raw-value construction that grants no capability, value `0` meaning "no entity", allocation by the registry only, hashable, and never a name. `ToString` yields `Settlement#2` / `Property#7`.

### Type-safety assertions

0.1A asserted non-interchangeability with five pairwise `static_assert`s for two families. Four families would need twenty-four, so the assertions were replaced by one predicate applied to the whole set. Later slices extend it by adding names to the list, which is what the mechanism was built for. Prototype 0.1F currently covers nine families.

```cpp
static_assert(
    SimulationIdContract::TMutuallyDistinct<FPersonId, FHouseholdId, FSettlementId, FPropertyId>::value,
    "Simulation identifier families must never be interchangeable with one another.");
```

That was the 0.1B list. `FGoodTypeId`, `FInventoryId`, `FPhysicalSiteId`, `FWorkTypeId`, and `FSkillTypeId` were appended later; the predicate itself did not change.

`TMutuallyDistinct` checks every unordered pair for sameness, convertibility in either direction, and constructibility in either direction. The assertion still lives next to the type aliases rather than in the tests, so it holds in every build. A future entity family is added by extending the list.

## Settlement representation

`Private/Simulation/SettlementRecord.h`:

| Field | Type | Justification |
|---|---|---|
| `Id` | `FSettlementId` | Stable identity. Data model "Stable settlement ID"; constitution integrity rule 1. |
| `Name` | `FString` | Display name. Data model "name". Never identity. |
| `Properties` | `TArray<FPropertyId>` | Data model "constituent property references". No duplicates. |
| `Households` | `TArray<FHouseholdId>` | Data model "Resident/registered household index". No duplicates. Includes unhoused households. |

Both lists are **registry-maintained indexes**, not independent truth. The authoritative statement of "this property is in Eichenfurt" is `FPropertyRecord::SettlementId`, and of "this household is in Eichenfurt" is `FHouseholdRecord::SettlementId`; the settlement's lists exist so that the reverse question does not require scanning every property or household record, which is what `DATA_MODEL_OVERVIEW.md` means by "summary indexes used for queries, with rebuild rules" and what `HIGH_LEVEL_ARCHITECTURE.md` means by avoiding repeated full-world scans. They can be rebuilt entirely from the back-references, and `ValidateInvariants` checks both directions agree.

Settlement population is **not stored**. It is derived by reading `Households` and then each household's `Members`, which is what DC-02 requires.

### Fields deliberately not added

Spatial extent, founding/origin data, regional and cultural context, road references, person-level resident index, institutions, offices, market and service references, jurisdiction links, and summary aggregates with timestamps are all named by `DATA_MODEL_OVERVIEW.md` but absent. Each needs a system this slice does not have (geometry, calendar, culture, roads, institutions, markets, aggregation rules), and the assignment forbids government, politics, taxation, law, markets, population classes, culture, religion, military, settlement AI, and settlement tiers. Nothing was scaffolded as a placeholder.

## Property representation

`Private/Simulation/PropertyRecord.h`:

| Field | Type | Justification |
|---|---|---|
| `Id` | `FPropertyId` | Stable identity. Data model "Stable property ID". |
| `SettlementId` | `FSettlementId` | Data model "Settlement 1 — contains/references — * Property". Always valid; see below. |
| `ResidentHouseholdId` | `FHouseholdId` | Data model "occupier"; invalid means unoccupied. At most one household. |
| `PhysicalSites` | `TArray<FPhysicalSiteId>` | Added by Prototype 0.1D as a registry-maintained reverse index of sites on this property. Occupancy is unchanged. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`. |

`SettlementId` is always valid on a stored record, because creation requires a resolvable settlement and this slice offers no operation that moves or detaches a property. `ValidateInvariants` treats a property with an unresolvable settlement as broken state rather than as an allowed "unassigned" case.

**No display name was added.** The assignment allows one "only if justified", and nothing in this slice needs it: no operation, invariant, or query takes a property name, and diagnostics identify properties by `Property#7`. Addresses and local names are data model fields that belong with roads and access, which do not exist. Adding a name now would create an unowned field and invite exactly the name-based reasoning the constitution forbids.

### Fields deliberately not added

Parcel geometry, boundary, acreage, soil, fertility, land use, buildings, fields, gardens, workshops, barns, improvements, construction and upgrade state, roads, access, address, zoning, property rights, deeds, owner, value, rent, sale, purchase, inheritance, and taxes. All are later systems and all are explicitly excluded by the assignment.

## Household settlement membership

`FHouseholdRecord` gains:

| Field | Type | Meaning |
|---|---|---|
| `SettlementId` | `FSettlementId` | Settlement the household is currently in; invalid means none. |
| `ResidenceId` | `FPropertyId` | Property the household occupies; invalid means none. |

### Why membership is stored, not derived from residence

The assignment asks this to be evaluated explicitly. Membership is **stored on the household**, because deriving it from residence would make "in a settlement" and "has a property" the same fact, and the simulation must represent:

```text
Household is in Eichenfurt, Residence = none
Household is in Eichenfurt, Residence = Property#12
```

Newly arrived immigrants, refugees, displaced families, travellers, and households awaiting a property all need the first state, and `DATA_MODEL_OVERVIEW.md` already names an "explicit transient/homeless state" as authoritative data. A derived scheme would have made an unhoused household either invisible or settlement-less, and immigration would have had to redesign the relationship rather than use it. The cost is one more field and one more pair of invariants, which `ValidateInvariants` covers.

Membership is never inferred from an Actor position, a property name, or a world search. It is set only by `PlaceHouseholdInSettlement` and cleared only by `RemoveHouseholdFromSettlement`.

## Household residence

Residence is a two-sided relationship between a household and a property: `FHouseholdRecord::ResidenceId` and `FPropertyRecord::ResidentHouseholdId`. Both sides are written by one private transition, `SetHouseholdResidence`, and there is no other write path.

### Behaviour

- **Assignment** requires that the household resolves, the property resolves, the household is in a settlement, the property belongs to *that* settlement, and the property is unoccupied.
- **Moving** is the same operation: assigning a household to a new property vacates the one it currently occupies, in a single call, because "a household resides on at most one property" is the invariant being protected. This mirrors how 0.1A's `AddPersonToHousehold` performs a move.
- **Re-assigning the same property** reports `AlreadyResident` and changes nothing.
- **Removal** requires the caller to name the property the household actually occupies, so a mistaken call is reported as `NotResident` rather than silently vacating something else. It clears both sides and leaves settlement membership intact.
- **Relocating between settlements** is refused while the household is still housed: `PlaceHouseholdInSettlement` and `RemoveHouseholdFromSettlement` return `StillResident`. This is the deliberate design choice of the slice: a household relocates by vacating, moving, then taking up a new residence — three explicit authoritative steps. The alternative, silently emptying a property as a side effect of a settlement change, would let one relationship destroy another and would make cross-settlement residence reachable through the settlement path. Cascading effects across relationship types are what makes invariants hard to reason about later.

### People are unaffected

Residence lives on the household, never on the person. No residence or settlement operation touches `PersonRecord::HouseholdId` or `HouseholdRecord::Members`, and the people of a household follow it implicitly wherever it goes. Two tests assert this directly.

## Occupancy versus ownership

This slice models **occupancy only**. Nothing here says a household owns anything.

`DATA_MODEL_OVERVIEW.md` already distinguishes the two — `Household 1 — occupies — 0..* Property` alongside `Household * — owns (optional) — * Property`, plus a separate `Property Right` supporting record with holder, right type, share, restrictions, and validity — and that distinction is preserved by implementing one side and not inventing the other. `FPropertyRecord::ResidentHouseholdId` means "lives here", nothing more. There is no owner field, no deed, no tenancy, no landlord, no rent, and no transfer. When ownership arrives it will be its own relationship (most likely its own record type, as the data model indicates), and it will be able to disagree with occupancy, which is exactly why occupancy was not named "owner" here.

The prototype also narrows occupancy to a single residence per household, where the data model allows `0..*` occupied properties. The narrowing is the assignment's required invariant ("a household cannot simultaneously reside on two properties") and is the right reading for a *residence*: a household has one home. Occupying additional properties for non-residential use is a later concern and is recorded under known limitations.

## Registry

`FSimulationRegistry` was extended, not replaced, and now authoritatively owns four entity families:

```cpp
TArray<FPersonRecord>     PersonRecords;
TArray<FHouseholdRecord>  HouseholdRecords;
TArray<FSettlementRecord> SettlementRecords;
TArray<FPropertyRecord>   PropertyRecords;
```

Every 0.1A principle is preserved: one dense array per family, identifier value equals slot plus one, lookup is a range check plus an index read (never a search or a world scan), the same unsigned-before-narrowing range check resolves all four identifier types, no Actor dependency, no `UObject` per entity, no per-entity tick, no stored totals, and no public pointer into reallocating storage.

### Public operations added

```cpp
FSettlementId CreateSettlement(const FString& Name);
FPropertyId   CreateProperty(FSettlementId SettlementId);   // invalid ID == rejected

bool ContainsSettlement(FSettlementId SettlementId) const;
bool ContainsProperty(FPropertyId PropertyId) const;

TOptional<FSettlementRecord> FindSettlement(FSettlementId SettlementId) const;  // snapshot copy
TOptional<FPropertyRecord>   FindProperty(FPropertyId PropertyId) const;        // snapshot copy

int32 GetSettlementCount() const;   // derived from storage
int32 GetPropertyCount() const;     // derived from storage

ESettlementMembershipResult PlaceHouseholdInSettlement(FHouseholdId, FSettlementId);
ESettlementMembershipResult RemoveHouseholdFromSettlement(FHouseholdId, FSettlementId);

EResidenceResult AssignHouseholdResidence(FHouseholdId, FPropertyId);
EResidenceResult RemoveHouseholdResidence(FHouseholdId, FPropertyId);
```

`ValidateInvariants` keeps its signature. No 0.1A operation changed name, signature, or behaviour.

### Result enums

Rejections are reported, never swallowed, and only `Success` changes state — constitution integrity rule 6.

`ESettlementMembershipResult`: `Success`, `AlreadyMember`, `UnknownHousehold`, `UnknownSettlement`, `NotAMember`, `StillResident`.

`EResidenceResult`: `Success`, `AlreadyResident`, `UnknownHousehold`, `UnknownProperty`, `HouseholdNotInSettlement`, `SettlementMismatch`, `PropertyOccupied`, `NotResident`.

Each relationship got its own enum rather than sharing one with `EHouseholdMembershipResult`, so a caller's `switch` covers exactly the outcomes its operation can produce, and so the accepted 0.1A enum did not have to change.

## Creation validation

`CreateProperty` rejects an unresolvable settlement and returns an invalid `FPropertyId`, using the same failure mechanism as `CreatePerson`: the operation has exactly one failure mode, and an identifier that does not resolve is already the established "no entity" signal.

Rejection is atomic, because validation happens before storage is touched:

- no property record is appended,
- `GetPropertyCount()` is unchanged,
- **no identifier is consumed**, so the next accepted creation continues the sequence (asserted directly in the tests),
- the settlement's property list is untouched,
- the returned identifier resolves to nothing.

This is the reason a property can never reference a settlement that does not exist, and therefore the reason `ValidateInvariants` can treat such a reference as broken state. `CreateSettlement` takes no authoritative reference and so has no failure mode. `CreatePerson` and `CreateHousehold` are unchanged.

## Relationship invariants

The invariants required by the assignment, and how each is made unreachable rather than merely detected:

| Invariant | How it is enforced |
|---|---|
| A household cannot reside on two properties at once | `ResidenceId` is a single field; the transition vacates the previous property before occupying the new one |
| A property cannot house two households at once | Assignment returns `PropertyOccupied` when the property is taken; the transition asserts the property is vacant before writing |
| Residence and occupancy cannot disagree | Both sides are written by the same private transition, and callers only ever receive copies |
| A household cannot reside in another settlement | Assignment compares the property's settlement with the household's and returns `SettlementMismatch`; a household in no settlement is refused with `HouseholdNotInSettlement`; the settlement path cannot break it either, because relocating while housed returns `StillResident` |
| Moving removes the old occupancy | The transition clears the previous property's occupant before attaching the new one; tested from both properties |
| Removing residence clears both sides | Removal runs the same transition with an invalid property |
| A household cannot be in two settlements at once | `SettlementId` is a single field; the transition removes the household from the previous settlement's list before adding it to the new one |
| Invalid household, property, or settlement IDs fail safely | All four identifier types resolve through the one range-checked helper; unresolvable values return an `Unknown*` result and change nothing |
| Failed operations never partially mutate | Every operation validates fully before calling its transition; creation validates before touching storage |
| Names never establish a relationship | No operation accepts a name; no name-keyed storage or lookup exists anywhere |
| 0.1A person/household membership stays valid | No settlement or residence operation touches a person record or a member list |

`ValidateInvariants` re-derives all of it from stored state and reports the first inconsistency found, now split into four record-family passes (person, household, settlement, property) for legibility. Beyond the 0.1A checks it detects:

- a property whose settlement does not resolve,
- a household whose settlement does not resolve,
- a duplicate property or duplicate household entry in a settlement's lists,
- a settlement listing a property or household whose back-reference names a different settlement (a property filed under the wrong settlement),
- a household claiming a settlement that does not list it exactly once, and the same for a property,
- a household residence pointing to an unknown property,
- a property occupancy pointing to an unknown household,
- residence and occupancy naming different partners in either direction,
- duplicate occupancy: two properties claiming the same household, since the household's single `ResidenceId` can match at most one of them,
- cross-settlement residence, including residence held by a household with no settlement.

It remains a detector, not a repair function: it never writes. The mutation APIs are what prevent invalid state.

## Read and snapshot contract

Unchanged from 0.1A and extended to the new families. `FindSettlement` and `FindProperty` return `TOptional<FSettlementRecord>` / `TOptional<FPropertyRecord>` — copies taken at the time of the call, which cannot be invalidated by any later registry operation and do not observe later changes. `ContainsSettlement` and `ContainsProperty` answer existence with no copy and are safe for any identifier value. No public API hands out an address into storage; the private resolve helpers still produce short-lived pointers that never leave the registry.

A settlement snapshot copies both of its arrays. That is the one new cost in this contract and it is recorded under future scaling concerns.

## Testing

`Private/Tests/SettlementPropertyResidenceTests.cpp`, guarded by `WITH_DEV_AUTOMATION_TESTS`. Nine new automation tests, alongside the nine accepted 0.1A tests, for **18 tests under `RealmsUnwritten.Simulation`** at the time of this slice. Later slices added further tests; these eighteen remain unchanged.

| Test | Covers |
|---|---|
| `Settlement.Creation` | Valid identifier; identically named settlements receive distinct identifiers; lookup returns the stored name and empty lists; `0`, one past range, `MAX_int32`, `0x80000000`, and `MAX_uint32` all fail safely through `Contains` and `Find`; failed lookups create nothing |
| `Property.Creation` | Valid and distinct identifiers; lookup; a property reports its settlement and starts unoccupied; the settlement lists exactly its own properties and not another settlement's; creation against `0`, an unallocated value, `MAX_int32`, `0x80000000`, and `MAX_uint32` is rejected atomically with no record added; the rejected identifier does not resolve; no identifier is consumed, proven by the next accepted creation continuing the sequence; extreme property identifiers fail safely |
| `Household.SettlementMembership` | Placement succeeds and both sides agree; a placed household has no residence; re-placing reports `AlreadyMember` without duplicating; moving between settlements leaves no stale membership in the origin; unknown household, unknown settlement, default identifier, and wrong-settlement removal all rejected with membership provably unchanged; removal clears both sides |
| `Residence.Assignment` | Occupying a property in the household's own settlement; both sides agree; settlement membership unchanged by it; re-assignment reports `AlreadyResident` and changes neither side; a second household is refused with `PropertyOccupied` and gains nothing; moving vacates the old property and occupies the new one; the vacated property can then house the other household |
| `Residence.Removal` | Naming the wrong property, an unknown household, an unknown property, and a default identifier are all rejected with both sides provably unchanged; a valid removal clears both sides while leaving settlement membership intact; vacating twice reports `NotResident` |
| `Residence.SettlementBoundary` | A property in another settlement is refused with `SettlementMismatch`; a household in no settlement is refused with `HouseholdNotInSettlement`; extreme identifiers on either side are rejected; a housed household cannot be relocated or removed from its settlement (`StillResident`) with nothing changed; the explicit vacate/move/re-house sequence succeeds and leaves the old property vacant |
| `Residence.CrossSettlementAtomicity` | A household already housed on a property in settlement A is refused a property in settlement B with `SettlementMismatch`, and keeps everything it had: its settlement, its residence, its property's record of it as occupant, the other property still unoccupied, and both settlements' household and property lists unmoved. Distinct from `Residence.SettlementBoundary`, which rejects from an unhoused state; this test proves a failed assignment cannot destroy an **already valid** residence |
| `Residence.PeopleUnaffected` | A household of three people occupies one property; a household sits in a settlement with no property and keeps its member; after vacate/move/re-house every member still belongs to the household, the household still lists every member, and the counts match |
| `Scenario.HeadlessSettlementNetwork` | 2 settlements, 5 properties, 4 households, 9 people; three households housed and one deliberately unhoused; an intra-settlement move, a full relocation between settlements, and a newcomer taking a vacated property; then every relationship cross-checked from both sides, housed and unhoused tallies, settlement list contents, vacant properties in the emptied settlement, and every 0.1A person/household link re-verified |

Every test constructs its own registry on the stack and ends by calling `ValidateInvariants`; the scenario test calls it at three points. No test spawns an Actor, creates a `UObject`, opens a map, or requires one to be opened.

The small test helpers (`SimulationTestFlags`, `MakePersonParams`, `VerifyInvariants`) are duplicated from the 0.1A test file rather than shared, deliberately, so that the accepted 0.1A test file is not modified by this slice. Extracting a shared test header is a reasonable later cleanup, not a change to make while 0.1A is under review.

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

Result: **18 succeeded, 0 failed, 0 with warnings**, `EXIT CODE: 0`. This includes all nine accepted 0.1A tests, unchanged and still passing. These eighteen tests remain unchanged by later slices.

## Known limitations

1. **One residence per household.** The data model allows a household to occupy `0..*` properties; this slice allows exactly one residence. Replacement trigger: the first non-residential occupancy (a workshop or field parcel worked by a household). That will likely need occupancy to become its own record with a purpose, not a second field on the household.
2. **Residence is a field pair, not a record.** `DATA_MODEL_OVERVIEW.md` sketches a `Residence Assignment` record with building space, validity interval, and status. This slice has no calendar, so an interval could not be honest, and no buildings, so there is no space to point at. Replacement trigger: residence history, move-in dates, or lodging. Moving to a record type later does not change any identifier.
3. **No property ownership.** Occupancy only, by design. Nothing models who holds a right to a property.
4. **A property cannot move or be removed.** `SettlementId` is set at creation and immutable, and no entity can be deleted in any slice so far, so settlement boundary changes and property retirement are unrepresentable. Same replacement trigger as 0.1A's deletion limitation: an explicit identifier allocator with generations or tombstones.
5. **Settlements have no extent.** Membership is purely by reference. Nothing spatial can be asked of a settlement, and nothing positions a property.
6. **Relocating while housed is refused.** The convenience of a single "move household to settlement X" call was traded for keeping one relationship from silently destroying another. If later systems find the three-step sequence tedious, the answer is a higher-level command in the application layer composing these operations, not a cascade hidden inside the registry.
7. **Settlement lists are indexes with no rebuild function.** They are maintained correctly and `ValidateInvariants` proves they agree with the back-references, but there is no `RebuildIndexes` operation, because nothing can corrupt them and there is no load path yet. A scenario loader or save format will need one.
8. **No person-level settlement index.** "Every person in this settlement" means walking the settlement's households and their members. That is correct but is a two-level walk, not an index. The data model names a person index; it should be added when a query pattern actually needs it.
9. **Snapshots can go stale**, and settlement snapshots now copy two arrays. Both are deliberate properties of the read contract, documented rather than worked around.
10. **No owner, no command boundary, no serialization.** As in 0.1A: nothing constructs the registry outside tests, callers would invoke methods directly, and no record has a schema version.
11. **No measured performance data.** The largest test holds 2 settlements, 5 properties, 4 households, and 9 people. No benchmark has been run and no scalability claim is made beyond the structural observations below.

## Deliberately deferred systems

Nothing outside settlements, properties, household settlement membership, and household residence was implemented, and nothing below was partially scaffolded — no placeholder fields, no empty types, no "for later" parameters:

- Actors, Pawns, Characters, components, visual villagers, meshes, buildings, burgage models, and animation
- Parcel geometry, lot drawing, boundaries, acreage, soil, fertility, roads, addresses, and zoning
- Construction, building upgrades, improvements, land use, and property development
- Fields, farms, crops, forestry, resources, goods, inventories, storage, custody, and logistics — good types, inventories, and conserved transfer landed in 0.1C; physical sites and inventory location landed in 0.1D; the rest remain deferred
- Production, household production, work, jobs, occupations, markets, prices, money, and wealth — exclusive current-work commitment landed in 0.1E; occupation, job markets, and production remain deferred
- Property ownership, deeds, rights, tenancy, landlords, rent, purchase, sale, inheritance, taxes, and property value
- Immigration, migration, refugee and displacement logic, and population spawning
- Calendar, simulation clock, aging, birth, death, marriage, and household formation or dissolution
- Government, titles, politics, jurisdiction, law, institutions, population classes, culture, religion, military, settlement AI, settlement levels, and city tiers
- Knowledge, libraries, education, diplomacy, war, and combat
- AI, navigation, save/load, serialization, networking, multiplayer, Mass Entity, and full ECS
- UI, widgets, Blueprint exposure, and placement tools

Each belongs to a later prototype slice or roadmap phase.

## Future scaling concerns

Recorded rather than solved, per `AI_DEVELOPMENT_RULES.md`.

1. **No measurement yet.** Everything here is structural observation. The synthetic harness `DEVELOPMENT_ROADMAP.md` requires before Prototype 0.1 closes has not been built.
2. **Settlement snapshot cost is the new one.** `FindSettlement` copies the property and household lists. At prototype sizes this is trivial; a settlement with thousands of properties would make the copy the dominant cost of asking "what is this settlement's name". If profiling shows it, the answer is a narrow query for the specific field, or a visitor that never lets an address escape — not a return to borrowed pointers. This is the concern the assignment asked to be documented rather than pre-optimised, and it is not being redesigned now.
3. **Membership lists are `TArray` with linear removal.** `RemoveSingle` and `Contains` are linear in the list length, and the invariant checker counts occurrences. That is correct and cheap for the small lists this slice creates, and deliberately not optimised without measurements. A settlement holding very many properties or households would want a set, a sorted array, or an intrusive index.
4. **Invariant validation is superlinear.** `ValidateInvariants` walks every record and, for each reference, counts occurrences in the partner's list. It is a test and diagnostic facility called at prototype sizes, not a runtime operation; it should not be wired into a shipping tick without being reworked into a sampled or incremental check.
5. **Dense arrays reallocate.** Same as 0.1A: growth copies records. Reserving at scenario load or chunked storage is the direction when a loader exists. It has no correctness implication for callers under the snapshot contract.
6. **Indexes stay minimal on purpose.** Settlement-to-property and settlement-to-household are the only reverse indexes, because they are the only ones this slice's questions need. "Occupied properties in a settlement" and "unhoused households in a settlement" are currently a filtered walk of a settlement's own list, which is bounded by the settlement, not the world. If they become hot, they are candidates for maintained counts or partitioned lists — with measurements first.
7. **Single-threaded and non-reentrant**, unchanged.
8. **What this slice already avoids.** No Actor or `UObject` per settlement or property; no tick; no world scan for any lookup or reverse lookup; no name-based identity or name-keyed storage; no existence of a household or property implied by a visual object; no stored population or occupancy totals; and no public API that can hand out a dangling reference.

## Open architectural questions

Raised for review, not decided here. The three open questions from 0.1A (who owns the registry, the module split, and definition identifiers) all still stand unchanged. This slice adds:

1. **Will occupancy become its own record?** The data model's `Residence Assignment` and `Property Right` supporting records suggest that residence and ownership eventually become relationship records with validity intervals rather than fields on the participants. If that is the intended destination, the field pair implemented here is a deliberate prototype simplification with a known migration, and the trigger is the calendar plus the first system that needs residence history.
2. **Does a household's settlement remain single-valued?** A household in transit between settlements, or one holding property in two, cannot be represented today. This is adequate for 0.1B and needs a decision when immigration is designed.
3. **Should a property be able to exist outside any settlement?** Today it cannot, which is what makes the reference invariant total. Wilderness holdings, isolated farmsteads, or properties in unincorporated land would change that, and would turn an invariant into an allowed case.
4. **Where does the relocation sequence belong?** Vacate, move, re-house is three registry operations. Whether a composed "relocate household" command lives in an application layer, and whether it is transactional across those steps, is a command-boundary question rather than a registry one.
