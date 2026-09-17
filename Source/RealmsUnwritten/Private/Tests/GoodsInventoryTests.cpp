#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for the authoritative good type and inventory foundation.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required, so inventory state demonstrably
 * depends on neither a world nor a loaded level.
 *
 * Good types are created at runtime under authored keys, never taken from a C++ enum. The
 * keys and names used here, `Goods.Wheat` and the rest, are test data only and appear in no
 * production code.
 *
 * Tests that populate an inventory construct the smallest valid stationary chain required
 * by Prototype 0.1D: Settlement -> Property -> PhysicalSite -> Inventory. Empty-inventory
 * creation tests may leave the inventory unlocated.
 *
 * The small helpers below are intentionally duplicated from the accepted test files rather
 * than shared, so that the accepted Prototype 0.1A and 0.1B test files stay untouched.
 */

namespace
{
	constexpr EAutomationTestFlags SimulationTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/** Identifier values chosen to exercise the edges of the unsigned identifier range. */
	constexpr uint32 Int32MaxIdValue = 0x7FFFFFFFu;
	constexpr uint32 SignBitIdValue = 0x80000000u;
	constexpr uint32 UInt32MaxIdValue = 0xFFFFFFFFu;

	/**
	 * Smallest valid stationary place an inventory may occupy: one settlement, one property,
	 * one physical site. Test data only; production code names no site purpose.
	 */
	FPhysicalSiteId CreateTestSite(FSimulationRegistry& Registry)
	{
		const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Test Settlement"));
		const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
		return Registry.CreatePhysicalSite(PropertyId, TEXT("Test.Storage"), TEXT("Test storage"));
	}

	/** Empty inventory assigned to an existing site. */
	FInventoryId CreateLocatedInventory(FSimulationRegistry& Registry, FPhysicalSiteId PhysicalSiteId)
	{
		const FInventoryId InventoryId = Registry.CreateInventory();
		Registry.AssignInventoryToSite(InventoryId, PhysicalSiteId);
		return InventoryId;
	}

	/** Empty inventory at a newly created test site. */
	FInventoryId CreateLocatedInventory(FSimulationRegistry& Registry)
	{
		return CreateLocatedInventory(Registry, CreateTestSite(Registry));
	}

	void VerifyInvariants(FAutomationTestBase& Test, const FSimulationRegistry& Registry, const TCHAR* Context)
	{
		FString FailureDescription;
		if (!Registry.ValidateInvariants(FailureDescription))
		{
			Test.AddError(FString::Printf(TEXT("Invariant broken %s: %s"), Context, *FailureDescription));
		}
	}

	/** Asserts that deliberately corrupted state is detected and described. */
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

	/** Asserts the quantity an inventory holds of one good type, through the query API. */
	void VerifyQuantity(
		FAutomationTestBase& Test,
		const FSimulationRegistry& Registry,
		FInventoryId InventoryId,
		FGoodTypeId GoodTypeId,
		int32 ExpectedQuantity,
		const TCHAR* Context)
	{
		const TOptional<int32> Quantity = Registry.GetQuantity(InventoryId, GoodTypeId);
		if (!Quantity.IsSet())
		{
			Test.AddError(FString::Printf(
				TEXT("%s: expected %d of %s in %s, but the quantity could not be read at all."),
				Context, ExpectedQuantity, *GoodTypeId.ToString(), *InventoryId.ToString()));
			return;
		}

		Test.TestEqual(Context, Quantity.GetValue(), ExpectedQuantity);
	}

	/**
	 * Total quantity of one good type across several inventories.
	 *
	 * Accumulated as int64 so that a total spanning inventories cannot overflow the int32
	 * quantities being summed.
	 */
	int64 SumQuantity(
		const FSimulationRegistry& Registry, const TArray<FInventoryId>& InventoryIds, FGoodTypeId GoodTypeId)
	{
		int64 Total = 0;
		for (const FInventoryId InventoryId : InventoryIds)
		{
			Total += Registry.GetQuantity(InventoryId, GoodTypeId).Get(0);
		}

		return Total;
	}

	/** Asserts every field of one audit record. */
	void VerifyAuditRecord(
		FAutomationTestBase& Test,
		const FSimulationRegistry& Registry,
		int32 RecordIndex,
		EGoodsAuditAction ExpectedAction,
		FInventoryId ExpectedInventoryId,
		FGoodTypeId ExpectedGoodTypeId,
		int32 ExpectedQuantity,
		FName ExpectedReason,
		const TCHAR* Context)
	{
		const TOptional<FGoodsAuditRecord> AuditRecord = Registry.GetGoodsAuditRecord(RecordIndex);
		if (!AuditRecord.IsSet())
		{
			Test.AddError(FString::Printf(TEXT("%s: audit record %d is missing."), Context, RecordIndex));
			return;
		}

		Test.TestTrue(FString::Printf(TEXT("%s: the audited action is correct"), Context),
			AuditRecord->Action == ExpectedAction);
		Test.TestTrue(FString::Printf(TEXT("%s: the audited inventory is correct"), Context),
			AuditRecord->InventoryId == ExpectedInventoryId);
		Test.TestTrue(FString::Printf(TEXT("%s: the audited good type is correct"), Context),
			AuditRecord->GoodTypeId == ExpectedGoodTypeId);
		Test.TestEqual(FString::Printf(TEXT("%s: the audited quantity is correct"), Context),
			AuditRecord->Quantity, ExpectedQuantity);
		Test.TestTrue(FString::Printf(TEXT("%s: the audited reason is correct"), Context),
			AuditRecord->Reason == ExpectedReason);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodTypeCreationTest,
	"RealmsUnwritten.Simulation.Goods.GoodTypeCreation", SimulationTestFlags)

bool FSimulationGoodTypeCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	// Deliberately the same display name under two different authored keys: a display name
	// is not an identity.
	const FGoodTypeId WinterWheat = Registry.CreateGoodType(TEXT("Goods.WinterWheat"), TEXT("Wheat"));
	const FGoodTypeId SpringWheat = Registry.CreateGoodType(TEXT("Goods.SpringWheat"), TEXT("Wheat"));
	const FGoodTypeId Bread = Registry.CreateGoodType(TEXT("Goods.Bread"), TEXT("Bread"));

	TestTrue(TEXT("A created good type has a valid runtime handle"), WinterWheat.IsValid());
	TestTrue(TEXT("A second created good type has a valid runtime handle"), SpringWheat.IsValid());
	TestTrue(TEXT("Good types sharing a display name receive distinct runtime handles"),
		WinterWheat != SpringWheat);
	TestTrue(TEXT("Differently named good types receive distinct runtime handles"),
		Bread != WinterWheat && Bread != SpringWheat);
	TestEqual(TEXT("All three good types are stored"), Registry.GetGoodTypeCount(), 3);

