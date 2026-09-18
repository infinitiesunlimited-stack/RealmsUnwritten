#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for the authoritative work-type and person current-work foundation.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Work types are created at runtime under authored keys, never taken from a C++ enum. The
 * keys used here, `Work.TestHarvest` and the rest, are test data only and appear in no
 * production code. Occupation is not implemented and is not tested.
 *
 * Current work targeted at a physical site is a labor commitment, not proof of physical
 * presence, travel, or arrival.
 *
 * The small helpers below are duplicated from the accepted test files rather than shared,
 * so that those files stay untouched.
 */

namespace
{
	constexpr EAutomationTestFlags SimulationTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	constexpr uint32 Int32MaxIdValue = 0x7FFFFFFFu;
	constexpr uint32 SignBitIdValue = 0x80000000u;
	constexpr uint32 UInt32MaxIdValue = 0xFFFFFFFFu;

	FPersonCreationParams MakePersonParams(const TCHAR* GivenName, const TCHAR* FamilyName, int32 AgeYears)
	{
		FPersonCreationParams Params;
		Params.GivenName = GivenName;
		Params.FamilyName = FamilyName;
		Params.AgeYears = AgeYears;

		return Params;
	}

	FPhysicalSiteId CreateTestSite(FSimulationRegistry& Registry, const TCHAR* DisplayName = TEXT("Barn"))
	{
		const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
		const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
		return Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), DisplayName);
	}

	void VerifyInvariants(FAutomationTestBase& Test, const FSimulationRegistry& Registry, const TCHAR* Context)
	{
		FString FailureDescription;
		if (!Registry.ValidateInvariants(FailureDescription))
		{
			Test.AddError(FString::Printf(TEXT("Invariant broken %s: %s"), Context, *FailureDescription));
		}
	}

	void VerifyInvariantsDetectFailure(
		FAutomationTestBase& Test, const FSimulationRegistry& Registry, const TCHAR* Context)
	{
		FString FailureDescription;
		if (Registry.ValidateInvariants(FailureDescription))
		{
			Test.AddError(FString::Printf(TEXT("Invariant validation failed to detect %s"), Context));
			return;
		}

		Test.TestFalse(FString::Printf(TEXT("The detected problem (%s) is described"), Context),
			FailureDescription.IsEmpty());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkTypeCreationTest,
	"RealmsUnwritten.Simulation.WorkType.Creation", SimulationTestFlags)

