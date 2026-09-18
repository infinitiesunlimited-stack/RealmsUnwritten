# Work and Labor Foundation

## Document status

- **Role:** Implementation record for the Prototype 0.1E work and labor foundation
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (0.1A), `SETTLEMENT_PROPERTY_RESIDENCE.md` (0.1B), `GOODS_INVENTORY.md` (0.1C), and `PHYSICAL_SITE_INVENTORY_LOCATION.md` (0.1D), none of which is superseded
- **Scope:** Work types, a person's exclusive current-work commitment, and stationary physical-site work targets
- **Describes:** Only what exists in the repository today

## Purpose

This slice begins the labor half of `PROTOTYPE_0_1_SCOPE.md` step 4 ("work and movement") without implementing movement. It answers one new authoritative question:

**What is this person currently committed to do, and where is that work targeted?**

It proves:

> A persistent simulated person can have one exclusive current work commitment at a stationary physical site.

Labor originates from people. There is no workforce counter, no household labor pool, and no job board. A person holds at most one current commitment. Reassignment replaces it.

Occupation is not this slice. A future profession (farmer, miller, baker) is a social-economic role; current work is the activity occupying the person now. A farmer may later harvest, repair, haul, or build. Those are work types, not occupations.

Assignment is not presence. `CurrentWork` targeted at a physical site means the commitment is aimed at that place. It does not mean the person is there, walked there, arrived, is animating work, or is within any distance.

Work does not produce goods, move goods, move people, pay wages, or change skill.

## Authoritative simulation boundary

Unchanged from 0.1A–0.1D: all state added here is plain C++ data owned by `FSimulationRegistry`, which is not a `UObject`, `AActor`, or subsystem, has no `Tick`, requires no loaded map, and is not exposed to Blueprint. Tests construct the registry on the stack.

Boundary points specific to work:

- **A work type is not a C++ enum of medieval jobs and not an occupation.** Harvest, repair, and any other kind are created at runtime under authored keys. Production code names no catalog.
- **Current work is not occupancy.** Physical sites do not list workers. The person names a site; the site does not claim the person.
- **Current work is not a job posting.** There is no `FWorkAssignmentId`, no issuer, no duration, no reservation, and no marketplace.

This is compatible with DC-19: future military service must use the same persistent person and must not leave civilian labor simultaneously available. The exclusive `CurrentWork` slot is that scarcity mechanism. This slice does not implement military service.

## Module layout

```text
Source/RealmsUnwritten/Private/Simulation/WorkTypeRecord.h
Source/RealmsUnwritten/Private/Simulation/CurrentWork.h
Source/RealmsUnwritten/Private/Tests/WorkLaborTests.cpp
```

Files extended: `SimulationIds.h`, `PersonRecord.h`, `SimulationRegistry.{h,cpp}`, `Tests/SimulationRegistryTestAccess.h`. **`RealmsUnwritten.Build.cs` is unchanged.**

## Entity ID types

`SimulationIds.h` gains one tag and alias:

```cpp
using FWorkTypeId = TSimulationId<FWorkTypeIdTag>;
```

It is the eighth mutually distinct family. `ToString` yields `WorkType#3`. There is no `FWorkAssignmentId`.

### Work type identity: durable key, runtime handle

The same split established for good types:

```text
AuthoredKey (FName, e.g. Work.TestHarvest)   durable definition identity
        |
        |  FindWorkTypeIdByKey
        v
FWorkTypeId (dense runtime handle)           valid within one registry
        |
        |  stored in
        v
FCurrentWork::WorkTypeId
```

`NAME_None` and duplicate work-type keys are rejected and consume no handle. Display names are not identity. Work-type keys are a separate namespace from good-type keys: `Work.Wheat` and `Goods.Wheat` do not conflict.

## Current work value

`FCurrentWork` is a tagged value object in the same style as `FInventoryLocation`. It is not an entity.

```text
ECurrentWorkKind::None          → no work; both payload IDs invalid
ECurrentWorkKind::PhysicalSite  → WorkTypeId + PhysicalSiteId, both resolvable
```

Factories: `Nowhere()` and `AtPhysicalSite(WorkTypeId, PhysicalSiteId)`. No other kinds exist. Fields are public on detached copies and snapshots; authoritative mutation is only through the registry.

`FCurrentWork` is not `FInventoryLocation`. Inventory place and labor commitment are different questions.

Work type and site purpose stay independent. A person may be assigned `Work.TestHarvest` at a site whose purpose key is `Test.BarnStorage`.

## Person record

`FPersonRecord` gains one field:

```text
CurrentWork
```

Default is `Nowhere()`. It is a single value, not an array. Occupation, profession, employer, schedule, skill, hours, and military service are not present.

