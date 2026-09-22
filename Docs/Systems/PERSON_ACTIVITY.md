# Person Activity Foundation

## Document status

- **Role:** Implementation record for Prototype 0.1J authored Activity Type identity and one optional Current Activity per person
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (0.1A) and the authored-definition identity pattern established by Good Type, Work Type, and Skill Type
- **Scope:** Activity Type identity and one tagged `CurrentActivity` value on `FPersonRecord`
- **Describes:** Only what exists in the repository today

## Purpose

This slice answers one new authoritative question:

> What broad kind of behavior is this person currently recorded as being engaged in?

It implements only:

```text
authored Activity Type identity
+
one optional Current Activity per Person
```

Activity Types are authored data. Working, Sleeping, Eating, Traveling, Learning, Teaching, Worshipping, Fighting, Caring, Resting, and Socializing are conceptual examples of broad behavior. They are not a locked production catalog. Tests may register keys such as `Activity.Working` at runtime.

## Architectural separation

The simulation distinguishes:

| Concept | Question | Status |
|---|---|---|
| Occupation | What is this person recognized as economically or socially? | Not implemented |
| Current Work | What work relationship or site is this person currently attached to? | 0.1E (`WORK_LABOR.md`) |
| Presence | Where is this person physically? | Not implemented |
| Current Activity | What broad kind of behavior is currently recorded? | 0.1J (this document) |
| Task | What specific kind of objective exists, and which occurrences of it exist? | Task Type identity 0.1K, Task Instance identity 0.1L (`TASKS.md`) |
| Current Task | What specifically is this person trying to accomplish? | Not implemented |
| Participation | What simulation processes is this person contributing to? | Not implemented |
| Capability | What broad transferable abilities has this person acquired? | 0.1G–0.1I (`PERSON_CAPABILITY.md`) |
| Practice | What meaningful developmental participation has accumulated? | 0.1H (`PERSON_CAPABILITY.md`) |

These are separate concepts. 0.1J does not collapse them.

## Persistent state is not continuously simulated state

`CurrentActivity` is authoritative current Activity when such Activity state is instantiated.

0.1J does not require every persistent Person to have a continuously resolved Activity. A Person with `CurrentActivity = None` may simply have no detailed moment-to-moment Activity currently instantiated.

This is intentional for future simulation level-of-detail. Local people may later resolve detailed Activity, regional people coarse Activity blocks, and distant people aggregated outcomes. 0.1J does not choose or implement that mechanism. There is no per-person ticking, activity timer, scheduler, background loop, AI behavior, or daily schedule.

## Module layout

```text
Source/RealmsUnwritten/Private/Simulation/ActivityTypeRecord.h
Source/RealmsUnwritten/Private/Simulation/CurrentActivity.h
Source/RealmsUnwritten/Private/Tests/PersonActivityTests.cpp
```

Files extended: `SimulationIds.h`, `PersonRecord.h`, `SimulationRegistry.{h,cpp}`, `Tests/SimulationRegistryTestAccess.h`. **`RealmsUnwritten.Build.cs` is unchanged.**

## Entity ID types

`SimulationIds.h` gains one tag and alias:

```cpp
using FActivityTypeId = TSimulationId<FActivityTypeIdTag>;
```

It is the tenth mutually distinct family. `ToString` yields `ActivityType#3`. The compile-time distinctness proof includes it alongside the nine earlier families and is not weakened.

0.1J added no `FTaskTypeId`, `FTaskId`, or `FOccupationId`. Prototype 0.1K later added
`FTaskTypeId` and Prototype 0.1L added `FTaskId` (`TASKS.md`); `FOccupationId` still does
not exist. Current Activity references neither task identifier.

### Activity Type identity: durable key, runtime handle

The same split established for Good Types, Work Types, and Skill Types:

```text
AuthoredKey (FName, e.g. Activity.Working)   durable definition identity
        |
        |  FindActivityTypeIdByKey
        v
FActivityTypeId (dense runtime handle)       valid within one registry
        |
        |  stored in
        v
FCurrentActivity::ActivityTypeId
```

`NAME_None` and duplicate activity-type keys are rejected and consume no handle. Display names are not identity. Activity-type keys are a separate namespace from every other authored-type family: the exact key `Working` may simultaneously identify a Good Type, Work Type, Skill Type, and Activity Type.

## Activity Type record

`FActivityTypeRecord` contains exactly:

```text
AuthoredKey  durable authored definition identity
Id           FActivityTypeId runtime handle within one registry
Name         display data; not identity
```

An Activity Type is not a Task, Skill Type, Occupation, Work Type, job, profession, location, production recipe, process, capability, or simulation event.

