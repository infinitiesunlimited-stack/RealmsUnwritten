# Simulation Foundation

## Document status

- **Role:** Implementation record for the Prototype 0.1A authoritative simulation foundation
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Scope:** Persistent people and households only
- **Describes:** Only what exists in the repository today
- **Revision:** Updated after the independent Prototype 0.1A review; factual corrections applied where Prototype 0.1B extended this foundation
- **Extended by:** `SETTLEMENT_PROPERTY_RESIDENCE.md` (Prototype 0.1B), which adds settlements, properties, and household residence; `GOODS_INVENTORY.md` (Prototype 0.1C), which adds good types and inventories; `PHYSICAL_SITE_INVENTORY_LOCATION.md` (Prototype 0.1D), which adds physical sites and inventory location; `WORK_LABOR.md` (Prototype 0.1E), which adds work types and a person's exclusive current-work commitment; and `SKILLS_KNOWLEDGE.md` (Prototype 0.1F), which adds Skill Type identity without person skill state. This document is still the record for people, households, and their membership relationship.

## Purpose

This is the first production slice of `PROTOTYPE_0_1_SCOPE.md` step 1–2 ("authoritative identity" and the person/household part of "place and population"). It provides the smallest authoritative representation of persistent people and households, so that later slices — residence, property, jobs, goods, production — have a stable identity layer to reference.

It answers one question: can people and households exist, be found, and be related to each other with no Actor, no map, no tick, and no visual representation?

Nothing else was implemented by this slice. Settlements, properties, and household residence arrived in Prototype 0.1B (`SETTLEMENT_PROPERTY_RESIDENCE.md`), good types and inventories in Prototype 0.1C (`GOODS_INVENTORY.md`), physical sites with inventory location in Prototype 0.1D (`PHYSICAL_SITE_INVENTORY_LOCATION.md`), work types with person current work in Prototype 0.1E (`WORK_LABOR.md`), and Skill Type identity in Prototype 0.1F (`SKILLS_KNOWLEDGE.md`). There is still no person skill state, knowledge, calendar, occupation, production, commands, events, or save format.

## Authoritative simulation boundary

All state added by this slice is plain C++ data owned by `FSimulationRegistry`:

- It is not a `UObject`, `AActor`, `APawn`, `ACharacter`, `UActorComponent`, or subsystem.
- It has no `Tick` and registers no per-entity update.
- It requires no loaded map, no world, and no rendering; the tests construct it on the stack.
- It is not exposed to Blueprint, and there is no UI, widget, or animation involvement.
- It is not reachable from gameplay code, because no gameplay code exists yet. The owning simulation host is a later decision (see *Deliberately deferred systems*).

A compile-time assertion in the test file (`static_assert(!std::is_base_of_v<UObject, FSimulationRegistry>)`) keeps the registry from silently becoming an engine object.

This satisfies DC-12 by construction: there is currently no presentation layer that could compete for ownership of person or household truth.

## Module layout

All files added by this slice live under the module's `Private/` directory:

```text
Source/RealmsUnwritten/Private/Simulation/{SimulationIds,PersonRecord,HouseholdRecord,SimulationRegistry}.h
Source/RealmsUnwritten/Private/Simulation/SimulationRegistry.cpp
Source/RealmsUnwritten/Private/Tests/SimulationRegistryTests.cpp
```

This is the standard Unreal module layout, and UnrealBuildTool adds `Private/` to the module's private include paths automatically, so `#include "Simulation/SimulationRegistry.h"` resolves with **no change to `RealmsUnwritten.Build.cs`**. The baseline `RealmsUnwritten.h` / `RealmsUnwritten.cpp` remain at the module root, untouched, and still compile.

`Private/` rather than `Public/` is deliberate and honest: nothing outside this module consumes these headers, and none of them export a module API. When the simulation is eventually split into its own module, the headers it must publish move to `Public/` at that point, along with the export macros that decision requires.

## Entity ID strategy

### What was implemented

`Private/Simulation/SimulationIds.h` defines one phantom-typed identifier template:

