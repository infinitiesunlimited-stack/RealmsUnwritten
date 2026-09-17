#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for the authoritative physical-site and inventory-location foundation.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Purpose keys used here, such as `Test.BarnStorage`, are test classification only and
 * appear in no production code. There is no site-type enumeration.
 *
 * Identifier type-safety for FPhysicalSiteId lives with the types in SimulationIds.h, which
 * already lists it among the mutually distinct families.
 */

namespace
{
	constexpr EAutomationTestFlags SimulationTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	constexpr uint32 Int32MaxIdValue = 0x7FFFFFFFu;
	constexpr uint32 SignBitIdValue = 0x80000000u;
	constexpr uint32 UInt32MaxIdValue = 0xFFFFFFFFu;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPhysicalSiteCreationTest,
	"RealmsUnwritten.Simulation.PhysicalSite.Creation", SimulationTestFlags)

bool FSimulationPhysicalSiteCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPropertyId OtherProperty = Registry.CreateProperty(SettlementId);

	const FPhysicalSiteId FirstSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
	const FPhysicalSiteId SecondSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Second barn"));
	const FPhysicalSiteId YardSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));
	const FPhysicalSiteId OtherPropertySite =
		Registry.CreatePhysicalSite(OtherProperty, TEXT("Test.HouseholdStorage"), TEXT("House stores"));

	TestTrue(TEXT("A created site has a valid identifier"), FirstSite.IsValid());
	TestTrue(TEXT("A second created site has a valid identifier"), SecondSite.IsValid());
	TestTrue(TEXT("Sites receive distinct identifiers"),
		FirstSite != SecondSite && YardSite != FirstSite && OtherPropertySite != FirstSite);
	TestEqual(TEXT("All four sites are stored"), Registry.GetPhysicalSiteCount(), 4);

	const TOptional<FPhysicalSiteRecord> SiteRecord = Registry.FindPhysicalSite(FirstSite);
	if (!SiteRecord.IsSet())
	{
		AddError(TEXT("A created site should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("A site's record reports its own identifier"), SiteRecord->Id == FirstSite);
	TestTrue(TEXT("A site records the property it belongs to"), SiteRecord->PropertyId == PropertyId);
	TestTrue(TEXT("A site stores its purpose key"), SiteRecord->PurposeKey == FName(TEXT("Test.BarnStorage")));
	TestEqual(TEXT("Stored display name matches creation"), SiteRecord->DisplayName, FString(TEXT("Barn")));
	TestEqual(TEXT("A new site holds no inventories"), SiteRecord->Inventories.Num(), 0);

	TestTrue(TEXT("Two sites may share a purpose key"),
		Registry.FindPhysicalSite(SecondSite)->PurposeKey == FName(TEXT("Test.BarnStorage")));
	TestTrue(TEXT("A different purpose key is stored as given"),
		Registry.FindPhysicalSite(YardSite)->PurposeKey == FName(TEXT("Test.OpenYard")));

	const TOptional<FPropertyRecord> PropertyRecord = Registry.FindProperty(PropertyId);
	const TOptional<FPropertyRecord> OtherPropertyRecord = Registry.FindProperty(OtherProperty);
	if (!PropertyRecord.IsSet() || !OtherPropertyRecord.IsSet())
	{
		AddError(TEXT("Both properties should resolve"));
		return false;
	}

	TestEqual(TEXT("The property lists its three sites"), PropertyRecord->PhysicalSites.Num(), 3);
	TestTrue(TEXT("The property lists its first site"), PropertyRecord->PhysicalSites.Contains(FirstSite));
	TestTrue(TEXT("The property lists its second site"), PropertyRecord->PhysicalSites.Contains(SecondSite));
	TestTrue(TEXT("The property lists its yard site"), PropertyRecord->PhysicalSites.Contains(YardSite));
	TestFalse(TEXT("The property does not list another property's site"),
		PropertyRecord->PhysicalSites.Contains(OtherPropertySite));
	TestEqual(TEXT("The other property lists only its own site"), OtherPropertyRecord->PhysicalSites.Num(), 1);
	TestTrue(TEXT("The other property lists its site"),
		OtherPropertyRecord->PhysicalSites.Contains(OtherPropertySite));

	const int32 SiteCountBeforeRejections = Registry.GetPhysicalSiteCount();
	const uint32 UnresolvableValues[] = { 0u, 99u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FPhysicalSiteId RejectedSite = Registry.CreatePhysicalSite(
			FPropertyId(UnresolvableValue), TEXT("Test.BarnStorage"), TEXT("Rejected"));

		TestFalse(FString::Printf(TEXT("Creating a site on property %u returns an invalid identifier"),
				UnresolvableValue),
			RejectedSite.IsValid());
		TestFalse(FString::Printf(TEXT("The identifier returned for property %u does not resolve"),
				UnresolvableValue),
			Registry.ContainsPhysicalSite(RejectedSite));
	}

	TestFalse(TEXT("Creating a site without a purpose key is rejected"),
		Registry.CreatePhysicalSite(PropertyId, NAME_None, TEXT("Nameless")).IsValid());

	TestEqual(TEXT("Rejected site creations add no site records"),
		Registry.GetPhysicalSiteCount(), SiteCountBeforeRejections);
	TestEqual(TEXT("Rejected site creations leave the property list unchanged"),
		Registry.FindProperty(PropertyId)->PhysicalSites.Num(), 3);

	const FPhysicalSiteId NextSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.Granary"), TEXT("Granary"));
	TestTrue(TEXT("A site can still be created after rejections"), NextSite.IsValid());
	TestEqual(TEXT("A rejected site creation consumes no identifier"),
		static_cast<int32>(NextSite.GetValue()),
		static_cast<int32>(OtherPropertySite.GetValue()) + 1);

	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FPhysicalSiteId PhysicalSiteId = FPhysicalSiteId(UnresolvableValue);

		TestFalse(FString::Printf(TEXT("Site identifier %u does not resolve"), UnresolvableValue),
			Registry.ContainsPhysicalSite(PhysicalSiteId));
		TestFalse(FString::Printf(TEXT("Site identifier %u cannot be read"), UnresolvableValue),
			Registry.FindPhysicalSite(PhysicalSiteId).IsSet());
	}

	VerifyInvariants(*this, Registry, TEXT("after creating physical sites"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPhysicalSiteSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.PhysicalSite.SnapshotRegression", SimulationTestFlags)

bool FSimulationPhysicalSiteSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPhysicalSiteId SiteId =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
	const FInventoryId InventoryId = Registry.CreateInventory();
	Registry.AssignInventoryToSite(InventoryId, SiteId);

	TOptional<FPhysicalSiteRecord> SiteSnapshot = Registry.FindPhysicalSite(SiteId);
	if (!SiteSnapshot.IsSet())
	{
		AddError(TEXT("The site snapshot should be readable"));
		return false;
	}

	for (int32 FillerIndex = 0; FillerIndex < 64; ++FillerIndex)
	{
		Registry.CreatePhysicalSite(
			PropertyId, TEXT("Test.BarnStorage"), *FString::Printf(TEXT("Filler %d"), FillerIndex));
	}

	const FInventoryId LaterInventory = Registry.CreateInventory();
	Registry.AssignInventoryToSite(LaterInventory, SiteId);

	TestTrue(TEXT("The held site snapshot still reports its identifier"), SiteSnapshot->Id == SiteId);
	TestTrue(TEXT("The held site snapshot still reports its property"), SiteSnapshot->PropertyId == PropertyId);
	TestTrue(TEXT("The held site snapshot still reports its purpose key"),
		SiteSnapshot->PurposeKey == FName(TEXT("Test.BarnStorage")));
	TestEqual(TEXT("The held site snapshot still reports its display name"),
		SiteSnapshot->DisplayName, FString(TEXT("Barn")));
	TestEqual(TEXT("The held site snapshot does not observe later inventories"),
		SiteSnapshot->Inventories.Num(), 1);
	TestTrue(TEXT("The held site snapshot still lists the original inventory"),
		SiteSnapshot->Inventories.Contains(InventoryId));

	TestEqual(TEXT("The registry now lists both inventories at the site"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), 2);
	TestEqual(TEXT("Later sites reallocated storage"), Registry.GetPhysicalSiteCount(), 65);

	SiteSnapshot->DisplayName = TEXT("Tampered");
	SiteSnapshot->PurposeKey = FName(TEXT("Test.Tampered"));
	SiteSnapshot->PropertyId = FPropertyId(UInt32MaxIdValue);
	SiteSnapshot->Id = FPhysicalSiteId(UInt32MaxIdValue);
	SiteSnapshot->Inventories.Empty();

	const TOptional<FPhysicalSiteRecord> SiteAfterTampering = Registry.FindPhysicalSite(SiteId);
	if (!SiteAfterTampering.IsSet())
	{
		AddError(TEXT("The site should still resolve after a snapshot was edited"));
		return false;
	}

	TestEqual(TEXT("Editing a snapshot does not change the display name"),
		SiteAfterTampering->DisplayName, FString(TEXT("Barn")));
	TestTrue(TEXT("Editing a snapshot does not change the purpose key"),
		SiteAfterTampering->PurposeKey == FName(TEXT("Test.BarnStorage")));
	TestTrue(TEXT("Editing a snapshot does not change the property"),
		SiteAfterTampering->PropertyId == PropertyId);
	TestEqual(TEXT("Emptying a snapshot's inventories does not empty the site"),
		SiteAfterTampering->Inventories.Num(), 2);

	VerifyInvariants(*this, Registry, TEXT("after site snapshot activity"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationInventoryLocationAssignmentTest,
	"RealmsUnwritten.Simulation.Inventory.LocationAssignment", SimulationTestFlags)

bool FSimulationInventoryLocationAssignmentTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPhysicalSiteId SiteId =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
	const FInventoryId InventoryId = Registry.CreateInventory();

	TestTrue(TEXT("A new inventory is nowhere"),
		Registry.FindInventory(InventoryId)->Location.Kind == EInventoryLocationKind::None);
	TestEqual(TEXT("A new site lists no inventories"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), 0);

	TestTrue(TEXT("An empty inventory can be assigned to a site"),
		Registry.AssignInventoryToSite(InventoryId, SiteId) == EInventoryLocationResult::Success);

	{
		const TOptional<FInventoryRecord> InventoryRecord = Registry.FindInventory(InventoryId);
		const TOptional<FPhysicalSiteRecord> SiteRecord = Registry.FindPhysicalSite(SiteId);
		if (!InventoryRecord.IsSet() || !SiteRecord.IsSet())
		{
			AddError(TEXT("Both records should resolve after an assignment"));
			return false;
		}

		TestTrue(TEXT("The inventory reports a physical-site location"),
			InventoryRecord->Location.Kind == EInventoryLocationKind::PhysicalSite);
		TestTrue(TEXT("The inventory names the site"),
			InventoryRecord->Location.PhysicalSiteId == SiteId);
		TestEqual(TEXT("The site lists exactly one inventory"), SiteRecord->Inventories.Num(), 1);
		TestTrue(TEXT("The site lists the inventory"), SiteRecord->Inventories.Contains(InventoryId));
	}

	TestTrue(TEXT("Re-assigning the same site reports an existing location"),
		Registry.AssignInventoryToSite(InventoryId, SiteId) == EInventoryLocationResult::AlreadyAtSite);
	TestEqual(TEXT("Re-assigning the same site does not duplicate the reverse entry"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), 1);

	const FInventoryId SecondInventory = Registry.CreateInventory();
	TestTrue(TEXT("A second inventory can occupy the same site"),
		Registry.AssignInventoryToSite(SecondInventory, SiteId) == EInventoryLocationResult::Success);
	TestEqual(TEXT("The site lists both inventories"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), 2);

	const TOptional<FInventoryRecord> SnapshotBeforeRejection = Registry.FindInventory(InventoryId);
	const TOptional<FPhysicalSiteRecord> SiteBeforeRejection = Registry.FindPhysicalSite(SiteId);

	TestTrue(TEXT("Assigning an unknown inventory is rejected"),
		Registry.AssignInventoryToSite(FInventoryId(UInt32MaxIdValue), SiteId)
			== EInventoryLocationResult::UnknownInventory);
	TestTrue(TEXT("Assigning a default inventory identifier is rejected"),
		Registry.AssignInventoryToSite(FInventoryId(), SiteId) == EInventoryLocationResult::UnknownInventory);
	TestTrue(TEXT("Assigning to an unknown site is rejected"),
		Registry.AssignInventoryToSite(InventoryId, FPhysicalSiteId(SignBitIdValue))
			== EInventoryLocationResult::UnknownSite);
	TestTrue(TEXT("Assigning to a default site identifier is rejected"),
		Registry.AssignInventoryToSite(InventoryId, FPhysicalSiteId()) == EInventoryLocationResult::UnknownSite);

	TestTrue(TEXT("Rejected assignments leave the inventory's location unchanged"),
		Registry.FindInventory(InventoryId)->Location.Kind == EInventoryLocationKind::PhysicalSite
			&& Registry.FindInventory(InventoryId)->Location.PhysicalSiteId == SiteId);
	TestEqual(TEXT("Rejected assignments leave the site list unchanged"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), SiteBeforeRejection->Inventories.Num());
	TestTrue(TEXT("A rejected assignment does not consume the prior snapshot"),
		SnapshotBeforeRejection->Location.PhysicalSiteId == SiteId);

	VerifyInvariants(*this, Registry, TEXT("after inventory location assignment"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationInventoryRelocationTest,
	"RealmsUnwritten.Simulation.Inventory.Relocation", SimulationTestFlags)

bool FSimulationInventoryRelocationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPhysicalSiteId SiteA =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
	const FPhysicalSiteId SiteB =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));
	const FInventoryId InventoryId = Registry.CreateInventory();
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));

	TestTrue(TEXT("The inventory is assigned to site A"),
		Registry.AssignInventoryToSite(InventoryId, SiteA) == EInventoryLocationResult::Success);
	TestTrue(TEXT("Goods can be created at site A"),
		Registry.AddGoods(InventoryId, Wheat, 12, TEXT("Test.Seed")) == EAddGoodsResult::Success);

	TestTrue(TEXT("A populated inventory can relocate to site B"),
		Registry.AssignInventoryToSite(InventoryId, SiteB) == EInventoryLocationResult::Success);

	{
		const TOptional<FInventoryRecord> InventoryRecord = Registry.FindInventory(InventoryId);
		const TOptional<FPhysicalSiteRecord> SiteARecord = Registry.FindPhysicalSite(SiteA);
		const TOptional<FPhysicalSiteRecord> SiteBRecord = Registry.FindPhysicalSite(SiteB);
		if (!InventoryRecord.IsSet() || !SiteARecord.IsSet() || !SiteBRecord.IsSet())
		{
			AddError(TEXT("Every record should resolve after a relocation"));
			return false;
		}

		TestTrue(TEXT("The inventory now names site B"),
			InventoryRecord->Location.Kind == EInventoryLocationKind::PhysicalSite
				&& InventoryRecord->Location.PhysicalSiteId == SiteB);
		TestEqual(TEXT("Site A no longer lists the inventory"), SiteARecord->Inventories.Num(), 0);
		TestEqual(TEXT("Site B lists the inventory once"), SiteBRecord->Inventories.Num(), 1);
		TestTrue(TEXT("Site B lists the inventory"), SiteBRecord->Inventories.Contains(InventoryId));
		TestEqual(TEXT("Relocation does not create or destroy goods"),
			Registry.GetQuantity(InventoryId, Wheat).Get(0), 12);
	}

	VerifyInvariants(*this, Registry, TEXT("after relocating an inventory"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationInventoryLocationRemovalTest,
	"RealmsUnwritten.Simulation.Inventory.LocationRemoval", SimulationTestFlags)

bool FSimulationInventoryLocationRemovalTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPhysicalSiteId SiteId =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
	const FInventoryId EmptyInventory = Registry.CreateInventory();
	const FInventoryId PopulatedInventory = Registry.CreateInventory();
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));

	Registry.AssignInventoryToSite(EmptyInventory, SiteId);
	Registry.AssignInventoryToSite(PopulatedInventory, SiteId);
	TestTrue(TEXT("Goods are created in the populated inventory"),
		Registry.AddGoods(PopulatedInventory, Wheat, 8, TEXT("Test.Seed")) == EAddGoodsResult::Success);

	TestTrue(TEXT("Removing location from a populated inventory is rejected"),
		Registry.RemoveInventoryLocation(PopulatedInventory) == EInventoryLocationResult::InventoryNotEmpty);
	TestTrue(TEXT("The populated inventory kept its location"),
		Registry.FindInventory(PopulatedInventory)->Location.PhysicalSiteId == SiteId);
	TestEqual(TEXT("The populated inventory kept its goods"),
		Registry.GetQuantity(PopulatedInventory, Wheat).Get(0), 8);
	TestTrue(TEXT("The site still lists the populated inventory"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Contains(PopulatedInventory));
	TestEqual(TEXT("The site still lists both inventories"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), 2);
	TestEqual(TEXT("A rejected unassignment appends no audit record"),
		Registry.GetGoodsAuditRecordCount(), 1);

	TestTrue(TEXT("An empty inventory can be unlocated"),
		Registry.RemoveInventoryLocation(EmptyInventory) == EInventoryLocationResult::Success);
	TestTrue(TEXT("The empty inventory is now nowhere"),
		Registry.FindInventory(EmptyInventory)->Location.Kind == EInventoryLocationKind::None);
	TestFalse(TEXT("The empty inventory names no site"),
		Registry.FindInventory(EmptyInventory)->Location.PhysicalSiteId.IsValid());
	TestFalse(TEXT("The site no longer lists the empty inventory"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Contains(EmptyInventory));
	TestEqual(TEXT("The site still lists the populated inventory once"),
		Registry.FindPhysicalSite(SiteId)->Inventories.Num(), 1);

	TestTrue(TEXT("Unlocating an already unlocated inventory is rejected"),
		Registry.RemoveInventoryLocation(EmptyInventory) == EInventoryLocationResult::NotLocated);
	TestTrue(TEXT("Removing location from an unknown inventory is rejected"),
		Registry.RemoveInventoryLocation(FInventoryId(UInt32MaxIdValue))
			== EInventoryLocationResult::UnknownInventory);

	VerifyInvariants(*this, Registry, TEXT("after inventory location removal"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodsLocationGateTest,
	"RealmsUnwritten.Simulation.Goods.LocationGate", SimulationTestFlags)

bool FSimulationGoodsLocationGateTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPhysicalSiteId SourceSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
	const FPhysicalSiteId DestinationSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));
	const FInventoryId UnlocatedInventory = Registry.CreateInventory();
	const FInventoryId SourceInventory = Registry.CreateInventory();
	const FInventoryId DestinationInventory = Registry.CreateInventory();

	Registry.AssignInventoryToSite(SourceInventory, SourceSite);
	Registry.AssignInventoryToSite(DestinationInventory, DestinationSite);

	TestTrue(TEXT("Adding goods to an unlocated inventory is rejected"),
		Registry.AddGoods(UnlocatedInventory, Wheat, 10, SeedReason) == EAddGoodsResult::InventoryNotLocated);
	TestEqual(TEXT("The unlocated inventory still holds nothing"),
		Registry.GetQuantity(UnlocatedInventory, Wheat).Get(0), 0);
	TestEqual(TEXT("The unlocated inventory still has no entries"),
		Registry.FindInventory(UnlocatedInventory)->Entries.Num(), 0);
	TestTrue(TEXT("The rejected addition did not assign a site"),
		Registry.FindInventory(UnlocatedInventory)->Location.Kind == EInventoryLocationKind::None);
	TestEqual(TEXT("The rejected addition appends no audit record"), Registry.GetGoodsAuditRecordCount(), 0);
	TestEqual(TEXT("The rejected addition creates no sites"), Registry.GetPhysicalSiteCount(), 2);

	TestTrue(TEXT("Adding goods to a located inventory succeeds"),
		Registry.AddGoods(SourceInventory, Wheat, 40, SeedReason) == EAddGoodsResult::Success);
	TestEqual(TEXT("The located inventory holds the created quantity"),
		Registry.GetQuantity(SourceInventory, Wheat).Get(0), 40);
	TestEqual(TEXT("A successful located addition appends exactly one audit record"),
		Registry.GetGoodsAuditRecordCount(), 1);
	TestTrue(TEXT("The creation audit names the located inventory"),
		Registry.GetGoodsAuditRecord(0)->InventoryId == SourceInventory
			&& Registry.GetGoodsAuditRecord(0)->Action == EGoodsAuditAction::Created
			&& Registry.GetGoodsAuditRecord(0)->Quantity == 40
			&& Registry.GetGoodsAuditRecord(0)->Reason == SeedReason);

	const int32 AuditCountAfterSeeding = Registry.GetGoodsAuditRecordCount();

	TestTrue(TEXT("Transferring from an unlocated inventory is rejected"),
		Registry.TransferGoods(UnlocatedInventory, DestinationInventory, Wheat, 5)
			== ETransferGoodsResult::SourceInventoryNotLocated);
	TestTrue(TEXT("Transferring to an unlocated inventory is rejected"),
		Registry.TransferGoods(SourceInventory, UnlocatedInventory, Wheat, 5)
			== ETransferGoodsResult::DestinationInventoryNotLocated);

	TestEqual(TEXT("A rejected unlocated transfer leaves the source unchanged"),
		Registry.GetQuantity(SourceInventory, Wheat).Get(0), 40);
	TestEqual(TEXT("A rejected unlocated transfer leaves the destination unchanged"),
		Registry.GetQuantity(DestinationInventory, Wheat).Get(0), 0);
	TestEqual(TEXT("A rejected unlocated transfer leaves the unlocated inventory empty"),
		Registry.GetQuantity(UnlocatedInventory, Wheat).Get(0), 0);
	TestEqual(TEXT("A rejected unlocated transfer appends no audit record"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);

	TestTrue(TEXT("A transfer between two located inventories succeeds"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 15)
			== ETransferGoodsResult::Success);
	TestEqual(TEXT("The source decreased by the transferred quantity"),
		Registry.GetQuantity(SourceInventory, Wheat).Get(0), 25);
	TestEqual(TEXT("The destination increased by the transferred quantity"),
		Registry.GetQuantity(DestinationInventory, Wheat).Get(0), 15);
	TestEqual(TEXT("The located transfer conserved the total"),
		Registry.GetQuantity(SourceInventory, Wheat).Get(0)
			+ Registry.GetQuantity(DestinationInventory, Wheat).Get(0),
		40);
	TestEqual(TEXT("A successful transfer appends no creation or destruction audit"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);

	VerifyInvariants(*this, Registry, TEXT("after location-gated goods operations"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPhysicalSiteInvariantDetectionTest,
	"RealmsUnwritten.Simulation.PhysicalSite.InvariantDetection", SimulationTestFlags)

bool FSimulationPhysicalSiteInvariantDetectionTest::RunTest(const FString& Parameters)
{
	const FName SeedReason(TEXT("Test.Seed"));

	auto MakeLocatedWorld = [](FSimulationRegistry& Registry, FPhysicalSiteId& OutSiteA,
		FPhysicalSiteId& OutSiteB, FInventoryId& OutInventoryId)
	{
		const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
		const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
		OutSiteA = Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BarnStorage"), TEXT("Barn"));
		OutSiteB = Registry.CreatePhysicalSite(PropertyId, TEXT("Test.OpenYard"), TEXT("Yard"));
		OutInventoryId = Registry.CreateInventory();
		Registry.AssignInventoryToSite(OutInventoryId, OutSiteA);
	};

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);
		VerifyInvariants(*this, Registry, TEXT("before corrupting a site identifier"));

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Id = FPhysicalSiteId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a physical site slot and identifier mismatch"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].PropertyId =
			FPropertyId(UInt32MaxIdValue);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a site that references an unknown property"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PropertyRecords(Registry)[0].PhysicalSites.Add(SiteA);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a duplicate site in a property reverse list"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.Add(
			FInventoryId(UInt32MaxIdValue));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a site that lists an unknown inventory"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.Add(InventoryId);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a duplicate inventory in a site reverse list"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.RemoveSingle(InventoryId);
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("an inventory located at a site that does not list it"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[1].Inventories.Add(InventoryId);
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("the same inventory listed by more than one site"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);
		const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		Registry.AddGoods(InventoryId, Wheat, 5, SeedReason);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.RemoveSingle(InventoryId);
		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Location = FInventoryLocation::Nowhere();
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a populated inventory with no location"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.RemoveSingle(InventoryId);
		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Location =
			FInventoryLocation::AtPhysicalSite(FPhysicalSiteId(UInt32MaxIdValue));
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("an inventory located at an unresolvable site"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PropertyRecords(Registry)[0].PhysicalSites.RemoveSingle(SiteA);
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("a property reverse list missing a site that claims it"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PropertyRecords(Registry)[0].PhysicalSites.Add(
			FPhysicalSiteId(UInt32MaxIdValue));
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("a property reverse list containing an unknown site"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		const FSettlementId SettlementId =
			FSimulationRegistryTestAccess::PropertyRecords(Registry)[0].SettlementId;
		Registry.CreateProperty(SettlementId);
		FSimulationRegistryTestAccess::PropertyRecords(Registry)[1].PhysicalSites.Add(SiteA);
		VerifyInvariantsDetectFailure(*this, Registry,
			TEXT("a valid property listing a site that belongs to a different valid property"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].PurposeKey = NAME_None;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a physical site with no purpose key"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.RemoveSingle(InventoryId);
		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Location.Kind =
			EInventoryLocationKind::None;
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("a nowhere location that still names a physical site"));
	}

	{
		FSimulationRegistry Registry;
		[[maybe_unused]] FPhysicalSiteId SiteA;
		[[maybe_unused]] FPhysicalSiteId SiteB;
		[[maybe_unused]] FInventoryId InventoryId;
		MakeLocatedWorld(Registry, SiteA, SiteB, InventoryId);

		FSimulationRegistryTestAccess::PhysicalSiteRecords(Registry)[0].Inventories.RemoveSingle(InventoryId);
		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Location.Kind =
			static_cast<EInventoryLocationKind>(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an unsupported inventory location kind"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
