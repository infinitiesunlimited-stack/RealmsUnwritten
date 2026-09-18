#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for authored Activity Type identity and one optional Current Activity per person.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Activity types are created at runtime under authored keys. The keys used here, such as
 * Activity.Working, are test data only and appear in no production catalog.
 *
 * Current Activity is independent of CurrentWork, Presence, Capability, and Practice.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationActivityTypeCreationTest,
	"RealmsUnwritten.Simulation.ActivityType.Creation", SimulationTestFlags)

bool FSimulationActivityTypeCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	TestFalse(TEXT("An activity type cannot be created without an authored key"),
		Registry.CreateActivityType(NAME_None, TEXT("Nameless")).IsValid());
	TestEqual(TEXT("A rejected nameless definition stores no activity type"),
		Registry.GetActivityTypeCount(), 0);

	const FActivityTypeId Working = Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
	TestTrue(TEXT("A valid authored key creates an activity type"), Working.IsValid());
	TestEqual(TEXT("The rejected nameless definition consumed no runtime handle"), Working.GetValue(), 1u);
	TestTrue(TEXT("The created activity type is contained"), Registry.ContainsActivityType(Working));

	const TOptional<FActivityTypeRecord> WorkingRecord = Registry.FindActivityType(Working);
	if (!WorkingRecord.IsSet())
	{
		AddError(TEXT("The created activity type should resolve by identifier"));
		return false;
	}

	TestTrue(TEXT("The record preserves its typed identifier"), WorkingRecord->Id == Working);
	TestTrue(TEXT("The record preserves its authored key"),
		WorkingRecord->AuthoredKey == FName(TEXT("Activity.Working")));
	TestEqual(TEXT("The record preserves its display name"),
		WorkingRecord->Name, FString(TEXT("Working")));

	const TOptional<FActivityTypeId> WorkingByKey =
		Registry.FindActivityTypeIdByKey(TEXT("Activity.Working"));
	TestTrue(TEXT("A known authored key resolves to its typed identifier"),
		WorkingByKey.IsSet() && WorkingByKey.GetValue() == Working);
	TestFalse(TEXT("An unknown authored key does not resolve"),
		Registry.FindActivityTypeIdByKey(TEXT("Activity.Unknown")).IsSet());
	TestFalse(TEXT("NAME_None does not resolve as an authored key"),
		Registry.FindActivityTypeIdByKey(NAME_None).IsSet());

	const int32 CountBeforeDuplicate = Registry.GetActivityTypeCount();
	const FActivityTypeId Duplicate =
		Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Altered Working"));
	TestFalse(TEXT("A duplicate authored key is rejected"), Duplicate.IsValid());
	TestEqual(TEXT("A duplicate authored key stores no additional record"),
		Registry.GetActivityTypeCount(), CountBeforeDuplicate);

	const TOptional<FActivityTypeRecord> OriginalAfterDuplicate = Registry.FindActivityType(Working);
	TestTrue(TEXT("Duplicate rejection leaves the original authoritative record unchanged"),
		OriginalAfterDuplicate.IsSet()
			&& OriginalAfterDuplicate->AuthoredKey == FName(TEXT("Activity.Working"))
			&& OriginalAfterDuplicate->Name == TEXT("Working"));

	const FActivityTypeId Sleeping = Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
	TestEqual(TEXT("A rejected duplicate consumed no runtime handle"),
		Sleeping.GetValue(), static_cast<uint32>(CountBeforeDuplicate + 1));
	const FActivityTypeId OtherWorking =
		Registry.CreateActivityType(TEXT("Activity.OtherWorking"), TEXT("Working"));
	TestTrue(TEXT("A duplicate display name under a distinct key is allowed"), OtherWorking.IsValid());

	const uint32 UnresolvableValues[] = {
		0u, OtherWorking.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FActivityTypeId UnknownId(UnresolvableValue);
		TestFalse(FString::Printf(TEXT("Unknown activity type %u is not contained"), UnresolvableValue),
			Registry.ContainsActivityType(UnknownId));
		TestFalse(FString::Printf(TEXT("Unknown activity type %u does not resolve"), UnresolvableValue),
			Registry.FindActivityType(UnknownId).IsSet());
	}

	VerifyInvariants(*this, Registry, TEXT("after activity-type creation and rejection"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationActivityTypeNamespaceIndependenceTest,
	"RealmsUnwritten.Simulation.ActivityType.NamespaceIndependence", SimulationTestFlags)

bool FSimulationActivityTypeNamespaceIndependenceTest::RunTest(const FString& Parameters)
{
	static_assert(!SimulationIdContract::bInterchangeable<FActivityTypeId, FPersonId>,
		"Activity-type and person identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FActivityTypeId, FWorkTypeId>,
		"Activity-type and work-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FActivityTypeId, FSkillTypeId>,
		"Activity-type and skill-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FActivityTypeId, FGoodTypeId>,
		"Activity-type and good-type identifiers must not be interchangeable.");

	FSimulationRegistry Registry;
	const FName SharedKey(TEXT("Working"));
	const FGoodTypeId GoodTypeId = Registry.CreateGoodType(SharedKey, TEXT("Working Good"));
	const FWorkTypeId WorkTypeId = Registry.CreateWorkType(SharedKey, TEXT("Working Work"));
	const FSkillTypeId SkillTypeId = Registry.CreateSkillType(SharedKey, TEXT("Working Skill"));
	const FActivityTypeId ActivityTypeId = Registry.CreateActivityType(SharedKey, TEXT("Working Activity"));

	TestTrue(TEXT("The Good Type accepts the exact shared key Working"), GoodTypeId.IsValid());
	TestTrue(TEXT("The Work Type accepts the exact shared key Working"), WorkTypeId.IsValid());
	TestTrue(TEXT("The Skill Type accepts the exact shared key Working"), SkillTypeId.IsValid());
	TestTrue(TEXT("The Activity Type accepts the exact shared key Working"), ActivityTypeId.IsValid());
	TestEqual(TEXT("The Good Type registry holds exactly its registration"), Registry.GetGoodTypeCount(), 1);
	TestEqual(TEXT("The Work Type registry holds exactly its registration"), Registry.GetWorkTypeCount(), 1);
	TestEqual(TEXT("The Skill Type registry holds exactly its registration"), Registry.GetSkillTypeCount(), 1);
	TestEqual(TEXT("The Activity Type registry holds exactly its registration"),
		Registry.GetActivityTypeCount(), 1);

	TestTrue(TEXT("The Activity Type registry independently resolves Working"),
		Registry.FindActivityTypeIdByKey(SharedKey).IsSet()
			&& Registry.FindActivityTypeIdByKey(SharedKey).GetValue() == ActivityTypeId);
	TestTrue(TEXT("The Skill Type registry independently resolves Working"),
		Registry.FindSkillTypeIdByKey(SharedKey).IsSet()
			&& Registry.FindSkillTypeIdByKey(SharedKey).GetValue() == SkillTypeId);

	VerifyInvariants(*this, Registry, TEXT("with one identical key in four definition namespaces"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationActivityTypeSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.ActivityType.SnapshotRegression", SimulationTestFlags)

bool FSimulationActivityTypeSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;
	const FActivityTypeId Working = Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
	TOptional<FActivityTypeRecord> Snapshot = Registry.FindActivityType(Working);
	if (!Snapshot.IsSet())
	{
		AddError(TEXT("The activity type snapshot should resolve"));
		return false;
	}

	for (int32 FillerIndex = 0; FillerIndex < 16; ++FillerIndex)
	{
		Registry.CreateActivityType(
			FName(*FString::Printf(TEXT("Activity.Filler%d"), FillerIndex)), TEXT("Filler"));
	}

	Snapshot->Id = FActivityTypeId(UInt32MaxIdValue);
	Snapshot->AuthoredKey = FName(TEXT("Activity.Tampered"));
	Snapshot->Name = TEXT("Tampered");

	const TOptional<FActivityTypeRecord> AuthoritativeRecord = Registry.FindActivityType(Working);
	if (!AuthoritativeRecord.IsSet())
	{
		AddError(TEXT("The authoritative activity type should still resolve after snapshot mutation"));
		return false;
	}

	TestTrue(TEXT("Snapshot mutation does not change the authoritative identifier"),
		AuthoritativeRecord->Id == Working);
	TestTrue(TEXT("Snapshot mutation does not change the authoritative key"),
		AuthoritativeRecord->AuthoredKey == FName(TEXT("Activity.Working")));
	TestEqual(TEXT("Snapshot mutation does not change the authoritative display name"),
		AuthoritativeRecord->Name, FString(TEXT("Working")));
	TestFalse(TEXT("The tampered snapshot key was not registered"),
		Registry.FindActivityTypeIdByKey(TEXT("Activity.Tampered")).IsSet());

	VerifyInvariants(*this, Registry, TEXT("after mutating a detached activity-type snapshot"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCurrentActivityTest,
	"RealmsUnwritten.Simulation.Activity.Current", SimulationTestFlags)

bool FSimulationPersonCurrentActivityTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FActivityTypeId Sleeping = Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
	const FActivityTypeId Working = Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));

	const TOptional<FCurrentActivity> DefaultActivity = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("A created person starts with no recorded activity"),
		DefaultActivity.IsSet() && DefaultActivity->Kind == ECurrentActivityKind::None
			&& !DefaultActivity->IsRecorded() && !DefaultActivity->ActivityTypeId.IsValid());
	TestTrue(TEXT("FindPerson also reports unrecorded current activity"),
		Registry.FindPerson(William)->CurrentActivity.Kind == ECurrentActivityKind::None);

	TestTrue(TEXT("Setting Sleeping succeeds"),
		Registry.SetPersonCurrentActivity(William, Sleeping) == EPersonActivityResult::Success);
	TOptional<FCurrentActivity> SleepingActivity = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("Sleeping is recorded as an activity type"),
		SleepingActivity.IsSet() && SleepingActivity->Kind == ECurrentActivityKind::ActivityType
			&& SleepingActivity->ActivityTypeId == Sleeping && SleepingActivity->IsRecorded());

	SleepingActivity->Kind = ECurrentActivityKind::ActivityType;
	SleepingActivity->ActivityTypeId = Working;
	const TOptional<FCurrentActivity> AfterSnapshotMutation = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("Mutating the returned current-activity copy leaves Sleeping authoritative"),
		AfterSnapshotMutation.IsSet()
			&& AfterSnapshotMutation->Kind == ECurrentActivityKind::ActivityType
			&& AfterSnapshotMutation->ActivityTypeId == Sleeping);

	TestTrue(TEXT("Setting Working replaces Sleeping"),
		Registry.SetPersonCurrentActivity(William, Working) == EPersonActivityResult::Success);
	const TOptional<FCurrentActivity> WorkingActivity = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("Only Working remains after replacement"),
		WorkingActivity.IsSet() && WorkingActivity->Kind == ECurrentActivityKind::ActivityType
			&& WorkingActivity->ActivityTypeId == Working);

	TestTrue(TEXT("Clearing current activity succeeds"),
		Registry.ClearPersonCurrentActivity(William) == EPersonActivityResult::Success);
	const TOptional<FCurrentActivity> ClearedActivity = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("Cleared activity is unrecorded"),
		ClearedActivity.IsSet() && ClearedActivity->Kind == ECurrentActivityKind::None
			&& !ClearedActivity->ActivityTypeId.IsValid());

	TestTrue(TEXT("Clearing already-unrecorded activity remains success"),
		Registry.ClearPersonCurrentActivity(William) == EPersonActivityResult::Success);
	TestTrue(TEXT("A second clear still leaves activity unrecorded"),
		Registry.GetPersonCurrentActivity(William)->Kind == ECurrentActivityKind::None);

	VerifyInvariants(*this, Registry, TEXT("after setting, replacing, and clearing current activity"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCurrentActivityRejectionTest,
	"RealmsUnwritten.Simulation.Activity.Rejection", SimulationTestFlags)

bool FSimulationPersonCurrentActivityRejectionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FActivityTypeId Sleeping = Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
	TestTrue(TEXT("Sleeping is recorded before rejection cases"),
		Registry.SetPersonCurrentActivity(William, Sleeping) == EPersonActivityResult::Success);

	const uint32 UnresolvableValues[] = {
		0u, William.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestTrue(FString::Printf(TEXT("Setting activity for person %u is rejected"), UnresolvableValue),
			Registry.SetPersonCurrentActivity(FPersonId(UnresolvableValue), Sleeping)
				== EPersonActivityResult::UnknownPerson);
		TestTrue(FString::Printf(TEXT("Clearing activity for person %u is rejected"), UnresolvableValue),
			Registry.ClearPersonCurrentActivity(FPersonId(UnresolvableValue))
				== EPersonActivityResult::UnknownPerson);
		TestFalse(FString::Printf(TEXT("Unknown person %u has no current activity snapshot"), UnresolvableValue),
			Registry.GetPersonCurrentActivity(FPersonId(UnresolvableValue)).IsSet());
		TestTrue(FString::Printf(TEXT("Unknown activity type %u is rejected"), UnresolvableValue),
			Registry.SetPersonCurrentActivity(William, FActivityTypeId(UnresolvableValue))
				== EPersonActivityResult::UnknownActivityType);
	}

	const TOptional<FCurrentActivity> AfterRejection = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("Rejected requests left Sleeping recorded"),
		AfterRejection.IsSet() && AfterRejection->ActivityTypeId == Sleeping
			&& AfterRejection->Kind == ECurrentActivityKind::ActivityType);

	VerifyInvariants(*this, Registry, TEXT("after rejected current-activity requests"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCurrentActivityIndependenceTest,
	"RealmsUnwritten.Simulation.Activity.Independence", SimulationTestFlags)

bool FSimulationPersonCurrentActivityIndependenceTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
	const FActivityTypeId Sleeping = Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
	const FActivityTypeId Working = Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	const FWorkTypeId Harvest = Registry.CreateWorkType(TEXT("Work.TestHarvest"), TEXT("Harvest"));
	const FPhysicalSiteId SiteId = CreateTestSite(Registry);

	TestTrue(TEXT("William may be assigned work before any activity is recorded"),
		Registry.AssignPersonWork(William, Harvest, SiteId) == EPersonWorkResult::Success);
	TestTrue(TEXT("William + Carpentry is recorded before activity changes"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("Some practice is recorded before activity changes"),
		Registry.AddPersonCapabilityPractice(William, Carpentry, 10u)
			== EPersonCapabilityPracticeResult::Success);

	const int32 CapabilityCountBefore = Registry.GetPersonCapabilityCount();
	const uint32 PracticeBefore = Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue();

	TestTrue(TEXT("Sleeping may be recorded while CurrentWork remains assigned"),
		Registry.SetPersonCurrentActivity(William, Sleeping) == EPersonActivityResult::Success);

	const TOptional<FPersonRecord> AfterSleeping = Registry.FindPerson(William);
	TestTrue(TEXT("CurrentWork is unchanged by setting Sleeping"),
		AfterSleeping.IsSet()
			&& AfterSleeping->CurrentWork.Kind == ECurrentWorkKind::PhysicalSite
			&& AfterSleeping->CurrentWork.WorkTypeId == Harvest
			&& AfterSleeping->CurrentWork.PhysicalSiteId == SiteId);
	TestTrue(TEXT("CurrentActivity Sleeping coexists with CurrentWork"),
		AfterSleeping->CurrentActivity.Kind == ECurrentActivityKind::ActivityType
			&& AfterSleeping->CurrentActivity.ActivityTypeId == Sleeping);

	TestTrue(TEXT("Working activity may be recorded with no extra work requirement"),
		Registry.SetPersonCurrentActivity(William, Working) == EPersonActivityResult::Success);
	TestTrue(TEXT("Clearing activity leaves CurrentWork assigned"),
		Registry.ClearPersonCurrentActivity(William) == EPersonActivityResult::Success
			&& Registry.FindPerson(William)->CurrentWork.IsAssigned());

	TestTrue(TEXT("Working activity may be recorded while CurrentWork is later removed"),
		Registry.RemovePersonWork(William) == EPersonWorkResult::Success
			&& Registry.SetPersonCurrentActivity(William, Working) == EPersonActivityResult::Success);
	TestTrue(TEXT("CurrentWork None with CurrentActivity Working is representable"),
		!Registry.FindPerson(William)->CurrentWork.IsAssigned()
			&& Registry.GetPersonCurrentActivity(William)->ActivityTypeId == Working);

	TestEqual(TEXT("Activity operations do not change capability count"),
		Registry.GetPersonCapabilityCount(), CapabilityCountBefore);
	TestTrue(TEXT("Activity operations do not change accumulated practice"),
		Registry.GetPersonCapabilityPractice(William, Carpentry).GetValue() == PracticeBefore);

	VerifyInvariants(*this, Registry, TEXT("after proving activity independence from work and capability"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonCurrentActivityInvariantDetectionTest,
	"RealmsUnwritten.Simulation.Activity.InvariantDetection", SimulationTestFlags)

bool FSimulationPersonCurrentActivityInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
		FSimulationRegistryTestAccess::ActivityTypeRecords(Registry)[0].Id = FActivityTypeId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an activity-type slot and identifier mismatch"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
		FSimulationRegistryTestAccess::ActivityTypeRecords(Registry)[0].AuthoredKey = NAME_None;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an activity type with no authored key"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
		Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
		FSimulationRegistryTestAccess::ActivityTypeRecords(Registry)[1].AuthoredKey =
			FName(TEXT("Activity.Working"));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("duplicate activity-type authored keys"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId William =
			Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
		const FActivityTypeId Sleeping =
			Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
		TestTrue(TEXT("A valid activity is recorded before ActivityTypeId corruption"),
			Registry.SetPersonCurrentActivity(William, Sleeping) == EPersonActivityResult::Success);
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentActivity.ActivityTypeId =
			FActivityTypeId(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("current activity naming an unresolvable type"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId William =
			Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
		const FActivityTypeId Sleeping =
			Registry.CreateActivityType(TEXT("Activity.Sleeping"), TEXT("Sleeping"));
		TestTrue(TEXT("A valid activity is recorded before None/stale-id corruption"),
			Registry.SetPersonCurrentActivity(William, Sleeping) == EPersonActivityResult::Success);
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentActivity.Kind =
			ECurrentActivityKind::None;
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentActivity.ActivityTypeId = Sleeping;
		VerifyInvariantsDetectFailure(
			*this, Registry, TEXT("none current activity that still names an activity type"));
		TestTrue(TEXT("The corrupted person is the expected person"),
			FSimulationRegistryTestAccess::PersonRecords(Registry)[0].Id == William);
	}

	{
		FSimulationRegistry Registry;
		Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carpenter"), 28));
		FSimulationRegistryTestAccess::PersonRecords(Registry)[0].CurrentActivity.Kind =
			static_cast<ECurrentActivityKind>(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("an unsupported current-activity kind"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
