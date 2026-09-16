# Data Model Overview

## Purpose

This document defines the conceptual runtime model for the first prototype and the long-term relationships it must preserve. It intentionally avoids Unreal class choices, database formats, and final memory layouts.

The central rule is that authoritative records outlive and remain independent from their visual representations.

## Model categories

The model distinguishes:

- **Definitions:** Authored types such as `Wheat`, `Bread`, `WheatCrop`, `MillingProcess`, or `BakeryBuilding`. Definitions have stable definition IDs and versioned content.
- **Entities:** Persistent world identities such as a person, household, property, building, field, settlement, or identifiable goods lot.
- **Assignments and processes:** Time-bounded relationships such as a job commitment, tenancy, crop cycle, reservation, transfer, or production batch.
- **Derived projections:** Rebuildable totals and UI summaries such as settlement population, available wheat, or employed adults.

Derived projections are never competing sources of truth.

## Conceptual relationship map

```text
Settlement 1 ---- contains/references ---- * Property
Settlement 1 ---- registers/hosts -------- * Household
Settlement 1 ---- residents -------------- * Person

Household 1 ---- has members ------------- 1..* Person
Household 1 ---- occupies ---------------- 0..* Property
Household * ---- owns (optional) ---------- * Property

Property 1 ---- contains ----------------- 0..* Building
Property 1 ---- contains/covers ---------- 0..* Field

Building 1 ---- exposes ------------------ 0..* Inventory
Building 1 ---- offers workplace --------- 0..* Job
Field    1 ---- has active --------------- 0..1 Crop Cycle
Field    1 ---- creates harvest into ----- 0..* Inventory / staged Lot

Person 1 ---- holds assignments ---------- 0..* Job Assignment
Job    1 ---- filled by ------------------ 0..* Job Assignment
Person 1 ---- carries/controls ----------- 0..* Inventory

Inventory 1 ---- contains ---------------- 0..* Resource Lot
Resource Lot * ---- instantiates --------- 1 Resource Definition
Crop Cycle  * ---- instantiates ---------- 1 Crop Definition
Crop Definition -- yields ---------------- Resource Definition(s)
```

Cardinality expresses the conceptual model, not necessarily the storage layout.

## Entity summaries

### Person

A `Person` is one persistent human being, not an Actor and not a population statistic.

Core authoritative data:

- Stable person ID.
- Name/display identity and birth date or age basis.
- Life-state and, when applicable, death date/cause reference.
- Origin place and cultural background references.
- Primary household ID.
- Parent, child, spouse, and other kin references as supported.
- Residence assignment or explicit transient/homeless state.
- Occupation and active job assignment references.
- Skills, qualifications, and later knowledge/proficiency references.
- Personal possessions through an inventory or ownership references.
- Current commitment/activity and logical/spatial location state.

Important rules:

- A person has at most one primary household at a time.
- Household membership and residence are separate; a person may travel or lodge elsewhere.
- Occupation is an identity/competence category; a job assignment is actual work.
- Current animation, visible destination marker, and UI selection are not person state.
- A person may be dormant or coarsely scheduled off-screen without losing identity.

### Household

A `Household` is a persistent domestic and economic unit composed of real people.

Core authoritative data:

- Stable household ID and origin/founding event.
- Member person IDs and relationship to a household representative where relevant.
- Occupied residence/property rights.
- Owned property and asset claims, distinct from occupation.
- Household-accessible inventories.
- Wealth/accounting state supported by the current economy model.
- Needs, consumption policy, domestic priorities, and migration status.
- Historical events such as settlement, property changes, branching, or dissolution.

Important rules:

- Population is derived from living member records.
- Membership changes are transactions that keep both sides consistent.
- A household can exist without owning property and can own property it does not occupy.
- Shared wealth does not erase personally owned goods.
- Dissolution requires explicit disposition of members, rights, goods, and history.

### Property

A `Property` is a spatially defined parcel or legally meaningful site with rights and access.

Core authoritative data:

- Stable property ID.
- Boundary geometry, area, access points, and settlement/spatial references.
- Owner/claimant, occupier/tenant, and applicable rights or restrictions.
- Contained building and field IDs.
- Road/access relationships and address/local name.
- Permitted uses or current land-use designations where modeled.
- Acquisition, grant, subdivision, merger, and succession provenance.

Important rules:

- Ownership, occupation, jurisdiction, and access are not synonyms.
- Buildings and fields reference the property they physically occupy.
- Boundary changes must preserve or explicitly migrate contained entities and rights.
- Property geometry is authoritative domain/spatial data; a rendered fence is not.

### Building

A `Building` is a persistent constructed entity on a property.

Core authoritative data:

