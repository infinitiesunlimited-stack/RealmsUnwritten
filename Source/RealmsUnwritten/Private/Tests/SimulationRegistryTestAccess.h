#pragma once

#include "Simulation/SimulationRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test-only access to the registry's private storage, befriended by FSimulationRegistry
 * under WITH_DEV_AUTOMATION_TESTS.
 *
 * It exists for one purpose: proving that ValidateInvariants actually detects corrupted
 * state. The public operations correctly make invalid state unreachable, so the only
 * alternative would have been a production API for corrupting the registry.
 *
 * Compiled out of shipping builds. Used by more than one test translation unit, which is
 * why it lives in this header rather than inside a single test file.
 */
struct FSimulationRegistryTestAccess
{
	static TArray<FGoodTypeRecord>& GoodTypeRecords(FSimulationRegistry& Registry)
	{
		return Registry.GoodTypeRecords;
	}

	static TArray<FWorkTypeRecord>& WorkTypeRecords(FSimulationRegistry& Registry)
	{
		return Registry.WorkTypeRecords;
	}

	static TArray<FSkillTypeRecord>& SkillTypeRecords(FSimulationRegistry& Registry)
	{
		return Registry.SkillTypeRecords;
	}

	static TArray<FActivityTypeRecord>& ActivityTypeRecords(FSimulationRegistry& Registry)
	{
		return Registry.ActivityTypeRecords;
	}

	static TArray<FTaskTypeRecord>& TaskTypeRecords(FSimulationRegistry& Registry)
	{
		return Registry.TaskTypeRecords;
	}

	static TArray<FTaskRecord>& TaskRecords(FSimulationRegistry& Registry)
	{
		return Registry.TaskRecords;
	}

	static TArray<FPersonCapabilityRecord>& PersonCapabilityRecords(FSimulationRegistry& Registry)
	{
		return Registry.PersonCapabilityRecords;
	}

	static TArray<FInventoryRecord>& InventoryRecords(FSimulationRegistry& Registry)
	{
		return Registry.InventoryRecords;
	}

	static TArray<FPersonRecord>& PersonRecords(FSimulationRegistry& Registry)
	{
		return Registry.PersonRecords;
	}

	static TArray<FPhysicalSiteRecord>& PhysicalSiteRecords(FSimulationRegistry& Registry)
	{
		return Registry.PhysicalSiteRecords;
	}

	static TArray<FPropertyRecord>& PropertyRecords(FSimulationRegistry& Registry)
	{
		return Registry.PropertyRecords;
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
