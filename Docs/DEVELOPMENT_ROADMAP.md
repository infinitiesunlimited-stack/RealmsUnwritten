# Development Roadmap

## Roadmap principles

This roadmap is capability-gated, not date-driven. A phase advances when its exit evidence is accepted, not because a calendar milestone arrived. Later systems may be researched and specified early, but production implementation should follow proven dependencies.

Each phase must:

- Name the player or simulation question it proves.
- Define performance, save, test, and diagnostic evidence.
- Keep a playable or headless executable scenario.
- Record prototype shortcuts and replacement triggers.
- Update source-of-truth documents when approved decisions change.
- Pass constitutional and architectural review before broadening scope.

Version numbers below communicate dependency and maturity, not final release numbering.

## Phase 0 — Source of truth and technical discovery

**Question:** Do the team and coding agents share a stable model of what is being built and what must not be compromised?

Deliverables:

- Ratified vision, design constitution, architecture, data model, Prototype 0.1 scope, roadmap, and AI development rules.
- Initial glossary and decision-record template when implementation planning begins.
- Research evidence format and initial Franconian 1370 research questions.
- Technical spikes for uncertain engine choices, kept separate from production code.
- First performance hypothesis and representative scale targets.

Exit gate:

- All seven baseline documents are reviewed by the Creative Director.
- Open architectural questions are labeled as deferred rather than accidentally decided.
- No production game code is required or authorized by this phase.

## Phase 0.1 — Franconian bread-chain vertical slice

**Question:** Can real people and households produce, transport, exchange, and consume physical food in a causally legible settlement?

Scope is governed by `PROTOTYPE_0_1_SCOPE.md`.

Primary capabilities:

- Independent simulation clock, identity, commands, events, and headless stepping.
- 20–30 people in roughly five households.
- Authored properties, residence, road access, forest, field, and work sites.
- Wheat cultivation and harvest.
- Physical inventory, reservation, transport, milling, baking, market transfer, and consumption.
- Basic logging to storage.
- Diagnostic inspection, failure cases, and save/load round trip.
- Synthetic scale harness exercising thousands of lightweight person records and representative workloads.

Exit gate:

- Every acceptance criterion in the prototype scope passes.
- The complete material chain is reproducible from a known scenario and seed.
- The review identifies which decisions may graduate into durable architecture.
- The Creative Director approves any move beyond the prototype.

## Phase 0.2 — Household continuity and labor resilience

**Question:** Does the settlement remain coherent when people and households change over time rather than following a fixed scenario script?

Candidate capabilities:

- Aging cadence, death, household membership changes, and basic household formation.
- Autonomous immigration/emigration evaluation at local scale.
- Residence pressure and property occupancy transitions.
- More robust schedules, competing job demands, rest, and domestic labor.
- Basic skill acquisition through practice and person-to-person teaching.
- Household wealth, consumption priorities, and minimal wages/exchange tested beyond seeded balances.
- Longer save/load soak scenarios.

Exit gate:

- Population changes have traceable sources and destinations.
- Household dissolution/formation preserves people, rights, and goods.
- Labor shortages and recovery emerge without invalid state.
- Multi-year runs pass invariants and performance budgets.

## Phase 0.3 — Land, agriculture, and seasonal risk

**Question:** Can land management create strategic food security rather than a fixed production formula?

Candidate capabilities:

- Multiple fields and crops appropriate to the test region: rye, barley, oats, peas/beans, vegetables, fruit, and regionally justified grapes.
- Crop rotation, fallow, fertility, manure, moisture, and weather at useful resolution.
- Seed retention, yield quality, spoilage, storage conditions, and seasonal labor peaks.
- Livestock introduced incrementally, beginning with one species and complete feed/reproduction/product/manure implications.
- Farm tools, draft power, maintenance, and land access.
- Property subdivision/merger experiments if household growth requires them.

Exit gate:

- Food security responds legibly to land, labor, weather, storage, and household choices.
- Crop and livestock data are geographically and historically scoped.
- Seasonal workload remains computationally bounded at target population scale.

