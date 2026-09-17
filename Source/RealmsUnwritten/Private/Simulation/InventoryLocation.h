#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Kind of place an inventory physically exists at.
 *
 * Exactly two kinds exist, and the list is deliberately short. A future mobile holder - a
 * person carrying goods, a cart, a pack animal, a ship - becomes its own named kind with its
 * own typed identifier field, decided when that holder type is actually built. It does not
 * become a generic identifier with a runtime tag, which is why this is an explicit domain
 * enumeration rather than a variant or an untyped holder handle.
 */
enum class EInventoryLocationKind : uint8
{
	/**
	 * The inventory is nowhere. Permitted only while it holds no goods: an inventory may be
	 * created before its place is known, but nothing may be put into it until it has one.
	 */
	None,

	/** The inventory exists at a stationary physical site. */
	PhysicalSite
};

/**
 * Where one inventory physically is: the authoritative answer to "where does this inventory
 * exist?", and through it, where the goods inside it are.
 *
 * The kind and the identifier belong together, so they are set together through the factory
 * functions below rather than assigned field by field. A location either names a kind and
 * carries the matching identifier, or it is nowhere and carries none; any other combination
 * is invalid state and is reported by ValidateInvariants.
 *
 * This says nothing about ownership, capacity, environment, or access. A site holding an
 * inventory is not its owner, and moving an inventory between sites is a state change rather
 * than a journey.
 */
struct FInventoryLocation
{
	/** The location of an inventory that is nowhere. */
	static FInventoryLocation Nowhere() { return FInventoryLocation(); }

	/** The location of an inventory that exists at one stationary physical site. */
	static FInventoryLocation AtPhysicalSite(FPhysicalSiteId PhysicalSiteId)
	{
		FInventoryLocation Location;
		Location.Kind = EInventoryLocationKind::PhysicalSite;
		Location.PhysicalSiteId = PhysicalSiteId;

		return Location;
	}

	/** Whether the inventory has a place at all. */
	bool IsLocated() const { return Kind != EInventoryLocationKind::None; }

	/** Which kind of place the inventory is at. */
	EInventoryLocationKind Kind = EInventoryLocationKind::None;

	/** Site the inventory is at. Valid and resolvable when Kind is PhysicalSite, unset otherwise. */
	FPhysicalSiteId PhysicalSiteId;
};
