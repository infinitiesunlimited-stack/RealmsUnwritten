# Prototype 0.1 Scope

## Prototype objective

Prototype 0.1 is a playable, inspectable vertical slice of one physical food chain and its human context. It exists to prove that persistent people and households can occupy real property, perform scheduled work, and move conserved goods through production sites without relying on global resource counters or per-person frame ticks.

It is not a miniature version of the full game and is not expected to contain combat, diplomacy, multiple civilizations, or the final economy.

## Testable hypotheses

The prototype succeeds if it provides evidence that:

1. A 20–30 person settlement can feel socially grounded through households and residences.
2. A wheat-to-bread chain remains understandable when every material transfer is physical.
3. Work allocation and hauling can operate through scheduled decisions and explicit jobs.
4. Property geometry and road access create visible settlement structure.
5. Authoritative simulation can run independently of Actor and UI lifetimes.
6. Failures such as missing labor, blocked access, or absent stock can be diagnosed from the game state.
7. The chosen data boundaries have a credible path to thousands of people.

## Player-facing scenario

The scenario contains one small Franconian settlement around 1370 with:

- Approximately five households and 20–30 persistent people.
- At least one arriving family that can be admitted and assigned a property.
- One road providing access between settlement properties and work sites.
- Household properties with visible boundaries and residences.
- One managed forest work area.
- One farm with at least one wheat field.
- One storage structure.
- One mill.
- One bakery.
- One market.

The scenario may start with limited seed grain, food, tools, and operating stocks. Every seeded physical good must begin in a declared inventory and be identifiable as scenario initialization.

## Required complete loop

The following chain must work through authoritative state:

1. A family immigrates from a declared origin.
2. The settlement admits the household and assigns occupation of a valid property.
3. Household members reside at the property's house.
4. A farmer prepares and cultivates a wheat field using seed from an inventory.
5. Time and labor advance the crop to harvest under simplified, declared growing conditions.
6. Harvest creates a located wheat lot at the field or harvest collection point.
7. A hauling need is created and a carrier physically moves wheat to storage.
8. The mill requests/reserves wheat and receives it through physical transport.
9. A worker operates the mill; a declared process consumes wheat and produces flour.
10. Flour is transported to the bakery.
11. A baker consumes flour and produces bread through a declared process.
12. Bread is transported to a market inventory.
13. A household obtains bread through an explicit market transfer.
14. Bread reaches household custody and can satisfy a food need through explicit consumption.

No step may be represented only by changing a global total.

## Included systems

### Calendar and simulation control

- Continuous date/time appropriate to agricultural work.
- Pause and a small set of time speeds.
- Scheduled updates independent from render frames.
- Seeded/reproducible scenario mode for tests.

The exact real-time-to-calendar-time ratio is a tuning variable.

### People

Each person has, at minimum:

- Stable ID and display name.
- Age or birth date.
- Life-state.
- Cultural background and origin reference.
- Household membership and family relationships needed by the scenario.
- Residence reference.
- One primary occupation/job assignment when applicable.
- A minimal skill/qualification set for their work.
- Current authoritative activity/commitment and coarse location state.

Birth, marriage, disease, and ordinary mortality simulation are deferred. Data must not preclude them.

### Households

Each household has:

- Stable ID, members, head/representative if applicable, and origin.
- Residence/property occupation.
- Household inventory access and a simple food need.
- Minimal wealth/exchange balance if the market transaction requires payment.
- A traceable arrival/settlement event.

Household splitting, inheritance, and autonomous emigration are deferred.

### Property, road, and access

- Authored property polygons/boundaries with stable IDs.
- One or more household properties capable of containing a house and reserved open area.
- Work-site properties or parcels for field, forest, storage, mill, bakery, and market as appropriate.
- Road-connected access points used for route eligibility and travel.
- Explicit property assignment/occupation state.

Freeform procedural parcel subdivision is deferred. The prototype may use authored plots while preserving the conceptual property model.

### Buildings and sites

