#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one kind of physical good.
 *
 * This is a definition-shaped record: it describes a kind of thing, not a thing. Quantities
 * of it live in inventories, never here, and never in a counter on a settlement, household,
 * or property.
 *
 * A good type carries two identifiers, and the distinction between them is deliberate:
 *
 *   AuthoredKey  durable definition identity. Chosen by whoever defines the good, never by
 *                the registry, so it does not depend on insertion order, slot position, or
 *                load order. This is the identity that content refers to and that a future
 *                save format would record.
 *   Id           runtime handle. A dense slot-derived FGoodTypeId, cheap to store and
 *                compare, used by inventory entries for the lifetime of one registry. It is
 *                not durable: the same authored key may map to a different Id in a different
 *                run, because it depends on the order good types were created.
 *
 * No good type is hard-coded. Wheat, flour, and bread are created at runtime like any other,
 * so authored content can define good types later without a new C++ enum per resource.
 *
 * Display names are neither identity: two good types with different authored keys may share
 * a display name, while a duplicate authored key is rejected outright.
 *
 * Unit of measure, weight, volume, quality, perishability, price, category, stack size, and
 * process references are all part of the eventual resource definition and are deliberately
 * absent here (see Docs/Systems/GOODS_INVENTORY.md).
 */
struct FGoodTypeRecord
{
	/**
	 * Durable definition identity, unique across good types and independent of creation
	 * order. Never None.
	 */
	FName AuthoredKey;

	/** Runtime handle assigned at creation. Stable within one registry, not across runs. */
	FGoodTypeId Id;

	/** Display name for the good type. Never an identifier. */
	FString Name;
};