```cpp
template <typename TEntityTag>
struct TSimulationId { /* uint32 value, 0 == no entity */ };

using FPersonId    = TSimulationId<FPersonIdTag>;
using FHouseholdId = TSimulationId<FHouseholdIdTag>;
```

Properties:

- **Typed.** `FPersonId` and `FHouseholdId` are unrelated types: distinct, non-convertible in either direction, and not even explicitly constructible from one another. One `static_assert` next to the aliases enforces this for every pair of identifier families; 0.1A used five pairwise assertions for the two families, which 0.1B replaced with the equivalent set-wide predicate when two more families arrived (see `SETTLEMENT_PROPERTY_RESIDENCE.md`). The tag also supplies a debug name for `ToString`, which yields `Person#4` / `Household#2` for diagnostics.
- **Meaningless without the registry.** The raw-value constructor is `explicit` and intentionally available, because validation code and boundary tests need to construct arbitrary values on purpose. Constructing an identifier grants no capability: any value the registry did not allocate fails to resolve, across the entire `uint32` range. Integers never convert to identifiers implicitly.
- **Explicitly invalid.** Value `0` is the default-constructed state and never resolves to a record. `IsValid()` reports it.
- **Allocated by the owner.** `FSimulationRegistry` assigns an identifier at creation. Two entities created in the same session cannot share an identifier, identifiers are not reused, and a rejected creation allocates nothing.
- **Not names.** There is no lookup by name anywhere in the API, and creating two people or two households with identical names produces distinct identifiers and distinct records. Tests assert this directly.
- **Hashable and comparable**, so identifiers work in `TSet`/`TMap` and `TArray::Contains`.

### Validation and range safety

Resolution converts an identifier to a storage slot through one helper that **range-checks the unsigned value before any narrowing**:

```cpp
const ValueType Value = Id.GetValue();
if (Value == TIdType::InvalidValue || Value > static_cast<ValueType>(RecordCount))
{
    return INDEX_NONE;
}
return static_cast<int32>(Value - 1);
```

Because the comparison happens in `uint32`, no identifier value can reach a signed narrowing or a signed-overflow subtraction. Every value outside `[1, RecordCount]` — including `0`, one past the allocated range, `MAX_int32`, `0x80000000`, and `MAX_uint32` — returns `INDEX_NONE` and therefore fails lookup safely. An earlier version narrowed to `int32` before subtracting one, which put the top half of the `uint32` range into signed-overflow territory; that is the defect this check replaces.

### Why this choice

The requirement in `DESIGN_CONSTITUTION.md` (simulation integrity rule 1) and `HIGH_LEVEL_ARCHITECTURE.md` ("Stable identifiers") is that every persistent entity has a stable identifier independent of its Unreal object lifetime, and that references between authoritative entities use identifiers rather than Actor pointers. A monotonic integer allocated by the owning registry is the smallest mechanism that satisfies this.

Alternatives considered and rejected as premature for Prototype 0.1A:

- **`FGuid`** — 16 bytes per reference, worse cache behaviour in member lists, and its only real advantage (uniqueness across separately generated data sets) matters for distributed generation, networking, and merge tooling that do not exist.
- **`FName`/string keys** — directly conflicts with "names are not identifiers".
- **Handle with generation counter** — solves stale-reference detection after deletion. Nothing is deleted in this slice, so the counter would be unused machinery. The replacement trigger is recorded below.
- **Definition IDs** — `HIGH_LEVEL_ARCHITECTURE.md` requires definition identifiers to be distinct from runtime entity identifiers. No definitions existed at 0.1A, so no definition identifier type was added then. Prototype 0.1C added the first definition family, good types, and after architectural review it carries two identifiers for two different jobs: an `FName` authored key that is the durable definition identity, and an `FGoodTypeId` that is only a dense runtime handle for lookup and inventory storage. So a definition identity is a different kind of thing from an entity identifier, not a differently tagged `TSimulationId`. See `GOODS_INVENTORY.md`.

The identifier is deliberately not serialized yet; there is no save format in this slice.

## Person representation

`Private/Simulation/PersonRecord.h`:

| Field | Type | Justification |
|---|---|---|
| `Id` | `FPersonId` | Stable identity. Constitution integrity rule 1; scope "Stable ID". |
| `GivenName` | `FString` | Display identity. Data model "Name/display identity"; scope "display name". |
| `FamilyName` | `FString` | Display identity, and the household name a person is usually associated with. |
| `AgeYears` | `int32` | Data model "birth date or age basis"; scope "Age or birth date". Never negative. |
| `LifeState` | `EPersonLifeState` | Constitution integrity rule 3 requires an explicit life-state; scope "Life-state". |
| `HouseholdId` | `FHouseholdId` | Data model "Primary household ID"; invalid means no household. |

`EPersonLifeState` has exactly two values, `Alive` and `Deceased`. Created people are `Alive`. No operation sets `Deceased` in this slice, because mortality belongs to Phase 0.2; the field exists because the constitution requires life-state to be explicit rather than inferred.

### Creation validation and failure

`CreatePerson` rejects a negative `AgeYears`. Rejection is expressed as an **invalid returned identifier**, which is the smallest failure mechanism consistent with the rest of the API: an identifier that does not resolve is already the established "no entity" signal, and person creation has exactly one failure mode. Operations with several distinct outcomes use a result enum instead, as household membership does.

A rejected creation is atomic, because validation happens before storage is touched:

- no record is appended,
- the person count is unchanged,
- no identifier is consumed, so the next accepted creation continues the sequence,
- the returned identifier resolves to nothing, through `ContainsPerson` or `FindPerson`.

No maximum age rule was invented, `0` is accepted, and `ValidateInvariants` also reports any stored negative age as a broken invariant.

### Fields deliberately not added

- **Birth date** — requires the authoritative calendar, which is a separate system and not part of this slice. `AgeYears` is the declared prototype age basis (see *Known limitations*).
- **Occupation** — still deferred. Occupation is a social-economic role and is not current work. Prototype 0.1E (`WORK_LABOR.md`) added an exclusive `CurrentWork` commitment on the person, targeted at a physical site; that is labor state, not a profession, and not physical presence. Adding a bare occupation string or enum now would still create an unowned field.
- **Origin place, cultural background** — neither field exists on a person record. Settlements exist as of Prototype 0.1B, but nothing records where a person came from, and there is no culture system to reference.
- **Residence** — a person record has no residence or settlement field, and gained none in 0.1B. Prototype 0.1B provides the property and residence foundation, but it is authoritative on the *household*: a household has an explicit settlement membership and an optional residential property, and a person's place is derived by reading their household's. See `SETTLEMENT_PROPERTY_RESIDENCE.md`.
- **Inventory, possessions, person skill state, kin links, activity, location** — need knowledge, job, and spatial systems that do not exist. Prototype 0.1C added inventories and Prototype 0.1D locates them at physical sites, but a person record still holds no inventory reference: a person is not an inventory holder. Prototype 0.1E added current work as labor commitment, not as person location or presence. Prototype 0.1F added Skill Type identity but no skill field to the person. See `PHYSICAL_SITE_INVENTORY_LOCATION.md`, `WORK_LABOR.md`, and `SKILLS_KNOWLEDGE.md`.
- **Death date and cause, genetics, disease, personality, religion, ideology, knowledge, marriage, reproduction, inheritance, needs, individual AI, appearance** — out of scope by the assignment and by `PROTOTYPE_0_1_SCOPE.md`.

## Household representation

`Private/Simulation/HouseholdRecord.h`:

| Field | Type | Justification |
|---|---|---|
| `Id` | `FHouseholdId` | Stable identity. Data model "Stable household ID". |
| `Name` | `FString` | Display/family name. Never an identifier. |
| `Members` | `TArray<FPersonId>` | Data model "Member person IDs". Contains no duplicates and no unresolvable identifiers. |

Household population is derived by reading `Members`; no population count is stored anywhere, which is what DC-02 requires of settlement population as well.

Prototype 0.1B added two further fields, `SettlementId` and `ResidenceId`, documented in `SETTLEMENT_PROPERTY_RESIDENCE.md`. They are written only by that slice's operations and do not affect anything described here.

### Fields deliberately not added

