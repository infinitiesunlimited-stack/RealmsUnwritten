#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Authoritative record for one settlement.
 *
 * A settlement is a grouping scope, not a container that owns local state. Its two lists
 * are registry-maintained indexes over the authoritative back-references
 * FPropertyRecord::SettlementId and FHouseholdRecord::SettlementId. They exist so that
 * "which properties or households belong here" never requires scanning every record, and
 * they can be rebuilt from those fields.
 *
 * This record is plain simulation data: no Actor, no UObject, no geometry, no extent, and
 * no tick. Names are display identity only; FSettlementId is the sole identity.
 */
struct FSettlementRecord
{
	/** Stable identity assigned at creation. */
	FSettlementId Id;

	/** Display name for the settlement. Never an identifier. */
	FString Name;

	/** Properties belonging to this settlement. Contains no duplicates. */
	TArray<FPropertyId> Properties;

	/**
	 * Households currently located in this settlement, whether or not they occupy a
	 * property. Contains no duplicates.
	 */
	TArray<FHouseholdId> Households;
};
