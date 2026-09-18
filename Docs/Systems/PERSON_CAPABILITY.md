# Person Capability Relationship Foundation

## Document status

- **Role:** Implementation record for Prototype 0.1G person-capability relationships, Prototype 0.1H accumulated practice, and Prototype 0.1I derived general capability
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** `SIMULATION_FOUNDATION.md` (0.1A) and `SKILLS_KNOWLEDGE.md` (0.1F), neither of which is superseded
- **Scope:** Sparse Person → Skill Type acquired-capability relationships, monotonic accumulated practice, and a read-only integer interpretation of that practice
- **Describes:** Only what exists in the repository today

## Purpose

Prototype 0.1F answered:

> What kinds of capabilities exist?

Prototype 0.1G answers:

> Which people possess which Skill Types as acquired capabilities?

Prototype 0.1H answers only:

> How much accumulated meaningful practice is recorded for an existing Person Capability?

Prototype 0.1I answers only:

> Given this person's existing capability relationship and its accumulated practice, what is the derived general learned capability in that broad Skill Type?

It does not generate practice, create relationships, or implement task performance.

The intended conceptual progression is:

```text
SkillType              kind of broad capability
PersonCapability       this Person has acquired this Skill Type
AccumulatedPractice    persistent developmental history for that acquired capability
GeneralCapability      derived broad learned ability (not stored)
future contextual systems
TaskPerformance        not implemented
```

## Implemented

- Broad Skill Type identity remains a separate definition family (`SKILLS_KNOWLEDGE.md`)
- Sparse registry-owned `FPersonCapabilityRecord` relationships
- Uniqueness of the pair `(PersonId, SkillTypeId)`
- `uint32 AccumulatedPractice` on the relationship, starting at `0`
- Controlled monotonic practice increase through `AddPersonCapabilityPractice`
- Overflow protection that rejects wrapping or clamping
- Query distinction between practice `0` on an existing relationship and a missing relationship
- Deterministic derived General Capability from AccumulatedPractice, not stored on any record
- Complete-registry invariant checks for unresolvable people, unresolvable skill types, and duplicate pairs

A person may possess several skill types. A skill type may be possessed by several people. The same pair may appear only once.

No relationship means only that no meaningful acquired capability relationship is currently recorded for that person and skill type. Absence is not biological or absolute inability. Universal human behaviours such as walking, eating, sleeping, and ordinary speech are not automatically Skill Types.

Joe may have Farming and Carpentry. He does not implicitly carry zero-valued records for Masonry, Smithing, Medicine, or any other Skill Type.

## Skill granularity

Skill Types are intentionally broad transferable capabilities. Tasks, products, techniques, materials, and tools do not automatically become separate Skill Types.

Keep kinds such as Carpentry, Farming, Masonry, and Smithing. Do not split those into RoofCarpentry, WheatFarming, PlowingSkill, or similar task-level or product-level micro-skills. A new Skill Type should represent a genuinely distinct broad transferable human capability.

## Module layout

```text
Source/RealmsUnwritten/Private/Simulation/PersonCapabilityRecord.h
Source/RealmsUnwritten/Private/Tests/PersonCapabilityTests.cpp
Source/RealmsUnwritten/Private/Tests/PersonGeneralCapabilityTests.cpp
```

Files extended: `SimulationRegistry.{h,cpp}`, `Tests/SimulationRegistryTestAccess.h`. **`RealmsUnwritten.Build.cs` is unchanged.** `FPersonRecord` is unchanged: it still holds no capability collection, practice, proficiency, or occupation.

## Data model

```text
FPersonCapabilityRecord
  PersonId              FPersonId        must resolve
  SkillTypeId           FSkillTypeId     must resolve
  AccumulatedPractice   uint32           starts at 0; increases only
```

The pair is the identity of the relationship. There is no `FPersonCapabilityId`, authored key, display name, status, timestamp, proficiency, level, XP, or metadata.

`uint32` is used because the project already treats unsigned 32-bit values as compact non-negative authoritative counters, and because it cannot represent negative practice. Floating point is not used.

## Meaning of AccumulatedPractice

