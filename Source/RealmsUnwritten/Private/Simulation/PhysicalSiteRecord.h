#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one physical site: a stationary place on a property at which
 * physical things may exist.
 *
 * A site is where something is, and nothing more. It is not a building, a room, a container,
 * a warehouse, a field, a coordinate, or an Actor, and it provides no storage capacity, no
 * environment, and no access rules. A building will eventually stand on a property and offer
 * several sites; a site does not imply that any building exists.
 *
 * Three concerns stay separate and this record answers only the first:
 *
 *   Site       where is it?
 *   Inventory  what goods are present?
 *   Ownership  who owns or has a claim on them?  (does not exist yet)
 *
 * This record is plain simulation data: no Actor, no UObject, no mesh, no geometry, and no
 * tick. See Docs/Systems/PHYSICAL_SITE_INVENTORY_LOCATION.md.
 */
struct FPhysicalSiteRecord
{
	/** Stable identity assigned at creation. */
	FPhysicalSiteId Id;

	/**
	 * Property this site is part of. Always valid: creation requires a resolvable property,
	 * and this slice provides no operation that moves a site to another property.
	 */
	FPropertyId PropertyId;

	/**
	 * What the site is for, as classifying metadata. Never None.
	 *
	 * This is not a definition identity in the sense of FGoodTypeRecord::AuthoredKey: it
	 * classifies an instantiated place rather than identifying it, so several sites may
	 * legitimately share one purpose key. Two barn storage sites on two properties are two
	 * distinct sites that are alike, not one site named twice. No purpose key is defined in
	 * production code, and there is no enumeration of site types.
	 */
	FName PurposeKey;

	/** Display name for the site. Never an identifier. */
	FString DisplayName;

	/**
	 * Inventories physically present at this site. Contains no duplicates.
	 *
	 * A registry-maintained index over the authoritative back-reference
	 * FInventoryRecord::Location, in the same way a settlement's property list indexes
	 * FPropertyRecord::SettlementId. It can be rebuilt from the inventory records. A site may
	 * hold several inventories; an inventory is at no more than one site.
	 */
	TArray<FInventoryId> Inventories;
};
