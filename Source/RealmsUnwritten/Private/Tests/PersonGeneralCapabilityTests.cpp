#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for derived general capability: a read-only integer interpretation of accumulated
 * practice. Practice remains authoritative. These tests do not cover proficiency labels,
 * task performance, learning, occupation, or reverse Skill Type search.
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

	void VerifyInvariants(FAutomationTestBase& Test, const FSimulationRegistry& Registry, const TCHAR* Context)
	{
		FString FailureDescription;
		if (!Registry.ValidateInvariants(FailureDescription))
		{
			Test.AddError(FString::Printf(TEXT("Invariant broken %s: %s"), Context, *FailureDescription));
		}
	}

	void SetAccumulatedPractice(FSimulationRegistry& Registry, uint32 Practice)
	{
		FSimulationRegistryTestAccess::PersonCapabilityRecords(Registry)[0].AccumulatedPractice = Practice;
	}

	bool ExpectGeneralCapability(
		FAutomationTestBase& Test,
		const FSimulationRegistry& Registry,
		FPersonId PersonId,
		FSkillTypeId SkillTypeId,
		uint32 Expected,
		const TCHAR* Context)
	{
		const TOptional<uint32> Capability = Registry.GetPersonGeneralCapability(PersonId, SkillTypeId);
		const bool bMatched = Capability.IsSet() && Capability.GetValue() == Expected;
		Test.TestTrue(
			FString::Printf(TEXT("%s: expected general capability %u"), Context, Expected), bMatched);
		return bMatched;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonGeneralCapabilityDerivationTest,
	"RealmsUnwritten.Simulation.PersonCapability.GeneralCapability.Derivation", SimulationTestFlags)

