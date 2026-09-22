# Task Identity Foundation

## Document status

- **Role:** Implementation record for Prototypes 0.1K and 0.1L
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** The authored-definition identity pattern established by Good Type, Work Type, Skill Type, and Activity Type
- **Scope:** Task Type identity (0.1K) and Task Instance identity (0.1L)
- **Describes:** Only what exists in the repository today

## The two questions

```text
Task Type      what reusable kind of objective is this?   Shape Beam
Task Instance  which particular occurrence exists?        the shaping of one roof beam
```

A Task Type is authored and reused. A Task Instance is one occurrence of that type, created because the simulation has accepted that objective as world state worth identifying across time, references, interruptions, and changes in how much local detail is simulated.

## Purpose

A Task Type answers one question:

> What specific reusable kind of objective or action is this?

Shape Beam, Cut Joint, Repair Cart, Harvest Wheat, Milk Cow, Fetch Water, Cook Meal, Feed Child, Carry Message, Attend Mass, Treat Wound, Guard Gate, Dig Grave, Buy Grain, and Sell Wool are conceptual examples only. Production code contains no task catalog. Authored content registers task types at runtime.

Task Types are not restricted to economic work. Domestic, religious, medical, social, and military objectives are all task types.

A Task Type is a definition, not an attempt. Registering one creates no occurrence of it; occurrences are Task Instances, added separately in 0.1L below. Neither slice links a person to a task or records a target, progress, or outcome.

## Task Type granularity

Task Types may be considerably narrower than Skill Types.

```text
Task Type:   Shape Beam
Skill Type:  Carpentry
```

That narrowness is correct for tasks and wrong for skills. A Beam Shaping skill must not be created to mirror a Shape Beam task. Skill Types remain broad transferable capabilities (`SKILLS_KNOWLEDGE.md`); Task Types describe specific objectives.

## Task Type identity model

`FTaskTypeRecord` contains exactly:

```text
AuthoredKey  durable authored definition identity
Id           FTaskTypeId runtime handle within one registry
Name         display data; not identity
```

No other field exists. There is no target, issuer, owner, site, duration, effort, progress, status, priority, parent, child, required capability, tool, material, or output.

`AuthoredKey` is caller-authored, must not be `NAME_None`, and is unique among Task Types. Duplicate display names are allowed because `Name` is not identity and may change without changing the definition.

`FTaskTypeId` is the eleventh mutually distinct simulation ID family. It follows the same default-invalid, value-zero-invalid, dense allocation, boundary-safe lookup conventions as the other simulation identifiers, and is covered by the single set-wide distinctness assertion in `SimulationIds.h`. It is not interchangeable with `FGoodTypeId`, `FWorkTypeId`, `FSkillTypeId`, `FActivityTypeId`, or any other identifier family.

Task Type authored keys occupy their own registry namespace. The exact key `Working` may simultaneously identify a Good Type, Work Type, Skill Type, Activity Type, and Task Type; each family resolves only its own record.

## Task Type registry authority

`FSimulationRegistry` exclusively owns Task Type storage in a private dense `TArray<FTaskTypeRecord>` and provides:

- `CreateTaskType`
- `ContainsTaskType`
- `FindTaskType`
- `FindTaskTypeIdByKey`
- `GetTaskTypeCount`

Creation rejects `NAME_None` and duplicate Task Type keys before appending storage, so a failed registration consumes no runtime ID. The first successful definition receives value `1`.

`FindTaskType` returns a copied optional snapshot. Its public fields may be edited by the caller without changing authoritative registry state, and a held snapshot stays valid when storage grows. The authoritative array remains private, and there is no reverse authored-key index. Test-only corruption access remains guarded by `WITH_DEV_AUTOMATION_TESTS` and exists only to prove invariant detection.

Registering a Task Type mutates nothing else. It does not change Person, Work, Activity, Capability, Practice, Goods, Inventory, Property, Physical Site, Residence, or Household state.

## Task Type invariants

For every stored Task Type:

- the record's ID must match its dense storage slot;
- `AuthoredKey` must not be `NAME_None`;
- no earlier Task Type may carry the same `AuthoredKey`.

These checks are part of `ValidateInvariants`, produce a non-empty failure description, do not repair corrupt state, and do not weaken or bypass any earlier simulation invariant.

## Task Instance identity model

Prototype 0.1L adds the occurrence. `FTaskRecord` contains exactly two authoritative fields:

```text
Id          FTaskId runtime handle within one registry
TaskTypeId  the task type this is an occurrence of
```

Nothing else exists on the record: no name, authored key, status, lifecycle, owner, issuer, participant, target, context, progress, priority, timestamp, duration, location, completion state, or domain data. The type's authored key and display name are deliberately *not* copied here, so a definition has exactly one owner and cannot drift between copies.