Accumulated Practice is persistent normalized meaningful developmental practice recorded for this acquired broad capability.

It is not automatically:

- clock hours
- days or years employed
- number of tasks
- XP
- proficiency
- percentage mastery
- occupation duration

`1` Practice is conceptually calibrated as approximately one hour-equivalent of meaningful developmental participation. That is not necessarily one literal clock hour, and 0.1I does not generate practice. Future activity and learning systems will determine how meaningful actions produce practice.

Zero means no accumulated practice has yet been quantified by the practice-state system for this relationship. It does not mean the person has never encountered the capability, and it does not mean biological inability. The capability relationship itself still means the broad capability has been meaningfully acquired.

Practice is historical accumulated state. It is conceptually monotonic. Disuse, aging, illness, injury, or forgetting must not subtract from it. Future systems may separately model fluency, readiness, health, current physical ability, forgetting, and task performance.

## Authority and storage

`FSimulationRegistry` exclusively owns the relationship array. The person record is not a skill bag. Callers cannot append a capability or rewrite practice by editing a person snapshot.

Storage remains a private sparse `TArray<FPersonCapabilityRecord>`. People are not initialized with every skill type. There is no reverse Skill Type → Persons index, geographic index, labor-market index, cache, or ECS conversion.

Test-only corruption access remains guarded by `WITH_DEV_AUTOMATION_TESTS` and exists only to prove invariant detection and overflow behaviour. There is no public `SetPractice` production API.

## Registry API

- `AddPersonCapability(PersonId, SkillTypeId)` → `EPersonCapabilityResult`
- `AddPersonCapabilityPractice(PersonId, SkillTypeId, Amount)` → `EPersonCapabilityPracticeResult`
- `PersonHasCapability(PersonId, SkillTypeId)`
- `GetPersonCapabilityPractice(PersonId, SkillTypeId)` → `TOptional<uint32>`
- `GetPersonGeneralCapability(PersonId, SkillTypeId)` → `TOptional<uint32>`
- `GetPersonCapabilityCount()`

`AddPersonCapability` still does not create missing people or skill types, does not infer skill types from work types, and does not infer capability from CurrentWork or occupation. A newly recorded relationship starts with `AccumulatedPractice = 0`.

`AddPersonCapabilityPractice` is an authority primitive, not the learning system. It does not decide why practice was earned. It does not automatically create a missing capability relationship.

The intended future acquisition path is that a higher-level Activity/Participation system will create the relationship on first meaningful participation, then increment practice. That Activity system is not implemented. Query operations do not create capabilities.

There is no `RemovePersonCapability` and no decrement of practice.

There is no public snapshot array of a person's capabilities, and no `GetPeopleWithSkill`.

## Practice mutation

`AddPersonCapabilityPractice` validates, then mutates, in this order:

1. the person identifier must resolve
2. the skill type identifier must resolve
3. the `(PersonId, SkillTypeId)` relationship must already exist
4. `Amount` must be greater than zero
5. `Amount` must not overflow `uint32`

A failed request leaves practice and relationship storage unchanged. Practice only increases.

Zero increment is rejected as `InvalidAmount`, matching the project's rejection of meaningless zero quantity mutations. It is not a successful no-op.

An increment that would wrap is rejected as `Overflow`. The original value is neither wrapped, clamped, reset, nor converted.

## Practice query

`GetPersonCapabilityPractice` returns a copied optional:

- existing relationship with practice `0` → populated optional containing `0`
- missing relationship, including unknown identifiers → unset optional

`0` therefore does not stand for both states.

## General Capability

General Capability is deterministic derived state. AccumulatedPractice remains the authoritative persistent history. The derived value is not stored on `FPersonCapabilityRecord` or `FPersonRecord`, is not cached, and is recomputed on each query.

Fixed representation: integer 0 through a mathematical upper bound of 10,000. 10,000 is not "100% mastery", Level 100, or a normal maximum skill state. Finite practice, including `MAX_uint32`, remains below 10,000.

Interpretation of practice `P`:

```text
GeneralCapability(P)
  = floor(sqrt(floor((100,000,000 * P) / (P + 4,000))))
```