Head/representative, origin/founding event, property rights, inventories, wealth, needs, consumption policy, migration status, and household history are all absent. Each needs a system that did not exist at 0.1A (property, inventory, market, migration, event history), and every one of them was explicitly deferred by the assignment. Residence was among them and landed in 0.1B; property *rights* remain absent, because occupancy is not ownership. Inventories themselves exist as of 0.1C and are located at physical sites as of 0.1D, but a household record still holds no inventory reference.

## Registry and state ownership

`Private/Simulation/SimulationRegistry.{h,cpp}` defines `FSimulationRegistry`, the sole owner of person and household records. Prototype 0.1B extended the same class with settlement and property records, Prototype 0.1C with good type and inventory records, and Prototype 0.1D with physical site records, all on the identical storage and read model.

### Storage

```cpp
TArray<FPersonRecord>    PersonRecords;
TArray<FHouseholdRecord> HouseholdRecords;
```

One dense array per entity family. An identifier's numeric value is its storage slot plus one, so:

- Lookup is a range check plus an index read, not a search, and never a world scan. No name index, no map iteration, no Actor iteration.
- Records of the same type are contiguous, which is the layout `HIGH_LEVEL_ARCHITECTURE.md` asks for ("structure high-volume authoritative records for batch iteration and compact storage") and a reasonable starting point for later profiling.
- Identifier allocation needs no separate counter, which avoids a second source of truth for "the next identifier".

### Read contract

**The registry never hands out an address into its storage.** `FindPerson` and `FindHousehold` return `TOptional<FPersonRecord>` / `TOptional<FHouseholdRecord>` — a copy taken at the time of the call:

- A snapshot is a read at a point in time. It is not a live view and does not observe later changes.
- A snapshot cannot be invalidated by any later registry operation, including creations that reallocate the dense arrays. Callers may hold one as long as they like; what they must not do is assume it is current.
- Callers that need current state call again. Callers that need to change state use the membership operations.
- `ContainsPerson` / `ContainsHousehold` answer existence without copying anything, and are safe for any identifier value.

This replaces an earlier API that returned `const FPersonRecord*` into the arrays. Those addresses were valid only until the next creation call, which made the hazard the caller's problem and would have become harder to correct as consumers appeared. Returning copies also keeps authoritative records immutable to callers, which is what makes the relationship invariants enforceable.

Internally, resolution still produces short-lived `const`/mutable record pointers, but they are private, used within a single operation, and never returned.

### Ownership notes

- The registry owns the records; nothing else holds a copy except transient snapshots.
- Nothing owns the registry yet. It is constructed by its tests. Choosing the simulation host (subsystem, game instance, standalone simulation object, or headless harness) is a separate decision that belongs with the simulation clock and command boundary, and it is a documented architectural review trigger, so it was not decided here.
- No singleton, no global accessor, and no static state were introduced.

## Person/household relationship invariants

All membership changes funnel through one private transition, `SetPersonHousehold`, which detaches from the previous household and attaches to the new one in a single call. The two public operations validate and delegate; there is no other write path.

| Invariant | How it is enforced |
|---|---|
| A person cannot belong to two households at once | `HouseholdId` is a single field; the transition detaches from the previous household before attaching |
| A household cannot list the same person twice | `AddPersonToHousehold` returns `AlreadyMember` without changing state; the transition asserts the member is absent before adding |
| `Person.HouseholdId` and `Household.Members` cannot disagree | Both sides are written by the same private transition, and callers only ever receive copies |
| Removal clears both sides | Removal runs the same transition with an invalid household, which removes the member entry and clears the person's field |
| Moving leaves no stale membership | The transition removes the old member entry before attaching; tested from both households |
| Invalid person identifiers fail safely | Resolved through the range-checked helper; unresolvable identifiers return `UnknownPerson` and change nothing |
| Invalid household identifiers fail safely | Resolved through the range-checked helper; unresolvable identifiers return `UnknownHousehold` and change nothing |
| Names never determine identity | No name-keyed storage or lookup exists; identical names produce distinct identifiers and records |
| Stored ages are never negative | `CreatePerson` rejects a negative age atomically; `ValidateInvariants` reports one if it ever appears |

