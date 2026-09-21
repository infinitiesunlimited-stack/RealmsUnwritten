# Task Type Identity Foundation

## Document status

- **Role:** Implementation record for Prototype 0.1K
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** The authored-definition identity pattern established by Good Type, Work Type, Skill Type, and Activity Type
- **Scope:** Task Type identity only
- **Describes:** Only what exists in the repository today

## Purpose

A Task Type answers one question:

> What specific reusable kind of objective or action is this?

Shape Beam, Cut Joint, Repair Cart, Harvest Wheat, Milk Cow, Fetch Water, Cook Meal, Feed Child, Carry Message, Attend Mass, Treat Wound, Guard Gate, Dig Grave, Buy Grain, and Sell Wool are conceptual examples only. Production code contains no task catalog. Authored content registers task types at runtime.

Task Types are not restricted to economic work. Domestic, religious, medical, social, and military objectives are all task types.

A Task Type is a definition, not an attempt. Prototype 0.1K creates no task instance, links no person to a task, and records no target, progress, or outcome.

## Task Type granularity

Task Types may be considerably narrower than Skill Types.

```text
Task Type:   Shape Beam
Skill Type:  Carpentry
```

That narrowness is correct for tasks and wrong for skills. A Beam Shaping skill must not be created to mirror a Shape Beam task. Skill Types remain broad transferable capabilities (`SKILLS_KNOWLEDGE.md`); Task Types describe specific objectives.

## Identity model

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

## Registry authority

`FSimulationRegistry` exclusively owns Task Type storage in a private dense `TArray<FTaskTypeRecord>` and provides:

- `CreateTaskType`
- `ContainsTaskType`
- `FindTaskType`
- `FindTaskTypeIdByKey`
- `GetTaskTypeCount`

Creation rejects `NAME_None` and duplicate Task Type keys before appending storage, so a failed registration consumes no runtime ID. The first successful definition receives value `1`.

`FindTaskType` returns a copied optional snapshot. Its public fields may be edited by the caller without changing authoritative registry state, and a held snapshot stays valid when storage grows. The authoritative array remains private, and there is no reverse authored-key index. Test-only corruption access remains guarded by `WITH_DEV_AUTOMATION_TESTS` and exists only to prove invariant detection.

Registering a Task Type mutates nothing else. It does not change Person, Work, Activity, Capability, Practice, Goods, Inventory, Property, Physical Site, Residence, or Household state.

## Invariants

For every stored Task Type:

- the record's ID must match its dense storage slot;
- `AuthoredKey` must not be `NAME_None`;
- no earlier Task Type may carry the same `AuthoredKey`.

These checks are part of `ValidateInvariants`, produce a non-empty failure description, do not repair corrupt state, and do not weaken or bypass any earlier simulation invariant.

## Concept boundaries

- **Task Type:** what specific kind of objective exists. Implemented here.
- **Task Instance:** one actual occurrence of that objective. Not implemented.
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

0.1K connects none of these. There is no mapping between Task Type and Work Type, Activity Type, or Skill Type.

## Intended future sequence

Documented, not implemented:

```text
0.1K  Task Type Identity          what kind of objective exists
0.1L  Task Instance Identity      one actual occurrence of that objective
0.1M  Person-Task Participation   who is recognized as contributing
0.1N  Person Current Task         the single task presently receiving attention
```

Conceptually:

```text
Task Type:      Shape Beam
Task Instance:  Shape roof beam #482
Participation:  William and Joe are recognized contributors to that task
CurrentTask:    the singular task presently receiving a person's attention
```

A future Task Instance must be capable of existing independently of any Person. "Repair damaged bridge" may exist before anyone accepts or begins it. Nothing in 0.1K assumes a task belongs to a person.

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

Those may eventually all be Task Instances connected through relationships. Prototype 0.1K implements no parent/child relationship, hierarchy, or decomposition of any kind.

Domain-specific records such as production batches, transfer orders, construction state, and crop cycles may still exist, because each owns real domain rules. They must not become competing generic objective hierarchies parallel to Task.

## Explicitly not implemented

Prototype 0.1K does not implement or scaffold:

- `FTaskId`, `FTaskRecord`, task instances, or `CurrentTask` on `FPersonRecord`;
- person-task participation, assignment, lifecycle, status, availability, queues, priorities, reservations, or scheduling;
- parent/child tasks, hierarchy, decomposition, or generic task targets;
- issuer, owner, beneficiary, responsible system, participant roles, or leader/helper roles;
- progress, duration, effort, deadlines, or start/end timestamps;
- materials, tools, recipes, outputs, or inventory mutation;
- required capabilities, exercised capabilities, Practice generation, or automatic capability acquisition;
- references to Person, Physical Site, Property, goods, or geography;
- AI plans, behavior trees, executors, per-frame ticks, timers, Actors, Components, or Blueprint exposure;
- hover or inspection UI;
- a hard-coded task hierarchy or permanent authored task catalog.

`FPersonRecord`, `FCurrentWork`, `FCurrentActivity`, Activity Types, Skill Types, Work Types, Good Types, Physical Sites, goods, and inventories are unchanged by this slice.

## Tests

Headless tests in `TaskTypeIdentityTests.cpp`:

| Test | Proves |
|---|---|
| `TaskType.Creation` | Authored key and display name preserved; `None` and duplicate-key rejection; no ID consumed by a rejected creation; duplicate display name accepted; lookup by typed ID and by authored key; boundary IDs fail safely; count behavior |
| `TaskType.NamespaceIndependence` | Compile-time non-interchangeability with other ID families; the exact key `Working` registered independently as Good, Work, Skill, Activity, and Task Type, each resolving to its own typed identity |
| `TaskType.SnapshotRegression` | A held snapshot stays valid across storage growth, the authoritative precondition holds before mutation, and editing the returned copy changes no authoritative state |
| `TaskType.InvariantDetection` | Isolated corruption of slot/ID mismatch, `NAME_None` authored key, and duplicate authored key are each detected with a non-empty description |

`FSimulationRegistryTestAccess` exposes `TaskTypeRecords` for corruption proofs only.