- A house/residence function.
- Storage, mill, bakery, and market functions.
- Inventories attached to valid storage points rather than the building archetype itself.
- Workplace slots/capacity and access points.
- Persistent building identity independent of rendering.

Construction, decay, fire, remodeling, and architectural aging are deferred. Buildings may be scenario-authored, but their construction date/provenance fields should exist where inexpensive.

### Field and wheat crop

- One crop definition: wheat.
- Field geometry and derived area.
- Minimal field state: availability, planted crop cycle, seed input, work progress, growth/maturity, harvestability, and recent crop history entry.
- Simplified fertility/moisture/weather parameters sufficient to alter or explain yield.
- Yield calculated from area, declared field conditions, and completed labor rather than emitted by a farm building.

Detailed crop rotation, pests, weeds, many soil layers, and other crops are deferred.

### Resources and inventories

Required resource definitions include at least seed wheat/wheat grain, flour, bread, raw timber/logs, and any explicit fuel or tool categories actually consumed by prototype processes.

- Goods exist as lots or safely aggregated stacks within specific inventories.
- Quantity uses declared units.
- Lots support owner, custodian/location, quality if relevant, reservation, and provenance.
- Transfers, consumption, production, loss, and scenario seeding are logged for diagnostics.
- Capacity constraints exist at least by supported type and total amount/volume/mass at the simplest useful resolution.

### Jobs and labor

- Jobs are requests/commitments for work at a place over time.
- People require basic eligibility and availability.
- Farming, hauling, milling, baking, logging, and market operation are represented.
- Travel and work consume simulation time.
- One person cannot perform conflicting committed jobs simultaneously.

Advanced labor markets, contracts, guild rules, wage negotiation, and apprenticeships are deferred.

### Production

- Data-defined process specifications for milling and baking.
- Declared input/output ratios, duration/work, site requirement, and worker requirement.
- Atomic completion or another transaction-safe rule prevents input duplication and output creation on interrupted work.
- Incomplete or blocked batches expose a reason.

Fuel may be omitted from a process only as an explicit prototype simplification. Water/wind power details for the mill must be declared by the scenario even if represented as site capability rather than a full environmental simulation.

### Logistics and movement

- Transfer requests with source, destination, resource, quantity, and priority.
- Reservation of goods before collection.
- A person-carried inventory; a simple handcart is optional only if scoped before implementation.
- Path/travel along valid traversable routes, with road access materially represented.
- Pickup and drop-off that change authoritative custody.
- Recovery from cancellation, blocked paths, unavailable goods, and incapacitated/removed carriers without duplication.

### Basic logging

- A forest work area with finite standing timber or a declared renewable stock model.
- Logging work produces located log lots.
- Logs are physically transported to storage.
- Timber growth, detailed forest ecology, charcoal, sawing, construction consumption, and tool wear are deferred unless needed for the vertical slice.

### Market handoff and consumption

Prototype 0.1 must prove that bread changes custody at the market and reaches a household. The initial implementation may use fixed test prices and seeded balances, provided:

- The rule is labeled as a prototype economy policy.
- A transaction identifies seller/market, buyer household, quantity, and consideration.
- Bread moves between actual inventories.
- Insufficient stock or means causes a legible failure.

Dynamic price formation, merchant competition, credit, tax, wages, and a full coin model are deferred. If a no-payment distribution policy is used instead, it must still record legal transfer and cannot be presented as the final market design.

### Minimal needs

- A household has a measurable food requirement.
- Bread in household custody can be consumed to reduce that requirement.
- Hunger status is visible and causally traceable.

Nutrition, meal preparation, health, disease, and starvation death are deferred.

### Inspection and diagnostics

The prototype must include development-facing views for:

- Person, household, property, building, field, and inventory identity.
- Current/queued job and reason for idle/blocked status.
- Lot origin, reservations, transfers, and transformations.
- Production batch inputs, outputs, progress, and blockers.
- The currently detected bottleneck in the bread chain.
- Simulation timing, scheduled workload, Actor count, and path/decision budgets.

Polished final UI is not required; causal inspectability is.

### Save and load