## Phase 0.4 — Local economy, craft, and construction

**Question:** Can specialized production and exchange grow without dissolving into global resource pools?

Candidate capabilities:

- Expanded materials and craft chains, selected for dependency coverage rather than quantity.
- Construction, repair, modification, condition, and visible building chronology.
- Dynamic offers/prices at local-market scale, contracts, wages, rents, and basic credit only as research supports.
- Enterprise/workshop operation distinct from household identity where needed.
- Tools, quality, maintenance, byproducts, fuel, and storage specialization.
- Roads and carrying technology affecting logistics capacity.
- Audit tools for bottlenecks, value flow, and conservation.

Exit gate:

- At least two interacting production sectors respond to price, access, skill, and logistics.
- Household property changes arise from accumulated means and decisions.
- Economic abstraction policies are documented and causally inspectable.
- Goods-lot and market workloads meet scale budgets.

## Phase 0.5 — Institutions and settlement-scale delegation

**Question:** Can a growing community govern complexity without requiring the player to issue every job and transfer?

Candidate capabilities:

- Village offices, elders/council, market administration, guild prototypes, and estate management.
- Mandates, budgets, jurisdictions, permissions, and accountability.
- Delegated policy with visible objectives, discretion, outcomes, and failure reasons.
- Settlement projects and service allocation.
- Social status, obligations, and conflict at the minimum resolution that changes decisions.
- UI transition from direct household control to institutional oversight.

Exit gate:

- A settlement substantially larger than Prototype 0.1 remains playable through delegation.
- AI institutions use ordinary commands, goods, labor, and budgets.
- The player can inspect why delegated actors made consequential choices.
- Delegation reduces actions per unit of growth without hiding systemic failures.

## Phase 0.6 — Knowledge, education, and technological adoption

**Question:** Can a society acquire, apply, transmit, and lose capability without a conventional technology tree?

Candidate capabilities:

- Knowledge definitions with historical/geographic availability evidence.
- Personal theory/practice proficiency and workshop traditions.
- Apprenticeship, teaching, books/manuscripts, copying, and institutional preservation.
- Application prerequisites linking knowledge to tools, resources, sites, and experience.
- Experimentation and adaptation with explainable uncertainty.
- Knowledge loss, migration, captured artifacts, and foreign specialists.
- A first gradual technology transition with coexistence of old and new methods.

Exit gate:

- Capability spreads through explicit carriers and contact.
- Removing carriers can impair future practice without retroactively erasing existing objects.
- Calendar eligibility alone cannot unlock capability.
- AI can value and pursue knowledge without privileged unlocks.

## Phase 0.7 — Regional world, trade, and migration

**Question:** Can multiple settlements exchange people, goods, and knowledge while retaining credible off-screen causality?

Candidate capabilities:

- Regional spatial partitions and routes.
- Multiple settlements with simulation/detail budgets.
- Merchants, journeys, caravans/river routes as appropriate, risk, capacity, and travel time.
- Regional price and scarcity signals derived from actual exchange.
- Household migration journeys driven by land, work, safety, kinship, taxes, and politics.
- Cross-settlement disease/ecological concerns only if justified by design value and research.
- Data streaming and safe reactivation of cold settlements.

Exit gate:

- Goods and migrants have origin, journey, destination, and consequences.
- Off-screen processing conserves people and material quantities.
- Regional runs meet target time acceleration and save budgets.
- No settlement outcome depends on whether it happened to be rendered.

## Phase 0.8 — Polities, law, and historically distinct regions

**Question:** Can different political and cultural systems produce distinct play without predetermined destinies?

Candidate capabilities:

- Titles, offices, claims, jurisdiction, taxation, service, legitimacy, and succession.
- Franconian/Holy Roman political relationships expanded first.
- Additional research-backed test regions introduced one at a time: Teutonic/Baltic Frontier, Serbian/Balkan, and Ottoman.
- Region-specific institutions, agriculture, architecture, military traditions, starting resources, and knowledge.
- Diplomacy, vassalage/administration, religious authority, and legal pluralism at useful resolution.
- Cultural exchange, minorities, refugees, and hybrid practice handled without essentialist bonuses.

