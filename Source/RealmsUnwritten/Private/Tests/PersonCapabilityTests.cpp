#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for the sparse person-capability relationship: which people possess which Skill Types.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Skill types are created at runtime under authored keys. The keys used here, such as
 * Skill.Carpentry, are test data only and appear in no production catalog.
 *
 * Capability is relationship existence only. These tests do not cover proficiency,
 * occupation, knowledge, credentials, removal, or reverse Skill Type to person search.
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

	FPhysicalSiteId CreateTestSite(FSimulationRegistry& Registry)
	{
		const FSettlementId SettlementId = Registry.CreateSettlement(TEXT("Eichenfurt"));
		const FPropertyId PropertyId = Registry.CreateProperty(SettlementId);
		return Registry.CreatePhysicalSite(PropertyId, TEXT("Test.Workshop"), TEXT("Workshop"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCapabilityAdditionTest,
	"RealmsUnwritten.Simulation.PersonCapability.Addition", SimulationTestFlags)

bool FSimulationPersonCapabilityAdditionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FPersonId Thomas = Registry.CreatePerson(MakePersonParams(TEXT("Thomas"), TEXT("Mason"), 31));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	const FSkillTypeId Masonry = Registry.CreateSkillType(TEXT("Skill.Masonry"), TEXT("Masonry"));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);

	TestTrue(TEXT("William may be assigned work before any capability is recorded"),
		Registry.AssignPersonWork(William, Harvest, SiteId) == EPersonWorkResult::Success);
	TestEqual(TEXT("No capability relationships exist until one is added"),
		Registry.GetPersonCapabilityCount(), 0);
	TestFalse(TEXT("William does not yet possess Carpentry"),
		Registry.PersonHasCapability(William, Carpentry));

	TestTrue(TEXT("Recording William + Carpentry succeeds"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("William possesses Carpentry after addition"),
		Registry.PersonHasCapability(William, Carpentry));
	TestEqual(TEXT("One sparse relationship is stored"), Registry.GetPersonCapabilityCount(), 1);

	TestTrue(TEXT("Recording William + Masonry succeeds"),
		Registry.AddPersonCapability(William, Masonry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("William possesses both Carpentry and Masonry"),
		Registry.PersonHasCapability(William, Carpentry) && Registry.PersonHasCapability(William, Masonry));

	TestTrue(TEXT("Recording Thomas + Carpentry succeeds"),
		Registry.AddPersonCapability(Thomas, Carpentry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("Thomas possesses Carpentry"), Registry.PersonHasCapability(Thomas, Carpentry));
	TestFalse(TEXT("Thomas does not possess Masonry; absence is not a stored entry"),
		Registry.PersonHasCapability(Thomas, Masonry));
	TestEqual(TEXT("Three recorded pairs, not a dense Person x SkillType matrix"),
		Registry.GetPersonCapabilityCount(), 3);

	const TOptional<FPersonRecord> WilliamRecord = Registry.FindPerson(William);
	const TOptional<FSkillTypeRecord> CarpentryRecord = Registry.FindSkillType(Carpentry);
	if (!WilliamRecord.IsSet() || !CarpentryRecord.IsSet())
	{
		AddError(TEXT("William and Carpentry should still resolve after capability addition"));
		return false;
	}

	TestTrue(TEXT("Capability addition does not change person identity"), WilliamRecord->Id == William);
	TestEqual(TEXT("Capability addition does not change the person's given name"),
		WilliamRecord->GivenName, FString(TEXT("William")));
	TestTrue(TEXT("Capability addition does not change CurrentWork kind"),
		WilliamRecord->CurrentWork.Kind == ECurrentWorkKind::PhysicalSite);
	TestTrue(TEXT("Capability addition does not change CurrentWork type"),
		WilliamRecord->CurrentWork.WorkTypeId == Harvest);
	TestTrue(TEXT("Capability addition does not change CurrentWork site"),
		WilliamRecord->CurrentWork.PhysicalSiteId == SiteId);
	TestTrue(TEXT("Capability addition does not change skill type identity"),
		CarpentryRecord->Id == Carpentry);
	TestTrue(TEXT("Capability addition does not change the skill type authored key"),
		CarpentryRecord->AuthoredKey == FName(TEXT("Skill.Carpentry")));

	VerifyInvariants(*this, Registry, TEXT("after recording sparse person-capability relationships"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCapabilityRejectionTest,
	"RealmsUnwritten.Simulation.PersonCapability.Rejection", SimulationTestFlags)

bool FSimulationPersonCapabilityRejectionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	TestTrue(TEXT("The original pair is recorded before rejection cases"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);

	const int32 CountAfterSuccess = Registry.GetPersonCapabilityCount();
	TestEqual(TEXT("Exactly one relationship exists before rejection cases"), CountAfterSuccess, 1);

	TestTrue(TEXT("The same pair is rejected as a duplicate"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::AlreadyHasCapability);
	TestEqual(TEXT("Duplicate rejection stores no additional relationship"),
		Registry.GetPersonCapabilityCount(), CountAfterSuccess);
	TestTrue(TEXT("The original relationship remains after duplicate rejection"),
		Registry.PersonHasCapability(William, Carpentry));

	const uint32 UnresolvableValues[] = {
		0u, William.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestTrue(FString::Printf(TEXT("Unknown person %u is rejected before mutation"), UnresolvableValue),
			Registry.AddPersonCapability(FPersonId(UnresolvableValue), Carpentry)
				== EPersonCapabilityResult::UnknownPerson);
		TestTrue(FString::Printf(TEXT("Unknown skill type %u is rejected before mutation"), UnresolvableValue),
			Registry.AddPersonCapability(William, FSkillTypeId(UnresolvableValue))
				== EPersonCapabilityResult::UnknownSkillType);
		TestTrue(
			FString::Printf(TEXT("Unknown person %u is reported before an also-unknown skill type"),
				UnresolvableValue),
			Registry.AddPersonCapability(FPersonId(UnresolvableValue), FSkillTypeId(UnresolvableValue))
				== EPersonCapabilityResult::UnknownPerson);
		TestFalse(FString::Printf(TEXT("Unknown pair %u is not recorded as possessed"), UnresolvableValue),
			Registry.PersonHasCapability(FPersonId(UnresolvableValue), FSkillTypeId(UnresolvableValue)));
	}

	TestEqual(TEXT("Every rejected request left relationship storage unchanged"),
		Registry.GetPersonCapabilityCount(), CountAfterSuccess);
	TestTrue(TEXT("The original pair remains the only recorded relationship"),
		Registry.PersonHasCapability(William, Carpentry));

	VerifyInvariants(*this, Registry, TEXT("after rejected person-capability requests"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCapabilityInvariantDetectionTest,
	"RealmsUnwritten.Simulation.PersonCapability.InvariantDetection", SimulationTestFlags)

bool FSimulationPersonCapabilityInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		const FPersonId William =
			Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
		const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
		TestTrue(TEXT("A valid pair is recorded before person-reference corruption"),
			Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
		FSimulationRegistryTestAccess::PersonCapabilityRecords(Registry)[0].PersonId = FPersonId(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a capability naming an unresolvable person"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId William =
			Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
		const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
		TestTrue(TEXT("A valid pair is recorded before skill-type-reference corruption"),
			Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
		FSimulationRegistryTestAccess::PersonCapabilityRecords(Registry)[0].SkillTypeId = FSkillTypeId(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a capability naming an unresolvable skill type"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId William =
			Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
		const FPersonId Thomas = Registry.CreatePerson(MakePersonParams(TEXT("Thomas"), TEXT("Mason"), 31));
		const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
		const FSkillTypeId Masonry = Registry.CreateSkillType(TEXT("Skill.Masonry"), TEXT("Masonry"));
		TestTrue(TEXT("Two distinct pairs are recorded before duplicate corruption"),
			Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success
				&& Registry.AddPersonCapability(Thomas, Masonry) == EPersonCapabilityResult::Success);
		FSimulationRegistryTestAccess::PersonCapabilityRecords(Registry)[1].PersonId = William;
		FSimulationRegistryTestAccess::PersonCapabilityRecords(Registry)[1].SkillTypeId = Carpentry;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a duplicated (Person, SkillType) pair"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