- At least one manual or automated save/load path for authoritative prototype state.
- Stable IDs and relationships survive a round trip.
- Inventories, reservations/in-transit custody, crop progress, jobs/commitments, calendar, and required scheduled actions survive or reconstruct safely.
- Schema version is recorded even if cross-version compatibility is not yet promised.

## Explicitly out of scope

- Combat, weapons, units, formations, and fortifications.
- Multiple civilizations or playable factions.
- Regional world simulation and long-distance trade.
- Diplomacy, titles, law, taxation, and political advancement.
- Birth, marriage, inheritance, normal aging consequences, and multi-generation play.
- Autonomous migration pressures beyond the scripted/commanded arrival used to prove immigration.
- Advanced knowledge transmission, libraries, schools, universities, and guilds.
- Technology diffusion or post-1370 transitions.
- Livestock and non-wheat crops.
- Detailed weather, disease, soil chemistry, ecology, fire, or building decay.
- Procedural roads, property subdivision, and full construction gameplay.
- Dynamic macroeconomy, credit, banking, or sophisticated price discovery.
- Multiplayer and networking.
- Final art, final animation, final UX, broad accessibility work, and production content scale.

An out-of-scope system may be represented by a narrow interface or data field. It must not be partially built without a scope amendment.

## Required failure cases

At minimum, automated or reproducible scenarios must cover:

- No seed wheat available.
- Field lacks assigned/available labor.
- Wheat exists but is reserved or inaccessible.
- Destination inventory is full or rejects the resource.
- Mill or bakery lacks an eligible worker.
- A route becomes unavailable during a transfer.
- A carrier releases or cancels a job while holding goods.
- Bread reaches market but a household cannot obtain it.
- Save/load occurs while goods are in transit or a production batch is active.

The system must conserve quantities and expose a reason; it need not solve every failure autonomously in this prototype.

## Acceptance criteria

Prototype 0.1 is complete only when:

1. The full immigrant-family-to-consumed-bread chain can complete from a known scenario state without developer mutation of inventories.
2. Every required good can be traced through its source, custody transfers, transformation, and consumption.
3. Removing one necessary input, worker, route, or capacity blocks the chain for the correct visible reason.
4. No physical resource relies on a global authoritative counter.
5. Unloading or removing a visual representation does not delete or materially alter its simulation entity.
6. A headless or low-presentation test can advance the core economic loop.
7. Save/load during active work preserves conservation and valid references.
8. Automated tests cover domain invariants, production ratios, transfer transaction safety, and the critical happy path.
9. Performance instrumentation confirms that people and economic entities do not require individual per-frame ticks.
10. Known shortcuts and deferred questions are recorded before the milestone is closed.

## Prototype sequence

Implementation should proceed through gated slices, each remaining executable:

1. **Authoritative identity and time:** Registry, calendar, scenario load, headless stepping.
2. **Place and population:** People, households, properties, residences, settlement membership.
3. **Goods and custody:** Resource definitions, inventories, lots, reservations, invariant tests.
4. **Work and movement:** Jobs, commitments, route movement, pickup/drop-off.
5. **Agriculture:** Field, wheat cycle, labor, harvest output.
6. **Transformation:** Storage, milling, flour, baking, bread.
7. **Exchange and need:** Market handoff, household acquisition, consumption.
8. **Secondary validation:** Logging to storage.
9. **Persistence and resilience:** Save/load, failure cases, performance harness.
10. **Presentation pass:** Replace diagnostic proxies only where useful to assess the experience.

Passing a slice requires tests and inspectability, not final visuals.

## Exit review

The milestone review must answer:

- Which parts of the loop produced meaningful player decisions?
- Which details improved causal understanding, and which added noise?
- Where did logistics create engaging constraints versus avoidable friction?
- Which data structures or queries fail synthetic scale tests?
- Can every quantity discrepancy be explained?
- Which prototype policies must be replaced before expanding the economy?
- Does the architecture still comply with every applicable constitutional law?

The next milestone begins only after these findings are documented and the source-of-truth documents are updated where approved.
