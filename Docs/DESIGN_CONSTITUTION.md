# Design Constitution

## Purpose and authority

This document defines the non-negotiable design laws of the project. All feature specifications, architecture decisions, content, code, tests, and production plans must comply with it.

When documents conflict, authority is:

1. The Creative Director's explicit decision.
2. This constitution.
3. Ratified architecture decision records (ADRs) and the high-level architecture.
4. Approved feature and prototype specifications.
5. Implementation details and existing code behavior.

Existing code is not authority merely because it exists. A temporary prototype shortcut does not silently amend this constitution.

The terms **MUST**, **MUST NOT**, **SHOULD**, and **MAY** describe requirement strength. Any exception to a MUST requires an explicit, documented, time-bounded waiver.

## Constitutional laws

### DC-01 — People are sourced, persistent entities

- A person MUST enter the simulation through a traceable process such as birth, immigration, scenario initialization, or an explicitly modeled exceptional event.
- Buildings MUST NOT generate population.
- Relevant identity and continuity—origin, age, household, relationships, occupation, residence, skills, and possessions—MUST survive presentation changes and ordinary simulation scaling.
- Aging, death, household formation, and migration MUST be supported by the long-term model, even when a prototype implements only a subset.

### DC-02 — Households are the primary domestic unit

- Residence, ordinary consumption, family wealth, and property occupation SHOULD resolve through households unless the design documents a clear exception.
- A household MUST refer to actual members and an actual residence or an explicit homeless/transient state.
- Settlement population MUST be derivable from people, not stored as an unrelated authoritative number.

### DC-03 — Land and property are finite and spatial

- Farming, forestry, extraction, building, roads, and settlement expansion MUST consume or govern physical space.
- Property boundaries, access, and dimensions MUST be capable of affecting use.
- A building MUST occupy a site; it cannot serve solely as a UI unlock detached from the world.

### DC-04 — Material goods are conserved and located

- Physical goods MUST have quantities, locations, and custody.
- Production MUST consume located inputs and create located outputs according to declared rules.
- Transfers MUST take time and require a valid path and carrying capacity whenever distance is materially relevant.
- Global resource counters MUST NOT serve as the authoritative store for physical goods.
- Creation, transformation, loss, spoilage, consumption, and destruction MUST be auditable in development builds.

### DC-05 — Knowledge has provenance and carriers

- Techniques MUST NOT unlock solely because a date arrived or abstract research points were spent.
- Knowledge MUST be held or conveyed by people, practice, objects, texts, institutions, or other explicit carriers.
- Adoption MUST be distinguishable from awareness: knowing that something exists is not the same as being able to make or use it reliably.
- Knowledge transmission and loss MUST be possible.

### DC-06 — The calendar permits; it does not grant

- Time advances continuously through a historical calendar.
- Dates MAY gate plausibility or external availability but MUST NOT automatically modernize a society.
- There MUST be no universal age transition that upgrades all buildings, units, or capabilities.

### DC-07 — Change is gradual, path-dependent, and visible

- Old and new techniques MAY coexist.
- Adoption SHOULD account for skills, tools, resources, infrastructure, institutions, cost, and experience.
- Existing buildings MUST NOT automatically restyle or upgrade when time or capability changes.
- Replacement, conversion, decay, repair, and extension SHOULD leave persistent evidence where feasible.

### DC-08 — History emerges after initialization

- The starting world SHOULD be historically grounded around 1370.
- Outcomes MUST be produced by simulation and action rather than protected historical scripts.
- Historical pressures MAY occur, but their participants, timing, reach, and outcomes MUST respond to current world state.
- No polity or culture may have an inviolable rise, decline, conquest, or survival path.

### DC-09 — Cultures differ through circumstances and institutions

- Regional identity SHOULD arise from geography, architecture, crops, laws, institutions, military traditions, trade, resources, and starting knowledge.
- Cultural designs MUST NOT reduce primarily to reskinned universal content plus arbitrary percentage bonuses.
- Cross-cultural learning, migration, adaptation, and hybridization MUST remain possible.

### DC-10 — Political authority is modeled

- Settlements and rulers MUST be able to exist inside larger political structures.
- Rights and obligations SHOULD distinguish ownership, jurisdiction, title, office, command, and influence.
- Expansion of player authority SHOULD occur through simulated political or military processes, not only by reaching population thresholds.

### DC-11 — Scale through scheduling and delegation