- Stable building ID and building-definition ID.
- Site/footprint, property ID, entrances, and functional spaces.
- Construction date/provenance, materials/fabric, condition, and modifications as supported.
- Owner, occupier/operator, and access rights.
- Residence capacity, workplace capacity, storage points, and service capabilities.
- Attached inventory and production-site IDs.

Important rules:

- The definition describes capabilities; the instance describes this structure and its history.
- A building does not create workers, residents, or resources.
- Production belongs to an explicit process at a capable site, not to elapsed Actor animation.
- Mesh, material instance, streamed Actor, and damage VFX are presentation state.

### Job

A `Job` represents work needed or a durable workplace/role, depending on subtype. The model must not conflate profession, offered work, and accepted commitment.

Recommended conceptual split:

- **Occupation Definition:** Farmer, miller, baker, hauler, logger; describes a vocational category.
- **Work Request / Job:** Work to perform, issuer, site, time window, priority, requirements, expected effort, and compensation policy if applicable.
- **Job Assignment / Commitment:** Link between a person and accepted work, including status and scheduled interval.
- **Activity:** The currently executing portion of an assignment, such as travel, pickup, milling, or delivery.

Core authoritative job data:

- Stable job ID, job type, issuer, work site, and responsible system.
- Required qualifications/tools/resources.
- Capacity or worker count, priority, earliest/latest time, and estimated work.
- Assignment IDs and state: offered, reserved, active, blocked, complete, cancelled.
- Declared output/effect or link to a production/transfer process.

Important rules:

- A person cannot have overlapping exclusive commitments.
- Completion is validated by the owning domain; an AI planner cannot declare success.
- Jobs must expose blocked reasons and cancellation cleanup.
- Large numbers of jobs require indexed queues, not every person scanning every job.

### Resource

`Resource` is split between a definition and physical runtime lots.

`Resource Definition` includes:

- Stable definition ID and category.
- Unit of measure and precision rules.
- Density/volume or storage traits when relevant.
- Quality dimensions, perishability, compatibility, and tags.
- Allowed processes or references used for validation.

`Resource Lot` includes:

- Stable lot ID when independent provenance is required.
- Resource definition ID and quantity.
- Quality/condition and created-at time.
- Owner and current custodian/location state.
- Source/provenance: scenario seed, harvest, production batch, split/merge parents.
- Reserved quantities and applicable expiry/spoilage state.

Important rules:

- Quantity cannot be negative.
- Available quantity equals held quantity minus valid reservations.
- Split and merge operations conserve quantity and preserve useful provenance.
- A lot is held by exactly one custody state: inventory, carrier/in-transit container, staged site, consumed, lost, or destroyed.
- Displayed settlement totals are derived indexes, not storage.

### Inventory

An `Inventory` is an authoritative container/custody boundary attached to a valid holder or site.

Core authoritative data:

- Stable inventory ID.
- Holder/site reference and physical location/access point.
- Owner/operator and access policy.
- Capacity constraints and supported resource rules.
- Contained lot references or equivalent compact lot records.
- Active reservations and pending transfer links.

Important rules:

- Moving goods is a transaction between custody states.
- Reservations prevent double allocation but do not themselves move goods.
- Transfers validate source availability and destination acceptance.
- Capacity and access failures are explicit.
- Caches such as `total wheat` can be rebuilt from contents.

### Field

A `Field` is a managed land area, not a farm-building output slot.

Core authoritative data:

- Stable field ID, geometry, derived area, property ID, and access points.
- Owner/occupier/operator rights.
- Soil characteristic references and current fertility/moisture state at the supported resolution.
- Active crop-cycle ID or fallow/other-use state.
- Work history, crop history, amendments/manure, and yield history as supported.
- Staging/harvest location or linked field inventory.

Important rules:

- Yield is based on physical area, crop parameters, environment, inputs, and labor.
- A field cannot host incompatible overlapping crop cycles.
- Field history persists independently from a currently planted crop.
- Farm ownership or a nearby building does not bypass land requirements.

### Crop

`Crop` is split into an authored crop definition and a field-specific crop cycle.

`Crop Definition` includes:

- Stable crop definition ID, appropriate regions/conditions, and historical availability metadata.
- Seed resource, harvest resources, and baseline ratios.
- Seasonal/growth requirements and work stages.
- Soil, moisture, temperature, and fertility responses at the chosen resolution.
- Supported process and byproduct references.

`Crop Cycle` includes:

- Stable cycle ID, field ID, crop definition ID, and season/year.
- Sowing date, seed lot/provenance, planted area, and stage.
- Completed/required work by stage.
- Accumulated environmental effects and expected/actual yield factors.
- Harvest state and output lot references.

Important rules:

- Reaching a calendar date does not create a harvest.
- Harvest output is created once through an auditable completion transaction.
- Abandonment or failure records disposition of seed, standing crop, and field history.