`FTaskId` is the twelfth mutually distinct simulation ID family, added to the same set-wide assertion in `SimulationIds.h` rather than to a second mechanism. `FTaskId` and `FTaskTypeId` are not interchangeable in either direction:

```text
FTaskTypeId  the Shape Beam definition
FTaskId      one particular Shape Beam occurrence
```

### What task existence means

Task existence means only that this objective occurrence exists, that it has its own runtime identity, and that it is an occurrence of the referenced Task Type.

It does **not** mean the task is available, assigned, current, active, located, executable, completed, successful, failed, known to a Person, associated with a Person, or being physically performed. Each of those is a separate concept, and none of them is implemented.

### Task Instance registry authority

`FSimulationRegistry` exclusively owns Task storage in a private dense `TArray<FTaskRecord>` and provides:

- `CreateTask`
- `ContainsTask`
- `FindTask`
- `GetTaskCount`

with a private `ResolveTask` and `ValidateTaskRecords`. There is deliberately no `FindTasksByType`, `GetAllTasks`, `RemoveTask`, `CompleteTask`, `CancelTask`, `AssignTask`, `SetCurrentTask`, `FindTasksForPerson`, or `FindAvailableTasks`, and no reverse index from Task Type to its occurrences.

### CreateTask semantics

`CreateTask` takes exactly one argument, `FTaskTypeId`, and proceeds in this order:

1. resolve the Task Type against authoritative Task Type storage;
2. if it does not resolve, return an invalid `FTaskId`, append nothing, and consume no Task ID;
3. otherwise append one record, assign the next dense `FTaskId` under the existing zero-invalid convention, store the supplied `FTaskTypeId`, and return the new handle.

Unresolvable input returns an invalid identifier rather than a result enum, matching `CreateProperty` and `CreatePhysicalSite`.

Occurrences are **not** deduplicated by Task Type. Three beams are three objectives:

```text
Task #1 -> Shape Beam
Task #2 -> Shape Beam
Task #3 -> Shape Beam
```

### Independence from Person

A Task Instance exists independently of Persons. `FTaskRecord` holds no Person reference, `CreateTask` requires no Person, and a registry containing zero Persons can create a Task Type, create a Task of that type, and query it successfully. This is intentional rather than incidental.

```text
Repair damaged bridge
```

must be able to exist before anyone accepts it, while nobody is working on it, while participants are away, and while its local execution is not being simulated in detail.

### Snapshot contract

`FindTask` returns `TOptional<FTaskRecord>` by value. A returned snapshot stays valid when `TaskRecords` reallocates, is freely mutable by the caller, never writes back to registry authority, carries the typed IDs as they were at query time, and is unset for unknown, default, and boundary Task IDs. The authoritative array is never exposed mutably outside `WITH_DEV_AUTOMATION_TESTS` corruption access.

### Task Instance invariants

Exactly two, checked for every stored Task as part of the `ValidateInvariants` chain:

1. the stored `FTaskId` matches its dense `TaskRecords` slot;
2. the stored `TaskTypeId` resolves to an authoritative `FTaskTypeRecord`.

No uniqueness constraint applies to `TaskTypeId`, because many occurrences of one objective are normal. No invariant exists for lifecycle, status, participants, targets, progress, availability, domain context, location, or CurrentTask, since none of those exist. Validation diagnoses corruption and never repairs it.

## Task granularity

A behavior deserves persistent Task identity when it is a meaningful objective that benefits from being referenced independently of whoever is executing it. Indicators:

- it survives interruption;
- it survives its local simulation being absent;
- multiple people or systems may coordinate around it;
- it produces consequential domain change;
- it can meaningfully be assigned, resumed, or explained;
- another authoritative record may need to reference it;
- it remains meaningful after animation and motor detail are discarded.

```text
Likely Tasks:      Shape Beam, Milk Cow, Fetch Water, Feed Child
Not Tasks:         Move left foot, Grip hammer, Swing hammer once, Turn body 15 degrees
```

This is a semantic distinction, not a numeric one. There is no duration threshold, and this list is not hard-coded anywhere.

## Domain authority boundary

A Task Instance identifies an objective. Domain records own authoritative physical, economic, legal, spatial, and transactional state. A Task must never own or duplicate that state.

| Domain record owns | Task only identifies |
|---|---|
| Construction state: structure, materials, physical progress | Shape Beam |
| Crop cycle: field, planted crop, maturity, yield, land condition | Harvest Wheat |
| Transfer order: source, destination, reservation, custody | Deliver Goods |
| Production batch: inputs, outputs, conservation | Operate production step |

