# High-Level Architecture

## Purpose

This document defines the durable system boundaries for the game without selecting premature implementation details. It describes responsibilities, data flow, and scaling constraints. It is not a class diagram and does not authorize production implementation.

## Architectural objectives

The architecture must:

- Preserve stable identity and history for thousands of people and world entities.
- Support continuous historical time without per-entity per-frame behavior.
- Keep physical goods, land, ownership, and knowledge auditable.
- Allow simulation, AI, visualization, and UI to evolve independently.
- Make small-settlement detail and large-polity delegation compatible.
- Keep civilization, settlement, and battle as different resolutions of one persistent world.
- Support save migration, automated tests, deterministic diagnostics, and headless simulation.
- Express regional and historical variation primarily through validated data and policies.

## Four strictly separated concerns

### 1. Authoritative simulation data

The simulation is the sole source of truth for people, households, time, land, buildings, inventories, ownership, work, production, transfers, and history. It accepts validated commands, advances through scheduled work, and emits domain events and read-only snapshots.

Authoritative data must not depend on whether an entity currently has an Unreal Actor, animation, widget, selected unit, or AI planner instance.

### 2. AI decision-making

AI observes approved snapshots and events, forms plans or policies, and submits the same types of commands available to human control at the relevant authority level. A plan is not world truth. Reservations accepted by the simulation are world truth; an AI's intended reservation is not.

AI may run at multiple scopes—person, household, enterprise, settlement, institution, polity, and military command—and at different cadences. Most decisions should be triggered by need, changed state, or scheduled review rather than every frame.

### 3. Unreal visual representation

The presentation layer creates Actors, components, meshes, animation, effects, and audio for the currently represented part of the world. Visual objects carry stable simulation identifiers and render interpolated simulation results. They may predict cosmetic motion, but they do not own durable economic or social state.

Actor destruction due to distance, pooling, streaming, or map transitions must not destroy the simulated entity.

### 4. UI representation

The UI consumes read models designed for explanation and interaction. It does not query arbitrary mutable objects or calculate authoritative totals from visible Actors. It issues commands through an application-facing command service and reports confirmed or rejected results.

UI projections may aggregate data, format history, and display forecasts, but must label estimates as estimates.

## Layer model

```text
Human input          AI planners and policies
     |                         |
     +------ proposed commands +
                    |
          Application / command boundary
                    |
          validation, authority, ordering
                    |
          Authoritative simulation core
        state + rules + scheduler + events
             /              |            \
     read models       domain events      save snapshots
         /   \              |                  |
       UI   Unreal presentation        persistence/migration
```

Dependencies point inward toward stable domain contracts. The simulation core must be runnable without rendering and without a player UI.

## Core architectural concepts

### Stable identifiers

Every persistent entity uses a stable, serialized ID. References between authoritative entities use IDs or validated handles, never an Unreal Actor pointer. Definitions such as `Wheat`, `Bread`, or a building archetype use separate definition IDs from runtime entity IDs.

### Commands

A command is a request to change authoritative state. It includes issuer, target scope, parameters, and ordering/time metadata. The owning system validates rights, prerequisites, current state, and invariants before applying it.

Examples include assigning a household to property, accepting a hauling job, moving a goods lot, beginning a production batch, or purchasing bread. Command names are conceptual until feature specifications ratify them.

### Domain events

Domain events describe committed facts such as `HouseholdSettled`, `HarvestCompleted`, `GoodsTransferred`, or `PersonDied`. Events notify other systems and feed history/debugging. They are not a universal substitute for current state, and the project is not committed to full event sourcing.

### Definitions, instances, and policies

- **Definitions** are authored, versioned data: crop types, resource types, recipes, building archetypes, occupations, skills, and historical availability constraints.
- **Instances** are runtime entities: this person, field, building, wheat lot, household, or production batch.
- **Policies** are replaceable decision rules: job selection, household consumption preference, delegation, market behavior, or AI strategy.

Definitions cannot mutate runtime state directly. Policies propose choices; domain systems enforce them.

### Ownership, custody, location, and access

These are distinct concepts:

- **Ownership:** Who holds the legal/economic claim.
- **Custody:** Which person, inventory, vehicle, or site physically holds an item now.
- **Location:** Where it is in world or transit space.
- **Access:** Who is permitted to use, enter, withdraw, or deposit.

Collapsing them would prevent theft, tenancy, contracted hauling, market exchange, taxation, and political rights from developing cleanly.

## Major future systems

The systems below define expected boundaries, not a mandate to build them now.

| System | Owns / governs | Principal relationships |
|---|---|---|
| Time and Calendar | Authoritative date, simulation steps, seasons, scheduled wake-ups | Drives agriculture, aging, work, events, weather, contracts |
| Identity and Registry | Stable IDs, life-cycle state, safe reference resolution | Used by every persistent domain; never owns their business rules |
| Demography and Kinship | Birth, age, death, family links, cultural origin | Household, migration, inheritance, labor, history |
| Household | Membership, domestic needs, pooled decisions, residence, household wealth | Person, property, inventories, market, migration |
| Land and Property | Parcels, boundaries, tenure, rights, access, succession | Settlement, field, building, roads, household, government |
| Spatial and Settlement | Settlement extent, addresses, roads, routes, service areas, membership; inherited form that constrains later growth | Property, logistics, construction, governance, presentation |
| Building and Construction | Building instances, condition, fabric, extensions, functional spaces; vacancy, reuse, replacement, and traces when those lifecycles exist | Property, inventories, workplaces, housing, historical record |
| Occupation and Labor | Jobs, qualifications, assignment, availability, work commitments | Person, household, production, logistics, institutions |
| Production | Recipes/processes, batches, work sites, tools, input/output transformation | Labor, inventory, building, knowledge, power sources |
| Resources and Inventory | Resource definitions, lots, quantity, quality, custody, reservations | Production, logistics, households, trade, military |
| Logistics | Transfer requests, routes, carriers, capacity, pickup/drop-off, delivery state | Inventory, labor, roads, market, production |
| Agriculture and Ecology | Fields, crop cycles, soil state, moisture, fertility, weather response | Land, labor, tools, animals, harvest inventory |
| Market and Economy | Offers, transactions, prices, contracts, credit/coin abstractions, accounts | Household, enterprise, inventories, government, trade |
| Knowledge and Practice | Knowledge items, proficiency, carriers, teaching, adoption, loss | People, texts, institutions, production, warfare |
| Migration | Push/pull evaluation, journeys, admission, origin/destination, settlement | People, households, labor, safety, property, politics |
| Institution and Delegation | Offices, membership, jurisdiction, mandates, delegated policies | Guilds, councils, estates, schools, military, government |
| Polity and Governance | Titles, offices, claims, law, taxation, obligations, legitimacy | Settlements, property, institutions, diplomacy, warfare |
| Diplomacy and Regional Trade | Relations, agreements, routes, cross-border movement | Polities, merchants, migration, knowledge, logistics |
| Military and Warfare | Forces composed from simulated people, command, training, equipment, morale, formation, campaigns; casualties and supply returned to civilization | People, economy, logistics, terrain, polity, knowledge |
| Event and World Memory | Significant facts, provenance, chronicles, entity timelines | Receives filtered domain events; serves UI and historical effects |
| Persistence and Migration | Save snapshots, schema versions, compatibility, validation | Serializes authoritative data and required scheduler state |
| Diagnostics and Telemetry | Invariants, causal traces, performance budgets, replay seeds | Observes all systems without becoming gameplay authority |

## Relationship and data flow

```text
Person --member of--> Household --occupies/owns--> Property
  |                       |                           |
works Job             consumes from              contains
  |                       |                    /             \
  v                       v                Building         Field
Production Site <---- Inventory <---- Logistics             |
  |                       ^                                Crop Cycle
  +-- transforms ---------+                                   |
                          +----------- Harvest ---------------+

Settlement groups properties, roads, residents, and institutions.
Government grants or constrains rights; it does not replace local entities.
Knowledge qualifies people and processes; it does not directly create goods.
```

Typical material flow:

