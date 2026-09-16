#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "UObject/Object.h"

#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for the authoritative person/household foundation.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Identifier type-safety assertions live with the types in SimulationIds.h so they hold in
 * every build, not only where tests are compiled.
 */

static_assert(!std::is_base_of_v<UObject, FSimulationRegistry>,
	"The authoritative simulation registry must not be a UObject or an Actor.");

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

	/** An identifier that is well formed but was never allocated by the registry. */
	template <typename TIdType>
	TIdType MakeUnallocatedId()
	{
		return TIdType(9999);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryCreationTest,
	"RealmsUnwritten.Simulation.Registry.Creation", SimulationTestFlags)

bool FSimulationRegistryCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId FirstHousehold = Registry.CreateHousehold(TEXT("Baumann"));
	const FHouseholdId SecondHousehold = Registry.CreateHousehold(TEXT("Baumann"));

	TestTrue(TEXT("A created household has a valid identifier"), FirstHousehold.IsValid());
	TestTrue(TEXT("A second created household has a valid identifier"), SecondHousehold.IsValid());
	TestTrue(TEXT("Households created with the same name receive distinct identifiers"),
		FirstHousehold != SecondHousehold);
	TestEqual(TEXT("Both households are stored"), Registry.GetHouseholdCount(), 2);

	const FPersonId FirstPerson = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));
	const FPersonId SecondPerson = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));

	TestTrue(TEXT("A created person has a valid identifier"), FirstPerson.IsValid());
	TestTrue(TEXT("A second created person has a valid identifier"), SecondPerson.IsValid());
	TestTrue(TEXT("Identically named people receive distinct identifiers"), FirstPerson != SecondPerson);
	TestEqual(TEXT("Both people are stored"), Registry.GetPersonCount(), 2);

	// Names are display data, so identical names must still describe two separate records.
	const TOptional<FPersonRecord> FirstRecord = Registry.FindPerson(FirstPerson);
	const TOptional<FPersonRecord> SecondRecord = Registry.FindPerson(SecondPerson);
	if (!FirstRecord.IsSet() || !SecondRecord.IsSet())
	{
		AddError(TEXT("Both created people should resolve to records"));
		return false;
	}

	TestTrue(TEXT("Each person's record reports its own identifier"),
		FirstRecord->Id == FirstPerson && SecondRecord->Id == SecondPerson);
	TestTrue(TEXT("Identically named people remain separate identities"),
		FirstRecord->Id != SecondRecord->Id);
	TestEqual(TEXT("Stored given name matches creation"), FirstRecord->GivenName, FString(TEXT("Konrad")));
	TestEqual(TEXT("Stored family name matches creation"), FirstRecord->FamilyName, FString(TEXT("Baumann")));
	TestEqual(TEXT("Stored age matches creation"), FirstRecord->AgeYears, 34);
	TestTrue(TEXT("A new person is alive"), FirstRecord->LifeState == EPersonLifeState::Alive);
	TestFalse(TEXT("A new person belongs to no household"), FirstRecord->HouseholdId.IsValid());

	const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(FirstHousehold);
	if (!HouseholdRecord.IsSet())
	{
		AddError(TEXT("A created household should resolve to a record"));
		return false;
	}

	TestTrue(TEXT("A household's record reports its own identifier"), HouseholdRecord->Id == FirstHousehold);
	TestEqual(TEXT("A new household has no members"), HouseholdRecord->Members.Num(), 0);
	TestEqual(TEXT("Stored household name matches creation"), HouseholdRecord->Name, FString(TEXT("Baumann")));

	VerifyInvariants(*this, Registry, TEXT("after creating people and households"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryCreationValidationTest,
	"RealmsUnwritten.Simulation.Registry.CreationValidation", SimulationTestFlags)

bool FSimulationRegistryCreationValidationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId ExistingPerson = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));
	const int32 PersonCountBeforeRejections = Registry.GetPersonCount();

	// A negative age must never reach authoritative state.
	const int32 RejectedAges[] = { -1, -34, MIN_int32 };
	for (const int32 RejectedAge : RejectedAges)
	{
		const FPersonId RejectedPerson =
			Registry.CreatePerson(MakePersonParams(TEXT("Unborn"), TEXT("Baumann"), RejectedAge));

		TestFalse(FString::Printf(TEXT("Creating a person aged %d returns an invalid identifier"), RejectedAge),
			RejectedPerson.IsValid());
		TestFalse(FString::Printf(TEXT("The identifier returned for age %d does not resolve"), RejectedAge),
			Registry.ContainsPerson(RejectedPerson));
		TestFalse(FString::Printf(TEXT("No record can be read for the rejected age %d"), RejectedAge),
			Registry.FindPerson(RejectedPerson).IsSet());
	}

	TestEqual(TEXT("Rejected creations add no person records"),
		Registry.GetPersonCount(), PersonCountBeforeRejections);
	TestTrue(TEXT("The person created before the rejections is untouched"),
		Registry.ContainsPerson(ExistingPerson));

	// Rejections must not consume identifiers: the next valid creation continues the sequence.
	const FPersonId NextPerson = Registry.CreatePerson(MakePersonParams(TEXT("Mechthild"), TEXT("Baumann"), 0));
	TestTrue(TEXT("A person aged zero is accepted"), NextPerson.IsValid());
	TestEqual(TEXT("A rejected creation consumes no identifier"),
		static_cast<int32>(NextPerson.GetValue()), static_cast<int32>(ExistingPerson.GetValue()) + 1);
	TestEqual(TEXT("Only the accepted creations are stored"), Registry.GetPersonCount(), 2);

	const TOptional<FPersonRecord> NextRecord = Registry.FindPerson(NextPerson);
	if (!NextRecord.IsSet())
	{
		AddError(TEXT("The accepted person should resolve"));
		return false;
	}

	TestEqual(TEXT("An age of zero is stored as given"), NextRecord->AgeYears, 0);

	VerifyInvariants(*this, Registry, TEXT("after rejected person creations"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryIdentifierBoundaryTest,
	"RealmsUnwritten.Simulation.Registry.IdentifierBoundaries", SimulationTestFlags)

bool FSimulationRegistryIdentifierBoundaryTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId AllocatedHousehold = Registry.CreateHousehold(TEXT("Baumann"));
	const FPersonId AllocatedPerson = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));
	Registry.AddPersonToHousehold(AllocatedPerson, AllocatedHousehold);

	// The allocated identifiers resolve.
	TestTrue(TEXT("The allocated person resolves"), Registry.ContainsPerson(AllocatedPerson));
	TestTrue(TEXT("The allocated household resolves"), Registry.ContainsHousehold(AllocatedHousehold));
	TestTrue(TEXT("The allocated person can be read"), Registry.FindPerson(AllocatedPerson).IsSet());
	TestTrue(TEXT("The allocated household can be read"), Registry.FindHousehold(AllocatedHousehold).IsSet());

	// Every value below must fail safely: zero, one past the allocated range, and the values
	// that a narrowing cast to int32 would turn negative or overflow.
	struct FBoundaryCase
	{
		const TCHAR* Description;
		uint32 Value;
	};

	const FBoundaryCase BoundaryCases[] = {
		{ TEXT("zero"), 0u },
		{ TEXT("one past the allocated range"), AllocatedPerson.GetValue() + 1u },
		{ TEXT("MAX_int32"), Int32MaxIdValue },
		{ TEXT("the int32 sign bit (0x80000000)"), SignBitIdValue },
		{ TEXT("MAX_uint32"), UInt32MaxIdValue }
	};

	for (const FBoundaryCase& BoundaryCase : BoundaryCases)
	{
		const FPersonId PersonId = FPersonId(BoundaryCase.Value);
		const FHouseholdId HouseholdId = FHouseholdId(BoundaryCase.Value);

		TestFalse(FString::Printf(TEXT("A person identifier of %s does not resolve"), BoundaryCase.Description),
			Registry.ContainsPerson(PersonId));
		TestFalse(FString::Printf(TEXT("A person identifier of %s cannot be read"), BoundaryCase.Description),
			Registry.FindPerson(PersonId).IsSet());
		TestFalse(FString::Printf(TEXT("A household identifier of %s does not resolve"), BoundaryCase.Description),
			Registry.ContainsHousehold(HouseholdId));
		TestFalse(FString::Printf(TEXT("A household identifier of %s cannot be read"), BoundaryCase.Description),
			Registry.FindHousehold(HouseholdId).IsSet());

		// Membership operations must reject them too, naming the unresolvable side.
		TestTrue(FString::Printf(TEXT("Adding a person identifier of %s is rejected"), BoundaryCase.Description),
			Registry.AddPersonToHousehold(PersonId, AllocatedHousehold) == EHouseholdMembershipResult::UnknownPerson);
		TestTrue(FString::Printf(TEXT("Adding to a household identifier of %s is rejected"), BoundaryCase.Description),
			Registry.AddPersonToHousehold(AllocatedPerson, HouseholdId) == EHouseholdMembershipResult::UnknownHousehold);
		TestTrue(FString::Printf(TEXT("Removing a person identifier of %s is rejected"), BoundaryCase.Description),
			Registry.RemovePersonFromHousehold(PersonId, AllocatedHousehold) == EHouseholdMembershipResult::UnknownPerson);
		TestTrue(FString::Printf(TEXT("Removing from a household identifier of %s is rejected"), BoundaryCase.Description),
			Registry.RemovePersonFromHousehold(AllocatedPerson, HouseholdId) == EHouseholdMembershipResult::UnknownHousehold);
	}

	// None of the above may have disturbed the registry.
	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(AllocatedPerson);
	const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(AllocatedHousehold);
	if (!PersonRecord.IsSet() || !HouseholdRecord.IsSet())
	{
		AddError(TEXT("Boundary identifiers must not remove existing records"));
		return false;
	}

	TestTrue(TEXT("Boundary identifiers leave the person side unchanged"),
		PersonRecord->HouseholdId == AllocatedHousehold);
	TestEqual(TEXT("Boundary identifiers leave the household side unchanged"),
		HouseholdRecord->Members.Num(), 1);
	TestEqual(TEXT("Boundary identifiers create no people"), Registry.GetPersonCount(), 1);
	TestEqual(TEXT("Boundary identifiers create no households"), Registry.GetHouseholdCount(), 1);

	VerifyInvariants(*this, Registry, TEXT("after boundary identifier operations"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryLookupTest,
	"RealmsUnwritten.Simulation.Registry.Lookup", SimulationTestFlags)

bool FSimulationRegistryLookupTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Reinmar"));
	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Adelheid"), TEXT("Reinmar"), 29));

	TestTrue(TEXT("An existing person resolves"), Registry.FindPerson(PersonId).IsSet());
	TestTrue(TEXT("An existing household resolves"), Registry.FindHousehold(HouseholdId).IsSet());

	TestFalse(TEXT("A default person identifier resolves to nothing"), Registry.FindPerson(FPersonId()).IsSet());
	TestFalse(TEXT("A default household identifier resolves to nothing"),
		Registry.FindHousehold(FHouseholdId()).IsSet());
	TestFalse(TEXT("An unallocated person identifier resolves to nothing"),
		Registry.FindPerson(MakeUnallocatedId<FPersonId>()).IsSet());
	TestFalse(TEXT("An unallocated household identifier resolves to nothing"),
		Registry.FindHousehold(MakeUnallocatedId<FHouseholdId>()).IsSet());

	// Failed lookups must not create or alter records.
	TestEqual(TEXT("Failed lookups create no people"), Registry.GetPersonCount(), 1);
	TestEqual(TEXT("Failed lookups create no households"), Registry.GetHouseholdCount(), 1);

	// A snapshot is a read at a point in time; growth of the registry cannot invalidate it.
	const TOptional<FPersonRecord> PersonSnapshot = Registry.FindPerson(PersonId);
	if (!PersonSnapshot.IsSet())
	{
		AddError(TEXT("The existing person should be readable"));
		return false;
	}

	for (int32 AdditionalPerson = 0; AdditionalPerson < 64; ++AdditionalPerson)
	{
		Registry.CreatePerson(MakePersonParams(TEXT("Villager"), TEXT("Franconian"), 20));
	}

	TestTrue(TEXT("A snapshot taken before the registry grew is still readable"),
		PersonSnapshot->Id == PersonId);
	TestEqual(TEXT("A snapshot taken before the registry grew still holds its values"),
		PersonSnapshot->GivenName, FString(TEXT("Adelheid")));
	TestTrue(TEXT("The same person still resolves after the registry grew"),
		Registry.ContainsPerson(PersonId));

	VerifyInvariants(*this, Registry, TEXT("after failed lookups and registry growth"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryMembershipTest,
	"RealmsUnwritten.Simulation.Registry.Membership", SimulationTestFlags)

bool FSimulationRegistryMembershipTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Baumann"));
	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));

	TestTrue(TEXT("A person can join a household"),
		Registry.AddPersonToHousehold(PersonId, HouseholdId) == EHouseholdMembershipResult::Success);

	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
	const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
	if (!PersonRecord.IsSet() || !HouseholdRecord.IsSet())
	{
		AddError(TEXT("Both records should resolve after a membership change"));
		return false;
	}

	TestTrue(TEXT("The person side records the household"), PersonRecord->HouseholdId == HouseholdId);
	TestEqual(TEXT("The household side lists one member"), HouseholdRecord->Members.Num(), 1);
	TestTrue(TEXT("The household side lists the person"), HouseholdRecord->Members.Contains(PersonId));

	// Re-adding must be a reported no-op rather than a second membership.
	TestTrue(TEXT("Re-adding a member reports an existing membership"),
		Registry.AddPersonToHousehold(PersonId, HouseholdId) == EHouseholdMembershipResult::AlreadyMember);

	const TOptional<FHouseholdRecord> HouseholdAfterReAdd = Registry.FindHousehold(HouseholdId);
	if (!HouseholdAfterReAdd.IsSet())
	{
		AddError(TEXT("The household should still resolve after a rejected re-add"));
		return false;
	}

	TestEqual(TEXT("Re-adding a member does not duplicate membership"),
		HouseholdAfterReAdd->Members.Num(), 1);

	VerifyInvariants(*this, Registry, TEXT("after adding a person to a household"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryMembershipTransferTest,
	"RealmsUnwritten.Simulation.Registry.MembershipTransfer", SimulationTestFlags)

bool FSimulationRegistryMembershipTransferTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId OriginHousehold = Registry.CreateHousehold(TEXT("Baumann"));
	const FHouseholdId DestinationHousehold = Registry.CreateHousehold(TEXT("Reinmar"));
	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Mechthild"), TEXT("Baumann"), 21));

	TestTrue(TEXT("The person joins the origin household"),
		Registry.AddPersonToHousehold(PersonId, OriginHousehold) == EHouseholdMembershipResult::Success);
	TestTrue(TEXT("The person moves to the destination household"),
		Registry.AddPersonToHousehold(PersonId, DestinationHousehold) == EHouseholdMembershipResult::Success);

	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
	const TOptional<FHouseholdRecord> OriginRecord = Registry.FindHousehold(OriginHousehold);
	const TOptional<FHouseholdRecord> DestinationRecord = Registry.FindHousehold(DestinationHousehold);
	if (!PersonRecord.IsSet() || !OriginRecord.IsSet() || !DestinationRecord.IsSet())
	{
		AddError(TEXT("All three records should resolve after a transfer"));
		return false;
	}

	TestTrue(TEXT("The person claims exactly the destination household"),
		PersonRecord->HouseholdId == DestinationHousehold);
	TestFalse(TEXT("The origin household no longer lists the person"),
		OriginRecord->Members.Contains(PersonId));
	TestEqual(TEXT("The origin household has no stale members"), OriginRecord->Members.Num(), 0);
	TestTrue(TEXT("The destination household lists the person"),
		DestinationRecord->Members.Contains(PersonId));
	TestEqual(TEXT("The destination household lists the person once"), DestinationRecord->Members.Num(), 1);

	VerifyInvariants(*this, Registry, TEXT("after moving a person between households"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryMembershipRemovalTest,
	"RealmsUnwritten.Simulation.Registry.MembershipRemoval", SimulationTestFlags)

bool FSimulationRegistryMembershipRemovalTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Baumann"));
	const FPersonId StayingPerson = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));
	const FPersonId LeavingPerson = Registry.CreatePerson(MakePersonParams(TEXT("Mechthild"), TEXT("Baumann"), 21));

	Registry.AddPersonToHousehold(StayingPerson, HouseholdId);
	Registry.AddPersonToHousehold(LeavingPerson, HouseholdId);

	TestTrue(TEXT("A member can be removed from their household"),
		Registry.RemovePersonFromHousehold(LeavingPerson, HouseholdId) == EHouseholdMembershipResult::Success);

	const TOptional<FPersonRecord> LeavingRecord = Registry.FindPerson(LeavingPerson);
	const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
	if (!LeavingRecord.IsSet() || !HouseholdRecord.IsSet())
	{
		AddError(TEXT("Both records should resolve after a removal"));
		return false;
	}

	TestFalse(TEXT("Removal clears the person side"), LeavingRecord->HouseholdId.IsValid());
	TestFalse(TEXT("Removal clears the household side"), HouseholdRecord->Members.Contains(LeavingPerson));
	TestEqual(TEXT("Removal leaves one member behind"), HouseholdRecord->Members.Num(), 1);
	TestTrue(TEXT("Removal does not affect other members"), HouseholdRecord->Members.Contains(StayingPerson));

	// A second removal is no longer a valid membership change.
	TestTrue(TEXT("Removing a non-member reports that the person does not belong"),
		Registry.RemovePersonFromHousehold(LeavingPerson, HouseholdId) == EHouseholdMembershipResult::NotAMember);

	// Removing a member from a household they do not belong to must not detach them.
	const FHouseholdId OtherHousehold = Registry.CreateHousehold(TEXT("Reinmar"));
	TestTrue(TEXT("Removing a person from the wrong household is rejected"),
		Registry.RemovePersonFromHousehold(StayingPerson, OtherHousehold) == EHouseholdMembershipResult::NotAMember);

	const TOptional<FPersonRecord> StayingRecord = Registry.FindPerson(StayingPerson);
	TestTrue(TEXT("A rejected removal leaves membership unchanged"),
		StayingRecord.IsSet() && StayingRecord->HouseholdId == HouseholdId);

	VerifyInvariants(*this, Registry, TEXT("after removing people from households"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryInvalidMembershipTest,
	"RealmsUnwritten.Simulation.Registry.InvalidMembership", SimulationTestFlags)

bool FSimulationRegistryInvalidMembershipTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FHouseholdId HouseholdId = Registry.CreateHousehold(TEXT("Baumann"));
	const FPersonId PersonId = Registry.CreatePerson(MakePersonParams(TEXT("Konrad"), TEXT("Baumann"), 34));
	Registry.AddPersonToHousehold(PersonId, HouseholdId);

	const FPersonId UnknownPerson = MakeUnallocatedId<FPersonId>();
	const FHouseholdId UnknownHousehold = MakeUnallocatedId<FHouseholdId>();

	TestTrue(TEXT("Adding an unknown person is rejected"),
		Registry.AddPersonToHousehold(UnknownPerson, HouseholdId) == EHouseholdMembershipResult::UnknownPerson);
	TestTrue(TEXT("Adding to an unknown household is rejected"),
		Registry.AddPersonToHousehold(PersonId, UnknownHousehold) == EHouseholdMembershipResult::UnknownHousehold);
	TestTrue(TEXT("Adding with a default person identifier is rejected"),
		Registry.AddPersonToHousehold(FPersonId(), HouseholdId) == EHouseholdMembershipResult::UnknownPerson);
	TestTrue(TEXT("Adding with a default household identifier is rejected"),
		Registry.AddPersonToHousehold(PersonId, FHouseholdId()) == EHouseholdMembershipResult::UnknownHousehold);

	TestTrue(TEXT("Removing an unknown person is rejected"),
		Registry.RemovePersonFromHousehold(UnknownPerson, HouseholdId) == EHouseholdMembershipResult::UnknownPerson);
	TestTrue(TEXT("Removing from an unknown household is rejected"),
		Registry.RemovePersonFromHousehold(PersonId, UnknownHousehold) == EHouseholdMembershipResult::UnknownHousehold);
	TestTrue(TEXT("Removing with a default household identifier is rejected"),
		Registry.RemovePersonFromHousehold(PersonId, FHouseholdId()) == EHouseholdMembershipResult::UnknownHousehold);

	// Every rejected operation must leave the registry exactly as it was.
	const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
	const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
	if (!PersonRecord.IsSet() || !HouseholdRecord.IsSet())
	{
		AddError(TEXT("Rejected operations must not remove existing records"));
		return false;
	}

	TestTrue(TEXT("Rejected operations leave the person side unchanged"),
		PersonRecord->HouseholdId == HouseholdId);
	TestEqual(TEXT("Rejected operations leave the household side unchanged"),
		HouseholdRecord->Members.Num(), 1);
	TestEqual(TEXT("Rejected operations create no people"), Registry.GetPersonCount(), 1);
	TestEqual(TEXT("Rejected operations create no households"), Registry.GetHouseholdCount(), 1);

	VerifyInvariants(*this, Registry, TEXT("after rejected membership operations"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationRegistryHeadlessPopulationChurnTest,
	"RealmsUnwritten.Simulation.Registry.HeadlessPopulationChurn", SimulationTestFlags)

bool FSimulationRegistryHeadlessPopulationChurnTest::RunTest(const FString& Parameters)
{
	// Builds a prototype-sized population and churns membership with no Actor and no map.
	FSimulationRegistry Registry;

	TArray<FHouseholdId> Households;
	for (int32 HouseholdIndex = 0; HouseholdIndex < 5; ++HouseholdIndex)
	{
		Households.Add(Registry.CreateHousehold(FString::Printf(TEXT("Household %d"), HouseholdIndex)));
	}

	TArray<FPersonId> People;
	for (int32 PersonIndex = 0; PersonIndex < 25; ++PersonIndex)
	{
		People.Add(Registry.CreatePerson(
			MakePersonParams(TEXT("Villager"), TEXT("Franconian"), 18 + (PersonIndex % 40))));
	}

	const TSet<FPersonId> DistinctPeople(People);
	const TSet<FHouseholdId> DistinctHouseholds(Households);
	TestEqual(TEXT("All person identifiers are distinct"), DistinctPeople.Num(), People.Num());
	TestEqual(TEXT("All household identifiers are distinct"), DistinctHouseholds.Num(), Households.Num());

	for (int32 PersonIndex = 0; PersonIndex < People.Num(); ++PersonIndex)
	{
		Registry.AddPersonToHousehold(People[PersonIndex], Households[PersonIndex % Households.Num()]);
	}

	// Move every third person into the first household.
	for (int32 PersonIndex = 0; PersonIndex < People.Num(); PersonIndex += 3)
	{
		Registry.AddPersonToHousehold(People[PersonIndex], Households[0]);
	}

	// Detach one person entirely, using their own record to find the household they are in.
	const TOptional<FPersonRecord> LastPersonRecord = Registry.FindPerson(People.Last());
	if (!LastPersonRecord.IsSet())
	{
		AddError(TEXT("The last created person should resolve"));
		return false;
	}

	TestTrue(TEXT("The last person can leave their household"),
		Registry.RemovePersonFromHousehold(People.Last(), LastPersonRecord->HouseholdId)
			== EHouseholdMembershipResult::Success);

	int32 TotalMemberships = 0;
	for (const FHouseholdId HouseholdId : Households)
	{
		const TOptional<FHouseholdRecord> HouseholdRecord = Registry.FindHousehold(HouseholdId);
		if (!HouseholdRecord.IsSet())
		{
			AddError(TEXT("Every created household should still resolve"));
			return false;
		}

		TotalMemberships += HouseholdRecord->Members.Num();
	}

	int32 PeopleWithHousehold = 0;
	for (const FPersonId PersonId : People)
	{
		const TOptional<FPersonRecord> PersonRecord = Registry.FindPerson(PersonId);
		if (!PersonRecord.IsSet())
		{
			AddError(TEXT("Every created person should still resolve"));
			return false;
		}

		if (PersonRecord->HouseholdId.IsValid())
		{
			++PeopleWithHousehold;
		}
	}

	TestEqual(TEXT("Membership totals agree from both sides"), TotalMemberships, PeopleWithHousehold);
	TestEqual(TEXT("Exactly one person was left without a household"),
		People.Num() - PeopleWithHousehold, 1);

	VerifyInvariants(*this, Registry, TEXT("after population churn"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