Arithmetic uses `uint64` intermediates. `100,000,000 * MAX_uint32` fits in `uint64`, so the locked floor expression is evaluated exactly. Rounding is truncation: the result is never rounded upward. Adjacent practice values may share one derived integer; practice itself remains exact.

Practice 0 derives 0. An existing zero-practice relationship returns a populated optional containing 0. A missing relationship returns unset.

General Capability is broad learned ability derived from accumulated meaningful developmental practice. It is not XP, a character level, a proficiency label, a mastery percentage, a task-success probability, an occupation, a credential, apprenticeship status, or task performance.

Knowledge, task familiarity, health, tools, materials, assistance, and environment belong to future task-performance systems, not to this interpretation.

## Invariants

For every stored person-capability record:

- `PersonId` must resolve to an existing person
- `SkillTypeId` must resolve to an existing skill type
- no earlier record may carry the same `(PersonId, SkillTypeId)` pair

`uint32` cannot represent negative practice, so no redundant runtime invariant claims that practice is non-negative. Overflow is prevented at the mutation boundary.

These checks are part of `ValidateInvariants` and do not weaken earlier simulation invariants.

## Architectural distinctions

- **Skill Type:** what kind of learned/differentiated capability exists (`SKILLS_KNOWLEDGE.md`).
- **Person Capability:** the recorded relationship that a particular person possesses that Skill Type.
- **Accumulated Practice:** persistent developmental history associated with that acquired capability.
- **General Capability:** derived broad learned ability interpreted from AccumulatedPractice. Not stored.
- **Task Performance:** future contextual effectiveness. Not implemented.
- **Knowledge:** what a person understands. Not implemented.
- **Occupation:** how a person is economically or socially identified. Not implemented.
- **CurrentWork:** what a person is currently assigned to (`WORK_LABOR.md`). Unchanged.
- **Presence:** where a person physically is. Not implemented.
- **Activity:** what a person is actually doing. Not implemented.
- **Credential / institutional recognition:** how institutions recognize capability. Not implemented.

Adding a capability or increasing practice mutates only the relationship collection. It does not change CurrentWork, inventory, residence, physical sites, goods, property, household, occupation, or movement.

## Persistent, not ticking

Person Capability records are persistent state, not continuously active simulation objects. They have no Tick, timers, per-day processing, automatic progression, automatic decay, or automatic XP. Future capability changes should be event-driven; this slice does not implement those events.

Stored learned capability is not task performance. No formula such as `Capability × Tools × Health × Materials` is evaluated here. The locked Practice → General Capability interpretation is the only derivation implemented, and it is not a proficiency level or success probability.

## Explicitly not implemented

Prototype 0.1G–0.1I do not implement or scaffold:

- proficiency labels, skill levels, mastery ranks, talent, potential, or percentage conversion
- XP, experience as a distinct field, fluency, familiarity, quality, speed, or reliability
- learning-rate formulas or practice-acquisition rules
- task difficulty, task performance, tools effects, health effects, or material effects
- specialization, contextual practice, or micro-skills
- skill prerequisites or skill trees
- forgetting, decay, or any decrement of AccumulatedPractice
- occupation, profession, career, or inferred occupation
- apprenticeship, guilds, credentials, certification, teachers, education, schools, universities, or monasteries as training systems
- Knowledge Types, books, literacy, technology transfer, innovation, or knowledge graphs
- automatic capability acquisition from age, work, household, observation, employment, or tasks
- Activity/Participation, even though first meaningful participation is the intended future acquisition trigger
- `GetPeopleWithSkill` or any reverse Skill Type → Persons index
- capability removal, history, archival state, or status enums
- a public `SetPractice` production API
- persistent or cached General Capability
- labor recruitment or regional skilled-worker search
- simulation ticking, LOD, or archival compression
- movement, presence, activity, or CurrentWork changes

`FPersonRecord`, Skill Types, Work Types, CurrentWork, Physical Sites, goods, inventories, households, settlements, and properties keep the semantics established in 0.1A–0.1F. 0.1G relationship addition remains the acquisition primitive.