An unhoused person may hold valid current work. Assignment does not require household, settlement, residence, or settlement matching.

## Registry operations

| Operation | Role |
|---|---|
| `CreateWorkType` | Definition identity; rejects `None` and duplicate work-type keys |
| `ContainsWorkType` / `FindWorkType` / `FindWorkTypeIdByKey` / `GetWorkTypeCount` | Snapshot reads |
| `AssignPersonWork` | Exclusive replace of `CurrentWork` with `AtPhysicalSite` |
| `RemovePersonWork` | Sets `CurrentWork` to `Nowhere()` |

`AssignPersonWork` validates person, then work type, then site, then writes. The exact same person/type/site triple returns `AlreadyAssigned` with no mutation. A different type or site returns `Success` and replaces the commitment. Failures leave the existing commitment unchanged.

`RemovePersonWork` returns `NotAssigned` when the person already has no work.

Reads of current work go through `FindPerson`. Snapshots are copies.

Private `SetPersonWork` is the single write path. It does not update any site list, because sites do not list workers.

## Result enum

`EPersonWorkResult`: `Success`, `AlreadyAssigned`, `UnknownPerson`, `UnknownWorkType`, `UnknownSite`, `NotAssigned`.

## Invariants

Work types: slot ID matches; authored key not `None`; authored keys unique within work types.

Person current work is validated independently of household membership:

- `None` → both payload IDs invalid
- `PhysicalSite` → both IDs valid and resolvable
- any other kind → failure

An unhoused person with valid current work is consistent.

## What this slice does not do

Not implemented, and not scaffolded:

- Production, harvest execution, recipes, construction
- Movement, pathfinding, presence, travel, arrival, distance
- Occupation, profession, person skill state, skill values, wages, contracts, job requests, job board
- `FWorkAssignmentId` or assignment-record arrays
- Schedules, hours, fatigue, eligibility
- Household labor AI, work priorities, seasonal labor
- Military service, formations, combat
- `PhysicalSite.Workers` or any reverse worker list
- A hard-coded work catalog or medieval job enum
- Goods mutation or goods audit from work operations
- Actors, UObjects, ticks, world scans, UI, animation

## Labeled prototype exceptions

Prototype 0.1E records these simplifications under the constitution's prototype exception standard:

1. **One exclusive current commitment, not 0..* job assignments.** `DATA_MODEL_OVERVIEW.md` describes occupation, job requests, assignments, and activity as distinct long-term concepts. This slice implements only the exclusive person-side commitment. Replacement trigger: the first system that needs an independent job lifecycle, multiple concurrent workers on one request, or assignment identity.
2. **Assignment is not presence.** Physical execution of work is later. Replacement trigger: movement/presence.
3. **Work types are allocated at runtime**, like good types. Replacement trigger: authored content pipeline.
4. **Stationary physical-site targets only.** Replacement trigger: the first non-site work target, added as a new `ECurrentWorkKind`.
5. **Occupation is unimplemented.** Current work must not be treated as profession.

## Future-extension notes

Documented, not implemented.

- Occupation becomes a separate definition family and person field.
- Job requests and `FWorkAssignmentId` appear when work has an independent lifecycle.
- New current-work kinds cover fields, mobile targets, or service without overloading `PhysicalSiteId`.
- A derived worker index may appear when production needs "who is committed here."
- Military service (DC-19) occupies or clears this exclusive slot on the same person.
- Death and injury APIs must clear or restrict the slot so labor does not remain available.

## Tests

Headless tests in `WorkLaborTests.cpp`:

| Test | Proves |
|---|---|
| `WorkType.Creation` | Authored key, display name, `None`/duplicate rejection, no ID consumed, `FindWorkTypeIdByKey`, creation-order handles vs stable keys, separate namespace from good types |
| `Work.Assignment` | Valid assignment; snapshot kind/type/site; unhoused person may work; invariants pass |
| `Work.AssignmentRejection` | Unknown person/type/site and boundary IDs; existing work unchanged |
| `Work.Reassignment` | Different type or site replaces; same triple is `AlreadyAssigned`; still one commitment |
| `Work.Removal` | Clears to canonical `Nowhere`; second removal is `NotAssigned` |
| `Work.SnapshotRegression` | Person and work-type snapshots are copies |
| `Work.DoesNotAffectGoods` | Assign/reassign/remove change no quantities, site inventory lists, locations, or goods audit |
| `Work.InvariantDetection` | Direct detection of the listed corruptions; unhoused person with valid work passes |

`FSimulationRegistryTestAccess` exposes `PersonRecords` and `WorkTypeRecords` for corruption proofs only.