`ValidateInvariants(FString& OutFailureDescription)` re-derives all of the above from stored state and reports the first inconsistency it finds: slot/identifier agreement, negative ages, unresolvable references on either side, duplicate member entries, and any person/household disagreement. It is a development and test facility, not part of normal operation, and it is called by every test.

Rejected operations are reported, never swallowed: each returns a specific `EHouseholdMembershipResult` and leaves state untouched, which matches constitution integrity rule 6 ("rejected commands leave state unchanged"). This is not yet a validated command pipeline; it is direct API validation, and the command boundary in `HIGH_LEVEL_ARCHITECTURE.md` remains unbuilt.

## Public operations

```cpp
FPersonId    CreatePerson(const FPersonCreationParams& Params);  // invalid ID == rejected
FHouseholdId CreateHousehold(const FString& Name);

bool ContainsPerson(FPersonId PersonId) const;
bool ContainsHousehold(FHouseholdId HouseholdId) const;

TOptional<FPersonRecord>    FindPerson(FPersonId PersonId) const;        // snapshot copy
TOptional<FHouseholdRecord> FindHousehold(FHouseholdId HouseholdId) const; // snapshot copy

int32 GetPersonCount() const;      // derived from storage, not an authoritative population
int32 GetHouseholdCount() const;   // derived from storage

EHouseholdMembershipResult AddPersonToHousehold(FPersonId PersonId, FHouseholdId HouseholdId);
EHouseholdMembershipResult RemovePersonFromHousehold(FPersonId PersonId, FHouseholdId HouseholdId);

bool ValidateInvariants(FString& OutFailureDescription) const;
```

`FPersonCreationParams` carries `GivenName`, `FamilyName`, and `AgeYears`. A new person starts alive and with no household; membership is always a separate, explicit operation.

`EHouseholdMembershipResult` values: `Success`, `AlreadyMember`, `UnknownPerson`, `UnknownHousehold`, `NotAMember`. Only `Success` changes state.

`AddPersonToHousehold` is the authoritative membership setter. Called for a person who already belongs to another household, it performs the move, because "a person belongs to exactly one household" is the invariant being protected. `RemovePersonFromHousehold` requires the caller to name the household the person actually belongs to, so a mistaken removal is reported as `NotAMember` rather than silently detaching them from somewhere else.

## Testing

`Private/Tests/SimulationRegistryTests.cpp`, guarded by `WITH_DEV_AUTOMATION_TESTS`. Nine Unreal automation tests under `RealmsUnwritten.Simulation.Registry`:

| Test | Covers |
|---|---|
| `Creation` | Valid and distinct identifiers for people and households, stored field values, new person is alive and unhoused, new household is empty, identically named entities stay separate identities |
| `CreationValidation` | Negative ages (`-1`, `-34`, `MIN_int32`) rejected with an invalid identifier, no record added, count unchanged, identifier unresolvable, no identifier consumed, and age `0` accepted |
| `IdentifierBoundaries` | `0`, a valid allocated identifier, one past the allocated range, `MAX_int32`, `0x80000000`, and `MAX_uint32`, for both entity families, through `Contains`, `Find`, add, and remove — with the registry provably unchanged |
| `Lookup` | Existing entities resolve; default and unallocated identifiers do not; failed lookups change nothing; a snapshot taken before 64 further creations is still valid and still holds its values |
| `Membership` | A person joins a household; membership is correct from both sides; re-adding reports `AlreadyMember` and does not duplicate |
| `MembershipTransfer` | Moving between households; the person claims only the destination; the origin has no stale member entry |
| `MembershipRemoval` | Removal clears both sides and leaves other members alone; removing a non-member and removing from the wrong household are both rejected without detaching anyone |
| `InvalidMembership` | Unknown and default identifiers rejected on both add and remove, with the registry provably unchanged afterwards |
| `HeadlessPopulationChurn` | Prototype-sized population (5 households, 25 people) with membership churn; all identifiers distinct; membership totals agree from both sides |

Every test constructs its own registry on the stack, and every test ends by calling `ValidateInvariants`. No test spawns an Actor, creates a `UObject`, opens a map, or requires one to be opened.