- The simulation MUST be designed for thousands of persistent people without requiring every person to execute expensive logic every frame.
- Decision frequency, simulation resolution, and visual representation MAY vary by relevance.
- Scaling MUST preserve consequential state and conservation laws.
- Institutions and delegated roles SHOULD reduce player micromanagement as scope grows.

### DC-12 — Decisions and presentation do not own truth

- Authoritative simulation state MUST be separate from Unreal visual actors, animation state, UI widgets, and AI planning state.
- Visual entities MUST reflect simulation state and MUST NOT become the sole store of identity, ownership, inventory, or economic truth.
- AI and UI MUST request changes through validated simulation commands rather than mutating authoritative records directly.

### DC-13 — Systems must remain causally legible

- Important outcomes MUST expose enough information to identify their principal causes.
- Hidden complexity that produces no meaningful choice, story, or consequence SHOULD be removed or aggregated.
- Randomness that affects important outcomes SHOULD be seeded, bounded, and explainable in development tools.

### DC-14 — Competitive warfare follows common rules

- Human and AI-controlled societies MUST use the same authoritative economic and military rules except for explicitly documented accessibility or difficulty assistance.
- Combat effectiveness SHOULD arise from equipment, training, morale, leadership, logistics, terrain, formation, experience, and productive capacity.
- Historical asymmetry is permitted; predetermined historical victory is not.

### DC-15 — The world retains provenance

- Persistent entities SHOULD record origins and major state transitions sufficient to construct useful histories.
- Goods, knowledge, structures, households, and institutions SHOULD preserve provenance at the coarsest level that still supports gameplay and explanation.
- Save/load MUST preserve identity and consequential history within the declared compatibility policy.

### DC-16 — Scope is a design constraint

- Features MUST demonstrate value at a controlled scale before they expand.
- Prototype-specific simplifications MUST be labeled and isolated behind concepts compatible with the intended architecture.
- A future system MUST NOT be implemented merely because the architecture acknowledges that it will eventually exist.

## Simulation integrity rules

The authoritative simulation must maintain these invariants:

1. Every persistent entity has a stable identifier independent of its Unreal object lifetime.
2. Every physical good is in exactly one valid custody/location state at a time, including in transit, consumed, lost, or destroyed.
3. A person has at most one primary household at a time and an explicit life-state.
4. A building has one spatial site and declared access relationships.
5. A field has defined geometry/area and cannot be cultivated as two overlapping authoritative fields without an explicit shared-use model.
6. Commands are validated before state changes; rejected commands leave state unchanged.
7. State changes significant to other systems emit typed domain events or an equivalent auditable record.
8. Cached totals are derived data and can be rebuilt from their authoritative sources.

## Historical evidence standard

Historical claims used to define content or rules should be recorded with:

- Geographic and chronological scope.
- Source citation and source type.
- Confidence level: well attested, plausible inference, contested, or gameplay abstraction.
- The specific design consequence drawn from the evidence.

Research should prefer primary evidence and reputable modern scholarship. A claim that is true in one place or decade must not silently become universal medieval behavior.

## Prototype exception standard

A prototype may simplify an intended system only when the specification states:

- What is simplified.
- Why the simplification is necessary.
- Which constitutional behavior remains protected.
- What would trigger replacement or expansion.
- How prototype data avoids blocking the future model.

For example, fixed prices may be used to test physical market delivery, but goods must still be located, ownership transfers must still be explicit, and the fixed-price rule must not become an undocumented permanent economy.

## Amendment and architectural review

Changes to a constitutional law require Creative Director approval and a recorded rationale. The amendment must identify affected documents, saved data, interfaces, tests, and migration work.

Architectural review is required before a change that:

- Moves authoritative ownership between layers or systems.
- Replaces stable identity or persistence strategy.
- Introduces a global manager with cross-domain mutation rights.
- Changes time-stepping, spatial partitioning, simulation level of detail, or determinism policy.
- Breaks a public interface, serialized schema, event contract, or data asset format.
- Adds a second source of truth for people, goods, land, ownership, or time.
- Makes a prototype shortcut part of the long-term architecture.

## Design review questions

Before approving a feature, ask:

1. What physical, social, legal, or informational entity causes it?
2. Where does its authoritative state live?
3. Which commands may change that state?
4. What does it consume, create, move, teach, or remember?
5. Can a player understand why its outcome occurred?
6. What happens when its people die, goods are delayed, building is lost, or institution fails?
7. How does it behave off-screen and at ten times the population?
8. Is the historical claim appropriately scoped?
9. Does it preserve unscripted futures?
10. Is this the smallest feature that proves the intended value?