None of those domain systems is implemented.

## Append-only storage is a prototype limitation

`TaskRecords` is append-only, consistent with the rest of the prototype registry. There is no `RemoveTask`, `DeleteTask`, `RetireTask`, `ArchiveTask`, tombstone, generation counter, ID reuse, garbage collection, or history compression.

**This is explicitly not a claim that every Task Instance will remain a live record forever in the final architecture.** High-volume ordinary behavior — Milk Cow, Fetch Water, Feed Child, Shape Beam — could otherwise accumulate an enormous number of Task Instances.

The distinction that will matter is between an *active objective* and *historical evidence*. Future architecture may retain the evidence that something happened in other forms — work history, practice evidence, biography, construction history, transactions, institutional records, aggregate history — without retaining every completed Task as a permanent live record. None of that is implemented now.

Retention and archival should be revisited after 0.1N, and before high-volume execution or scheduling work or long-duration simulation testing.

## LOD compatibility

`FTaskRecord` is plain registry-owned simulation state with no dependency on Actors, Components, animation, navigation, pathfinding, behavior trees, local executor objects, per-frame ticks, loaded map state, or UI. A task therefore survives its executor and its local detail not being simulated. That structural independence is all 0.1L claims: there are no LOD tiers, compression, executor restoration, coarse task simulation, scheduling, or activity synthesis.

## Not an AI plan

`CreateTask` is a low-level authority primitive. It records an objective only after some future simulation or domain system has already decided that objective should exist. An AI thought such as "I should fetch water" is not automatically authoritative Task state, and `FTaskRecord` must not become AI planner scratch space.

## Concept boundaries

- **Task Type:** what specific kind of objective exists. Implemented here.
- **Task Instance:** one actual occurrence of that objective. Implemented in 0.1L as identity only.
- **Activity:** what broad behavior a person is currently doing (`PERSON_ACTIVITY.md`). A task is not an activity.
- **CurrentWork:** what work attachment a person presently has (`WORK_LABOR.md`). A task is not a work commitment.
- **Capability:** what broad transferable ability a person acquired (`PERSON_CAPABILITY.md`). A task is not a capability, and defining one grants none.
- **Occupation:** how a person is economically or socially identified. Not implemented. A task is not an occupation.
- **Presence:** where a person physically is. Not implemented. A task names no place.
- **Knowledge:** what a person understands. Not implemented.

The established separation remains:

```text
Knowledge != Capability != Occupation != Current Work != Presence != Activity != Task
```

Neither 0.1K nor 0.1L connects these. There is no mapping between Task Type and Work Type, Activity Type, or Skill Type, and creating a Task changes no Person, Household, Settlement, Property, Residence, Goods, Inventory, Physical Site, Inventory Location, CurrentWork, Work Type, Activity Type, CurrentActivity, Skill Type, Person Capability, Accumulated Practice, or derived General Capability state. Creating `Shape Beam` does not set `CurrentActivity = Working`; creating `Feed Child` does not set `CurrentActivity = Caring` and does not alter anyone's work attachment. Those orchestrations may later be performed externally, and each of those systems remains independently authoritative.

Task creation also implies no location, Physical Site, Property, Settlement, coordinates, or Person presence, requires no Skill Type or capability, grants no capability, adds no Accumulated Practice, queries no General Capability, and validates no proficiency. A task needs only a valid Task Type.

## Sequence

```text
0.1K  Task Type Identity          what kind of objective exists            implemented
0.1L  Task Instance Identity      one actual occurrence of that objective  implemented
0.1M  Person-Task Participation   who is recognized as contributing        future
0.1N  Person Current Task         the single task presently receiving attention  future
```

0.1M and 0.1N are documented, not implemented. There is no `FPersonTaskRecord`, `TaskParticipants`, `Person.Tasks`, assignment, participant role, leader/helper distinction, or participation state, and Task existence implies no participation. There is no `Person.CurrentTask`, `SetPersonCurrentTask`, `ClearPersonCurrentTask`, or `GetPersonCurrentTask`.

Conceptually:

```text
Task Type:      Shape Beam
Task Instance:  Shape roof beam #482
Participation:  William and Joe are recognized contributors to that task
CurrentTask:    the singular task presently receiving a person's attention
```

A Task Instance is capable of existing independently of any Person. "Repair damaged bridge" may exist before anyone accepts or begins it. Nothing in 0.1K or 0.1L assumes a task belongs to a person.

## Future decomposition stays under Task

Objective decomposition is intended to remain under one Task concept rather than introducing generic Project, Process, Operation, or Job Activity identity families:

```text
Build House
    Prepare Foundation
    Raise Timber Frame
        Cut Posts
        Shape Beams
        Cut Joints
        Raise Frame
    Build Walls
    Construct Roof
```