Activity Types describe broad behavior. Granularity such as ShapeRoofBeam, HarvestWheat, MilkCow, ForgeHorseshoe, RepairCart, DigIronOre, BakeBread, SawOakBoard, or FitMortiseJoint belongs to the Task system, whose authored Task Type identity landed in 0.1K (`TASKS.md`). 0.1J does not introduce micro-activities and does not author a permanent catalog.

## Current activity value

`FCurrentActivity` is a tagged value object in the same style as `FCurrentWork`. It is not an entity.

```text
ECurrentActivityKind::None          → no recorded activity; ActivityTypeId invalid
ECurrentActivityKind::ActivityType  → ActivityTypeId valid and resolvable
```

Factories: `Unrecorded()` and `OfType(ActivityTypeId)`. No other kinds exist. Fields are public on detached copies and snapshots; authoritative mutation is only through the registry.

`CurrentActivity` contains no PhysicalSiteId, SettlementId, PropertyId, InventoryLocationId, coordinates, Actor pointer, movement state, road, destination, task, duration, start time, history, or schedule.

## Person record

`FPersonRecord` gains one field:

```text
CurrentActivity
```

This is intentional. Singular authoritative current state about the person may live directly on `FPersonRecord`. Sparse many-to-many relationships such as Person Capabilities remain registry-owned relational state.

There is exactly one authoritative `CurrentActivity` value per Person. It is not an array, not a PersonActivity relationship table, and not history.

Default is `Unrecorded()` (`Kind == None`). A new Person does not require an Activity Type.

Person Capability state stays on the registry. `CurrentWork` stays on `FPersonRecord` and is independent.

## None semantics

`CurrentActivity = None` means:

> No authoritative current activity is presently recorded for this Person.

It does not necessarily mean physically inactive, idle, unemployed, unconscious, resting, or doing literally nothing.

No recorded Current Activity is not the same as a physically inactive Person. A persistent distant Person may exist without moment-to-moment Activity being instantiated.

## Registry operations

| Operation | Role |
|---|---|
| `CreateActivityType` | Definition identity; rejects `None` and duplicate activity-type keys |
| `ContainsActivityType` / `FindActivityType` / `FindActivityTypeIdByKey` / `GetActivityTypeCount` | Snapshot reads |
| `SetPersonCurrentActivity` | Exclusive replace of `CurrentActivity` with `OfType` |
| `ClearPersonCurrentActivity` | Sets `CurrentActivity` to `Unrecorded()` |
| `GetPersonCurrentActivity` | Copied current-activity snapshot, or unset if the person does not resolve |

`CreateActivityType` rejects `NAME_None` and duplicate keys before allocation. The first successful definition receives value `1`. Failed creation consumes no runtime handle and stores no record.

`FindActivityType` returns a copied optional snapshot. Mutating that copy does not alter registry authority. There is no reverse authored-key index.

`SetPersonCurrentActivity` validates the person, then the activity type, then writes. A second known type replaces the previous one. There is no activity list and no hidden previous-activity field. Setting the same type again is `Success` and is a replace, not a uniqueness error.

`ClearPersonCurrentActivity` returns `Success` for a known person whether activity was recorded or already `None`. Clearing already-`None` is a valid deterministic no-op write of `Unrecorded()`. Unknown people are rejected. This differs from `RemovePersonWork`, which returns `NotAssigned` because exclusive labor scarcity treats an empty slot as a distinct outcome.

`GetPersonCurrentActivity` returns a copied `FCurrentActivity`. A populated optional with `Kind == None` means the person exists and no activity is instantiated. An unset optional means the person identifier does not resolve. The query does not expose mutable `FPersonRecord` authority.

Private `SetPersonActivity` is the single write path.

## Result enum

`EPersonActivityResult`: `Success`, `UnknownPerson`, `UnknownActivityType`.

## Independence from Current Work

Current Work describes work attachment or assignment. Current Activity describes broad behavior. Neither automatically determines the other.

0.1J does not implement:

```text
if CurrentWork != None: CurrentActivity = Working
if CurrentActivity == Working: require CurrentWork
```

These combinations remain representable:

```text
CurrentWork = Workshop, CurrentActivity = Sleeping
CurrentWork = None,     CurrentActivity = Working
CurrentWork = Workshop, CurrentActivity = Traveling
CurrentWork = None,     CurrentActivity = Caring
```

Setting or clearing Current Activity does not mutate CurrentWork. Assigning or removing work does not set Current Activity.

## Independence from presence, capability, and practice

Activity = Traveling does not identify where the Person is. Activity = Working does not identify where the Person is. Presence is not inferred from Activity.

Activity does not imply a Capability, create a Person Capability relationship, or generate Practice. `SetPersonCurrentActivity` and `ClearPersonCurrentActivity` do not call `AddPersonCapability` or `AddPersonCapabilityPractice`. 0.1H and 0.1I behavior is unchanged.

