#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Largest quantity of a single good type that one inventory may hold.
 *
 * Quantities are int32. Signed, because the mutation API must be able to receive a negative
 * request and reject it rather than have it wrap into an enormous positive one at the call
 * boundary. The limit is the type's own maximum, so every arithmetic check is a headroom
 * comparison performed before the addition rather than an overflow detected after it.
 */
inline constexpr int32 MaxGoodQuantity = MAX_int32;

/**
 * One good type and how much of it an inventory holds.
 *
 * Quantity is always strictly positive: an entry exists only while the inventory holds some
 * of that good, and reaching zero removes the entry rather than storing a zero.
 */
struct FInventoryEntry
{
	/** Good type held. Resolvable, and appears at most once per inventory. */
	FGoodTypeId GoodTypeId;

	/** Amount held, in abstract integral simulation units. Always greater than zero. */
	int32 Quantity = 0;
};

/**
 * Authoritative record for one inventory: a custody boundary that holds quantities of goods.
 *
 * An inventory has no holder, site, location, owner, capacity, or reservation in this slice.
 * It exists independently, which is deliberate: the eventual holders (buildings, fields,
 * markets, vehicles, households, people) do not exist yet, and attaching the inventory to a
 * property or household now would prejudge that model. This is a labeled prototype
 * exception, documented in Docs/Systems/GOODS_INVENTORY.md.
 *
 * What is already protected: goods exist only inside an explicit inventory, so no settlement
 * or household carries a resource counter, and every quantity is in exactly one inventory.
 *
 * This record is plain simulation data: no Actor, no UObject, no mesh, and no tick.
 */
struct FInventoryRecord
{
	/** Stable identity assigned at creation. */
	FInventoryId Id;

	/**
	 * Goods held. Contains no duplicate good types, no unresolvable good types, and no
	 * non-positive quantities. Order is insertion order and is not part of the contract.
	 */
	TArray<FInventoryEntry> Entries;
};