	const TOptional<FGoodTypeRecord> GoodTypeRecord = Registry.FindGoodType(WinterWheat);
	if (!GoodTypeRecord.IsSet())
	{
		AddError(TEXT("A created good type should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("A good type's record reports its own runtime handle"), GoodTypeRecord->Id == WinterWheat);
	TestTrue(TEXT("A good type's record reports its authored key"),
		GoodTypeRecord->AuthoredKey == FName(TEXT("Goods.WinterWheat")));
	TestEqual(TEXT("Stored display name matches creation"), GoodTypeRecord->Name, FString(TEXT("Wheat")));

	// Invalid and extreme runtime handles must fail safely across the whole unsigned range.
	const uint32 UnresolvableValues[] = {
		0u, Bread.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FGoodTypeId GoodTypeId = FGoodTypeId(UnresolvableValue);

		TestFalse(FString::Printf(TEXT("Good type handle %u does not resolve"), UnresolvableValue),
			Registry.ContainsGoodType(GoodTypeId));
		TestFalse(FString::Printf(TEXT("Good type handle %u cannot be read"), UnresolvableValue),
			Registry.FindGoodType(GoodTypeId).IsSet());
	}

	TestEqual(TEXT("Failed good type lookups create no good types"), Registry.GetGoodTypeCount(), 3);

	VerifyInvariants(*this, Registry, TEXT("after creating good types"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodTypeAuthoredKeyTest,
	"RealmsUnwritten.Simulation.Goods.AuthoredKeyIdentity", SimulationTestFlags)

bool FSimulationGoodTypeAuthoredKeyTest::RunTest(const FString& Parameters)
{
	// The authored key is the durable definition identity; the runtime handle is not. This
	// test is about keeping those two roles separate.
	FSimulationRegistry Registry;

	// Creation requires a valid authored key.
	TestFalse(TEXT("A good type cannot be created without an authored key"),
		Registry.CreateGoodType(NAME_None, TEXT("Nameless")).IsValid());
	TestEqual(TEXT("A rejected creation stores no good type"), Registry.GetGoodTypeCount(), 0);

	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FGoodTypeId Flour = Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	TestTrue(TEXT("A good type with an authored key is created"), Wheat.IsValid());
	TestEqual(TEXT("Both good types are stored"), Registry.GetGoodTypeCount(), 2);

	// The authored key resolves to the runtime handle, without consulting the display name.
	const TOptional<FGoodTypeId> ResolvedWheat = Registry.FindGoodTypeIdByKey(TEXT("Goods.Wheat"));
	const TOptional<FGoodTypeId> ResolvedFlour = Registry.FindGoodTypeIdByKey(TEXT("Goods.Flour"));
	if (!ResolvedWheat.IsSet() || !ResolvedFlour.IsSet())
	{
		AddError(TEXT("An authored key should resolve to its runtime handle"));
		return false;
	}

	TestTrue(TEXT("The authored key resolves to the wheat handle"), ResolvedWheat.GetValue() == Wheat);
	TestTrue(TEXT("The authored key resolves to the flour handle"), ResolvedFlour.GetValue() == Flour);

	// An unknown or absent authored key resolves to nothing, and a display name is not a key.
	TestFalse(TEXT("An unknown authored key resolves to nothing"),
		Registry.FindGoodTypeIdByKey(TEXT("Goods.Bread")).IsSet());
	TestFalse(TEXT("No authored key resolves to nothing"), Registry.FindGoodTypeIdByKey(NAME_None).IsSet());
	TestFalse(TEXT("A display name is not an authored key"),
		Registry.FindGoodTypeIdByKey(TEXT("Wheat")).IsSet());

	// A duplicate authored key is rejected atomically and consumes no runtime handle.
	const int32 GoodTypeCountBeforeDuplicate = Registry.GetGoodTypeCount();
	const FGoodTypeId DuplicateKey = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Different Wheat"));
	TestFalse(TEXT("A duplicate authored key is rejected"), DuplicateKey.IsValid());
	TestEqual(TEXT("A duplicate authored key stores no good type"),
		Registry.GetGoodTypeCount(), GoodTypeCountBeforeDuplicate);

	// Proof that the rejected duplicate consumed no runtime handle: the next good type takes
	// the handle the duplicate would have received.
	const FGoodTypeId Bread = Registry.CreateGoodType(TEXT("Goods.Bread"), TEXT("Bread"));
	TestTrue(TEXT("The next good type is created"), Bread.IsValid());
	TestEqual(TEXT("The rejected duplicate left no gap in the runtime handles"),
		(int32)Bread.GetValue(), GoodTypeCountBeforeDuplicate + 1);

	// The original good type is untouched by the rejected duplicate.
	const TOptional<FGoodTypeRecord> WheatRecord = Registry.FindGoodType(Wheat);
	if (!WheatRecord.IsSet())
	{
		AddError(TEXT("The original good type should still resolve"));
		return false;
	}

	TestEqual(TEXT("The original display name survived the rejected duplicate"),
		WheatRecord->Name, FString(TEXT("Wheat")));
	TestTrue(TEXT("The authored key still resolves to the original handle"),
		Registry.FindGoodTypeIdByKey(TEXT("Goods.Wheat")).GetValue() == Wheat);

	// Duplicate display names remain allowed when the authored keys differ.
	const FGoodTypeId RyeBread = Registry.CreateGoodType(TEXT("Goods.RyeBread"), TEXT("Bread"));
	TestTrue(TEXT("A duplicate display name under a new authored key is allowed"), RyeBread.IsValid());
	TestTrue(TEXT("The duplicate display name received its own runtime handle"), RyeBread != Bread);
	TestTrue(TEXT("Each authored key resolves to its own handle"),
		Registry.FindGoodTypeIdByKey(TEXT("Goods.RyeBread")).GetValue() == RyeBread
			&& Registry.FindGoodTypeIdByKey(TEXT("Goods.Bread")).GetValue() == Bread);

	// Insertion order is not the authored identity. The same keys created in the opposite
	// order produce different runtime handles, while the keys still resolve correctly.
	FSimulationRegistry ReorderedRegistry;
	const FGoodTypeId ReorderedFlour = ReorderedRegistry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	const FGoodTypeId ReorderedWheat = ReorderedRegistry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));

	TestTrue(TEXT("Creation order changes which runtime handle an authored key maps to"),
		ReorderedWheat != Wheat && ReorderedFlour != Flour);
	TestTrue(TEXT("The authored key still resolves after a different creation order"),
		ReorderedRegistry.FindGoodTypeIdByKey(TEXT("Goods.Wheat")).GetValue() == ReorderedWheat);
	TestTrue(TEXT("The second authored key also still resolves"),
		ReorderedRegistry.FindGoodTypeIdByKey(TEXT("Goods.Flour")).GetValue() == ReorderedFlour);

	// The authored key survives the reordering; the handle does not. That is the distinction.
	TestTrue(TEXT("The authored key identifies the same definition in both registries"),
		ReorderedRegistry.FindGoodType(ReorderedWheat)->AuthoredKey == WheatRecord->AuthoredKey);

	VerifyInvariants(*this, Registry, TEXT("after authored key operations"));
	VerifyInvariants(*this, ReorderedRegistry, TEXT("after creating good types in another order"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationInventoryCreationTest,
	"RealmsUnwritten.Simulation.Goods.InventoryCreation", SimulationTestFlags)

bool FSimulationInventoryCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FInventoryId FirstInventory = Registry.CreateInventory();
	const FInventoryId SecondInventory = Registry.CreateInventory();

	TestTrue(TEXT("A created inventory has a valid identifier"), FirstInventory.IsValid());
	TestTrue(TEXT("A second created inventory has a valid identifier"), SecondInventory.IsValid());
	TestTrue(TEXT("Inventories receive distinct identifiers"), FirstInventory != SecondInventory);
	TestEqual(TEXT("Both inventories are stored"), Registry.GetInventoryCount(), 2);

	const TOptional<FInventoryRecord> InventoryRecord = Registry.FindInventory(FirstInventory);
	if (!InventoryRecord.IsSet())
	{
		AddError(TEXT("A created inventory should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("An inventory's record reports its own identifier"), InventoryRecord->Id == FirstInventory);
	TestEqual(TEXT("A new inventory holds no entries"), InventoryRecord->Entries.Num(), 0);
	TestTrue(TEXT("A new inventory is located nowhere"),
		InventoryRecord->Location.Kind == EInventoryLocationKind::None);
	TestFalse(TEXT("A new inventory names no physical site"),
		InventoryRecord->Location.PhysicalSiteId.IsValid());
	VerifyQuantity(*this, Registry, FirstInventory, Wheat, 0,
		TEXT("A new inventory holds zero of a known good type"));

	// Invalid and extreme identifiers must fail safely, including through the quantity query.
	const uint32 UnresolvableValues[] = {
		0u, SecondInventory.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FInventoryId InventoryId = FInventoryId(UnresolvableValue);

		TestFalse(FString::Printf(TEXT("Inventory identifier %u does not resolve"), UnresolvableValue),
			Registry.ContainsInventory(InventoryId));
		TestFalse(FString::Printf(TEXT("Inventory identifier %u cannot be read"), UnresolvableValue),
			Registry.FindInventory(InventoryId).IsSet());
		TestFalse(FString::Printf(TEXT("Inventory identifier %u has no readable quantity"), UnresolvableValue),
			Registry.GetQuantity(InventoryId, Wheat).IsSet());
	}

	// A resolvable inventory with an unresolvable good type is also unreadable, which is what
	// separates "no such good type" from "holds none of it".
	TestFalse(TEXT("A quantity cannot be read for an unresolvable good type"),
		Registry.GetQuantity(FirstInventory, FGoodTypeId(UInt32MaxIdValue)).IsSet());

	TestEqual(TEXT("Failed inventory lookups create no inventories"), Registry.GetInventoryCount(), 2);

	VerifyInvariants(*this, Registry, TEXT("after creating inventories"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationAddGoodsTest,
	"RealmsUnwritten.Simulation.Goods.AddGoods", SimulationTestFlags)

bool FSimulationAddGoodsTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FGoodTypeId Flour = Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	const FInventoryId InventoryId = CreateLocatedInventory(Registry);

	TestTrue(TEXT("A valid quantity can be added"),
		Registry.AddGoods(InventoryId, Wheat, 10, SeedReason) == EAddGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 10, TEXT("The added quantity is held"));

	TestTrue(TEXT("Adding again succeeds"),
		Registry.AddGoods(InventoryId, Wheat, 5, SeedReason) == EAddGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 15, TEXT("Repeated additions accumulate"));
	TestEqual(TEXT("Accumulating does not create a second entry"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 1);

	// Different good types coexist without interfering.
	TestTrue(TEXT("A second good type can be added"),
		Registry.AddGoods(InventoryId, Flour, 7, SeedReason) == EAddGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Flour, 7, TEXT("The second good type is held"));
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 15, TEXT("The first good type is unaffected"));
	TestEqual(TEXT("Two good types mean two entries"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 2);

	// Every rejection must leave the inventory exactly as it was.
	TestTrue(TEXT("Adding to an unknown inventory is rejected"),
		Registry.AddGoods(FInventoryId(UInt32MaxIdValue), Wheat, 5, SeedReason)
			== EAddGoodsResult::UnknownInventory);
	TestTrue(TEXT("Adding to a default inventory identifier is rejected"),
		Registry.AddGoods(FInventoryId(), Wheat, 5, SeedReason) == EAddGoodsResult::UnknownInventory);
	TestTrue(TEXT("Adding an unknown good type is rejected"),
		Registry.AddGoods(InventoryId, FGoodTypeId(SignBitIdValue), 5, SeedReason)
			== EAddGoodsResult::UnknownGoodType);
	TestTrue(TEXT("Adding a default good type handle is rejected"),
		Registry.AddGoods(InventoryId, FGoodTypeId(), 5, SeedReason) == EAddGoodsResult::UnknownGoodType);
	TestTrue(TEXT("Adding zero is rejected"),
		Registry.AddGoods(InventoryId, Wheat, 0, SeedReason) == EAddGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Adding a negative quantity is rejected"),
		Registry.AddGoods(InventoryId, Wheat, -5, SeedReason) == EAddGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Adding the most negative quantity is rejected"),
		Registry.AddGoods(InventoryId, Wheat, MIN_int32, SeedReason) == EAddGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Adding without a reason is rejected"),
		Registry.AddGoods(InventoryId, Wheat, 5, NAME_None) == EAddGoodsResult::InvalidReason);

	VerifyQuantity(*this, Registry, InventoryId, Wheat, 15, TEXT("Rejected additions leave wheat unchanged"));
	VerifyQuantity(*this, Registry, InventoryId, Flour, 7, TEXT("Rejected additions leave flour unchanged"));
	TestEqual(TEXT("Rejected additions create no entries"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 2);
	TestEqual(TEXT("Rejected additions create no inventories"), Registry.GetInventoryCount(), 1);

	VerifyInvariants(*this, Registry, TEXT("after adding goods"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRemoveGoodsTest,
	"RealmsUnwritten.Simulation.Goods.RemoveGoods", SimulationTestFlags)

bool FSimulationRemoveGoodsTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FName ConsumeReason(TEXT("Test.Consume"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FGoodTypeId Flour = Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	const FInventoryId InventoryId = CreateLocatedInventory(Registry);
	const FInventoryId EmptyInventory = Registry.CreateInventory();

	Registry.AddGoods(InventoryId, Wheat, 10, SeedReason);
	Registry.AddGoods(InventoryId, Flour, 5, SeedReason);

	TestTrue(TEXT("A valid quantity can be removed"),
		Registry.RemoveGoods(InventoryId, Wheat, 4, ConsumeReason) == ERemoveGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 6, TEXT("The remaining quantity is held"));
	TestEqual(TEXT("A partial removal keeps the entry"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 2);

	// Reaching zero must drop the entry rather than store a zero.
	TestTrue(TEXT("Removing the remainder succeeds"),
		Registry.RemoveGoods(InventoryId, Wheat, 6, ConsumeReason) == ERemoveGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 0, TEXT("The inventory now holds no wheat"));
	TestEqual(TEXT("Reaching zero removes the stored entry"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 1);
	TestTrue(TEXT("The surviving entry is the other good type"),
		Registry.FindInventory(InventoryId)->Entries[0].GoodTypeId == Flour);

	// Rejections.
	TestTrue(TEXT("Removing a good type the inventory no longer holds is rejected"),
		Registry.RemoveGoods(InventoryId, Wheat, 1, ConsumeReason) == ERemoveGoodsResult::InsufficientQuantity);
	TestTrue(TEXT("Removing more than is held is rejected"),
		Registry.RemoveGoods(InventoryId, Flour, 6, ConsumeReason) == ERemoveGoodsResult::InsufficientQuantity);
	TestTrue(TEXT("Removing from an empty inventory is rejected"),
		Registry.RemoveGoods(EmptyInventory, Flour, 1, ConsumeReason) == ERemoveGoodsResult::InsufficientQuantity);
	TestTrue(TEXT("Removing from an unknown inventory is rejected"),
		Registry.RemoveGoods(FInventoryId(UInt32MaxIdValue), Flour, 1, ConsumeReason)
			== ERemoveGoodsResult::UnknownInventory);
	TestTrue(TEXT("Removing a default inventory identifier is rejected"),
		Registry.RemoveGoods(FInventoryId(), Flour, 1, ConsumeReason) == ERemoveGoodsResult::UnknownInventory);
	TestTrue(TEXT("Removing an unknown good type is rejected"),
		Registry.RemoveGoods(InventoryId, FGoodTypeId(SignBitIdValue), 1, ConsumeReason)
			== ERemoveGoodsResult::UnknownGoodType);
	TestTrue(TEXT("Removing zero is rejected"),
		Registry.RemoveGoods(InventoryId, Flour, 0, ConsumeReason) == ERemoveGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Removing a negative quantity is rejected"),
		Registry.RemoveGoods(InventoryId, Flour, -3, ConsumeReason) == ERemoveGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Removing the most negative quantity is rejected"),
		Registry.RemoveGoods(InventoryId, Flour, MIN_int32, ConsumeReason) == ERemoveGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Removing without a reason is rejected"),
		Registry.RemoveGoods(InventoryId, Flour, 1, NAME_None) == ERemoveGoodsResult::InvalidReason);

	VerifyQuantity(*this, Registry, InventoryId, Flour, 5, TEXT("Rejected removals leave flour unchanged"));
	TestEqual(TEXT("Rejected removals leave the entry list unchanged"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 1);
	TestEqual(TEXT("Rejected removals leave the empty inventory empty"),
		Registry.FindInventory(EmptyInventory)->Entries.Num(), 0);

	VerifyInvariants(*this, Registry, TEXT("after removing goods"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationQuantityBoundaryTest,
	"RealmsUnwritten.Simulation.Goods.QuantityBoundaries", SimulationTestFlags)

bool FSimulationQuantityBoundaryTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FName ConsumeReason(TEXT("Test.Consume"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	const FInventoryId InventoryId = CreateLocatedInventory(Registry, SiteId);

	// The maximum representable quantity is itself a valid holding.
	TestTrue(TEXT("The maximum quantity can be added"),
		Registry.AddGoods(InventoryId, Wheat, MaxGoodQuantity, SeedReason) == EAddGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, MaxGoodQuantity,
		TEXT("The inventory holds the maximum quantity"));

	// Anything beyond it is refused, never wrapped.
	TestTrue(TEXT("Adding one more than the maximum is rejected"),
		Registry.AddGoods(InventoryId, Wheat, 1, SeedReason) == EAddGoodsResult::Overflow);
	TestTrue(TEXT("Adding the maximum again is rejected"),
		Registry.AddGoods(InventoryId, Wheat, MaxGoodQuantity, SeedReason) == EAddGoodsResult::Overflow);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, MaxGoodQuantity,
		TEXT("Rejected overflow leaves the quantity unchanged"));
	VerifyInvariants(*this, Registry, TEXT("after a rejected overflow"));

	TestTrue(TEXT("The maximum quantity can be removed again"),
		Registry.RemoveGoods(InventoryId, Wheat, MaxGoodQuantity, ConsumeReason) == ERemoveGoodsResult::Success);
	TestEqual(TEXT("Removing the maximum empties the inventory"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 0);

	// Destination overflow during a transfer, checked before either side is touched.
	const FInventoryId SourceInventory = CreateLocatedInventory(Registry, SiteId);
	const FInventoryId DestinationInventory = CreateLocatedInventory(Registry, SiteId);
	Registry.AddGoods(SourceInventory, Wheat, 5, SeedReason);
	Registry.AddGoods(DestinationInventory, Wheat, MaxGoodQuantity - 2, SeedReason);

	TestTrue(TEXT("A transfer that would overflow the destination is rejected"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 3)
			== ETransferGoodsResult::Overflow);
	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 5,
		TEXT("A rejected overflow leaves the source unchanged"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, MaxGoodQuantity - 2,
		TEXT("A rejected overflow leaves the destination unchanged"));

	// Filling the destination to exactly the maximum is allowed.
	TestTrue(TEXT("A transfer that exactly fills the destination succeeds"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 2)
			== ETransferGoodsResult::Success);
	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 3, TEXT("The source paid the transfer"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, MaxGoodQuantity,
		TEXT("The destination is exactly full"));

	TestTrue(TEXT("A full destination accepts nothing further"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 1)
			== ETransferGoodsResult::Overflow);
	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 3, TEXT("The source kept its remainder"));

	VerifyInvariants(*this, Registry, TEXT("after quantity boundary operations"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTransferGoodsTest,
	"RealmsUnwritten.Simulation.Goods.Transfer", SimulationTestFlags)

bool FSimulationTransferGoodsTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FGoodTypeId Flour = Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	const FInventoryId SourceInventory = CreateLocatedInventory(Registry, SiteId);
	const FInventoryId DestinationInventory = CreateLocatedInventory(Registry, SiteId);
	const TArray<FInventoryId> AllInventories = { SourceInventory, DestinationInventory };

	Registry.AddGoods(SourceInventory, Wheat, 100, SeedReason);
	Registry.AddGoods(SourceInventory, Flour, 50, SeedReason);
	Registry.AddGoods(DestinationInventory, Wheat, 10, SeedReason);

	const int64 WheatTotalBefore = SumQuantity(Registry, AllInventories, Wheat);
	const int64 FlourTotalBefore = SumQuantity(Registry, AllInventories, Flour);
	TestEqual(TEXT("The scenario starts with the expected wheat total"), WheatTotalBefore, (int64)110);

	// A partial transfer moves the quantity and keeps the source entry.
	TestTrue(TEXT("A transfer between two inventories succeeds"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 40)
			== ETransferGoodsResult::Success);
	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 60, TEXT("The source decreased by the quantity"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, 50,
		TEXT("The destination increased by the quantity"));
	TestEqual(TEXT("A partial transfer conserves the total"),
		SumQuantity(Registry, AllInventories, Wheat), WheatTotalBefore);
	TestEqual(TEXT("A partial transfer retains the source entry"),
		Registry.FindInventory(SourceInventory)->Entries.Num(), 2);

	// Other good types are untouched by a transfer.
	VerifyQuantity(*this, Registry, SourceInventory, Flour, 50, TEXT("Flour in the source is untouched"));
	VerifyQuantity(*this, Registry, DestinationInventory, Flour, 0,
		TEXT("Flour was not created in the destination"));
	TestEqual(TEXT("The flour total is unaffected by a wheat transfer"),
		SumQuantity(Registry, AllInventories, Flour), FlourTotalBefore);

	VerifyInvariants(*this, Registry, TEXT("after a partial transfer"));

	// A full transfer empties the source of that good type and drops the entry.
	TestTrue(TEXT("Transferring the remainder succeeds"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 60)
			== ETransferGoodsResult::Success);
	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 0, TEXT("The source holds no more wheat"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, 110,
		TEXT("The destination holds all the wheat"));
	TestEqual(TEXT("A full transfer conserves the total"),
		SumQuantity(Registry, AllInventories, Wheat), WheatTotalBefore);
	TestEqual(TEXT("A full transfer removes the source entry"),
		Registry.FindInventory(SourceInventory)->Entries.Num(), 1);
	TestTrue(TEXT("The source's surviving entry is the untransferred good type"),
		Registry.FindInventory(SourceInventory)->Entries[0].GoodTypeId == Flour);

	// Moving a second good type independently, in the other direction.
	TestTrue(TEXT("A different good type transfers independently"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Flour, 20)
			== ETransferGoodsResult::Success);
	VerifyQuantity(*this, Registry, SourceInventory, Flour, 30, TEXT("Flour left the source"));
	VerifyQuantity(*this, Registry, DestinationInventory, Flour, 20, TEXT("Flour reached the destination"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, 110,
		TEXT("The wheat holding is unaffected by a flour transfer"));
	TestEqual(TEXT("Both totals are still conserved"),
		SumQuantity(Registry, AllInventories, Flour), FlourTotalBefore);
	TestEqual(TEXT("The wheat total is still conserved"),
		SumQuantity(Registry, AllInventories, Wheat), WheatTotalBefore);

	VerifyInvariants(*this, Registry, TEXT("after transferring goods"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTransferRejectionTest,
	"RealmsUnwritten.Simulation.Goods.TransferRejection", SimulationTestFlags)

bool FSimulationTransferRejectionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	const FInventoryId SourceInventory = CreateLocatedInventory(Registry, SiteId);
	const FInventoryId DestinationInventory = CreateLocatedInventory(Registry, SiteId);
	const TArray<FInventoryId> AllInventories = { SourceInventory, DestinationInventory };

	Registry.AddGoods(SourceInventory, Wheat, 10, SeedReason);
	Registry.AddGoods(DestinationInventory, Wheat, 3, SeedReason);
	const int64 WheatTotalBefore = SumQuantity(Registry, AllInventories, Wheat);

	TestTrue(TEXT("Transferring more than the source holds is rejected"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 11)
			== ETransferGoodsResult::InsufficientQuantity);

	const uint32 UnresolvableValues[] = { 0u, 99u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestTrue(FString::Printf(TEXT("Transferring from source %u is rejected"), UnresolvableValue),
			Registry.TransferGoods(FInventoryId(UnresolvableValue), DestinationInventory, Wheat, 5)
				== ETransferGoodsResult::UnknownSourceInventory);
		TestTrue(FString::Printf(TEXT("Transferring to destination %u is rejected"), UnresolvableValue),
			Registry.TransferGoods(SourceInventory, FInventoryId(UnresolvableValue), Wheat, 5)
				== ETransferGoodsResult::UnknownDestinationInventory);
		TestTrue(FString::Printf(TEXT("Transferring good type %u is rejected"), UnresolvableValue),
			Registry.TransferGoods(SourceInventory, DestinationInventory, FGoodTypeId(UnresolvableValue), 5)
				== ETransferGoodsResult::UnknownGoodType);
	}

	TestTrue(TEXT("Transferring zero is rejected"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 0)
			== ETransferGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Transferring a negative quantity is rejected"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, -5)
			== ETransferGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Transferring the most negative quantity is rejected"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, MIN_int32)
			== ETransferGoodsResult::InvalidQuantity);

	// Every rejection above must have left both inventories exactly as they were.
	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 10,
		TEXT("Rejected transfers leave the source unchanged"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, 3,
		TEXT("Rejected transfers leave the destination unchanged"));
	TestEqual(TEXT("Rejected transfers conserve the total"),
		SumQuantity(Registry, AllInventories, Wheat), WheatTotalBefore);
	TestEqual(TEXT("Rejected transfers create no inventories"), Registry.GetInventoryCount(), 2);
	TestEqual(TEXT("Rejected transfers create no good types"), Registry.GetGoodTypeCount(), 1);
	TestEqual(TEXT("Rejected transfers leave the source entry list unchanged"),
		Registry.FindInventory(SourceInventory)->Entries.Num(), 1);
	TestEqual(TEXT("Rejected transfers leave the destination entry list unchanged"),
		Registry.FindInventory(DestinationInventory)->Entries.Num(), 1);

	VerifyInvariants(*this, Registry, TEXT("after rejected transfers"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationSameInventoryTransferTest,
	"RealmsUnwritten.Simulation.Goods.SameInventoryTransfer", SimulationTestFlags)

bool FSimulationSameInventoryTransferTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FInventoryId InventoryId = CreateLocatedInventory(Registry);
	Registry.AddGoods(InventoryId, Wheat, 10, TEXT("Test.Seed"));

	// A transfer to itself is a caller error, not a request, so it is rejected rather than
	// quietly succeeding. Goods are neither duplicated nor destroyed.
	TestTrue(TEXT("A transfer to the same inventory is rejected"),
		Registry.TransferGoods(InventoryId, InventoryId, Wheat, 5) == ETransferGoodsResult::SameInventory);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 10,
		TEXT("A same-inventory transfer neither duplicates nor destroys goods"));
	TestEqual(TEXT("A same-inventory transfer creates no second entry"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 1);

	// The structural rejection is reported even when the request would have failed anyway.
	TestTrue(TEXT("A same-inventory transfer of more than is held still reports SameInventory"),
		Registry.TransferGoods(InventoryId, InventoryId, Wheat, 50) == ETransferGoodsResult::SameInventory);

	// Quantity and identifier validation still come first, so their rejections are reported
	// ahead of the same-inventory check.
	TestTrue(TEXT("A same-inventory transfer of zero reports the invalid quantity"),
		Registry.TransferGoods(InventoryId, InventoryId, Wheat, 0) == ETransferGoodsResult::InvalidQuantity);
	TestTrue(TEXT("A same-inventory transfer of a negative quantity reports the invalid quantity"),
		Registry.TransferGoods(InventoryId, InventoryId, Wheat, -5) == ETransferGoodsResult::InvalidQuantity);
	TestTrue(TEXT("A same-inventory transfer of an unknown good type reports the unknown good type"),
		Registry.TransferGoods(InventoryId, InventoryId, FGoodTypeId(UInt32MaxIdValue), 5)
			== ETransferGoodsResult::UnknownGoodType);

	// An unresolvable inventory transferred to itself is reported as an unknown source.
	TestTrue(TEXT("An unknown inventory transferred to itself reports an unknown source"),
		Registry.TransferGoods(FInventoryId(UInt32MaxIdValue), FInventoryId(UInt32MaxIdValue), Wheat, 5)
			== ETransferGoodsResult::UnknownSourceInventory);

	VerifyQuantity(*this, Registry, InventoryId, Wheat, 10,
		TEXT("No same-inventory attempt changed the holding"));

	VerifyInvariants(*this, Registry, TEXT("after same-inventory transfer attempts"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodsAuditTest,
	"RealmsUnwritten.Simulation.Goods.Audit", SimulationTestFlags)

bool FSimulationGoodsAuditTest::RunTest(const FString& Parameters)
{
	// Creating and destroying quantity must leave an inspectable record of what happened and
	// why. Moving quantity must not.
	FSimulationRegistry Registry;

	const FName HarvestReason(TEXT("Test.Harvest"));
	const FName ConsumeReason(TEXT("Test.Consume"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FInventoryId InventoryId = CreateLocatedInventory(Registry);

	TestEqual(TEXT("A new registry has audited nothing"), Registry.GetGoodsAuditRecordCount(), 0);
	TestFalse(TEXT("No audit record can be read from an empty audit trail"),
		Registry.GetGoodsAuditRecord(0).IsSet());

	// A successful creation appends exactly one Created record.
	TestTrue(TEXT("Goods are created"),
		Registry.AddGoods(InventoryId, Wheat, 40, HarvestReason) == EAddGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 40, TEXT("The created quantity is held"));
	TestEqual(TEXT("Creating goods appends exactly one audit record"),
		Registry.GetGoodsAuditRecordCount(), 1);
	VerifyAuditRecord(*this, Registry, 0, EGoodsAuditAction::Created, InventoryId, Wheat, 40, HarvestReason,
		TEXT("the creation record"));

	// A successful destruction appends exactly one Destroyed record.
	TestTrue(TEXT("Goods are destroyed"),
		Registry.RemoveGoods(InventoryId, Wheat, 15, ConsumeReason) == ERemoveGoodsResult::Success);
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 25, TEXT("The destroyed quantity is gone"));
	TestEqual(TEXT("Destroying goods appends exactly one further audit record"),
		Registry.GetGoodsAuditRecordCount(), 2);
	VerifyAuditRecord(*this, Registry, 1, EGoodsAuditAction::Destroyed, InventoryId, Wheat, 15, ConsumeReason,
		TEXT("the destruction record"));

	// The earlier record is untouched by the later one, and records read oldest first.
	VerifyAuditRecord(*this, Registry, 0, EGoodsAuditAction::Created, InventoryId, Wheat, 40, HarvestReason,
		TEXT("the creation record after a later destruction"));

	// An audit record read is a copy, so it must survive the audit storage growing under it.
	TOptional<FGoodsAuditRecord> AuditSnapshot = Registry.GetGoodsAuditRecord(0);
	if (!AuditSnapshot.IsSet())
	{
		AddError(TEXT("The first audit record should be readable"));
		return false;
	}

	// Append enough further records to force the audit array to reallocate several times,
	// which would dangle any borrowed pointer.
	for (int32 AppendIndex = 0; AppendIndex < 64; ++AppendIndex)
	{
		Registry.AddGoods(InventoryId, Wheat, 1, FName(*FString::Printf(TEXT("Test.Filler%d"), AppendIndex)));
	}

	TestEqual(TEXT("The additional mutations were all audited"), Registry.GetGoodsAuditRecordCount(), 66);

	// The retained copy still describes the mutation it was taken from, field by field.
	TestTrue(TEXT("The retained audit snapshot keeps its action"),
		AuditSnapshot->Action == EGoodsAuditAction::Created);
	TestTrue(TEXT("The retained audit snapshot keeps its inventory"),
		AuditSnapshot->InventoryId == InventoryId);
	TestTrue(TEXT("The retained audit snapshot keeps its good type"), AuditSnapshot->GoodTypeId == Wheat);
	TestEqual(TEXT("The retained audit snapshot keeps its quantity"), AuditSnapshot->Quantity, 40);
	TestTrue(TEXT("The retained audit snapshot keeps its reason"), AuditSnapshot->Reason == HarvestReason);

	// Editing the retained copy cannot rewrite what was audited.
	AuditSnapshot->Quantity = 9999;
	AuditSnapshot->Action = EGoodsAuditAction::Destroyed;
	AuditSnapshot->Reason = FName(TEXT("Test.Tampered"));
	AuditSnapshot->InventoryId = FInventoryId(UInt32MaxIdValue);
	AuditSnapshot->GoodTypeId = FGoodTypeId(UInt32MaxIdValue);
	VerifyAuditRecord(*this, Registry, 0, EGoodsAuditAction::Created, InventoryId, Wheat, 40, HarvestReason,
		TEXT("the creation record after editing a copy of it"));
	VerifyAuditRecord(*this, Registry, 1, EGoodsAuditAction::Destroyed, InventoryId, Wheat, 15, ConsumeReason,
		TEXT("the destruction record after editing a copy of another"));

	TestFalse(TEXT("An audit index past the end reads nothing"),
		Registry.GetGoodsAuditRecord(Registry.GetGoodsAuditRecordCount()).IsSet());
	TestFalse(TEXT("A negative audit index reads nothing"), Registry.GetGoodsAuditRecord(-1).IsSet());

	VerifyInvariants(*this, Registry, TEXT("after audited creation and destruction"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodsAuditRejectionTest,
	"RealmsUnwritten.Simulation.Goods.AuditRejection", SimulationTestFlags)

bool FSimulationGoodsAuditRejectionTest::RunTest(const FString& Parameters)
{
	// Nothing that failed may appear in the audit trail, and nothing authoritative may be
	// created or destroyed anonymously.
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	const FInventoryId InventoryId = CreateLocatedInventory(Registry, SiteId);
	const FInventoryId OtherInventory = CreateLocatedInventory(Registry, SiteId);

	Registry.AddGoods(InventoryId, Wheat, 20, SeedReason);
	const int32 AuditCountAfterSeeding = Registry.GetGoodsAuditRecordCount();
	TestEqual(TEXT("Seeding produced one audit record"), AuditCountAfterSeeding, 1);

	// Failed additions.
	TestTrue(TEXT("Adding to an unknown inventory fails"),
		Registry.AddGoods(FInventoryId(UInt32MaxIdValue), Wheat, 5, SeedReason)
			== EAddGoodsResult::UnknownInventory);
	TestTrue(TEXT("Adding an unknown good type fails"),
		Registry.AddGoods(InventoryId, FGoodTypeId(UInt32MaxIdValue), 5, SeedReason)
			== EAddGoodsResult::UnknownGoodType);
	TestTrue(TEXT("Adding zero fails"),
		Registry.AddGoods(InventoryId, Wheat, 0, SeedReason) == EAddGoodsResult::InvalidQuantity);
	TestTrue(TEXT("Adding an overflowing quantity fails"),
		Registry.AddGoods(InventoryId, Wheat, MaxGoodQuantity, SeedReason) == EAddGoodsResult::Overflow);

	// Failed removals.
	TestTrue(TEXT("Removing more than is held fails"),
		Registry.RemoveGoods(InventoryId, Wheat, 21, SeedReason) == ERemoveGoodsResult::InsufficientQuantity);
	TestTrue(TEXT("Removing from an unknown inventory fails"),
		Registry.RemoveGoods(FInventoryId(UInt32MaxIdValue), Wheat, 5, SeedReason)
			== ERemoveGoodsResult::UnknownInventory);
	TestTrue(TEXT("Removing a negative quantity fails"),
		Registry.RemoveGoods(InventoryId, Wheat, -5, SeedReason) == ERemoveGoodsResult::InvalidQuantity);

	VerifyQuantity(*this, Registry, InventoryId, Wheat, 20, TEXT("Failed mutations leave the quantity alone"));
	TestEqual(TEXT("Failed mutations append no audit records"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);

	// Anonymous creation and destruction are refused outright.
	TestTrue(TEXT("Creating goods without a reason is rejected"),
		Registry.AddGoods(InventoryId, Wheat, 5, NAME_None) == EAddGoodsResult::InvalidReason);
	TestTrue(TEXT("Destroying goods without a reason is rejected"),
		Registry.RemoveGoods(InventoryId, Wheat, 5, NAME_None) == ERemoveGoodsResult::InvalidReason);

	VerifyQuantity(*this, Registry, InventoryId, Wheat, 20,
		TEXT("A rejected reasonless mutation changes no quantity"));
	TestEqual(TEXT("A rejected reasonless mutation appends no audit record"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);
	TestEqual(TEXT("A rejected reasonless mutation leaves the entry list alone"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 1);

	// A failed transfer changes neither state nor audit trail.
	TestTrue(TEXT("An insufficient transfer fails"),
		Registry.TransferGoods(InventoryId, OtherInventory, Wheat, 50)
			== ETransferGoodsResult::InsufficientQuantity);
	TestTrue(TEXT("A transfer to an unknown destination fails"),
		Registry.TransferGoods(InventoryId, FInventoryId(UInt32MaxIdValue), Wheat, 5)
			== ETransferGoodsResult::UnknownDestinationInventory);
	TestTrue(TEXT("A same-inventory transfer fails"),
		Registry.TransferGoods(InventoryId, InventoryId, Wheat, 5) == ETransferGoodsResult::SameInventory);

	VerifyQuantity(*this, Registry, InventoryId, Wheat, 20, TEXT("A failed transfer leaves the source alone"));
	VerifyQuantity(*this, Registry, OtherInventory, Wheat, 0,
		TEXT("A failed transfer leaves the destination alone"));
	TestEqual(TEXT("A failed transfer appends no audit record"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);

	VerifyInvariants(*this, Registry, TEXT("after rejected mutations"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodsAuditTransferTest,
	"RealmsUnwritten.Simulation.Goods.AuditExcludesTransfer", SimulationTestFlags)

bool FSimulationGoodsAuditTransferTest::RunTest(const FString& Parameters)
{
	// A transfer conserves quantity, so it must not appear as creation or destruction no
	// matter how it is implemented internally.
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	const FInventoryId SourceInventory = CreateLocatedInventory(Registry, SiteId);
	const FInventoryId DestinationInventory = CreateLocatedInventory(Registry, SiteId);
	const TArray<FInventoryId> AllInventories = { SourceInventory, DestinationInventory };

	Registry.AddGoods(SourceInventory, Wheat, 100, SeedReason);
	const int32 AuditCountAfterSeeding = Registry.GetGoodsAuditRecordCount();
	const int64 TotalAfterSeeding = SumQuantity(Registry, AllInventories, Wheat);
	TestEqual(TEXT("Seeding produced exactly one audit record"), AuditCountAfterSeeding, 1);

	// A partial transfer, then a full one that empties the source entry.
	TestTrue(TEXT("A partial transfer succeeds"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 30)
			== ETransferGoodsResult::Success);
	TestTrue(TEXT("A full transfer succeeds"),
		Registry.TransferGoods(SourceInventory, DestinationInventory, Wheat, 70)
			== ETransferGoodsResult::Success);

	VerifyQuantity(*this, Registry, SourceInventory, Wheat, 0, TEXT("The source is emptied"));
	VerifyQuantity(*this, Registry, DestinationInventory, Wheat, 100, TEXT("The destination holds it all"));
	TestEqual(TEXT("The transfers conserved the total"),
		SumQuantity(Registry, AllInventories, Wheat), TotalAfterSeeding);
	TestEqual(TEXT("The emptied source has no entries"),
		Registry.FindInventory(SourceInventory)->Entries.Num(), 0);

	// The point of the test: movement created and destroyed nothing.
	TestEqual(TEXT("Transfers append no audit records at all"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);

	// The only record present is still the seeding creation, unchanged.
	VerifyAuditRecord(*this, Registry, 0, EGoodsAuditAction::Created, SourceInventory, Wheat, 100, SeedReason,
		TEXT("the seeding record after two transfers"));

	// A subsequent real destruction is still audited normally, proving transfers did not
	// disable auditing.
	TestTrue(TEXT("Goods can still be destroyed after transfers"),
		Registry.RemoveGoods(DestinationInventory, Wheat, 100, TEXT("Test.Consume"))
			== ERemoveGoodsResult::Success);
	TestEqual(TEXT("The destruction after transfers is audited"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding + 1);
	VerifyAuditRecord(*this, Registry, 1, EGoodsAuditAction::Destroyed, DestinationInventory, Wheat, 100,
		FName(TEXT("Test.Consume")), TEXT("the destruction record"));

	VerifyInvariants(*this, Registry, TEXT("after transfers and a destruction"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodsSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.Goods.SnapshotRegression", SimulationTestFlags)

bool FSimulationGoodsSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	// Snapshots are copies. Later registry activity must neither invalidate a held snapshot
	// nor be visible through it, and editing a snapshot must not reach authoritative state.
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.Seed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);
	const FInventoryId InventoryId = CreateLocatedInventory(Registry, SiteId);
	Registry.AddGoods(InventoryId, Wheat, 30, SeedReason);

	TOptional<FGoodTypeRecord> GoodTypeSnapshot = Registry.FindGoodType(Wheat);
	TOptional<FInventoryRecord> InventorySnapshot = Registry.FindInventory(InventoryId);
	if (!GoodTypeSnapshot.IsSet() || !InventorySnapshot.IsSet())
	{
		AddError(TEXT("Both snapshots should be readable"));
		return false;
	}

	// Grow every array the snapshots came from, which would reallocate storage and dangle
	// any borrowed pointer, and change the snapshotted inventory's contents as well.
	const FGoodTypeId Flour = Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	const FInventoryId OtherInventory = CreateLocatedInventory(Registry, SiteId);
	for (int32 FillerIndex = 0; FillerIndex < 16; ++FillerIndex)
	{
		Registry.CreateGoodType(FName(*FString::Printf(TEXT("Goods.Filler%d"), FillerIndex)), TEXT("Filler"));
		Registry.CreateInventory();
	}

	Registry.AddGoods(InventoryId, Flour, 5, SeedReason);
	Registry.RemoveGoods(InventoryId, Wheat, 10, TEXT("Test.Consume"));
	Registry.TransferGoods(InventoryId, OtherInventory, Wheat, 5);

	// The held snapshots still describe the state they were taken from.
	TestTrue(TEXT("The held good type snapshot still reports its handle"), GoodTypeSnapshot->Id == Wheat);
	TestTrue(TEXT("The held good type snapshot still reports its authored key"),
		GoodTypeSnapshot->AuthoredKey == FName(TEXT("Goods.Wheat")));
	TestEqual(TEXT("The held good type snapshot still reports its display name"),
		GoodTypeSnapshot->Name, FString(TEXT("Wheat")));

	TestTrue(TEXT("The held inventory snapshot still reports its identifier"),
		InventorySnapshot->Id == InventoryId);
	TestTrue(TEXT("The held inventory snapshot still reports its location kind"),
		InventorySnapshot->Location.Kind == EInventoryLocationKind::PhysicalSite);
	TestTrue(TEXT("The held inventory snapshot still reports its site"),
		InventorySnapshot->Location.PhysicalSiteId == SiteId);
	TestEqual(TEXT("The held inventory snapshot does not observe later entries"),
		InventorySnapshot->Entries.Num(), 1);
	TestTrue(TEXT("The held inventory snapshot still describes the good type it held"),
		InventorySnapshot->Entries[0].GoodTypeId == Wheat);
	TestEqual(TEXT("The held inventory snapshot does not observe later quantity changes"),
		InventorySnapshot->Entries[0].Quantity, 30);

	// Authoritative state moved on, independently of the snapshots.
	VerifyQuantity(*this, Registry, InventoryId, Wheat, 15, TEXT("The registry reports the current wheat"));
	VerifyQuantity(*this, Registry, InventoryId, Flour, 5, TEXT("The registry reports the added flour"));
	VerifyQuantity(*this, Registry, OtherInventory, Wheat, 5, TEXT("The registry reports the transferred wheat"));

	// Record fields are publicly writable, so editing a snapshot must be inert.
	GoodTypeSnapshot->AuthoredKey = FName(TEXT("Goods.Tampered"));
	GoodTypeSnapshot->Name = TEXT("Tampered");
	GoodTypeSnapshot->Id = FGoodTypeId(UInt32MaxIdValue);
	InventorySnapshot->Id = FInventoryId(UInt32MaxIdValue);
	InventorySnapshot->Location = FInventoryLocation::Nowhere();
	InventorySnapshot->Entries.Empty();

	const TOptional<FGoodTypeRecord> GoodTypeAfterTampering = Registry.FindGoodType(Wheat);
	if (!GoodTypeAfterTampering.IsSet())
	{
		AddError(TEXT("The good type should still resolve after a snapshot was edited"));
		return false;
	}

	TestTrue(TEXT("Editing a snapshot does not change the authored key"),
		GoodTypeAfterTampering->AuthoredKey == FName(TEXT("Goods.Wheat")));
	TestEqual(TEXT("Editing a snapshot does not change the display name"),
		GoodTypeAfterTampering->Name, FString(TEXT("Wheat")));
	TestTrue(TEXT("Editing a snapshot does not change the authored key resolution"),
		Registry.FindGoodTypeIdByKey(TEXT("Goods.Wheat")).GetValue() == Wheat);
	TestFalse(TEXT("A tampered authored key resolves to nothing"),
		Registry.FindGoodTypeIdByKey(TEXT("Goods.Tampered")).IsSet());

	VerifyQuantity(*this, Registry, InventoryId, Wheat, 15,
		TEXT("Emptying a snapshot's entries does not empty the inventory"));
	TestEqual(TEXT("The authoritative inventory still holds both entries"),
		Registry.FindInventory(InventoryId)->Entries.Num(), 2);
	TestTrue(TEXT("Clearing a snapshot's location does not unlocate the inventory"),
		Registry.FindInventory(InventoryId)->Location.Kind == EInventoryLocationKind::PhysicalSite
			&& Registry.FindInventory(InventoryId)->Location.PhysicalSiteId == SiteId);

	VerifyInvariants(*this, Registry, TEXT("after snapshot activity"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationGoodsInvariantDetectionTest,
	"RealmsUnwritten.Simulation.Goods.InvariantDetection", SimulationTestFlags)

bool FSimulationGoodsInvariantDetectionTest::RunTest(const FString& Parameters)
{
	// Invariant validation is worth nothing unless it fails on bad state. The public
	// operations make these states unreachable, so each is produced through the test-only
	// access hook, on its own registry.
	const FName SeedReason(TEXT("Test.Seed"));

	// A good type record whose runtime handle does not match its slot.
	{
		FSimulationRegistry Registry;
		Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		VerifyInvariants(*this, Registry, TEXT("before corrupting a good type handle"));

		FSimulationRegistryTestAccess::GoodTypeRecords(Registry)[0].Id = FGoodTypeId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a good type slot and handle mismatch"));
	}

	// An inventory record whose identifier does not match its slot.
	{
		FSimulationRegistry Registry;
		Registry.CreateInventory();
		VerifyInvariants(*this, Registry, TEXT("before corrupting an inventory identifier"));

		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Id = FInventoryId(9);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an inventory slot and identifier mismatch"));
	}

	// Two entries for the same good type in one inventory.
	{
		FSimulationRegistry Registry;
		const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		const FInventoryId InventoryId = CreateLocatedInventory(Registry);
		Registry.AddGoods(InventoryId, Wheat, 10, SeedReason);

		FInventoryEntry& DuplicateEntry =
			FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Entries.AddDefaulted_GetRef();
		DuplicateEntry.GoodTypeId = Wheat;
		DuplicateEntry.Quantity = 5;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a duplicate good type entry"));
	}

	// An entry naming a good type that does not resolve.
	{
		FSimulationRegistry Registry;
		const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		const FInventoryId InventoryId = CreateLocatedInventory(Registry);
		Registry.AddGoods(InventoryId, Wheat, 10, SeedReason);

		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Entries[0].GoodTypeId =
			FGoodTypeId(UInt32MaxIdValue);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an unresolvable good type reference"));
	}

	// A stored quantity of zero, which should have been a removed entry instead.
	{
		FSimulationRegistry Registry;
		const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		const FInventoryId InventoryId = CreateLocatedInventory(Registry);
		Registry.AddGoods(InventoryId, Wheat, 10, SeedReason);

		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Entries[0].Quantity = 0;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a stored quantity of zero"));
	}

	// A negative stored quantity.
	{
		FSimulationRegistry Registry;
		const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		const FInventoryId InventoryId = CreateLocatedInventory(Registry);
		Registry.AddGoods(InventoryId, Wheat, 10, SeedReason);

		FSimulationRegistryTestAccess::InventoryRecords(Registry)[0].Entries[0].Quantity = -5;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a negative stored quantity"));
	}

	// A good type with no authored key, and two good types sharing one.
	{
		FSimulationRegistry Registry;
		Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
		Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));

		FSimulationRegistryTestAccess::GoodTypeRecords(Registry)[1].AuthoredKey = NAME_None;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a good type with no authored key"));

		FSimulationRegistryTestAccess::GoodTypeRecords(Registry)[1].AuthoredKey = FName(TEXT("Goods.Wheat"));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a duplicated authored key"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationHeadlessConservationScenarioTest,
	"RealmsUnwritten.Simulation.Goods.Scenario.HeadlessConservation", SimulationTestFlags)

bool FSimulationHeadlessConservationScenarioTest::RunTest(const FString& Parameters)
{
	// Four good types and five inventories moving goods along a chain, with no Actor, no
	// world, no map, and no production: nothing here converts one good into another.
	//
	// The inventories sit on distinct physical sites of one property. The purpose keys are
	// test classification only; they do not implement harvest, milling, baking, or a household.
	FSimulationRegistry Registry;

	const FName SeedReason(TEXT("Test.ScenarioSeed"));
	const FGoodTypeId Wheat = Registry.CreateGoodType(TEXT("Goods.Wheat"), TEXT("Wheat"));
	const FGoodTypeId Flour = Registry.CreateGoodType(TEXT("Goods.Flour"), TEXT("Flour"));
	const FGoodTypeId Bread = Registry.CreateGoodType(TEXT("Goods.Bread"), TEXT("Bread"));
	const FGoodTypeId Firewood = Registry.CreateGoodType(TEXT("Goods.Firewood"), TEXT("Firewood"));
	const TArray<FGoodTypeId> AllGoodTypes = { Wheat, Flour, Bread, Firewood };

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
	const FPhysicalSiteId HarvestSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.HarvestPoint"), TEXT("Harvest point"));
	const FPhysicalSiteId StorageSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.Storage"), TEXT("Storage"));
	const FPhysicalSiteId MillSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.MillStorage"), TEXT("Mill storage"));
	const FPhysicalSiteId BakerySite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.BakeryStorage"), TEXT("Bakery storage"));
	const FPhysicalSiteId HouseholdSite =
		Registry.CreatePhysicalSite(PropertyId, TEXT("Test.HouseholdStorage"), TEXT("Household storage"));

	const FInventoryId HarvestInventory = CreateLocatedInventory(Registry, HarvestSite);
	const FInventoryId StorageInventory = CreateLocatedInventory(Registry, StorageSite);
	const FInventoryId MillInventory = CreateLocatedInventory(Registry, MillSite);
	const FInventoryId BakeryInventory = CreateLocatedInventory(Registry, BakerySite);
	const FInventoryId HouseholdInventory = CreateLocatedInventory(Registry, HouseholdSite);
	const TArray<FInventoryId> AllInventories = {
		HarvestInventory, StorageInventory, MillInventory, BakeryInventory, HouseholdInventory };

	TestEqual(TEXT("The scenario holds four good types"), Registry.GetGoodTypeCount(), 4);
	TestEqual(TEXT("The scenario holds five inventories"), Registry.GetInventoryCount(), 5);

	// Seeding is explicit low-level creation, the stand-in for scenario initialization.
	Registry.AddGoods(HarvestInventory, Wheat, 1000, SeedReason);
	Registry.AddGoods(StorageInventory, Wheat, 200, SeedReason);
	Registry.AddGoods(StorageInventory, Firewood, 30, SeedReason);
	Registry.AddGoods(MillInventory, Flour, 150, SeedReason);
	Registry.AddGoods(BakeryInventory, Bread, 60, SeedReason);
	Registry.AddGoods(HouseholdInventory, Bread, 5, SeedReason);
	Registry.AddGoods(HouseholdInventory, Firewood, 10, SeedReason);

	// Every seeded good is accounted for as a creation, and nothing else has been audited.
	const int32 AuditCountAfterSeeding = Registry.GetGoodsAuditRecordCount();
	TestEqual(TEXT("Each seeded holding was audited as a creation"), AuditCountAfterSeeding, 7);

	// Totals recorded per good type before any transfer.
	TMap<FGoodTypeId, int64> TotalsBefore;
	for (const FGoodTypeId GoodTypeId : AllGoodTypes)
	{
		TotalsBefore.Add(GoodTypeId, SumQuantity(Registry, AllInventories, GoodTypeId));
	}

	TestEqual(TEXT("Seeded wheat total"), TotalsBefore[Wheat], (int64)1200);
	TestEqual(TEXT("Seeded flour total"), TotalsBefore[Flour], (int64)150);
	TestEqual(TEXT("Seeded bread total"), TotalsBefore[Bread], (int64)65);
	TestEqual(TEXT("Seeded firewood total"), TotalsBefore[Firewood], (int64)40);

	VerifyInvariants(*this, Registry, TEXT("after seeding the scenario"));

	// Six transfers along the chain, including one that empties a source entry.
	TestTrue(TEXT("Wheat moves from the harvest point to storage"),
		Registry.TransferGoods(HarvestInventory, StorageInventory, Wheat, 800)
			== ETransferGoodsResult::Success);
	TestTrue(TEXT("Wheat moves from storage to the mill"),
		Registry.TransferGoods(StorageInventory, MillInventory, Wheat, 500)
			== ETransferGoodsResult::Success);
	TestTrue(TEXT("Flour moves from the mill to the bakery"),
		Registry.TransferGoods(MillInventory, BakeryInventory, Flour, 120)
			== ETransferGoodsResult::Success);
	TestTrue(TEXT("Bread moves from the bakery to the household"),
		Registry.TransferGoods(BakeryInventory, HouseholdInventory, Bread, 40)
			== ETransferGoodsResult::Success);
	TestTrue(TEXT("Firewood moves from storage to the household"),
		Registry.TransferGoods(StorageInventory, HouseholdInventory, Firewood, 25)
			== ETransferGoodsResult::Success);
	TestTrue(TEXT("The rest of the wheat leaves the harvest point"),
		Registry.TransferGoods(HarvestInventory, StorageInventory, Wheat, 200)
			== ETransferGoodsResult::Success);

	VerifyInvariants(*this, Registry, TEXT("after the scenario transfers"));

	// Conservation: every good type's total is exactly what it was before the transfers.
	for (const FGoodTypeId GoodTypeId : AllGoodTypes)
	{
		TestEqual(
			*FString::Printf(TEXT("The total of %s is conserved across all transfers"),
				*GoodTypeId.ToString()),
			SumQuantity(Registry, AllInventories, GoodTypeId),
			TotalsBefore[GoodTypeId]);
	}

	// Conservation again, from the other direction: the transfers created and destroyed
	// nothing, so the audit trail has not grown.
	TestEqual(TEXT("Moving goods along the chain audited no creation or destruction"),
		Registry.GetGoodsAuditRecordCount(), AuditCountAfterSeeding);

	// Individual holdings, so conservation cannot be satisfied by goods sitting in the wrong
	// place.
	VerifyQuantity(*this, Registry, HarvestInventory, Wheat, 0, TEXT("The harvest point is emptied of wheat"));
	VerifyQuantity(*this, Registry, StorageInventory, Wheat, 700, TEXT("Storage holds the remaining wheat"));
	VerifyQuantity(*this, Registry, MillInventory, Wheat, 500, TEXT("The mill holds the delivered wheat"));
	VerifyQuantity(*this, Registry, MillInventory, Flour, 30, TEXT("The mill kept its remaining flour"));
	VerifyQuantity(*this, Registry, BakeryInventory, Flour, 120, TEXT("The bakery holds the delivered flour"));
	VerifyQuantity(*this, Registry, BakeryInventory, Bread, 20, TEXT("The bakery kept its remaining bread"));
	VerifyQuantity(*this, Registry, HouseholdInventory, Bread, 45, TEXT("The household holds its bread"));
	VerifyQuantity(*this, Registry, HouseholdInventory, Firewood, 35, TEXT("The household holds its firewood"));
	VerifyQuantity(*this, Registry, StorageInventory, Firewood, 5, TEXT("Storage kept its remaining firewood"));

	// An emptied inventory holds no entries at all, rather than zero-quantity entries.
	TestEqual(TEXT("The emptied harvest inventory holds no entries"),
		Registry.FindInventory(HarvestInventory)->Entries.Num(), 0);

	// Goods never appeared where none were sent.
	VerifyQuantity(*this, Registry, HouseholdInventory, Wheat, 0, TEXT("No wheat reached the household"));
	VerifyQuantity(*this, Registry, MillInventory, Bread, 0, TEXT("No bread appeared at the mill"));
	VerifyQuantity(*this, Registry, HarvestInventory, Firewood, 0,
		TEXT("No firewood appeared at the harvest point"));

	// Every good type is still reachable by its durable authored key, not by creation order.
	TestTrue(TEXT("The authored keys still resolve after the whole scenario"),
		Registry.FindGoodTypeIdByKey(TEXT("Goods.Wheat")).GetValue() == Wheat
			&& Registry.FindGoodTypeIdByKey(TEXT("Goods.Firewood")).GetValue() == Firewood);

	VerifyInvariants(*this, Registry, TEXT("at the end of the headless conservation scenario"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