### Settlement

A `Settlement` is a spatial, social, and administrative grouping. It is not a container that owns copies of all local state.

Core authoritative data:

- Stable settlement ID, name, founding/origin data, and regional/cultural context.
- Spatial extent, constituent property and road references.
- Resident/registered household and person indexes.
- Institutions, offices, market/service references, and jurisdiction links.
- Summary indexes used for queries, with rebuild rules.
- Important settlement history.

Important rules:

- Population, stocks, and production totals are derived from underlying entities.
- Physical inclusion, legal jurisdiction, market service, and cultural affiliation may differ.
- A settlement provides query and governance scope; it does not directly mutate every member domain.
- Settlement summaries must identify their timestamp and aggregation rules.

## Supporting records needed for the relationships

Although not in the required core list, several records prevent ambiguous ownership:

- **Residence Assignment:** Person/household, building space, validity interval, and status.
- **Property Right:** Holder, property, right type, share, restrictions, and validity.
- **Reservation:** Requester, lot/resource quantity, source inventory, expiry, and state.
- **Transfer Order:** Source, destination, requested quantity, reservation, carrier, and status.
- **Transit Custody:** Carrier/container, lots held, route state, and destination.
- **Production Batch:** Process definition, site, inputs, outputs, worker effort, timestamps, and status.
- **Crop Cycle:** Field-specific planted instance as described above.
- **Domain Event / History Entry:** Typed committed fact with time, participants, place, and provenance.

These relationships should be explicit records when they have their own lifecycle, history, or failure states.

## Authoritative versus non-authoritative examples

| Concern | Authoritative | Derived or presentation-only |
|---|---|---|
| Person location | Logical position/route/custody state | Interpolated skeletal mesh transform |
| Household membership | Person and household relationship transaction | Household UI member list |
| Goods | Lots/quantities in custody states | Floating resource icon or warehouse pile mesh |
| Crop | Cycle state and accumulated factors | Crop growth material/mesh stage |
| Work | Accepted commitment and domain progress | Animation montage and progress-bar smoothing |
| Building | Identity, footprint, capability, condition | Spawned Actor, LOD mesh, effects |
| Population | Count derived from living person records | Cached dashboard text |
| AI plan | Only committed orders/reservations are authoritative | Candidate goals, scores, search tree |

## Transactions and invariants

Multi-entity changes must be atomic from the perspective of other simulation systems. Examples include:

- Moving a person between households updates both memberships or neither.
- Transferring a lot removes source custody and creates destination custody exactly once.
- Starting production reserves/consumes inputs according to one declared policy.
- Completing harvest creates output once and closes the crop cycle consistently.
- Assigning property updates rights, occupier state, and relevant household references together.

Development builds should validate orphan references, duplicate custody, negative quantity, overlapping exclusive commitments, invalid containment, and cached-total mismatches.

## Scale and storage implications

Thousands of people imply more jobs, events, goods lots, and relationship records than visible Actors. Therefore:

- Runtime records should be compact and batch-queryable where volume justifies it.
- Stable handles must detect stale/deleted references.
- Systems should maintain targeted indexes for household, settlement, spatial region, due time, resource type, and job eligibility.
- Indexes and summary caches must be rebuildable.
- Records wake on due time or relevant invalidation rather than owning independent ticks.
- Lots may merge when resource, quality, ownership, location, and provenance policy permit it.
- Historical detail requires retention tiers: permanent landmark history, summarized ordinary history, and discardable diagnostics.
- Cold/off-screen data may be stored differently, provided reactivation preserves identity and constitutional invariants.

No implementation container—Actor array, UObject graph, ECS fragment, database table—is selected by this overview.

## Serialization and evolution

Every serialized entity record needs:

- Type and schema version.
- Stable ID.
- Definition/version references where interpretation depends on authored data.
- Required relationship IDs.
- Enough state to reconstruct schedules, indexes, and presentation safely.

Migrations must preserve identity and conservation. Removed concepts require explicit conversion or disposition rather than silent deletion. Save validation should be able to report unresolved references and quantity discrepancies before accepting a migrated save.

## Prototype 0.1 minimum relationship chain

The smallest valid prototype model is:

```text
Person -> Household -> Residence/Property -> Settlement
Person -> Job Assignment -> Farming/Hauling/Production Work
Property -> Field -> Wheat Crop Cycle -> Wheat Resource Lot
Building/Site -> Inventory -> Resource Lots
Transfer Order -> Reservation -> Carrier Transit -> Destination Inventory
Mill/Bakery -> Production Batch -> Input Lots -> Output Lots
Market Inventory -> Household Inventory -> Consumption
```

Every arrow must resolve through a stable ID or owned transaction. Unreal representations may mirror these relationships for display but may not replace them.