bool FSimulationWorkTypeCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	TestFalse(TEXT("A work type cannot be created without an authored key"),
		Registry.CreateWorkType(NAME_None, TEXT("Nameless")).IsValid());
	TestEqual(TEXT("A rejected creation stores no work type"), Registry.GetWorkTypeCount(), 0);

	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	TestEqual(TEXT("The rejected nameless work type consumed no runtime handle"),
		Harvest.GetValue(), 1u);
	const FWorkTypeId Repair = Registry.CreateWorkType(TEXT("Work.TestRepair"), TEXT("Repair"));
	TestTrue(TEXT("A work type with an authored key is created"), Harvest.IsValid());
	TestTrue(TEXT("A second work type has a distinct runtime handle"), Harvest != Repair);
	TestEqual(TEXT("Both work types are stored"), Registry.GetWorkTypeCount(), 2);

	const TOptional<FWorkTypeRecord> HarvestRecord = Registry.FindWorkType(Harvest);
	if (!HarvestRecord.IsSet())
	{
		AddError(TEXT("A created work type should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("A work type's record reports its own runtime handle"), HarvestRecord->Id == Harvest);
	TestTrue(TEXT("A work type's record reports its authored key"),
		HarvestRecord->AuthoredKey == FName(TEXT("Work.TestHarvest")));
	TestEqual(TEXT("Stored display name matches creation"), HarvestRecord->Name, FString(TEXT("Harvest")));

	const TOptional<FWorkTypeId> ResolvedHarvest = Registry.FindWorkTypeIdByKey(TEXT("Work.TestHarvest"));
	const TOptional<FWorkTypeId> ResolvedRepair = Registry.FindWorkTypeIdByKey(TEXT("Work.TestRepair"));
	if (!ResolvedHarvest.IsSet() || !ResolvedRepair.IsSet())
	{
		AddError(TEXT("An authored key should resolve to its runtime handle"));
		return false;
	}

	TestTrue(TEXT("The authored key resolves to the harvest handle"), ResolvedHarvest.GetValue() == Harvest);
	TestTrue(TEXT("The authored key resolves to the repair handle"), ResolvedRepair.GetValue() == Repair);
	TestFalse(TEXT("An unknown authored key resolves to nothing"),
		Registry.FindWorkTypeIdByKey(TEXT("Work.TestHaul")).IsSet());
	TestFalse(TEXT("No authored key resolves to nothing"), Registry.FindWorkTypeIdByKey(NAME_None).IsSet());
	TestFalse(TEXT("A display name is not an authored key"),
		Registry.FindWorkTypeIdByKey(TEXT("Harvest")).IsSet());

	const int32 WorkTypeCountBeforeDuplicate = Registry.GetWorkTypeCount();
	const FWorkTypeId DuplicateKey = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Different Harvest"));
	TestFalse(TEXT("A duplicate authored key is rejected"), DuplicateKey.IsValid());
	TestEqual(TEXT("A duplicate authored key stores no work type"),
		Registry.GetWorkTypeCount(), WorkTypeCountBeforeDuplicate);

	const FWorkTypeId Haul = Registry.CreateWorkType(TEXT("Work.TestHaul"), TEXT("Haul"));
	TestTrue(TEXT("The next work type is created"), Haul.IsValid());
	TestEqual(TEXT("The rejected duplicate left no gap in the runtime handles"),
		(int32)Haul.GetValue(), WorkTypeCountBeforeDuplicate + 1);

	const FWorkTypeId OtherHarvest = Registry.CreateWorkType(TEXT("Work.TestOtherHarvest"), TEXT("Harvest"));
	TestTrue(TEXT("A duplicate display name under a new authored key is allowed"), OtherHarvest.IsValid());
	TestTrue(TEXT("The duplicate display name received its own runtime handle"), OtherHarvest != Harvest);

	static_assert(!SimulationIdContract::bInterchangeable<FGoodTypeId, FWorkTypeId>,
		"Good-type and work-type identifiers must remain distinct types.");
	const FName SharedWheatKey(TEXT("Wheat"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(SharedWheatKey, TEXT("Wheat"));
	const FWorkTypeId WorkWheat = Registry.CreateWorkType(SharedWheatKey, TEXT("Wheat"));
	TestTrue(TEXT("A good type may use the shared authored key"), Wheat.IsValid());
	TestTrue(TEXT("A work type may use the exact same authored key"), WorkWheat.IsValid());
	TestTrue(TEXT("The Good Type registry independently resolves the shared key"),
		Registry.FindGoodTypeIdByKey(SharedWheatKey).IsSet()
			&& Registry.FindGoodTypeIdByKey(SharedWheatKey).GetValue() == Wheat);
	TestTrue(TEXT("The Work Type registry independently resolves the shared key"),
		Registry.FindWorkTypeIdByKey(SharedWheatKey).IsSet()
			&& Registry.FindWorkTypeIdByKey(SharedWheatKey).GetValue() == WorkWheat);

	FSimulationRegistry ReorderedRegistry;
	const FWorkTypeId ReorderedRepair = ReorderedRegistry.CreateWorkType(TEXT("Work.TestRepair"), TEXT("Repair"));
	const FWorkTypeId ReorderedHarvest = ReorderedRegistry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	TestTrue(TEXT("Creation order changes which runtime handle an authored key maps to"),
		ReorderedHarvest != Harvest && ReorderedRepair != Repair);
	TestTrue(TEXT("The authored key still resolves after a different creation order"),
		ReorderedRegistry.FindWorkTypeIdByKey(TEXT("Work.TestHarvest")).GetValue() == ReorderedHarvest);
	TestTrue(TEXT("The authored key identifies the same definition in both registries"),
		ReorderedRegistry.FindWorkType(ReorderedHarvest)->AuthoredKey == HarvestRecord->AuthoredKey);

	const uint32 UnresolvableValues[] = {
		0u, WorkWheat.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FWorkTypeId WorkTypeId = FWorkTypeId(UnresolvableValue);
		TestFalse(FString::Printf(TEXT("Work type handle %u does not resolve"), UnresolvableValue),
			Registry.ContainsWorkType(WorkTypeId));
		TestFalse(FString::Printf(TEXT("Work type handle %u cannot be read"), UnresolvableValue),
			Registry.FindWorkType(WorkTypeId).IsSet());
	}

	VerifyInvariants(*this, Registry, TEXT("after creating work types"));
	VerifyInvariants(*this, ReorderedRegistry, TEXT("after creating reordered work types"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkAssignmentTest,
	"RealmsUnwritten.Simulation.Work.Assignment", SimulationTestFlags)

bool FSimulationWorkAssignmentTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);

	TestTrue(TEXT("A created person starts with no work"),
		Registry.FindPerson(PersonId)->CurrentWork.Kind == ECurrentWorkKind::None);
	TestFalse(TEXT("A created person starts unassigned"),
		Registry.FindPerson(PersonId)->CurrentWork.IsAssigned());
	TestFalse(TEXT("An unhoused person has no household"),
		Registry.FindPerson(PersonId)->HouseholdId.IsValid());

	TestTrue(TEXT("An unhoused person may be assigned work"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteId) == EPersonWorkResult::Success);

	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
	if (!PersonRecord.IsSet())
	{
		AddError(TEXT("The assigned person should resolve"));
		return false;
	}

	TestTrue(TEXT("Assigned work is a physical-site commitment"),
		PersonRecord->CurrentWork.Kind == ECurrentWorkKind::PhysicalSite);
	TestTrue(TEXT("Assigned work names the work type"), PersonRecord->CurrentWork.WorkTypeId == Harvest);
	TestTrue(TEXT("Assigned work names the site"), PersonRecord->CurrentWork.PhysicalSiteId == SiteId);
	TestTrue(TEXT("Assigned work reports as assigned"), PersonRecord->CurrentWork.IsAssigned());
	TestFalse(TEXT("Assignment does not house the person"), PersonRecord->HouseholdId.IsValid());

	VerifyInvariants(*this, Registry, TEXT("after assigning work to an unhoused person"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkAssignmentRejectionTest,
	"RealmsUnwritten.Simulation.Work.AssignmentRejection", SimulationTestFlags)

bool FSimulationWorkAssignmentRejectionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	TestTrue(TEXT("The person is assigned before rejection cases"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteId) == EPersonWorkResult::Success);

	const uint32 UnresolvableValues[] = { 0u, 99u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestTrue(FString::Printf(TEXT("Assigning work for person %u is rejected"), UnresolvableValue),
			Registry.AssignPersonWork(FPersonId(UnresolvableValue), Harvest, SiteId)
				== EPersonWorkResult::UnknownPerson);
		TestTrue(FString::Printf(TEXT("Assigning unknown work type %u is rejected"), UnresolvableValue),
			Registry.AssignPersonWork(PersonId, FWorkTypeId(UnresolvableValue), SiteId)
				== EPersonWorkResult::UnknownWorkType);
		TestTrue(FString::Printf(TEXT("Assigning work at unknown site %u is rejected"), UnresolvableValue),
			Registry.AssignPersonWork(PersonId, Harvest, FPhysicalSiteId(UnresolvableValue))
				== EPersonWorkResult::UnknownSite);
	}

	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
	if (!PersonRecord.IsSet())
	{
		AddError(TEXT("The person should still resolve after rejected reassignment"));
		return false;
	}

	TestTrue(TEXT("Rejected reassignment left the work type unchanged"),
		PersonRecord->CurrentWork.WorkTypeId == Harvest);
	TestTrue(TEXT("Rejected reassignment left the site unchanged"),
		PersonRecord->CurrentWork.PhysicalSiteId == SiteId);
	TestTrue(TEXT("Rejected reassignment left the kind unchanged"),
		PersonRecord->CurrentWork.Kind == ECurrentWorkKind::PhysicalSite);

	TestTrue(TEXT("Removing work for an unknown person is rejected"),
		Registry.RemovePersonWork(FPersonId(UInt32MaxIdValue)) == EPersonWorkResult::UnknownPerson);
	TestTrue(TEXT("A rejected removal left the person assigned"),
		Registry.FindPerson(PersonId)->CurrentWork.IsAssigned());

	VerifyInvariants(*this, Registry, TEXT("after rejected work assignment"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkReassignmentTest,
	"RealmsUnwritten.Simulation.Work.Reassignment", SimulationTestFlags)

bool FSimulationWorkReassignmentTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FWorkTypeId Repair = Registry.CreateWorkType(TEXT("Work.TestRepair"), TEXT("Repair"));
	const FPhysicalSiteId SiteA = CreateTestSite(Registry, TEXT("Barn"));
	const FPhysicalSiteId SiteB = Registry.CreatePhysicalSite(
		Registry.FindPhysicalSite(SiteA)->PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));

	TestTrue(TEXT("Initial assignment succeeds"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteA) == EPersonWorkResult::Success);
	TestTrue(TEXT("The exact same triple is already assigned"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteA) == EPersonWorkResult::AlreadyAssigned);

	TestTrue(TEXT("A different work type replaces the commitment"),
		Registry.AssignPersonWork(PersonId, Repair, SiteA) == EPersonWorkResult::Success);
	TestTrue(TEXT("Reassignment by type stores the new work type"),
		Registry.FindPerson(PersonId)->CurrentWork.WorkTypeId == Repair);
	TestTrue(TEXT("Reassignment by type keeps the site"),
		Registry.FindPerson(PersonId)->CurrentWork.PhysicalSiteId == SiteA);

	TestTrue(TEXT("A different site replaces the commitment"),
		Registry.AssignPersonWork(PersonId, Repair, SiteB) == EPersonWorkResult::Success);
	TestTrue(TEXT("Reassignment by site stores the new site"),
		Registry.FindPerson(PersonId)->CurrentWork.PhysicalSiteId == SiteB);
	TestTrue(TEXT("Reassignment by site keeps the work type"),
		Registry.FindPerson(PersonId)->CurrentWork.WorkTypeId == Repair);
	TestTrue(TEXT("The person still has exactly one current-work value"),
		Registry.FindPerson(PersonId)->CurrentWork.Kind == ECurrentWorkKind::PhysicalSite);

	TestTrue(TEXT("Changing type and site together succeeds"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteA) == EPersonWorkResult::Success);
	TestTrue(TEXT("The replaced commitment is harvest at site A"),
		Registry.FindPerson(PersonId)->CurrentWork.WorkTypeId == Harvest
			&& Registry.FindPerson(PersonId)->CurrentWork.PhysicalSiteId == SiteA);

	VerifyInvariants(*this, Registry, TEXT("after reassignment"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkRemovalTest,
	"RealmsUnwritten.Simulation.Work.Removal", SimulationTestFlags)

bool FSimulationWorkRemovalTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);

	TestTrue(TEXT("Removing work from an unassigned person is NotAssigned"),
		Registry.RemovePersonWork(PersonId) == EPersonWorkResult::NotAssigned);
	TestTrue(TEXT("Assignment succeeds"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteId) == EPersonWorkResult::Success);
	TestTrue(TEXT("Assigned work can be removed"),
		Registry.RemovePersonWork(PersonId) == EPersonWorkResult::Success);

	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
	if (!PersonRecord.IsSet())
	{
		AddError(TEXT("The person should still resolve after removal"));
		return false;
	}

	TestTrue(TEXT("Removed work is the canonical nowhere kind"),
		PersonRecord->CurrentWork.Kind == ECurrentWorkKind::None);
	TestFalse(TEXT("Removed work is unassigned"), PersonRecord->CurrentWork.IsAssigned());
	TestFalse(TEXT("Removed work carries no work type"), PersonRecord->CurrentWork.WorkTypeId.IsValid());
	TestFalse(TEXT("Removed work carries no site"), PersonRecord->CurrentWork.PhysicalSiteId.IsValid());

	TestTrue(TEXT("A second removal is NotAssigned"),
		Registry.RemovePersonWork(PersonId) == EPersonWorkResult::NotAssigned);
	TestTrue(TEXT("A second removal left the person unassigned"),
		Registry.FindPerson(PersonId)->CurrentWork.Kind == ECurrentWorkKind::None);

	VerifyInvariants(*this, Registry, TEXT("after work removal"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.Work.SnapshotRegression", SimulationTestFlags)

bool FSimulationWorkSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	Registry.AssignPersonWork(PersonId, Harvest, SiteId);

	TOptional<FPersonRecord> PersonSnapshot = Registry.FindPerson(PersonId);
	TOptional<FWorkTypeRecord> WorkTypeSnapshot = Registry.FindWorkType(Harvest);
	if (!PersonSnapshot.IsSet() || !WorkTypeSnapshot.IsSet())
	{
		AddError(TEXT("Both snapshots should be readable"));
		return false;
	}

	const FWorkTypeId Repair = Registry.CreateWorkType(TEXT("Work.TestRepair"), TEXT("Repair"));
	for (int32 FillerIndex = 0; FillerIndex < 16; ++FillerIndex)
	{
		Registry.CreatePerson(MakePersonParams(TEXT("Filler"), TEXT("Person"), 20));
		Registry.CreateWorkType(
			FName(*FString::Printf(TEXT("Work.Filler%d"), FillerIndex)), TEXT("Filler"));
	}

	const FPhysicalSiteId OtherSite = Registry.CreatePhysicalSite(
		Registry.FindPhysicalSite(SiteId)->PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));
	Registry.AssignPersonWork(PersonId, Repair, OtherSite);

	TestTrue(TEXT("The held person snapshot still reports its identifier"), PersonSnapshot->Id == PersonId);
	TestTrue(TEXT("The held person snapshot still reports harvest"),
		PersonSnapshot->CurrentWork.WorkTypeId == Harvest);
	TestTrue(TEXT("The held person snapshot still reports the original site"),
		PersonSnapshot->CurrentWork.PhysicalSiteId == SiteId);
	TestTrue(TEXT("The held work type snapshot still reports its key"),
		WorkTypeSnapshot->AuthoredKey == FName(TEXT("Work.TestHarvest")));
	TestEqual(TEXT("The held work type snapshot still reports its display name"),
		WorkTypeSnapshot->Name, FString(TEXT("Harvest")));

	TestTrue(TEXT("The registry reports the reassigned work type"),
		Registry.FindPerson(PersonId)->CurrentWork.WorkTypeId == Repair);
	TestTrue(TEXT("The registry reports the reassigned site"),
		Registry.FindPerson(PersonId)->CurrentWork.PhysicalSiteId == OtherSite);

	PersonSnapshot->CurrentWork = FCurrentWork::Nowhere();
	PersonSnapshot->Id = FPersonId(UInt32MaxIdValue);
	WorkTypeSnapshot->AuthoredKey = FName(TEXT("Work.Tampered"));
	WorkTypeSnapshot->Name = TEXT("Tampered");
	WorkTypeSnapshot->Id = FWorkTypeId(UInt32MaxIdValue);

	const TOptional<FPersonRecord> PersonAfterTampering = Registry.FindPerson(PersonId);
	const TOptional<FWorkTypeRecord> WorkTypeAfterTampering = Registry.FindWorkType(Harvest);
	if (!PersonAfterTampering.IsSet() || !WorkTypeAfterTampering.IsSet())
	{
		AddError(TEXT("Authoritative records should still resolve after snapshots were edited"));
		return false;
	}

	TestTrue(TEXT("Editing a person snapshot does not clear current work"),
		PersonAfterTampering->CurrentWork.WorkTypeId == Repair
			&& PersonAfterTampering->CurrentWork.PhysicalSiteId == OtherSite);
	TestTrue(TEXT("Editing a work-type snapshot does not change the authored key"),
		WorkTypeAfterTampering->AuthoredKey == FName(TEXT("Work.TestHarvest")));
	TestEqual(TEXT("Editing a work-type snapshot does not change the display name"),
		WorkTypeAfterTampering->Name, FString(TEXT("Harvest")));
	TestTrue(TEXT("A tampered authored key resolves to nothing"),
		!Registry.FindWorkTypeIdByKey(TEXT("Work.Tampered")).IsSet());

	VerifyInvariants(*this, Registry, TEXT("after snapshot tampering"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkDoesNotAffectGoodsTest,
	"RealmsUnwritten.Simulation.Work.DoesNotAffectGoods", SimulationTestFlags)

bool FSimulationWorkDoesNotAffectGoodsTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPhysicalSiteId SiteA = CreateTestSite(Registry, TEXT("Barn"));
	const FPhysicalSiteId SiteB = Registry.CreatePhysicalSite(
		Registry.FindPhysicalSite(SiteA)->PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));
	const FInventoryId InventoryId = Registry.CreateInventory();
	Registry.AssignInventoryToSite(InventoryId, SiteA);

	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	TestTrue(TEXT("Seed goods are created"),
		Registry.AddGoods(InventoryId, Wheat, 12, TEXT("Test.Seed")) == EAddGoodsResult::Success);
	const int32 AuditCountAfterSeed = Registry.GetGoodsAuditRecordCount();

	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FWorkTypeId Repair = Registry.CreateWorkType(TEXT("Work.TestRepair"), TEXT("Repair"));

	TestTrue(TEXT("Assigning work succeeds"),
		Registry.AssignPersonWork(PersonId, Harvest, SiteA) == EPersonWorkResult::Success);
	TestTrue(TEXT("Reassigning work succeeds"),
		Registry.AssignPersonWork(PersonId, Repair, SiteB) == EPersonWorkResult::Success);
	TestTrue(TEXT("Removing work succeeds"), Registry.RemovePersonWork(PersonId) == EPersonWorkResult::Success);

	TestEqual(TEXT("Work assignment does not change wheat quantity"),
		Registry.GetQuantity(InventoryId, Wheat).Get(0), 12);
	TestEqual(TEXT("Work assignment does not change the site inventory list"),
		Registry.FindPhysicalSite(SiteA)->Inventories.Num(), 1);
	TestTrue(TEXT("The inventory remains listed by its original site"),
		Registry.FindPhysicalSite(SiteA)->Inventories[0] == InventoryId);
	TestEqual(TEXT("The second site gained no inventory from work"),
		Registry.FindPhysicalSite(SiteB)->Inventories.Num(), 0);
	TestTrue(TEXT("The inventory is still at the original site"),
		Registry.FindInventory(InventoryId)->Location.PhysicalSiteId == SiteA);
	TestEqual(TEXT("Work assignment appends no goods audit records"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeed);

	VerifyInvariants(*this, Registry, TEXT("after work operations that must not touch goods"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationWorkInvariantDetectionTest,
	"RealmsUnwritten.Simulation.Work.InvariantDetection", SimulationTestFlags)

bool FSimulationWorkInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
		const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		const FPhysicalSiteId SiteId = CreateTestSite(Registry);
		Registry.AssignPersonWork(PersonId, Harvest, SiteId);
		VerifyInvariants(*this, Registry, TEXT("an unhoused person with valid current work"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		VerifyInvariants(*this, Registry, TEXT("before corrupting a work type identifier"));

		FSimulationRegistryTestAccess::WorkTypeRecords(Registry)[0].Id = FWorkTypeId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a work type slot and identifier mismatch"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		FSimulationRegistryTestAccess::WorkTypeRecords(Registry)[0].AuthoredKey = NAME_None;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a work type with no authored key"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		Registry.CreateWorkType(TEXT("Work.TestRepair"), TEXT("Repair"));
		FSimulationRegistryTestAccess::WorkTypeRecords(Registry)[1].AuthoredKey =
			FName(TEXT("Work.TestHarvest"));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a duplicate work type authored key"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
		const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		CreateTestSite(Registry);
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork.Kind =
			ECurrentWorkKind::None;
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork.WorkTypeId = Harvest;
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("a nowhere current work that still names a work type"));
		TestTrue(TEXT("The corrupted person is the expected person"),
			FSimulationRegistryTestAccess::PersonRecords(Registry)[0].Id == PersonId);
	}

	{
		FSimulationRegistry Registry;
		Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
		const FPhysicalSiteId SiteId = CreateTestSite(Registry);
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork.Kind =
			ECurrentWorkKind::None;
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork.PhysicalSiteId = SiteId;
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("a nowhere current work that still names a physical site"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
		Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		const FPhysicalSiteId SiteId = CreateTestSite(Registry);
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork =
			FCurrentWork::AtPhysicalSite(FWorkTypeId(UInt32MaxIdValue), SiteId);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("current work that names an unknown work type"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
		const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
		CreateTestSite(Registry);
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork =
			FCurrentWork::AtPhysicalSite(Harvest, FPhysicalSiteId(UInt32MaxIdValue));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("current work that names an unknown site"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreatePerson(MakePersonParams(TEXT("Greta"), TEXT("Bauer"), 34));
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentWork.Kind =
			static_cast<ECurrentWorkKind>(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an unsupported current-work kind"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
