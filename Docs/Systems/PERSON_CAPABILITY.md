# Person Capability Relationship Foundation

## Document status

- **Role:** Implementation record for the Prototype 0.1G person-capability relationship foundation
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (0.1A) and `SKILLS_KNOWLEDGE.md` (0.1F), neither of which is superseded
- **Scope:** Sparse Person → Skill Type acquired-capability relationships only
- **Describes:** Only what exists in the repository today

## Purpose

Prototype 0.1F answered:

> What kinds of capabilities exist?

Prototype 0.1G answers one further question:

> Which people possess which Skill Types as acquired capabilities?

It does not answer how good a person is at a capability. That belongs to a later Capability State slice.

## Implemented

- A registry-owned `FPersonCapabilityRecord` containing only `PersonId` and `SkillTypeId`
- Sparse storage: a relationship exists only when it has been recorded
- Uniqueness of the pair `(PersonId, SkillTypeId)`
- `AddPersonCapability`, which validates the person, then the skill type, then uniqueness, and mutates only on success
- `PersonHasCapability` and `GetPersonCapabilityCount`
- Complete-registry invariant checks for unresolvable people, unresolvable skill types, and duplicate pairs

A person may possess several skill types. A skill type may be possessed by several people. The same pair may appear only once.

No relationship means only that no meaningful acquired capability relationship is currently recorded for that person and skill type. Absence is not biological or absolute inability. Universal human behaviours such as walking, eating, sleeping, and ordinary speech are not automatically Skill Types.

## Module layout

```text
Source/RealmsUnwritten/Private/Simulation/PersonCapabilityRecord.h
Source/RealmsUnwritten/Private/Tests/PersonCapabilityTests.cpp
```

Files extended: `SimulationRegistry.{h,cpp}`, `Tests/SimulationRegistryTestAccess.h`. **`RealmsUnwritten.Build.cs` is unchanged.** `FPersonRecord` is unchanged: it still holds no capability collection.

## Data model

```text
FPersonCapabilityRecord
  PersonId      FPersonId        must resolve
  SkillTypeId   FSkillTypeId     must resolve
```

The pair is the identity of the relationship. There is no `FPersonCapabilityId`, authored key, display name, status, timestamp, or metadata.

## Authority and storage

`FSimulationRegistry` exclusively owns the relationship array. The person record is not a skill bag. Callers cannot append a capability by editing a person snapshot.

Storage is a private `TArray<FPersonCapabilityRecord>`. It is sparse: people are not initialized with every skill type, and zero/default entries are not stored. There is no reverse Skill Type → Persons index, geographic index, labor-market index, cache, or ECS conversion.

Test-only corruption access remains guarded by `WITH_DEV_AUTOMATION_TESTS` and exists only to prove invariant detection.

## Registry API

- `AddPersonCapability(PersonId, SkillTypeId)` → `EPersonCapabilityResult`
- `PersonHasCapability(PersonId, SkillTypeId)`
- `GetPersonCapabilityCount()`

`AddPersonCapability` does not create missing people or skill types, does not infer skill types from work types, and does not infer capability from CurrentWork or occupation.

There is no `RemovePersonCapability`. Historical loss, forgetting, decline, and archival state are not designed.

`PersonHasCapability` reports whether a recorded pair exists. It does not return a mutable record. There is no public snapshot array of a person's capabilities in this slice, and no `GetPeopleWithSkill`.

## Invariants

For every stored person-capability record:

- `PersonId` must resolve to an existing person
- `SkillTypeId` must resolve to an existing skill type
- no earlier record may carry the same `(PersonId, SkillTypeId)` pair

These checks are part of `ValidateInvariants` and do not weaken earlier simulation invariants.

## Architectural distinctions

- **Skill Type:** what kind of learned/differentiated capability exists (`SKILLS_KNOWLEDGE.md`).
- **Person Capability:** the recorded relationship that a particular person possesses that Skill Type.
- **Capability State:** future degree, proficiency, or experience. Not implemented.
- **Knowledge:** what a person understands. Not implemented.
- **Occupation:** how a person is economically or socially identified. Not implemented.
- **CurrentWork:** what a person is currently assigned to (`WORK_LABOR.md`). Unchanged.
- **Presence:** where a person physically is. Not implemented.
- **Activity:** what a person is actually doing. Not implemented.
- **Credential / institutional recognition:** how institutions recognize capability. Not implemented.

Adding a capability mutates only the relationship collection. It does not change CurrentWork, inventory, residence, physical sites, goods, property, household, occupation, or movement.

## Persistent, not ticking

Person Capability records are persistent state, not continuously active simulation objects. They have no Tick, timers, per-day processing, automatic progression, automatic decay, or automatic XP. Future capability changes should be event-driven; this slice does not implement those events.

Stored learned capability is not task performance. No formula such as `Capability × Tools × Health × Materials` is evaluated here.

## Explicitly not implemented

Prototype 0.1G does not implement or scaffold:

- proficiency, level, XP, experience, practice, fluency, quality, speed, reliability, specialization, familiarity, learning rate, or decay
- novice/apprentice/journeyman/master ranks or any magnitude
- capability removal, history, archival state, or status enums
- `GetPeopleWithSkill` or any reverse Skill Type → Persons index
- occupation, profession, career, or inferred occupation
- apprenticeship, guilds, credentials, certification, teachers, education, schools, universities, or monasteries as training systems
- Knowledge Types, books, literacy, technology transfer, innovation, or knowledge graphs
- task performance, tools effects, health effects, material effects, or production
- labor recruitment or regional skilled-worker search
- simulation ticking, LOD, or archival compression
- movement, presence, activity, or CurrentWork changes

`FPersonRecord`, Skill Types, Work Types, CurrentWork, Physical Sites, goods, inventories, households, settlements, and properties keep the semantics established in 0.1A–0.1F.
