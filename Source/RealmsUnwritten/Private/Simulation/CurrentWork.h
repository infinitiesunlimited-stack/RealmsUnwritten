#pragma once

#include "CoreMinimal.h"

#include "Simulation/SimulationIds.h"

/**
 * Kind of target a person's current work commitment is aimed at.
 *
 * Exactly two kinds exist, and the list is deliberately short. Stationary physical-site
 * work is the only supported target in Prototype 0.1E. A future target - a field, a road,
 * a mobile holder, military service - becomes its own named kind with its own typed
 * identifier field, decided when that target is actually built. It does not become a
 * generic identifier with a runtime tag, which is why this is an explicit domain
 * enumeration rather than a variant or an untyped handle.
 *
 * This is not occupation, not presence, and not a job-board posting.
 */
enum class ECurrentWorkKind : uint8
{
	/** The person currently has no work commitment. */
	None,

	/** The person's current work is targeted at a stationary physical site. */
	PhysicalSite
};

/**
 * A person's authoritative current work commitment: what they are assigned to do, and
 * where that work is targeted.
 *
 * This is labor state, not presence. Kind PhysicalSite naming a site means the commitment
 * is aimed at that place. It does not mean the person is physically there, has walked
 * there, has arrived, is animating work, or is within any distance. Movement and presence
 * are later systems.
 *
 * The kind and the identifiers belong together, so they are set together through the
 * factory functions below rather than assigned field by field. A commitment either names a
 * kind and carries the matching identifiers, or it is nowhere and carries none; any other
 * combination is invalid state and is reported by ValidateInvariants.
 *
 * A person holds at most one of these. Reassignment replaces it. Occupation is a separate
 * unimplemented concept and is not stored here.
 */
struct FCurrentWork
{
	/** The current work of a person who has none. */
	static FCurrentWork Nowhere() { return FCurrentWork(); }

	/** Current work of a given type targeted at one stationary physical site. */
	static FCurrentWork AtPhysicalSite(FWorkTypeId WorkTypeId, FPhysicalSiteId PhysicalSiteId)
	{
		FCurrentWork CurrentWork;
		CurrentWork.Kind = ECurrentWorkKind::PhysicalSite;
		CurrentWork.WorkTypeId = WorkTypeId;
		CurrentWork.PhysicalSiteId = PhysicalSiteId;

		return CurrentWork;
	}

	/** Whether the person currently has a work commitment at all. */
	bool IsAssigned() const { return Kind != ECurrentWorkKind::None; }

	/** Which kind of target the current work is aimed at. */
	ECurrentWorkKind Kind = ECurrentWorkKind::None;

	/** Work type of the commitment. Valid and resolvable when Kind is PhysicalSite, unset otherwise. */
	FWorkTypeId WorkTypeId;

	/** Site the commitment is targeted at. Valid and resolvable when Kind is PhysicalSite, unset otherwise. */
	FPhysicalSiteId PhysicalSiteId;
};