Compile-time coverage: the identifier type-safety assertions live in `SimulationIds.h` so they hold in every build rather than only where tests are compiled, and the test file asserts that the registry is not a `UObject`.

`HeadlessPopulationChurn` was previously named `HeadlessSettlement`, which implied a settlement entity that does not exist. It tests registry behaviour without a world, Actor, or map, and is now named for that.

### Commands and results

Build:

```text
Engine\Build\BatchFiles\Build.bat RealmsUnwrittenEditor Win64 Development ^
  -Project="<repo>\RealmsUnwritten.uproject" -WaitMutex
```

Result: **Succeeded**, no warnings from the new code.

Tests:

```text
UnrealEditor-Cmd.exe "<repo>\RealmsUnwritten.uproject" ^
  -ExecCmds="Automation RunTests RealmsUnwritten.Simulation" ^
  -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nullrhi -NoSound
```

Result: **9 found, 9 succeeded, 0 failed, 0 with warnings**, total duration 0.140 s. These nine tests are unchanged by Prototype 0.1B, 0.1C, and 0.1D and still pass; later slices added further tests under the same `RealmsUnwritten.Simulation` prefix.

## Known limitations

1. **Age instead of birth date.** `AgeYears` is a declared prototype simplification, used because no authoritative calendar exists. Ages do not advance. Replacement trigger: the Time and Calendar system landing. At that point `AgeYears` becomes a birth date and age becomes derived. Nothing depends on the field yet, so the change is cheap now and expensive later.
2. **No removal or deletion.** People and households can be created but not removed, so identifier reuse and stale-handle detection are untested concerns. Replacement trigger: the first system that needs to retire an entity (household dissolution in Phase 0.2). That work requires an explicit identifier allocator plus generation counters or tombstones, and `ValidateInvariants` will need to cover it.
3. **Identifier value is the storage slot.** Convenient and fast while nothing is removed. Any future compaction or removal must either keep slots stable (tombstones) or introduce an identifier-to-index map; it must not renumber existing identifiers. Note that lookup already range-checks against the current record count, so this coupling is not also a safety property.
4. **Reads copy.** A person snapshot copies two `FString`s; a household snapshot also copies its member array. This is the price of a read contract with no lifetime hazard, and it is negligible at prototype scale. If profiling later shows it matters on a hot path, the answer is a narrow query for the specific field or a visitor that never lets an address escape — not a return to borrowed pointers.
5. **Snapshots can go stale.** Callers holding a snapshot across a mutation see old values. This is a deliberate, documented property rather than a bug; nothing in the codebase currently holds one across a mutation except the test that proves snapshots survive reallocation.
6. **No `Deceased` transition.** The life-state field exists and is validated as `Alive` on creation, but nothing can set it yet.
7. **No command boundary, events, or read models.** Callers invoke registry methods directly. The application/command layer, domain events, and UI-facing projections described in `HIGH_LEVEL_ARCHITECTURE.md` are not built.
8. **No serialization.** Records have no schema version and cannot be saved or loaded. Nothing has been promised about save compatibility.
9. **No owner.** Nothing constructs the registry outside tests, so the foundation is not yet reachable from a running game.
10. **No measured performance data.** The tests exercise at most 65 people; no benchmark has been run, so no scalability claim is made beyond the structural properties described below.

## Deliberately deferred systems

Nothing outside people, households, and their membership relationship was implemented. Specifically not implemented, and not partially scaffolded:

- Visual villagers, Actors, Pawns, Characters, components, animation, meshes, UI, and widgets
- Blueprint exposure of any kind
- Calendar, simulation clock, scheduling, and headless stepping
- Settlements, properties, boundaries, roads, residences, and buildings — settlements, properties, and residence landed in 0.1B; boundaries, roads, and buildings remain deferred
- Fields, crops, agriculture, farming, and forestry
- Resources, lots, inventories, custody, reservations, logistics, and transport — good types, inventories, and conserved transfer landed in 0.1C; physical sites and inventory location landed in 0.1D; lots, reservations, capacity, logistics, and transport remain deferred
- Production, milling, baking, market exchange, prices, wealth, and consumption
- Jobs, occupations, labor assignment, and needs — exclusive current-work commitment landed in 0.1E; occupation, job markets, schedules, wages, person skill state, and needs remain deferred
- Birth, death, aging, marriage, reproduction, inheritance, household formation and dissolution, migration, and immigration
- Knowledge, person skill state, teaching, and technology — Skill Type identity landed in 0.1F; all person capability state and behavior remain deferred
- Institutions, government, politics, law, taxation, diplomacy, combat, and military
- AI planners, policies, and behaviour of any scope
- Save/load, serialization, and schema migration
- Networking, multiplayer, and replication
- Mass Entity, ECS, external databases, World Partition integration, multithreading, simulation LOD, regional simulation, and large-scale schedulers
- Domain events, command validation pipeline, and read models
- Entity deletion and identifier generation counters

