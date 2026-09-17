#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/** Whether an audited mutation brought simulation quantity into existence or ended it. */
enum class EGoodsAuditAction : uint8
{
	/** Quantity that did not previously exist was added to an inventory. */
	Created,

	/** Quantity that existed was removed from an inventory and ceased to exist. */
	Destroyed
};

/**
 * Development-authoritative record of one successful creation or destruction of goods.
 *
 * DC-04 requires that creation, loss, and destruction of physical goods be auditable in
 * development builds. AddGoods and RemoveGoods are the only operations that can create or
 * destroy quantity, so they are the only operations that append one of these.
 *
 * Transfers deliberately produce no audit record. A transfer conserves quantity, so nothing
 * was created or destroyed and recording one would misreport movement as creation.
 *
 * This is not an event-sourcing log and not a save format. It carries no calendar or world
 * time, no lot, no recipe, no worker, no Actor, no owner, and no value; the simulation clock
 * that would give an entry a date does not exist yet.
 */
struct FGoodsAuditRecord
{
	/** Whether quantity was created or destroyed. */
	EGoodsAuditAction Action = EGoodsAuditAction::Created;

	/** Inventory whose contents changed. */
	FInventoryId InventoryId;

	/** Good type whose quantity changed. */
	FGoodTypeId GoodTypeId;

	/** Amount created or destroyed. Always greater than zero. */
	int32 Quantity = 0;

	/**
	 * Why the caller created or destroyed the quantity, supplied at the call site. Never
	 * None: authoritative quantity cannot appear or disappear anonymously.
	 *
	 * An FName keeps a reason comparable in tests and cheap to store while staying open to
	 * whatever future domain systems need to say. No reason is defined in production code,
	 * so this does not encode a fixed list of future systems.
	 */
	FName Reason;
};
