#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for the authoritative settlement, property, and residence relationships.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * The small helpers below are intentionally duplicated from SimulationRegistryTests.cpp
 * rather than shared, so that the accepted Prototype 0.1A test file stays untouched.
 */

namespace
{
	constexpr EAutomationTestFlags SimulationTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/** Identifier values chosen to exercise the edges of the unsigned identifier range. */
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

	void VerifyInvariants(FAutomationTestBase& Test, const FSimulationRegistry& Registry, const TCHAR* Context)
	{
		FString FailureDescription;
		if (!Registry.ValidateInvariants(FailureDescription))
		{
			Test.AddError(FString::Printf(TEXT("Invariant broken %s: %s"), Context, *FailureDescription));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationSettlementCreationTest,
	"RealmsUnwritten.Simulation.Settlement.Creation", SimulationTestFlags)

bool FSimulationSettlementCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId FirstSettlement = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId SecondSettlement = Registry.CreateSettlement(TEXT("Eichenfurt"));

	TestTrue(TEXT("A created settlement has a valid identifier"), FirstSettlement.IsValid());
	TestTrue(TEXT("A second created settlement has a valid identifier"), SecondSettlement.IsValid());
	TestTrue(TEXT("Settlements created with the same name receive distinct identifiers"),
		FirstSettlement != SecondSettlement);
	TestEqual(TEXT("Both settlements are stored"), Registry.GetSettlementCount(), 2);

	const TOptional<FSettlementRecord> SettlementRecord = Registry.FindSettlement(FirstSettlement);
	if (!SettlementRecord.IsSet())
	{
		AddError(TEXT("A created settlement should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("A settlement's record reports its own identifier"), SettlementRecord->Id == FirstSettlement);
	TestEqual(TEXT("Stored settlement name matches creation"), SettlementRecord->Name, FString(TEXT("Eichenfurt")));
	TestEqual(TEXT("A new settlement has no properties"), SettlementRecord->Properties.Num(), 0);
	TestEqual(TEXT("A new settlement has no households"), SettlementRecord->Households.Num(), 0);

	// Invalid and extreme identifiers must fail safely across the whole unsigned range.
	const uint32 UnresolvableValues[] = {
		0u, FirstSettlement.GetValue() + 2u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FSettlementId SettlementId = FSettlementId(UnresolvableValue);

		TestFalse(FString::Printf(TEXT("Settlement identifier %u does not resolve"), UnresolvableValue),
			Registry.ContainsSettlement(SettlementId));
		TestFalse(FString::Printf(TEXT("Settlement identifier %u cannot be read"), UnresolvableValue),
			Registry.FindSettlement(SettlementId).IsSet());
	}

	TestEqual(TEXT("Failed settlement lookups create no settlements"), Registry.GetSettlementCount(), 2);

	VerifyInvariants(*this, Registry, TEXT("after creating settlements"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPropertyCreationTest,
	"RealmsUnwritten.Simulation.Property.Creation", SimulationTestFlags)

bool FSimulationPropertyCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId OtherSettlementId = Registry.CreateSettlement(TEXT("Lindenbach"));

	const FPropertyId FirstProperty = Registry.CreateProperty(SettlementId);
	const FPropertyId SecondProperty = Registry.CreateProperty(SettlementId);
	const FPropertyId OtherSettlementProperty = Registry.CreateProperty(OtherSettlementId);

	TestTrue(TEXT("A created property has a valid identifier"), FirstProperty.IsValid());
	TestTrue(TEXT("A second created property has a valid identifier"), SecondProperty.IsValid());
	TestTrue(TEXT("Properties receive distinct identifiers"), FirstProperty != SecondProperty);
	TestTrue(TEXT("Properties in different settlements also receive distinct identifiers"),
		OtherSettlementProperty != FirstProperty && OtherSettlementProperty != SecondProperty);
	TestEqual(TEXT("All three properties are stored"), Registry.GetPropertyCount(), 3);

	const TOptional<FPropertyRecord> PropertyRecord = Registry.FindProperty(FirstProperty);
	if (!PropertyRecord.IsSet())
	{
		AddError(TEXT("A created property should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("A property's record reports its own identifier"), PropertyRecord->Id == FirstProperty);
	TestTrue(TEXT("A property records the settlement it belongs to"),
		PropertyRecord->SettlementId == SettlementId);
	TestFalse(TEXT("A new property is unoccupied"), PropertyRecord->ResidentHouseholdId.IsValid());

	// The settlement side must list exactly its own properties.
	const TOptional<FSettlementRecord> SettlementRecord = Registry.FindSettlement(SettlementId);
	const TOptional<FSettlementRecord> OtherSettlementRecord = Registry.FindSettlement(OtherSettlementId);
	if (!SettlementRecord.IsSet() || !OtherSettlementRecord.IsSet())
	{
		AddError(TEXT("Both settlements should resolve"));
		return false;
	}

	TestEqual(TEXT("The settlement lists its two properties"), SettlementRecord->Properties.Num(), 2);
	TestTrue(TEXT("The settlement lists its first property"),
		SettlementRecord->Properties.Contains(FirstProperty));
	TestTrue(TEXT("The settlement lists its second property"),
		SettlementRecord->Properties.Contains(SecondProperty));
	TestFalse(TEXT("The settlement does not list another settlement's property"),
		SettlementRecord->Properties.Contains(OtherSettlementProperty));
	TestEqual(TEXT("The other settlement lists only its own property"),
		OtherSettlementRecord->Properties.Num(), 1);

	// Creating a property against an unresolvable settlement must fail atomically.
	const int32 PropertyCountBeforeRejections = Registry.GetPropertyCount();
	const uint32 UnresolvableValues[] = { 0u, 99u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FPropertyId RejectedProperty = Registry.CreateProperty(FSettlementId(UnresolvableValue));

		TestFalse(FString::Printf(TEXT("Creating a property in settlement %u returns an invalid identifier"),
				UnresolvableValue),
			RejectedProperty.IsValid());
		TestFalse(FString::Printf(TEXT("The identifier returned for settlement %u does not resolve"),
				UnresolvableValue),
			Registry.ContainsProperty(RejectedProperty));
	}

	TestEqual(TEXT("Rejected property creations add no property records"),
		Registry.GetPropertyCount(), PropertyCountBeforeRejections);

	// Rejections must not consume identifiers: the next valid creation continues the sequence.
	const FPropertyId NextProperty = Registry.CreateProperty(SettlementId);
	TestTrue(TEXT("A property can still be created after rejections"), NextProperty.IsValid());
	TestEqual(TEXT("A rejected property creation consumes no identifier"),
		static_cast<int32>(NextProperty.GetValue()),
		static_cast<int32>(OtherSettlementProperty.GetValue()) + 1);

	// Invalid and extreme property identifiers must fail safely.
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FPropertyId PropertyId = FPropertyId(UnresolvableValue);

		TestFalse(FString::Printf(TEXT("Property identifier %u does not resolve"), UnresolvableValue),
			Registry.ContainsProperty(PropertyId));
		TestFalse(FString::Printf(TEXT("Property identifier %u cannot be read"), UnresolvableValue),
			Registry.FindProperty(PropertyId).IsSet());
	}

	VerifyInvariants(*this, Registry, TEXT("after creating properties"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationHouseholdSettlementMembershipTest,
	"RealmsUnwritten.Simulation.Household.SettlementMembership", SimulationTestFlags)

bool FSimulationHouseholdSettlementMembershipTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId OriginSettlement = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId DestinationSettlement = Registry.CreateSettlement(TEXT("Lindenbach"));
	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Baumann"));

	TestTrue(TEXT("A household can be placed into a settlement"),
		Registry.PlaceHouseholdInSettlement(HouseholdId, OriginSettlement)
			== ESettlementMembershipResult::Success);

	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
		const TOptional<FSettlementRecord> SettlementRecord = Registry.FindSettlement(OriginSettlement);
		if (!HouseholdRecord.IsSet() || !SettlementRecord.IsSet())
		{
			AddError(TEXT("Both records should resolve after a settlement placement"));
			return false;
		}

		TestTrue(TEXT("The household side records the settlement"),
			HouseholdRecord->SettlementId == OriginSettlement);
		TestEqual(TEXT("The settlement side lists one household"), SettlementRecord->Households.Num(), 1);
		TestTrue(TEXT("The settlement side lists the household"),
			SettlementRecord->Households.Contains(HouseholdId));

		// A household in a settlement need not occupy anything.
		TestFalse(TEXT("A household placed in a settlement has no residence"),
			HouseholdRecord->ResidenceId.IsValid());
	}

	TestTrue(TEXT("Re-placing a household reports an existing membership"),
		Registry.PlaceHouseholdInSettlement(HouseholdId, OriginSettlement)
			== ESettlementMembershipResult::AlreadyMember);
	TestEqual(TEXT("Re-placing a household does not duplicate membership"),
		Registry.FindSettlement(OriginSettlement)->Households.Num(), 1);

	// Moving between settlements must leave no stale membership behind.
	TestTrue(TEXT("A household can move to another settlement"),
		Registry.PlaceHouseholdInSettlement(HouseholdId, DestinationSettlement)
			== ESettlementMembershipResult::Success);

	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
		const TOptional<FSettlementRecord> OriginRecord = Registry.FindSettlement(OriginSettlement);
		const TOptional<FSettlementRecord> DestinationRecord = Registry.FindSettlement(DestinationSettlement);
		if (!HouseholdRecord.IsSet() || !OriginRecord.IsSet() || !DestinationRecord.IsSet())
		{
			AddError(TEXT("All records should resolve after a settlement move"));
			return false;
		}

		TestTrue(TEXT("The household claims exactly the destination settlement"),
			HouseholdRecord->SettlementId == DestinationSettlement);
		TestEqual(TEXT("The origin settlement has no stale households"), OriginRecord->Households.Num(), 0);
		TestEqual(TEXT("The destination settlement lists the household once"),
			DestinationRecord->Households.Num(), 1);
		TestTrue(TEXT("The destination settlement lists the household"),
			DestinationRecord->Households.Contains(HouseholdId));
	}

	// Invalid operations must fail without touching authoritative state.
	const FHouseholdId UnknownHousehold = FHouseholdId(UInt32MaxIdValue);
	const FSettlementId UnknownSettlement = FSettlementId(SignBitIdValue);

	TestTrue(TEXT("Placing an unknown household is rejected"),
		Registry.PlaceHouseholdInSettlement(UnknownHousehold, OriginSettlement)
			== ESettlementMembershipResult::UnknownHousehold);
	TestTrue(TEXT("Placing into an unknown settlement is rejected"),
		Registry.PlaceHouseholdInSettlement(HouseholdId, UnknownSettlement)
			== ESettlementMembershipResult::UnknownSettlement);
	TestTrue(TEXT("Placing with a default settlement identifier is rejected"),
		Registry.PlaceHouseholdInSettlement(HouseholdId, FSettlementId())
			== ESettlementMembershipResult::UnknownSettlement);
	TestTrue(TEXT("Removing from a settlement the household is not in is rejected"),
		Registry.RemoveHouseholdFromSettlement(HouseholdId, OriginSettlement)
			== ESettlementMembershipResult::NotAMember);
	TestTrue(TEXT("Removing an unknown household is rejected"),
		Registry.RemoveHouseholdFromSettlement(UnknownHousehold, DestinationSettlement)
			== ESettlementMembershipResult::UnknownHousehold);

	TestTrue(TEXT("Rejected settlement operations leave membership unchanged"),
		Registry.FindHousehold(HouseholdId)->SettlementId == DestinationSettlement);
	TestEqual(TEXT("Rejected settlement operations leave the settlement side unchanged"),
		Registry.FindSettlement(DestinationSettlement)->Households.Num(), 1);

	// Leaving a settlement clears both sides.
	TestTrue(TEXT("A household can be removed from its settlement"),
		Registry.RemoveHouseholdFromSettlement(HouseholdId, DestinationSettlement)
			== ESettlementMembershipResult::Success);
	TestFalse(TEXT("Removal clears the household side"),
		Registry.FindHousehold(HouseholdId)->SettlementId.IsValid());
	TestEqual(TEXT("Removal clears the settlement side"),
		Registry.FindSettlement(DestinationSettlement)->Households.Num(), 0);

	VerifyInvariants(*this, Registry, TEXT("after settlement membership changes"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationResidenceAssignmentTest,
	"RealmsUnwritten.Simulation.Residence.Assignment", SimulationTestFlags)

bool FSimulationResidenceAssignmentTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId FirstProperty = Registry.CreateProperty(SettlementId);
	const FPropertyId SecondProperty = Registry.CreateProperty(SettlementId);
	const FHouseholdId FirstHousehold = Registry.CreateHousehold(TEXT("Baumann"));
	const FHouseholdId SecondHousehold = Registry.CreateHousehold(TEXT("Reinmar"));

	Registry.PlaceHouseholdInSettlement(FirstHousehold, SettlementId);
	Registry.PlaceHouseholdInSettlement(SecondHousehold, SettlementId);

	TestTrue(TEXT("A household can occupy a property in its own settlement"),
		Registry.AssignHouseholdResidence(FirstHousehold, FirstProperty) == EResidenceResult::Success);

	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(FirstHousehold);
		const TOptional<FPropertyRecord> PropertyRecord = Registry.FindProperty(FirstProperty);
		if (!HouseholdRecord.IsSet() || !PropertyRecord.IsSet())
		{
			AddError(TEXT("Both records should resolve after a residence assignment"));
			return false;
		}

		TestTrue(TEXT("The household side records its residence"),
			HouseholdRecord->ResidenceId == FirstProperty);
		TestTrue(TEXT("The property side records its occupant"),
			PropertyRecord->ResidentHouseholdId == FirstHousehold);
		TestTrue(TEXT("Taking up residence does not change settlement membership"),
			HouseholdRecord->SettlementId == SettlementId);
	}

	// Re-assigning the same property must be a reported no-op.
	TestTrue(TEXT("Re-assigning the same property reports an existing residence"),
		Registry.AssignHouseholdResidence(FirstHousehold, FirstProperty) == EResidenceResult::AlreadyResident);
	TestTrue(TEXT("Re-assigning the same property does not change the household side"),
		Registry.FindHousehold(FirstHousehold)->ResidenceId == FirstProperty);
	TestTrue(TEXT("Re-assigning the same property does not change the property side"),
		Registry.FindProperty(FirstProperty)->ResidentHouseholdId == FirstHousehold);

	// A property houses at most one household.
	TestTrue(TEXT("A second household cannot occupy an occupied property"),
		Registry.AssignHouseholdResidence(SecondHousehold, FirstProperty) == EResidenceResult::PropertyOccupied);
	TestFalse(TEXT("The rejected household gained no residence"),
		Registry.FindHousehold(SecondHousehold)->ResidenceId.IsValid());
	TestTrue(TEXT("The occupied property kept its original occupant"),
		Registry.FindProperty(FirstProperty)->ResidentHouseholdId == FirstHousehold);

	// Moving residence vacates the previous property, so a household never occupies two.
	TestTrue(TEXT("A household can move to another property"),
		Registry.AssignHouseholdResidence(FirstHousehold, SecondProperty) == EResidenceResult::Success);

	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(FirstHousehold);
		const TOptional<FPropertyRecord> OriginRecord = Registry.FindProperty(FirstProperty);
		const TOptional<FPropertyRecord> DestinationRecord = Registry.FindProperty(SecondProperty);
		if (!HouseholdRecord.IsSet() || !OriginRecord.IsSet() || !DestinationRecord.IsSet())
		{
			AddError(TEXT("All records should resolve after a residence move"));
			return false;
		}

		TestTrue(TEXT("The household claims exactly the new property"),
			HouseholdRecord->ResidenceId == SecondProperty);
		TestFalse(TEXT("The previous property is vacated"), OriginRecord->ResidentHouseholdId.IsValid());
		TestTrue(TEXT("The new property records the occupant"),
			DestinationRecord->ResidentHouseholdId == FirstHousehold);
	}

	// The vacated property can now house the other household.
	TestTrue(TEXT("A vacated property can be occupied by another household"),
		Registry.AssignHouseholdResidence(SecondHousehold, FirstProperty) == EResidenceResult::Success);
	TestTrue(TEXT("Both households now occupy one property each"),
		Registry.FindProperty(FirstProperty)->ResidentHouseholdId == SecondHousehold
			&& Registry.FindProperty(SecondProperty)->ResidentHouseholdId == FirstHousehold);

	VerifyInvariants(*this, Registry, TEXT("after residence assignments"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationResidenceRemovalTest,
	"RealmsUnwritten.Simulation.Residence.Removal", SimulationTestFlags)

bool FSimulationResidenceRemovalTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FPropertyId OccupiedProperty = Registry.CreateProperty(SettlementId);
	const FPropertyId VacantProperty = Registry.CreateProperty(SettlementId);
	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Baumann"));

	Registry.PlaceHouseholdInSettlement(HouseholdId, SettlementId);
	Registry.AssignHouseholdResidence(HouseholdId, OccupiedProperty);

	// Naming the wrong property must not vacate the real one.
	TestTrue(TEXT("Removing residence from a property the household does not occupy is rejected"),
		Registry.RemoveHouseholdResidence(HouseholdId, VacantProperty) == EResidenceResult::NotResident);
	TestTrue(TEXT("Removing an unknown household is rejected"),
		Registry.RemoveHouseholdResidence(FHouseholdId(UInt32MaxIdValue), OccupiedProperty)
			== EResidenceResult::UnknownHousehold);
	TestTrue(TEXT("Removing an unknown property is rejected"),
		Registry.RemoveHouseholdResidence(HouseholdId, FPropertyId(SignBitIdValue))
			== EResidenceResult::UnknownProperty);
	TestTrue(TEXT("Removing with a default property identifier is rejected"),
		Registry.RemoveHouseholdResidence(HouseholdId, FPropertyId()) == EResidenceResult::UnknownProperty);

	TestTrue(TEXT("Rejected removals leave the household side unchanged"),
		Registry.FindHousehold(HouseholdId)->ResidenceId == OccupiedProperty);
	TestTrue(TEXT("Rejected removals leave the property side unchanged"),
		Registry.FindProperty(OccupiedProperty)->ResidentHouseholdId == HouseholdId);

	// A valid removal clears both sides and nothing else.
	TestTrue(TEXT("A household can vacate the property it occupies"),
		Registry.RemoveHouseholdResidence(HouseholdId, OccupiedProperty) == EResidenceResult::Success);

	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
		const TOptional<FPropertyRecord> PropertyRecord = Registry.FindProperty(OccupiedProperty);
		if (!HouseholdRecord.IsSet() || !PropertyRecord.IsSet())
		{
			AddError(TEXT("Both records should resolve after a residence removal"));
			return false;
		}

		TestFalse(TEXT("Removal clears the household side"), HouseholdRecord->ResidenceId.IsValid());
		TestFalse(TEXT("Removal clears the property side"), PropertyRecord->ResidentHouseholdId.IsValid());
		TestTrue(TEXT("Vacating does not remove the household from its settlement"),
			HouseholdRecord->SettlementId == SettlementId);
	}

	TestTrue(TEXT("Vacating twice is rejected"),
		Registry.RemoveHouseholdResidence(HouseholdId, OccupiedProperty) == EResidenceResult::NotResident);

	VerifyInvariants(*this, Registry, TEXT("after residence removals"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationResidenceSettlementBoundaryTest,
	"RealmsUnwritten.Simulation.Residence.SettlementBoundary", SimulationTestFlags)

bool FSimulationResidenceSettlementBoundaryTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId HomeSettlement = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId ForeignSettlement = Registry.CreateSettlement(TEXT("Lindenbach"));
	const FPropertyId HomeProperty = Registry.CreateProperty(HomeSettlement);
	const FPropertyId ForeignProperty = Registry.CreateProperty(ForeignSettlement);
	const FHouseholdId SettledHousehold = Registry.CreateHousehold(TEXT("Baumann"));
	const FHouseholdId UnplacedHousehold = Registry.CreateHousehold(TEXT("Wanderer"));

	Registry.PlaceHouseholdInSettlement(SettledHousehold, HomeSettlement);

	// A household may not occupy a property in a settlement it does not belong to.
	TestTrue(TEXT("A household cannot occupy a property in another settlement"),
		Registry.AssignHouseholdResidence(SettledHousehold, ForeignProperty)
			== EResidenceResult::SettlementMismatch);
	TestFalse(TEXT("The rejected household gained no residence"),
		Registry.FindHousehold(SettledHousehold)->ResidenceId.IsValid());
	TestFalse(TEXT("The foreign property gained no occupant"),
		Registry.FindProperty(ForeignProperty)->ResidentHouseholdId.IsValid());

	// A household in no settlement cannot occupy anything.
	TestTrue(TEXT("A household in no settlement cannot take up residence"),
		Registry.AssignHouseholdResidence(UnplacedHousehold, HomeProperty)
			== EResidenceResult::HouseholdNotInSettlement);
	TestFalse(TEXT("The unplaced household gained no residence"),
		Registry.FindHousehold(UnplacedHousehold)->ResidenceId.IsValid());
	TestFalse(TEXT("The home property gained no occupant"),
		Registry.FindProperty(HomeProperty)->ResidentHouseholdId.IsValid());

	// Unresolvable identifiers on either side must fail safely.
	const uint32 UnresolvableValues[] = { 0u, 99u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestTrue(FString::Printf(TEXT("Assigning residence for household %u is rejected"), UnresolvableValue),
			Registry.AssignHouseholdResidence(FHouseholdId(UnresolvableValue), HomeProperty)
				== EResidenceResult::UnknownHousehold);
		TestTrue(FString::Printf(TEXT("Assigning residence on property %u is rejected"), UnresolvableValue),
			Registry.AssignHouseholdResidence(SettledHousehold, FPropertyId(UnresolvableValue))
				== EResidenceResult::UnknownProperty);
	}

	// A housed household cannot be relocated to another settlement while it still occupies a
	// property; that is what keeps cross-settlement residence unreachable.
	TestTrue(TEXT("The household takes up residence at home"),
		Registry.AssignHouseholdResidence(SettledHousehold, HomeProperty) == EResidenceResult::Success);
	TestTrue(TEXT("A housed household cannot be moved to another settlement"),
		Registry.PlaceHouseholdInSettlement(SettledHousehold, ForeignSettlement)
			== ESettlementMembershipResult::StillResident);
	TestTrue(TEXT("A housed household cannot be removed from its settlement"),
		Registry.RemoveHouseholdFromSettlement(SettledHousehold, HomeSettlement)
			== ESettlementMembershipResult::StillResident);

	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(SettledHousehold);
		if (!HouseholdRecord.IsSet())
		{
			AddError(TEXT("The settled household should resolve"));
			return false;
		}

		TestTrue(TEXT("The rejected relocation left settlement membership unchanged"),
			HouseholdRecord->SettlementId == HomeSettlement);
		TestTrue(TEXT("The rejected relocation left the residence unchanged"),
			HouseholdRecord->ResidenceId == HomeProperty);
		TestEqual(TEXT("The foreign settlement gained no household"),
			Registry.FindSettlement(ForeignSettlement)->Households.Num(), 0);
	}

	// Relocating properly: vacate, move, then take up a new residence.
	TestTrue(TEXT("The household vacates its property"),
		Registry.RemoveHouseholdResidence(SettledHousehold, HomeProperty) == EResidenceResult::Success);
	TestTrue(TEXT("The household moves to the other settlement"),
		Registry.PlaceHouseholdInSettlement(SettledHousehold, ForeignSettlement)
			== ESettlementMembershipResult::Success);
	TestTrue(TEXT("The household occupies a property in its new settlement"),
		Registry.AssignHouseholdResidence(SettledHousehold, ForeignProperty) == EResidenceResult::Success);
	TestFalse(TEXT("The property in the previous settlement is still vacant"),
		Registry.FindProperty(HomeProperty)->ResidentHouseholdId.IsValid());

	VerifyInvariants(*this, Registry, TEXT("after cross-settlement residence attempts"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationResidenceCrossSettlementAtomicityTest,
	"RealmsUnwritten.Simulation.Residence.CrossSettlementAtomicity", SimulationTestFlags)

bool FSimulationResidenceCrossSettlementAtomicityTest::RunTest(const FString& Parameters)
{
	// A rejected cross-settlement assignment must leave an already valid residence intact,
	// rather than only refusing a household that had nothing to lose.
	FSimulationRegistry Registry;

	const FSettlementId SettlementA = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId SettlementB = Registry.CreateSettlement(TEXT("Lindenbach"));
	const FPropertyId PropertyA = Registry.CreateProperty(SettlementA);
	const FPropertyId PropertyB = Registry.CreateProperty(SettlementB);
	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Baumann"));

	TestTrue(TEXT("The household is placed in settlement A"),
		Registry.PlaceHouseholdInSettlement(HouseholdId, SettlementA)
			== ESettlementMembershipResult::Success);
	TestTrue(TEXT("The household takes up residence on property A"),
		Registry.AssignHouseholdResidence(HouseholdId, PropertyA) == EResidenceResult::Success);

	VerifyInvariants(*this, Registry, TEXT("before the rejected cross-settlement assignment"));

	TestTrue(TEXT("Assigning a housed household to a property in another settlement is rejected"),
		Registry.AssignHouseholdResidence(HouseholdId, PropertyB) == EResidenceResult::SettlementMismatch);

	const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
	const TOptional<FPropertyRecord> PropertyARecord = Registry.FindProperty(PropertyA);
	const TOptional<FPropertyRecord> PropertyBRecord = Registry.FindProperty(PropertyB);
	const TOptional<FSettlementRecord> SettlementARecord = Registry.FindSettlement(SettlementA);
	const TOptional<FSettlementRecord> SettlementBRecord = Registry.FindSettlement(SettlementB);
	if (!HouseholdRecord.IsSet() || !PropertyARecord.IsSet() || !PropertyBRecord.IsSet()
		|| !SettlementARecord.IsSet() || !SettlementBRecord.IsSet())
	{
		AddError(TEXT("Every record should still resolve after a rejected assignment"));
		return false;
	}

	// The household keeps both sides of the residence it already had.
	TestTrue(TEXT("The household is still in settlement A"), HouseholdRecord->SettlementId == SettlementA);
	TestTrue(TEXT("The household still resides on property A"), HouseholdRecord->ResidenceId == PropertyA);
	TestTrue(TEXT("Property A still records the household as its occupant"),
		PropertyARecord->ResidentHouseholdId == HouseholdId);
	TestFalse(TEXT("Property B is still unoccupied"), PropertyBRecord->ResidentHouseholdId.IsValid());

	// Neither settlement's membership or property list moved.
	TestEqual(TEXT("Settlement A still lists exactly one household"), SettlementARecord->Households.Num(), 1);
	TestTrue(TEXT("Settlement A still lists the household"),
		SettlementARecord->Households.Contains(HouseholdId));
	TestEqual(TEXT("Settlement A still lists exactly its own property"),
		SettlementARecord->Properties.Num(), 1);
	TestTrue(TEXT("Settlement A still lists property A"), SettlementARecord->Properties.Contains(PropertyA));

	TestEqual(TEXT("Settlement B still lists no households"), SettlementBRecord->Households.Num(), 0);
	TestEqual(TEXT("Settlement B still lists exactly its own property"),
		SettlementBRecord->Properties.Num(), 1);
	TestTrue(TEXT("Settlement B still lists property B"), SettlementBRecord->Properties.Contains(PropertyB));

	VerifyInvariants(*this, Registry, TEXT("after the rejected cross-settlement assignment"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationResidencePeopleUnaffectedTest,
	"RealmsUnwritten.Simulation.Residence.PeopleUnaffected", SimulationTestFlags)

bool FSimulationResidencePeopleUnaffectedTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FSettlementId OriginSettlement = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId DestinationSettlement = Registry.CreateSettlement(TEXT("Lindenbach"));
	const FPropertyId OriginProperty = Registry.CreateProperty(OriginSettlement);
	const FPropertyId DestinationProperty = Registry.CreateProperty(DestinationSettlement);

	const FHouseholdId HousedHousehold = Registry.CreateHousehold(TEXT("Baumann"));
	const FHouseholdId UnhousedHousehold = Registry.CreateHousehold(TEXT("Wanderer"));

	// One household with several people, as a household occupying a single property must allow.
	TArray<FPersonId> HousedMembers;
	HousedMembers.Add(Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 41)));
	HousedMembers.Add(Registry.CreatePerson(MakePersonParams(TEXT("Mechthild"), TEXT("Baumann"), 38)));
	HousedMembers.Add(Registry.CreatePerson(MakePersonParams(TEXT("Adelheid"), TEXT("Baumann"), 12)));
	for (const FPersonId PersonId : HousedMembers)
	{
		Registry.AddPersonToHousehold(PersonId, HousedHousehold);
	}

	const FPersonId UnhousedPerson = Registry.CreatePerson(MakePersonParams(TEXT("Hedwig"), TEXT("Wanderer"), 27));
	Registry.AddPersonToHousehold(UnhousedPerson, UnhousedHousehold);

	Registry.PlaceHouseholdInSettlement(HousedHousehold, OriginSettlement);
	Registry.PlaceHouseholdInSettlement(UnhousedHousehold, OriginSettlement);
	TestTrue(TEXT("A household with several people can occupy one property"),
		Registry.AssignHouseholdResidence(HousedHousehold, OriginProperty) == EResidenceResult::Success);

	// A household may sit in a settlement with no property at all.
	{
		const TOptional<FHouseholdRecord> UnhousedRecord = Registry.FindHousehold(UnhousedHousehold);
		if (!UnhousedRecord.IsSet())
		{
			AddError(TEXT("The unhoused household should resolve"));
			return false;
		}

		TestTrue(TEXT("The unhoused household is in a settlement"),
			UnhousedRecord->SettlementId == OriginSettlement);
		TestFalse(TEXT("The unhoused household has no residence"), UnhousedRecord->ResidenceId.IsValid());
		TestEqual(TEXT("The unhoused household still has its member"), UnhousedRecord->Members.Num(), 1);
	}

	// Relocating the housed household must not disturb its people.
	Registry.RemoveHouseholdResidence(HousedHousehold, OriginProperty);
	Registry.PlaceHouseholdInSettlement(HousedHousehold, DestinationSettlement);
	Registry.AssignHouseholdResidence(HousedHousehold, DestinationProperty);

	const TOptional<FHouseholdRecord> HousedRecord = Registry.FindHousehold(HousedHousehold);
	if (!HousedRecord.IsSet())
	{
		AddError(TEXT("The housed household should resolve"));
		return false;
	}

	TestTrue(TEXT("The household ended in the destination settlement"),
		HousedRecord->SettlementId == DestinationSettlement);
	TestTrue(TEXT("The household ended on the destination property"),
		HousedRecord->ResidenceId == DestinationProperty);
	TestEqual(TEXT("The household kept all of its members through the move"),
		HousedRecord->Members.Num(), HousedMembers.Num());

	for (const FPersonId PersonId : HousedMembers)
	{
		const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
		if (!PersonRecord.IsSet())
		{
			AddError(TEXT("Every household member should still resolve"));
			return false;
		}

		TestTrue(TEXT("A member still belongs to the household after the move"),
			PersonRecord->HouseholdId == HousedHousehold);
		TestTrue(TEXT("The household still lists the member after the move"),
			HousedRecord->Members.Contains(PersonId));
	}

	// People carry no settlement or residence of their own in this slice; those live on the
	// household, so moving a household is the only thing that relocates its people.
	TestTrue(TEXT("The unhoused household's person is untouched"),
		Registry.FindPerson(UnhousedPerson)->HouseholdId == UnhousedHousehold);

	VerifyInvariants(*this, Registry, TEXT("after relocating a household with members"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationHeadlessSettlementScenarioTest,
	"RealmsUnwritten.Simulation.Scenario.HeadlessSettlementNetwork", SimulationTestFlags)

bool FSimulationHeadlessSettlementScenarioTest::RunTest(const FString& Parameters)
{
	// Two settlements, five properties, four households, nine people, three of the households
	// housed and one deliberately unhoused - all with no Actor, no world, and no map.
	FSimulationRegistry Registry;

	const FSettlementId Eichenfurt = Registry.CreateSettlement(TEXT("Eichenfurt"));
	const FSettlementId Lindenbach = Registry.CreateSettlement(TEXT("Lindenbach"));

	TArray<FPropertyId> EichenfurtProperties;
	for (int32 PropertyIndex = 0; PropertyIndex < 3; ++PropertyIndex)
	{
		EichenfurtProperties.Add(Registry.CreateProperty(Eichenfurt));
	}

	TArray<FPropertyId> LindenbachProperties;
	for (int32 PropertyIndex = 0; PropertyIndex < 2; ++PropertyIndex)
	{
		LindenbachProperties.Add(Registry.CreateProperty(Lindenbach));
	}

	const FHouseholdId Baumann = Registry.CreateHousehold(TEXT("Baumann"));
	const FHouseholdId Reinmar = Registry.CreateHousehold(TEXT("Reinmar"));
	const FHouseholdId Vogt = Registry.CreateHousehold(TEXT("Vogt"));
	const FHouseholdId Newcomer = Registry.CreateHousehold(TEXT("Newcomer"));

	// Nine people spread across the four households.
	const FHouseholdId HouseholdByPerson[] = {
		Baumann, Baumann, Baumann, Reinmar, Reinmar, Vogt, Vogt, Newcomer, Newcomer };
	TArray<FPersonId> People;
	for (const FHouseholdId HouseholdId : HouseholdByPerson)
	{
		const FPersonId PersonId = Registry.CreatePerson(
			MakePersonParams(TEXT("Villager"), TEXT("Franconian"), 20 + People.Num()));
		Registry.AddPersonToHousehold(PersonId, HouseholdId);
		People.Add(PersonId);
	}

	TestEqual(TEXT("The scenario holds nine people"), Registry.GetPersonCount(), 9);
	TestEqual(TEXT("The scenario holds four households"), Registry.GetHouseholdCount(), 4);
	TestEqual(TEXT("The scenario holds two settlements"), Registry.GetSettlementCount(), 2);
	TestEqual(TEXT("The scenario holds five properties"), Registry.GetPropertyCount(), 5);

	// Place all four households; house three of them and leave the newcomer waiting.
	Registry.PlaceHouseholdInSettlement(Baumann, Eichenfurt);
	Registry.PlaceHouseholdInSettlement(Reinmar, Eichenfurt);
	Registry.PlaceHouseholdInSettlement(Vogt, Lindenbach);
	Registry.PlaceHouseholdInSettlement(Newcomer, Eichenfurt);

	TestTrue(TEXT("Baumann is housed"),
		Registry.AssignHouseholdResidence(Baumann, EichenfurtProperties[0]) == EResidenceResult::Success);
	TestTrue(TEXT("Reinmar is housed"),
		Registry.AssignHouseholdResidence(Reinmar, EichenfurtProperties[1]) == EResidenceResult::Success);
	TestTrue(TEXT("Vogt is housed"),
		Registry.AssignHouseholdResidence(Vogt, LindenbachProperties[0]) == EResidenceResult::Success);

	VerifyInvariants(*this, Registry, TEXT("after settling the initial scenario"));

	// Baumann moves within Eichenfurt; Vogt relocates to Eichenfurt entirely; the newcomer
	// finally takes the property Baumann vacated.
	TestTrue(TEXT("Baumann moves to another property in the same settlement"),
		Registry.AssignHouseholdResidence(Baumann, EichenfurtProperties[2]) == EResidenceResult::Success);
	TestTrue(TEXT("Vogt vacates its Lindenbach property"),
		Registry.RemoveHouseholdResidence(Vogt, LindenbachProperties[0]) == EResidenceResult::Success);
	TestTrue(TEXT("Vogt relocates to Eichenfurt"),
		Registry.PlaceHouseholdInSettlement(Vogt, Eichenfurt) == ESettlementMembershipResult::Success);
	TestTrue(TEXT("The newcomer takes the property Baumann vacated"),
		Registry.AssignHouseholdResidence(Newcomer, EichenfurtProperties[0]) == EResidenceResult::Success);

	VerifyInvariants(*this, Registry, TEXT("after scenario membership and residence moves"));

	// Cross-check both sides of every relationship, and the unhoused/vacant tallies.
	int32 HousedHouseholds = 0;
	int32 HouseholdsInEichenfurt = 0;
	for (const FHouseholdId HouseholdId : { Baumann, Reinmar, Vogt, Newcomer })
	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
		if (!HouseholdRecord.IsSet())
		{
			AddError(TEXT("Every scenario household should resolve"));
			return false;
		}

		if (HouseholdRecord->SettlementId == Eichenfurt)
		{
			++HouseholdsInEichenfurt;
		}

		if (!HouseholdRecord->ResidenceId.IsValid())
		{
			continue;
		}

		++HousedHouseholds;

		const TOptional<FPropertyRecord> PropertyRecord = Registry.FindProperty(HouseholdRecord->ResidenceId);
		if (!PropertyRecord.IsSet())
		{
			AddError(TEXT("Every occupied property should resolve"));
			return false;
		}

		TestTrue(TEXT("A housed household and its property agree"),
			PropertyRecord->ResidentHouseholdId == HouseholdId);
		TestTrue(TEXT("A household resides in its own settlement"),
			PropertyRecord->SettlementId == HouseholdRecord->SettlementId);
	}

	TestEqual(TEXT("Three of the four households are housed"), HousedHouseholds, 3);
	TestEqual(TEXT("All four households ended up in Eichenfurt"), HouseholdsInEichenfurt, 4);

	const TOptional<FSettlementRecord> EichenfurtRecord = Registry.FindSettlement(Eichenfurt);
	const TOptional<FSettlementRecord> LindenbachRecord = Registry.FindSettlement(Lindenbach);
	if (!EichenfurtRecord.IsSet() || !LindenbachRecord.IsSet())
	{
		AddError(TEXT("Both scenario settlements should resolve"));
		return false;
	}

	TestEqual(TEXT("Eichenfurt lists all four households"), EichenfurtRecord->Households.Num(), 4);
	TestEqual(TEXT("Lindenbach lists no households"), LindenbachRecord->Households.Num(), 0);
	TestEqual(TEXT("Eichenfurt still lists its three properties"), EichenfurtRecord->Properties.Num(), 3);
	TestEqual(TEXT("Lindenbach still lists its two properties"), LindenbachRecord->Properties.Num(), 2);

	// Vogt is the unhoused household now, and Lindenbach's properties are both vacant.
	TestFalse(TEXT("Vogt is in a settlement but unhoused"),
		Registry.FindHousehold(Vogt)->ResidenceId.IsValid());
	for (const FPropertyId PropertyId : LindenbachProperties)
	{
		TestFalse(TEXT("A Lindenbach property is vacant"),
			Registry.FindProperty(PropertyId)->ResidentHouseholdId.IsValid());
	}

	// The 0.1A person/household relationships survived all of it.
	for (int32 PersonIndex = 0; PersonIndex < People.Num(); ++PersonIndex)
	{
		const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(People[PersonIndex]);
		if (!PersonRecord.IsSet())
		{
			AddError(TEXT("Every scenario person should resolve"));
			return false;
		}

		TestTrue(TEXT("A person still belongs to the household they joined"),
			PersonRecord->HouseholdId == HouseholdByPerson[PersonIndex]);
	}

	VerifyInvariants(*this, Registry, TEXT("at the end of the headless scenario"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