Each belongs to a later prototype slice or roadmap phase. None of them was needed to satisfy this slice, and none was represented by a placeholder field.

## Future scaling concerns

Recorded rather than solved, per `AI_DEVELOPMENT_RULES.md`.

1. **No measurement yet.** Everything below is a structural observation, not a benchmark result. `DEVELOPMENT_ROADMAP.md` requires a synthetic harness exercising thousands of lightweight person records before Prototype 0.1 closes; that harness is not part of this slice.
2. **`FString` names dominate the record.** Two `FString` fields mean two heap allocations per person, poor locality for batch passes that do not need names, and two string copies per snapshot read. At thousands of people this is the first thing to measure. Candidate directions: `FName`, an interned name table, or moving names out of the hot record into a side table. This should be driven by profiling, not assumption.
3. **Snapshot cost on wide reads.** Reading every household to build an aggregate copies every member array. Aggregates of that shape belong in a purpose-built read model or a visitor pass, which is exactly what `HIGH_LEVEL_ARCHITECTURE.md` describes; the snapshot API is for resolving individual entities.
4. **`TArray<FPersonId> Members` per household.** Fine at household sizes, but it is one allocation per household. If household counts grow into the tens of thousands, a shared member pool or chunked storage is the direction.
5. **Growth reallocation.** Dense arrays reallocate and move all records as the population grows, which costs a copy. Reserving capacity at scenario load, or chunked storage, addresses this when a scenario loader exists. This no longer has correctness implications for callers, only cost.
6. **Indexes are absent by design.** Lookup by identifier is O(1), but nothing indexes people by age band or life-state, and 0.1A added no reverse index beyond a household's member list. The first query pattern that would otherwise scan all people (for example "living members of this settlement") should add a targeted, rebuildable index rather than a scan. `DATA_MODEL_OVERVIEW.md` already names the expected index set; 0.1B added the first two entries of it, settlement-to-property and settlement-to-household.
7. **Single-threaded and non-reentrant.** The registry assumes one writer. Any future concurrent access is a design decision, not something to bolt on.
8. **Cold and off-screen storage.** `DATA_MODEL_OVERVIEW.md` allows cold data to be stored differently. Nothing here prevents that, but nothing here implements it either.
9. **What this slice already avoids.** No Actor, `UObject`, or tick per person; no world scan for lookup; no name-based identity; no dependence on visual representation for existence; no stored population totals; and no public API that can hand out a dangling reference.

## Open architectural questions

Raised for review, not decided here:

1. **Who owns the registry?** Selecting the simulation host and its lifetime touches the command boundary, the clock, and eventually save/load. It is an architectural review trigger and should be decided with the calendar slice rather than implied by this one.
2. **Module split.** `AI_DEVELOPMENT_RULES.md` sketches a target layout with separate `SimulationCore` / `SimulationSystems` / `Application` modules, and marks it as a target rather than a scaffolding request. This slice stays inside the single existing `RealmsUnwritten` module, using the standard `Private/` layout. The split into real modules remains an open decision; the code depends on nothing beyond `Core`, so it can move when that decision is made.
3. **Definition identifiers.** Answered for good types by the Prototype 0.1C architectural review: a durable `FName` authored key is the definition identity, and the dense typed ID is only a runtime handle. Work Types reused the split in 0.1E and Skill Types reused it in 0.1F. Whether authored keys eventually need versioning rules remains open.