Those may eventually all be Task Instances connected through relationships. Whether those relationships form a tree, a DAG, an ordered graph, or something else is intentionally unresolved. Prototypes 0.1K and 0.1L implement no `ParentTaskId`, `ChildTaskIds`, task edge, hierarchy, or decomposition of any kind.

Domain-specific records such as production batches, transfer orders, construction state, and crop cycles may still exist, because each owns real domain rules. They must not become competing generic objective hierarchies parallel to Task.

## Explicitly not implemented

Through 0.1L, the Task system does not implement or scaffold:

- lifecycle or outcome state of any kind: no `Created`, `Active`, `Paused`, `Blocked`, `Completed`, `Cancelled`, `Failed`, or `Abandoned`, no status enum, no `bCompleted`, and no completion API. Record existence is sufficient for 0.1L, and completion semantics are deliberately deferred;
- deletion, retirement, archival, tombstones, generations, ID reuse, or garbage collection;
- `CurrentTask` on `FPersonRecord`, person-task participation, assignment, availability, offers, queues, priorities, reservations, or scheduling;
- parent/child tasks, hierarchy, decomposition, or generic Project, Process, Operation, or JobActivity identities;
- targets of any kind: no `TaskTarget`, `ETaskTargetKind`, target union, generic entity reference, or Person, Property, Physical Site, item, or geographic target. Domain-owned context keyed by `FTaskId` is the safer provisional direction, and it is not scaffolded here;
- issuer, owner, beneficiary, responsible system, participant roles, worker arrays, or leader/helper roles;
- progress, duration, effort, deadlines, timestamps, or history records;
- construction, farming, journey, or military context; materials, tools, inputs, outputs, recipes, or inventory mutation;
- capability requirements, capability exercise mappings, capability acquisition, or Practice generation;
- CurrentActivity mutation, CurrentWork mutation, physical presence, or movement;
- AI planning, AI executors, behavior trees, per-frame ticks, timers, Actors, Components, or Blueprint exposure;
- hover or inspection UI;
- `FindTasksByType`, `GetAllTasks`, reverse indexes, or a permanent authored task catalog.

`FPersonRecord`, `FCurrentWork`, `FCurrentActivity`, Activity Types, Skill Types, Work Types, Good Types, Physical Sites, goods, and inventories are unchanged by these slices.

## Tests

Headless tests in `TaskTypeIdentityTests.cpp`:

| Test | Proves |
|---|---|
| `TaskType.Creation` | Authored key and display name preserved; `None` and duplicate-key rejection; no ID consumed by a rejected creation; duplicate display name accepted; lookup by typed ID and by authored key; boundary IDs fail safely; count behavior |
| `TaskType.NamespaceIndependence` | Compile-time non-interchangeability with other ID families; the exact key `Working` registered independently as Good, Work, Skill, Activity, and Task Type, each resolving to its own typed identity |
| `TaskType.SnapshotRegression` | A held snapshot stays valid across storage growth, the authoritative precondition holds before mutation, and editing the returned copy changes no authoritative state |
| `TaskType.InvariantDetection` | Isolated corruption of slot/ID mismatch, `NAME_None` authored key, and duplicate authored key are each detected with a non-empty description |

Headless tests in `TaskInstanceTests.cpp`:

| Test | Proves |
|---|---|
| `Task.Creation` | A Task is created from a resolvable Task Type in a registry holding zero Persons; the first handle is `1`; `Id` and `TaskTypeId` are stored correctly; several Tasks may share one Task Type and are not deduplicated; default, one-past-range, and extreme Task Type IDs are all rejected without storing a record or consuming a handle; the Task Type count and authoritative Task Type record are untouched; unknown, default, and boundary Task IDs fail `ContainsTask` and `FindTask` safely |
| `Task.SnapshotRegression` | A held snapshot keeps its `Id` and `TaskTypeId` after storage grows by 32 further Tasks; the authoritative record is proven intact before the snapshot is edited; mutating both snapshot fields then re-querying shows authoritative state unchanged |
| `Task.Independence` | With a Person, CurrentWork, CurrentActivity, Skill Type, capability, accumulated practice, and located goods established first, creating Tasks changes none of them. A regression boundary, not a dependency |
| `Task.InvariantDetection` | Isolated corruption of a Task slot/ID mismatch, a default `TaskTypeId`, and a nonzero but never-allocated `TaskTypeId` are each detected with a non-empty description, having first confirmed the registry was valid |

`FSimulationRegistryTestAccess` exposes `TaskTypeRecords` and `TaskRecords` for corruption proofs only. The public API cannot produce any of the corrupted states above.