Exit gate:

- Political authority operates above and across settlements.
- At least two regional starts feel structurally different under common simulation laws.
- AI and player have equivalent political affordances at the same authority level.
- Long runs demonstrate divergent, non-scripted outcomes.

## Phase 0.9 — Warfare vertical slice

**Question:** Can readable competitive warfare emerge from people, equipment, training, leadership, terrain, morale, and logistics?

Candidate capabilities:

- Recruitment from actual people and institutions.
- Individual or safely grouped equipment linked to physical supply.
- Formation, command, training, experience, morale, fatigue, and terrain.
- Campaign movement, food, ammunition where relevant, replacement, and medical/casualty consequences.
- A narrow historically appropriate roster before broad content.
- Defensive sites and siege concepts only after field-combat foundations are proven.
- Competitive telemetry and AI using ordinary constraints.

Exit gate:

- Battles are strategically readable and materially connected to settlements.
- Casualties and equipment losses persist in households and economies.
- Outcomes are not decided by faction destiny modifiers.
- Performance and control remain viable at the agreed force scale.

## Phase 1.0 — Integrated generational campaign foundation

**Question:** Do the systems combine into a stable game capable of producing remembered alternate histories?

Candidate capabilities:

- Integrated settlement, regional economy, knowledge, politics, delegation, and warfare.
- Multi-decade and generational continuity.
- Historical event pressures conditioned on world state.
- Chronicle, lineage, provenance, and settlement-history presentation.
- Scenario authoring and validation tools.
- Formal save-compatibility policy, migration suite, performance budgets, and regression corpus.
- Player onboarding, accessibility foundations, and campaign UX.

Exit gate:

- A sustained campaign produces explainable, materially plausible divergent history.
- Old buildings, families, institutions, objects, and events remain meaningfully legible.
- Large-scale play relies on delegation without abandoning underlying causality.
- The project is ready for content scale and production planning rather than foundational redesign.

## Continuous workstreams

### Historical research

Research stays ahead of the implementation phase it informs. Maintain location/date scope, citations, confidence, contested interpretations, and design consequences. Consult domain specialists for high-impact or sensitive systems.

### Performance and scale

Maintain headless benchmark scenarios for small settlement, large settlement, region, and later military loads. Track simulation-step cost, memory, save size/time, path requests, planner work, event volume, and visual Actor counts separately.

### Persistence and reproducibility

Record schema versions from Prototype 0.1. Build representative saved-state fixtures at milestone gates. Keep scenario seed, authored-data version, and command history sufficient for defect reproduction.

### Testing

Use unit tests for domain rules, scenario tests for chains, invariant tests for conservation/references, migration tests for saves, performance tests for scale, and deterministic/reproduction tests where randomness matters.

### Player-facing explainability

Every system must budget for causal information and inspection. Debug views may precede polished interfaces, but unexplained state must not become normalized technical debt.

### Architecture and document stewardship

Record significant choices as ADRs. Review deferred decisions at their trigger phase. Update documentation in the same change as an approved interface or behavior change.

## Decision gates before major investment

The following are mandatory review moments:

- Before selecting the high-volume simulation storage model.
- Before committing the time-step and scheduling design.
- Before promising cross-version save compatibility.
- Before implementing regional/off-screen aggregation.
- Before implementing multiplayer/networking.
- Before adding a second playable region.
- Before production combat architecture.
- Before locking data formats for external content production.

At each gate, use prototype evidence, profiling, save implications, and constitutional fit rather than convenience alone.

## Roadmap change policy

The Creative Director may reorder or remove phases. A proposed change must identify dependency consequences, abandoned evidence, document updates, and any temporary constitutional waiver. Roadmap movement does not itself authorize production code; implementation begins only on an explicitly approved scope.
