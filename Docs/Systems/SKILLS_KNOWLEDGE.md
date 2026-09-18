# Skill Type Identity Foundation

## Document status

- **Role:** Implementation record for Prototype 0.1F
- **Authority:** Subordinate to `DESIGN_CONSTITUTION.md`, `HIGH_LEVEL_ARCHITECTURE.md`, `DATA_MODEL_OVERVIEW.md`, and `PROTOTYPE_0_1_SCOPE.md`
- **Extends:** The authored-definition identity pattern established by Good Type and Work Type
- **Extended by:** `PERSON_CAPABILITY.md` (Prototype 0.1G–0.1I), which records which people possess which Skill Types, stores accumulated practice for those relationships, and derives a read-only general capability from that practice without adding skill fields to `FPersonRecord`
- **Scope:** Skill Type identity only
- **Describes:** Only what exists in the repository today

## Purpose

A Skill Type answers one question:

> What kind of capability is this?

Stone Masonry, Carpentry, Blacksmithing, Navigation, Accounting, and Literacy are conceptual
examples only. Production code contains no skill catalog. Authored content registers skill
types at runtime.

A Skill Type is a definition, not a person's skill state. Prototype 0.1F creates no link from
a person to a skill and records no value, proficiency, experience, progress, or ability.
Prototype 0.1G (`PERSON_CAPABILITY.md`) records the sparse Person → Skill Type relationship
on the registry. Prototype 0.1H stores accumulated practice on that relationship. Prototype
0.1I derives general capability from that practice without storing proficiency or a
capability collection on the person.

## Identity model

`FSkillTypeRecord` contains exactly:

```text
AuthoredKey  durable authored definition identity
Id           FSkillTypeId runtime handle within one registry
Name         display data; not identity
```

`AuthoredKey` is caller-authored, must not be `NAME_None`, and is unique among Skill Types.
Duplicate display names are allowed because `Name` is not identity and may change without
changing the definition.

`FSkillTypeId` is the ninth mutually distinct simulation ID family. It follows the same
default-invalid, value-zero-invalid, dense allocation, boundary-safe lookup conventions as
the other simulation identifiers. It is not interchangeable with `FGoodTypeId`,
`FWorkTypeId`, or any other identifier family.

Skill Type authored keys occupy their own registry namespace. The exact key `Wheat` may
simultaneously identify a Good Type, Work Type, and Skill Type; each family resolves only its
own record.

## Registry authority

`FSimulationRegistry` exclusively owns Skill Type storage and provides:

- `CreateSkillType`
- `ContainsSkillType`
- `FindSkillType`
- `FindSkillTypeIdByKey`
- `GetSkillTypeCount`

Creation rejects `NAME_None` and duplicate Skill Type keys before appending storage, so a
failed registration consumes no runtime ID. The first successful definition receives value
`1`.

`FindSkillType` returns a copied optional snapshot. Its public fields may be edited by the
caller without changing authoritative registry state. The authoritative array remains
private. Test-only corruption access remains guarded by `WITH_DEV_AUTOMATION_TESTS` and
exists only to prove invariant detection.

## Invariants

For every stored Skill Type:

- the record's ID must match its dense storage slot;
- `AuthoredKey` must not be `NAME_None`;
- no earlier Skill Type may carry the same `AuthoredKey`.

These checks are part of `ValidateInvariants` and do not weaken or bypass any earlier
simulation invariant.

## Concept boundaries

- **Skill Type:** what kind of capability this is.
- **Person Capability:** the recorded relationship that a particular person possesses this Skill Type (`PERSON_CAPABILITY.md`).
- **Accumulated Practice:** persistent developmental history for that acquired capability; not proficiency.
- **General Capability:** derived broad learned ability interpreted from AccumulatedPractice; not stored.
- **Knowledge:** what a person understands; not implemented here.
- **Occupation:** a person's economic or social work identity; not implemented here.
- **CurrentWork:** what a person is presently assigned to; unchanged from 0.1E.
- **Presence:** where a person physically is; not implemented here.
- **Activity:** what a person is actually doing; not implemented here.

Skill Types do not require Work Types, and Work Types do not require Skill Types. A Skill
Type has no production effect and does not imply knowledge, occupation, work assignment,
presence, or activity.

## Explicitly not implemented

Prototype 0.1F does not implement or scaffold:

- person skill collections on `FPersonRecord`, skill values, proficiency, experience, progression, learning, or decay;
- knowledge or Knowledge Types;
- occupation, profession, jobs, or role catalogs;
- apprenticeship, education, training, teachers, books, or institutions;
- production effects, productivity bonuses, or skill requirements;
- skill hierarchy, categories, parents, prerequisites, related skills, or skill trees;
- work scheduling, employment, wages, contracts, or markets;
- movement, physical presence, activities, AI, ticks, Actors, UObjects, Components, or world scans.

`FPersonRecord`, `FCurrentWork`, `ECurrentWorkKind`, Work Types, Physical Sites, goods, and
inventories are unchanged by this slice.