1. Production creates a transfer need for declared input lots.
2. Logistics reserves a valid quantity at a source inventory.
3. A qualified carrier accepts work and physically collects the reserved goods.
4. Custody changes to an in-transit inventory.
5. Movement follows a route whose distance and capacity impose time/cost.
6. Delivery transfers custody to the destination inventory.
7. Production consumes inputs and creates output lots with provenance.

No visual animation may substitute for steps 2–6 in the authoritative simulation.

## Time and execution model

### Simulation time

The world advances using an authoritative simulation clock independent from render frames. Exact tick duration is a prototype measurement, not a constitutional constant. Calendar conversion, time speed, and pausing sit above domain systems.

### Scheduled and event-driven work

Entities do not all update every simulation step. Systems maintain due work, relevant events, and batch queues. Examples:

- A sleeping person's next routine review can be scheduled for morning.
- A crop can update at an agronomic interval or on meaningful weather changes.
- A production batch wakes when labor, inputs, or elapsed process time changes.
- A household reevaluates food sourcing when stock crosses a threshold, price changes materially, or its planned review is due.

Continuous-looking effects may be computed analytically from a start time, rate, and current time rather than incremented every frame.

### Simulation levels of detail

Level of detail applies separately to visualization, decision-making, and simulation frequency:

- **Presentation LOD:** Whether an entity has a full Actor, simplified proxy, marker, or no rendering.
- **Decision LOD:** How frequently and at what scope plans are reconsidered.
- **Simulation LOD:** Whether a process is evaluated individually, in a safe batch, or analytically between events.

Simulation LOD must not invent or delete people or goods. Aggregation is allowed only behind documented conservation and de-aggregation rules. A distant person may have a coarse schedule, but their identity, household, job, location state, and consequential possessions remain authoritative.

These LODs serve three player-facing resolutions of the same world:

```text
CIVILIZATION SCALE
Kingdom / region / politics / economy / war
        ↓
SETTLEMENT SCALE
Households / production / construction / trade
        ↓
BATTLE SCALE
People / formations / equipment / terrain / command
```

A battle may increase local simulation detail. It must not replace the settlement's people, goods, equipment, or provenance with a parallel combat model. Future military logistics, when built, must use the same physical custody and movement path as other goods rather than an abstract global supply pool.

### Spatial partitioning

The simulation should index entities by logical regions, settlements, parcels, routes, and proximity queries without treating streamed Unreal levels as authority. Only active partitions need dense pathing and presentation updates. Cross-partition transfers use explicit journey/transit state.

## AI architecture

AI is hierarchical and budgeted:

- **Person routines** select among available, authorized actions based on commitments and needs.
- **Household policies** handle domestic budgets, consumption, work preferences, property use, and migration consideration. When construction exists, ordinary household buildings and adaptations are household decisions inside land, access, resource, and institutional constraints—not unrestricted sprawl and not a requirement that the player place each structure.
- **Work coordinators** expose jobs and production needs rather than directly puppeting every worker.
- **Settlement/institution planners** allocate mandates, projects, access rules, land-use constraints, and priorities, including commissioned major works. They should change conditions rather than stamp every domestic building.
- **Polity and military planners** operate through law, budgets, appointments, diplomacy, and command structures, raising and directing forces composed of simulated people rather than spawning a disconnected army.

Higher-level AI should change constraints and priorities; it should not bypass lower-level material requirements. AI computation uses time budgets, staggered reviews, cached queries, and invalidation on meaningful events. Expensive path searches and market searches require queues and budgets.

AI state that matters to saves—accepted goals, commitments, contracts, orders, reservations—must be promoted into authoritative or persistently coordinated records. Disposable search trees and scoring caches are not authoritative.

## Unreal presentation boundary

The Unreal-facing layer is responsible for:

- Spawning and pooling representations for relevant simulation IDs.
- Interpolating movement between authoritative updates.
- Selecting meshes/materials from building age, fabric, region, condition, and modifications.
- Animation, sound, effects, hit feedback, and selection affordances.
- Translating interaction into commands and displaying command outcomes.

It is not responsible for:

- Storing the only copy of inventory or household membership.
- Deciding production completion from an animation notify.
- Advancing age or crops from Actor tick.
- Treating unloaded Actors as nonexistent.
- Allowing widgets or Blueprints to mutate domain data without validation.

Blueprints are suitable for presentation assembly and bounded content behavior. Core rules, high-volume data processing, invariants, and save contracts should use testable modules with clearly owned interfaces. The exact C++/Blueprint split will be specified before implementation.

## UI and explainability

The UI reads purpose-built projections such as:

- Household summary with member, food, residence, wealth, and recent-change explanations.
- Inventory summary with reserved, available, in-transit, and expected quantities.
- Production chain status with the current bottleneck and causal trace.
- Person summary with relationships, job commitment, current activity, and provenance.
- Settlement ledger aggregated from authoritative entities.

Read models should carry source timestamps/version numbers so stale displays are recognizable. Debug builds should permit drilling from an aggregate into source entities and recent events.

## Persistence and compatibility

A save contains authoritative state, stable IDs, calendar state, required scheduled work, random seeds where outcomes depend on them, and version metadata. Visual Actors, widgets, ephemeral path caches, and disposable AI search state are reconstructed.

Serialized records require explicit schema versions. Changes must be classified as:

- Compatible without migration.
- Compatible with an automated migration.
- Intentionally incompatible during an approved prototype window.

Compatibility requirements are set per milestone. Once a milestone declares save stability, breaking it requires review, a migration plan, and tests using representative old saves.

## Data-driven content and validation

Resources, crops, processes, occupations, building functions, and regional parameters should be data-defined where practical. Data must be validated on load for missing references, impossible quantities, cycles in ownership, invalid process inputs, unsupported units, and historical/geographic scope.

Data-driven does not mean rule-free. Domain systems own semantics and invariants; content chooses among supported behavior.

## Determinism and reproducibility policy

Perfect cross-platform lockstep is not assumed. The architecture must nevertheless support:

- Explicit random streams/seeds for consequential systems.
- Stable command and event ordering within a simulation step.
- Headless repeat runs for test scenarios.
- State checksums or invariant summaries at diagnostic checkpoints.
- Reproduction records containing scenario version, seed, commands, and relevant configuration.

A stricter multiplayer determinism policy must be decided before network architecture or large-scale combat implementation.

## Performance strategy

Performance work begins with budgets and measurement, not premature loss of simulation meaning. Core tactics are:

- Structure high-volume authoritative records for batch iteration and compact storage.
- Avoid one ticking UObject/Actor per simulated entity.
- Use dirty flags, threshold events, due-time queues, and batched system passes.
- Cache derived queries with clear invalidation and rebuild paths.
- Bound planners, pathfinding, and allocation work per frame/step.
- Partition spatial and economic searches.
- Record population-scale benchmarks early and run them headlessly.

Prototype 0.1 should instrument counts and timings even though its population is small. Before committing to data layout, synthetic tests should model at least the order of magnitude expected from thousands of people and substantially more goods lots, jobs, and scheduled actions.

## Integration rules

- Each authoritative concept has one owning system.
- Cross-system mutation occurs through commands or narrow owned services.
- Cross-system observation uses stable queries, read models, or typed events.
- Circular synchronous dependencies require redesign or explicit orchestration.
- A new global singleton is not a substitute for ownership design.
- An Unreal Actor or widget may cache a view, never become a competing authority.
- Prototype systems must expose seams for replacement without pretending unbuilt future systems already exist.

## Decisions intentionally deferred

The following require experiments or later milestone decisions:

- Exact simulation step duration and calendar speed.
- ECS, Mass Entity, custom data-oriented store, UObject, or hybrid implementation choices.
- Navigation technology and agent crowd representation.
- Multiplayer topology and deterministic lockstep requirements.
- Regional map partition and off-map simulation model.
- Exact monetary, credit, contract, and accounting representation.
- Genetics, detailed disease, religion, and language models.
- Granularity of item lots, provenance retention, and aggregation thresholds.

Deferring these choices prevents Prototype 0.1 from hardening assumptions unsupported by evidence.