Examples:

```text
CurrentActivity = Working   does not imply Carpentry, Farming, Smithing, or any capability
CurrentActivity = Learning  does not generate Practice
CurrentActivity = Teaching  does not modify another Person
```

## Occupation, tasks, history, and UI

Occupation remains unimplemented. William may later be a Carpenter while Traveling; Joe may later be a Farmer while Working. Occupation is not inferred from Activity. Future hover or UI may show Occupation and current behavior separately; neither UI nor Occupation is implemented here.

Tasks were unimplemented at 0.1J. The intended layering is:

```text
Activity Type     broad behavior     (Working)
Task              specific objective (Shape roof beam)
Capability        broad ability      (Carpentry)
```

0.1J implements only the first layer. Prototypes 0.1K and 0.1L added the second layer's
identity and nothing more (`TASKS.md`): authored Task Types, then Task Instances holding only
their own identifier and their type.

Task Instances now exist, but `FCurrentActivity` does not reference them, having an activity
does not imply any task, and creating a task does not create, set, clear, or otherwise mutate
anyone's CurrentActivity. Creating a `Shape Beam` task does not make anyone `Working`. The two
remain independently authoritative. There is still no CurrentTask, participation, TaskTarget,
lifecycle, duration, progress, priority, issuer, location, or requirements.

There is no ActivityStartedAt, ActivityEndedAt, Duration, ElapsedTime, PreviousActivity, ActivityHistory, ActivitySchedule, NextActivity, PlannedActivity, ActivityQueue, ActivityPriority, or ActivityReason. 0.1J has no clock-driven transition system and no automatic transitions.

## Invariants

Activity types: slot ID matches; authored key not `None`; authored keys unique within activity types.

Person current activity:

- `None` is valid without resolving an Activity Type; `ActivityTypeId` must be invalid
- `ActivityType` requires `ActivityTypeId` to resolve to an authoritative Activity Type
- any other kind is failure

Not every Person is required to have a non-None Activity. These checks are part of `ValidateInvariants` and do not weaken earlier simulation invariants.

Test-only corruption access remains guarded by `WITH_DEV_AUTOMATION_TESTS`. There is no production mutation API solely for testing.

## Failure atomicity

Rejected operations leave authoritative state unchanged. Invalid or duplicate Activity Type creation consumes no ID. Unknown-person set or clear, and unknown Activity Type set, change neither CurrentActivity nor CurrentWork, capabilities, Practice, or unrelated Person state.

## What this slice does not do

Not implemented, and not scaffolded:

- Task identity, CurrentTask, assignment, progress, targets, requirements, or participants; Task Type identity arrived separately in 0.1K and Task Instance identity in 0.1L (`TASKS.md`), and Current Activity references neither
- Participation, production, or practice generation
- Automatic capability acquisition or automatic Activity selection
- AI behavior, schedules, daily routines, timers, duration, Activity history, or Activity events
- Movement, Presence, location, pathfinding, or Actor state
- Occupation, profession, job titles, apprenticeship, teaching effects, or learning effects
- Knowledge, education, credentials, task performance, capability performance modifiers
- Specialization, familiarity, health, tool, material, or environment effects
- Activity reverse indexes, Person-by-Activity queries, or Activity ranking
- UI, hover panels, micro-skills, micro-activities, or an authored permanent Activity catalog
- Continuous per-Person ticking or simulation LOD implementation

## Tests

Headless tests in `PersonActivityTests.cpp`:

| Test | Proves |
|---|---|
| `ActivityType.Creation` | Authored key, display name, `None`/duplicate rejection, no ID consumed, `FindActivityTypeIdByKey`, creation-order handles |
| `ActivityType.NamespaceIndependence` | Compile-time ID-family distinctness; exact key `Working` in Good, Work, Skill, and Activity Type namespaces |
| `ActivityType.SnapshotRegression` | Mutating a returned Activity Type copy does not alter registry authority |
| `Activity.Current` | Default `None`; set; replacement of Sleeping by Working; clear; clearing already-`None` is `Success` |
| `Activity.Rejection` | Unknown person set/clear and unknown Activity Type set; existing activity unchanged |
| `Activity.Independence` | CurrentWork and capability/practice unchanged by activity operations; `CurrentWork != None` with Sleeping; `CurrentWork == None` with Working |
| `Activity.InvariantDetection` | Activity-type slot/ID mismatch, `NAME_None` key, duplicate key, CurrentActivity naming an unresolvable type |

`FSimulationRegistryTestAccess` exposes `ActivityTypeRecords` and `PersonRecords` for corruption proofs only.
