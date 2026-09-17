#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one property: a persistent place or holding within a settlement,
 * capable of later carrying physical improvements and land uses.
 *
 * A property is not a building. This slice gives it no geometry, area, address, soil, value,
 * improvements, or owner - only where it belongs and who lives on it. Occupancy is not
 * ownership; no ownership, tenancy, or rent is modelled here.
 *
 * This record is plain simulation data: no Actor, no UObject, no mesh, and no tick.
 */
struct FPropertyRecord
{
	/** Stable identity assigned at creation. */
	FPropertyId Id;

	/**
	 * Settlement this property belongs to. Always valid: creation requires a resolvable
	 * settlement, and this slice provides no operation that moves a property.
	 */
	FSettlementId SettlementId;

	/**
	 * Household occupying this property as its residence, or an invalid identifier when the
	 * property is unoccupied. At most one household at a time.
	 */
	FHouseholdId ResidentHouseholdId;

	/**
	 * Physical sites that are part of this property. Contains no duplicates.
	 *
	 * A registry-maintained index over the authoritative back-reference
	 * FPhysicalSiteRecord::PropertyId, like the settlement's lists. A property may have no
	 * sites at all, one, or many.
	 */
	TArray<FPhysicalSiteId> PhysicalSites;
};