bool FSimulationPersonGeneralCapabilityDerivationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FPersonId Thomas = Registry.CreatePerson(MakePersonParams(TEXT("Thomas"), TEXT("Farmer"), 31));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	const FSkillTypeId Farming = Registry.CreateSkillType(TEXT("Skill.Farming"), TEXT("Farming"));

	TestTrue(TEXT("William + Carpentry is recorded"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("William + Farming is recorded"),
		Registry.AddPersonCapability(William, Farming) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("Thomas + Carpentry is recorded"),
		Registry.AddPersonCapability(Thomas, Carpentry) == EPersonCapabilityResult::Success);

	const TOptional<uint32> ZeroCapability = Registry.GetPersonGeneralCapability(William, Carpentry);
	TestTrue(TEXT("Zero practice on an existing relationship derives general capability 0"),
		ZeroCapability.IsSet() && ZeroCapability.GetValue() == 0u);
	TestFalse(TEXT("A missing relationship does not derive general capability 0"),
		Registry.GetPersonGeneralCapability(Thomas, Farming).IsSet());
	TestFalse(TEXT("An unknown skill type does not derive general capability"),
		Registry.GetPersonGeneralCapability(William, FSkillTypeId(99)).IsSet());

	TestTrue(TEXT("Practice remains 0 before known-value queries"),
		Registry.GetPersonCapabilityPractice(William, Carpentry).IsSet()
			&& Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue() == 0u);

	struct FKnownPracticeCapability
	{
		uint32 Practice;
		uint32 Capability;
	};

	const FKnownPracticeCapability KnownValues[] = {
		{0u, 0u},
		{20u, 705u},
		{100u, 1561u},
		{400u, 3015u},
		{1000u, 4472u},
		{2000u, 5773u},
		{5000u, 7453u},
		{10000u, 8451u},
		{20000u, 9128u},
	};

	for (const FKnownPracticeCapability& Known : KnownValues)
	{
		SetAccumulatedPractice(Registry, Known.Practice);
		if (!ExpectGeneralCapability(*this, Registry, William, Carpentry, Known.Capability,
				*FString::Printf(TEXT("Practice %u"), Known.Practice)))
		{
			return false;
		}
	}

	TestTrue(TEXT("Adjacent practice 4501 and 4502 may share one derived integer"),
		[&Registry, William, Carpentry]()
		{
			SetAccumulatedPractice(Registry, 4501u);
			const TOptional<uint32> First = Registry.GetPersonGeneralCapability(William, Carpentry);
			SetAccumulatedPractice(Registry, 4502u);
			const TOptional<uint32> Second = Registry.GetPersonGeneralCapability(William, Carpentry);
			return First.IsSet() && Second.IsSet() && First.GetValue() == 7276u
				&& Second.GetValue() == 7276u;
		}());

	SetAccumulatedPractice(Registry, 20000u);
	const int32 CountBeforeSuccessfulQuery = Registry.GetPersonCapabilityCount();
	const TOptional<uint32> PracticeBeforeSuccessfulQuery =
		Registry.GetPersonCapabilityPractice(William, Carpentry);
	TestTrue(TEXT("Practice is 20000 before the successful general-capability query"),
		PracticeBeforeSuccessfulQuery.IsSet() && PracticeBeforeSuccessfulQuery.GetValue() == 20000u);

	const TOptional<uint32> CapabilityFromKnownPractice =
		Registry.GetPersonGeneralCapability(William, Carpentry);
	TestTrue(TEXT("Practice 20000 derives general capability 9128"),
		CapabilityFromKnownPractice.IsSet() && CapabilityFromKnownPractice.GetValue() == 9128u);

	const TOptional<uint32> PracticeAfterSuccessfulQuery =
		Registry.GetPersonCapabilityPractice(William, Carpentry);
	TestTrue(TEXT("Querying general capability does not change accumulated practice"),
		PracticeAfterSuccessfulQuery.IsSet()
			&& PracticeBeforeSuccessfulQuery.IsSet()
			&& PracticeAfterSuccessfulQuery.GetValue() == PracticeBeforeSuccessfulQuery.GetValue());
	TestEqual(TEXT("Querying general capability does not change capability count"),
		Registry.GetPersonCapabilityCount(), CountBeforeSuccessfulQuery);
	TestTrue(TEXT("William still possesses Carpentry after interpretation"),
		Registry.PersonHasCapability(William, Carpentry));
	TestTrue(TEXT("William's Farming remains at practice 0"),
		Registry.GetPersonCapabilityPractice(William, Farming).GetValue() == 0u
			&& Registry.GetPersonGeneralCapability(William, Farming).GetValue() == 0u);
	TestTrue(TEXT("Thomas's Carpentry remains independent at practice 0"),
		Registry.GetPersonCapabilityPractice(Thomas, Carpentry).GetValue() == 0u
			&& Registry.GetPersonGeneralCapability(Thomas, Carpentry).GetValue() == 0u);

	TestTrue(TEXT("William + Farming can hold a different practice and derive independently"),
		Registry.AddPersonCapabilityPractice(William, Farming, 100u)
			== EPersonCapabilityPracticeResult::Success);
	TestTrue(TEXT("Farming at practice 100 derives 1561 independently of Carpentry"),
		Registry.GetPersonGeneralCapability(William, Farming).GetValue() == 1561u);
	TestTrue(TEXT("Carpentry remains at practice 20000 after Farming practice is added"),
		Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue() == 20000u
			&& Registry.GetPersonGeneralCapability(William, Carpentry).GetValue() == 9128u);

	VerifyInvariants(*this, Registry, TEXT("after deriving general capability"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonGeneralCapabilityQueryRejectionTest,
	"RealmsUnwritten.Simulation.PersonCapability.GeneralCapability.QueryRejection", SimulationTestFlags)

bool FSimulationPersonGeneralCapabilityQueryRejectionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	const FSkillTypeId Farming = Registry.CreateSkillType(TEXT("Skill.Farming"), TEXT("Farming"));
	TestTrue(TEXT("William + Carpentry exists before query rejection"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("A little practice is recorded before rejected queries"),
		Registry.AddPersonCapabilityPractice(William, Carpentry, 100u)
			== EPersonCapabilityPracticeResult::Success);

	const int32 CountBefore = Registry.GetPersonCapabilityCount();
	const uint32 PracticeBefore = Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue();

	TestFalse(TEXT("A missing relationship returns unset general capability"),
		Registry.GetPersonGeneralCapability(William, Farming).IsSet());
	TestFalse(TEXT("Querying a missing relationship does not acquire it"),
		Registry.PersonHasCapability(William, Farming));

	const uint32 UnresolvableValues[] = {
		0u, Farming.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestFalse(FString::Printf(TEXT("Unknown person %u does not derive general capability"), UnresolvableValue),
			Registry.GetPersonGeneralCapability(FPersonId(UnresolvableValue), Carpentry).IsSet());
		TestFalse(
			FString::Printf(TEXT("Unknown skill type %u does not derive general capability"), UnresolvableValue),
			Registry.GetPersonGeneralCapability(William, FSkillTypeId(UnresolvableValue)).IsSet());
	}

	TestEqual(TEXT("Rejected queries leave capability count unchanged"),
		Registry.GetPersonCapabilityCount(), CountBefore);
	TestTrue(TEXT("Rejected queries leave accumulated practice unchanged"),
		Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue() == PracticeBefore);
	TestTrue(TEXT("The existing relationship still derives 1561 from practice 100"),
		Registry.GetPersonGeneralCapability(William, Carpentry).GetValue() == 1561u);

	VerifyInvariants(*this, Registry, TEXT("after rejected general-capability queries"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonGeneralCapabilityMonotonicityTest,
	"RealmsUnwritten.Simulation.PersonCapability.GeneralCapability.Monotonicity", SimulationTestFlags)

bool FSimulationPersonGeneralCapabilityMonotonicityTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	TestTrue(TEXT("William + Carpentry exists before monotonicity cases"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);

	uint32 PreviousCapability = 0;
	for (uint32 Practice = 0; Practice <= 2500u; ++Practice)
	{
		SetAccumulatedPractice(Registry, Practice);
		const TOptional<uint32> Capability = Registry.GetPersonGeneralCapability(William, Carpentry);
		if (!Capability.IsSet())
		{
			AddError(TEXT("An existing relationship must derive a general capability"));
			return false;
		}

		if (Capability.GetValue() < PreviousCapability)
		{
			AddError(FString::Printf(
				TEXT("General capability decreased from %u to %u at practice %u"),
				PreviousCapability, Capability.GetValue(), Practice));
			return false;
		}

		PreviousCapability = Capability.GetValue();
	}

	const uint32 BoundaryPractices[] = {3999u, 4000u, 4001u, 4501u, 4502u, 10000u, 20000u};
	for (const uint32 Practice : BoundaryPractices)
	{
		SetAccumulatedPractice(Registry, Practice);
		const TOptional<uint32> Capability = Registry.GetPersonGeneralCapability(William, Carpentry);
		if (!Capability.IsSet() || Capability.GetValue() < PreviousCapability)
		{
			AddError(FString::Printf(TEXT("General capability was not monotonic at practice %u"), Practice));
			return false;
		}

		PreviousCapability = Capability.GetValue();
	}

	SetAccumulatedPractice(Registry, MAX_uint32 - 1u);
	const TOptional<uint32> NearMaximum = Registry.GetPersonGeneralCapability(William, Carpentry);
	SetAccumulatedPractice(Registry, MAX_uint32);
	const TOptional<uint32> AtMaximum = Registry.GetPersonGeneralCapability(William, Carpentry);
	if (!NearMaximum.IsSet() || !AtMaximum.IsSet())
	{
		AddError(TEXT("uint32-maximum practice must still derive general capability"));
		return false;
	}

	TestTrue(TEXT("MAX_uint32 - 1 derives a positive general capability below 10000"),
		NearMaximum.GetValue() > 0u && NearMaximum.GetValue() < 10000u);
	TestTrue(TEXT("MAX_uint32 derives a positive general capability below 10000"),
		AtMaximum.GetValue() > 0u && AtMaximum.GetValue() < 10000u);
	TestTrue(TEXT("MAX_uint32 does not derive the mathematical bound 10000"),
		AtMaximum.GetValue() != 10000u);
	TestTrue(TEXT("MAX_uint32 does not derive less capability than MAX_uint32 - 1"),
		AtMaximum.GetValue() >= NearMaximum.GetValue());
	TestTrue(TEXT("MAX_uint32 derives exactly 9999"), AtMaximum.GetValue() == 9999u);
	TestTrue(TEXT("MAX_uint32 - 1 derives exactly 9999"), NearMaximum.GetValue() == 9999u);
	TestTrue(TEXT("uint32-maximum practice is left unchanged by interpretation"),
		Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue() == MAX_uint32);

	VerifyInvariants(*this, Registry, TEXT("after monotonic general-capability interpretation"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
