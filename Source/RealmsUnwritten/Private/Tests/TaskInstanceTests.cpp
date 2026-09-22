#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for persistent Task Instance identity.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * A task is one occurrence of a task type. Creating one asserts that the objective exists
 * and nothing more: not that it is available, assigned, current, located, started, or
 * finished. Task types are registered at runtime under authored keys that are test data
 * only and appear in no production catalog.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskCreationTest,
	"RealmsUnwritten.Simulation.Task.Creation", SimulationTestFlags)

bool FSimulationTaskCreationTest::RunTest(const FString& Parameters)
{
	static_assert(!SimulationIdContract::bInterchangeable<FTaskId, FTaskTypeId>,
		"A task and its task type must never be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FTaskId, FPersonId>,
		"Task and person identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FTaskId, FActivityTypeId>,
		"Task and activity-type identifiers must not be interchangeable.");

	// No person is created anywhere in this test. A task is an objective occurrence, not
	// somebody's intention, so it must be creatable in a registry with no people at all.
	FSimulationRegistry Registry;
	TestEqual(TEXT("The registry starts with no people"), Registry.GetPersonCount(), 0);
	TestEqual(TEXT("The registry starts with no tasks"), Registry.GetTaskCount(), 0);

	const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	TestTrue(TEXT("The task type used by these occurrences exists"), ShapeBeamType.IsValid());

	const FTaskId FirstBeam = Registry.CreateTask(ShapeBeamType);
	TestTrue(TEXT("A resolvable task type creates a task"), FirstBeam.IsValid());
	TestEqual(TEXT("The first task takes the first dense handle"), FirstBeam.GetValue(), 1u);
	TestTrue(TEXT("The created task is contained"), Registry.ContainsTask(FirstBeam));
	TestEqual(TEXT("The created task is counted"), Registry.GetTaskCount(), 1);
	TestEqual(TEXT("Creating a task creates no person"), Registry.GetPersonCount(), 0);

	const TOptional<FTaskRecord> FirstBeamRecord = Registry.FindTask(FirstBeam);
	if (!FirstBeamRecord.IsSet())
	{
		AddError(TEXT("The created task should resolve by identifier"));
		return false;
	}

	TestTrue(TEXT("The record preserves its own typed identifier"), FirstBeamRecord->Id == FirstBeam);
	TestTrue(TEXT("The record names the task type it is an occurrence of"),
		FirstBeamRecord->TaskTypeId == ShapeBeamType);

	// Three beams are three objectives, not one objective mentioned three times.
	const FTaskId SecondBeam = Registry.CreateTask(ShapeBeamType);
	const FTaskId ThirdBeam = Registry.CreateTask(ShapeBeamType);
	TestTrue(TEXT("A second occurrence of the same task type is created"), SecondBeam.IsValid());
	TestTrue(TEXT("A third occurrence of the same task type is created"), ThirdBeam.IsValid());
	TestTrue(TEXT("Occurrences of one task type receive distinct identifiers"),
		FirstBeam != SecondBeam && SecondBeam != ThirdBeam && FirstBeam != ThirdBeam);
	TestEqual(TEXT("Occurrences of one task type are not deduplicated"), Registry.GetTaskCount(), 3);

	const TOptional<FTaskRecord> SecondBeamRecord = Registry.FindTask(SecondBeam);
	TestTrue(TEXT("Each occurrence still names the same shared task type"),
		SecondBeamRecord.IsSet() && SecondBeamRecord->TaskTypeId == ShapeBeamType);

	const FTaskTypeId HarvestWheatType =
		Registry.CreateTaskType(TEXT("Task.HarvestWheat"), TEXT("Harvest Wheat"));
	const FTaskId Harvest = Registry.CreateTask(HarvestWheatType);
	const TOptional<FTaskRecord> HarvestRecord = Registry.FindTask(Harvest);
	TestTrue(TEXT("A task of a second type names that second type"),
		HarvestRecord.IsSet() && HarvestRecord->TaskTypeId == HarvestWheatType);

	const int32 TaskCountBeforeRejection = Registry.GetTaskCount();
	const int32 TaskTypeCountBeforeRejection = Registry.GetTaskTypeCount();

	const uint32 UnresolvableTypeValues[] = {
		0u, static_cast<uint32>(TaskTypeCountBeforeRejection) + 1u,
		Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableTypeValue : UnresolvableTypeValues)
	{
		const FTaskId Rejected = Registry.CreateTask(FTaskTypeId(UnresolvableTypeValue));
		TestFalse(FString::Printf(TEXT("Task type %u does not create a task"), UnresolvableTypeValue),
			Rejected.IsValid());
	}

	TestFalse(TEXT("A default task type does not create a task"),
		Registry.CreateTask(FTaskTypeId()).IsValid());
	TestEqual(TEXT("Rejected creation stores no task"), Registry.GetTaskCount(), TaskCountBeforeRejection);
	TestEqual(TEXT("Rejected creation registers no task type"),
		Registry.GetTaskTypeCount(), TaskTypeCountBeforeRejection);

	const FTaskId AfterRejection = Registry.CreateTask(ShapeBeamType);
	TestEqual(TEXT("Rejected creation consumed no runtime handle"),
		AfterRejection.GetValue(), static_cast<uint32>(TaskCountBeforeRejection + 1));

	const TOptional<FTaskTypeRecord> ShapeBeamTypeRecord = Registry.FindTaskType(ShapeBeamType);
	TestTrue(TEXT("Creating occurrences leaves the authoritative task type record unchanged"),
		ShapeBeamTypeRecord.IsSet()
			&& ShapeBeamTypeRecord->Id == ShapeBeamType
			&& ShapeBeamTypeRecord->AuthoredKey == FName(TEXT("Task.ShapeBeam"))
			&& ShapeBeamTypeRecord->Name == TEXT("Shape Beam"));

	const uint32 UnresolvableTaskValues[] = {
		0u, AfterRejection.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableTaskValue : UnresolvableTaskValues)
	{
		const FTaskId UnknownId(UnresolvableTaskValue);
		TestFalse(FString::Printf(TEXT("Unknown task %u is not contained"), UnresolvableTaskValue),
			Registry.ContainsTask(UnknownId));
		TestFalse(FString::Printf(TEXT("Unknown task %u does not resolve"), UnresolvableTaskValue),
			Registry.FindTask(UnknownId).IsSet());
	}

	TestFalse(TEXT("A default task identifier is not contained"), Registry.ContainsTask(FTaskId()));
	TestFalse(TEXT("A default task identifier does not resolve"), Registry.FindTask(FTaskId()).IsSet());

	VerifyInvariants(*this, Registry, TEXT("after task creation and rejection"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.Task.SnapshotRegression", SimulationTestFlags)

bool FSimulationTaskSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;
	const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	const FTaskTypeId FillerType = Registry.CreateTaskType(TEXT("Task.FetchWater"), TEXT("Fetch Water"));
	const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);

	TOptional<FTaskRecord> Snapshot = Registry.FindTask(ShapeBeam);
	if (!Snapshot.IsSet())
	{
		AddError(TEXT("The task snapshot should resolve"));
		return false;
	}

	for (int32 FillerIndex = 0; FillerIndex < 32; ++FillerIndex)
	{
		Registry.CreateTask(FillerType);
	}

	TestEqual(TEXT("Task storage grew substantially past its original capacity"),
		Registry.GetTaskCount(), 33);
	TestTrue(TEXT("The held snapshot keeps its identifier across storage growth"),
		Snapshot->Id == ShapeBeam);
	TestTrue(TEXT("The held snapshot keeps its task type across storage growth"),
		Snapshot->TaskTypeId == ShapeBeamType);

	const TOptional<FTaskRecord> AuthoritativeBeforeMutation = Registry.FindTask(ShapeBeam);
	if (!AuthoritativeBeforeMutation.IsSet())
	{
		AddError(TEXT("The authoritative task should resolve before snapshot mutation"));
		return false;
	}

	TestTrue(TEXT("The authoritative record holds the original identity before the snapshot is edited"),
		AuthoritativeBeforeMutation->Id == ShapeBeam
			&& AuthoritativeBeforeMutation->TaskTypeId == ShapeBeamType);

	Snapshot->Id = FTaskId(UInt32MaxIdValue);
	Snapshot->TaskTypeId = FillerType;

	const TOptional<FTaskRecord> AuthoritativeRecord = Registry.FindTask(ShapeBeam);
	if (!AuthoritativeRecord.IsSet())
	{
		AddError(TEXT("The authoritative task should still resolve after snapshot mutation"));
		return false;
	}

	TestTrue(TEXT("Snapshot mutation does not change the authoritative identifier"),
		AuthoritativeRecord->Id == ShapeBeam);
	TestTrue(TEXT("Snapshot mutation does not change the authoritative task type"),
		AuthoritativeRecord->TaskTypeId == ShapeBeamType);
	TestFalse(TEXT("The tampered snapshot identifier resolves to nothing"),
		Registry.ContainsTask(FTaskId(UInt32MaxIdValue)));
	TestEqual(TEXT("Snapshot mutation stores no additional task"), Registry.GetTaskCount(), 33);

	VerifyInvariants(*this, Registry, TEXT("after mutating a detached task snapshot"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskIndependenceTest,
	"RealmsUnwritten.Simulation.Task.Independence", SimulationTestFlags)

bool FSimulationTaskIndependenceTest::RunTest(const FString& Parameters)
{
	// A regression boundary, not a dependency: the fixture exists only so that creating a
	// task has something it could wrongly disturb. A task needs none of this state.
	FSimulationRegistry Registry;

	FPersonCreationParams PersonParams;
	PersonParams.GivenName = TEXT("William");
	PersonParams.FamilyName = TEXT("Carter");
	PersonParams.AgeYears = 34;
	const FPersonId William = Registry.CreatePerson(PersonParams);

	const FSettlementId Settlement = Registry.CreateSettlement(TEXT("Ashford"));
	const FPropertyId Property = Registry.CreateProperty(Settlement);
	const FPhysicalSiteId Workshop =
		Registry.CreatePhysicalSite(Property, TEXT("Site.Workshop"), TEXT("Carpenter's Workshop"));
	const FWorkTypeId Carpentry = Registry.CreateWorkType(TEXT("Work.Carpentry"), TEXT("Carpentry"));
	TestEqual(TEXT("The fixture assigns current work"),
		Registry.AssignPersonWork(William, Carpentry, Workshop), EPersonWorkResult::Success);

	const FActivityTypeId Working = Registry.CreateActivityType(TEXT("Activity.Working"), TEXT("Working"));
	TestEqual(TEXT("The fixture sets a current activity"),
		Registry.SetPersonCurrentActivity(William, Working), EPersonActivityResult::Success);

	const FSkillTypeId CarpentrySkill =
		Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	TestEqual(TEXT("The fixture records a capability"),
		Registry.AddPersonCapability(William, CarpentrySkill), EPersonCapabilityResult::Success);
	Registry.AddPersonCapabilityPractice(William, CarpentrySkill, 40u);

	const FGoodTypeId Timber = Registry.CreateGoodType(TEXT("Good.Timber"), TEXT("Timber"));
	const FInventoryId WorkshopStore = Registry.CreateInventory();
	Registry.AssignInventoryToSite(WorkshopStore, Workshop);
	Registry.AddGoods(WorkshopStore, Timber, 12, TEXT("Reason.Test"));

	VerifyInvariants(*this, Registry, TEXT("with the pre-task fixture in place"));

	const TOptional<FPersonRecord> PersonBefore = Registry.FindPerson(William);
	const TOptional<FCurrentActivity> ActivityBefore = Registry.GetPersonCurrentActivity(William);
	const TOptional<uint32> PracticeBefore = Registry.GetPersonCapabilityPractice(William, CarpentrySkill);
	const TOptional<uint32> GeneralBefore = Registry.GetPersonGeneralCapability(William, CarpentrySkill);
	const TOptional<int32> TimberBefore = Registry.GetQuantity(WorkshopStore, Timber);
	if (!PersonBefore.IsSet() || !ActivityBefore.IsSet() || !PracticeBefore.IsSet()
		|| !GeneralBefore.IsSet() || !TimberBefore.IsSet())
	{
		AddError(TEXT("The fixture preconditions should all resolve before any task exists"));
		return false;
	}

	const int32 PersonCountBefore = Registry.GetPersonCount();
	const int32 CapabilityCountBefore = Registry.GetPersonCapabilityCount();
	const int32 InventoryCountBefore = Registry.GetInventoryCount();
	const int32 SiteCountBefore = Registry.GetPhysicalSiteCount();

	const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	const FTaskTypeId FeedChildType = Registry.CreateTaskType(TEXT("Task.FeedChild"), TEXT("Feed Child"));
	const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);
	const FTaskId FeedChild = Registry.CreateTask(FeedChildType);
	TestTrue(TEXT("Both tasks were created"), ShapeBeam.IsValid() && FeedChild.IsValid());

	const TOptional<FPersonRecord> PersonAfter = Registry.FindPerson(William);
	if (!PersonAfter.IsSet())
	{
		AddError(TEXT("The person should still resolve after tasks exist"));
		return false;
	}

	TestTrue(TEXT("Task creation does not change the person's identity or names"),
		PersonAfter->Id == PersonBefore->Id
			&& PersonAfter->GivenName == PersonBefore->GivenName
			&& PersonAfter->FamilyName == PersonBefore->FamilyName
			&& PersonAfter->AgeYears == PersonBefore->AgeYears
			&& PersonAfter->LifeState == PersonBefore->LifeState
			&& PersonAfter->HouseholdId == PersonBefore->HouseholdId);

	// Shape Beam does not employ anyone, and Feed Child does not end an employment.
	TestTrue(TEXT("Task creation does not change current work"),
		PersonAfter->CurrentWork.Kind == PersonBefore->CurrentWork.Kind
			&& PersonAfter->CurrentWork.WorkTypeId == PersonBefore->CurrentWork.WorkTypeId
			&& PersonAfter->CurrentWork.PhysicalSiteId == PersonBefore->CurrentWork.PhysicalSiteId);

	// Shape Beam does not make anyone Working, and it does not stop them being Working.
	const TOptional<FCurrentActivity> ActivityAfter = Registry.GetPersonCurrentActivity(William);
	TestTrue(TEXT("Task creation does not change current activity"),
		ActivityAfter.IsSet()
			&& ActivityAfter->Kind == ActivityBefore->Kind
			&& ActivityAfter->ActivityTypeId == ActivityBefore->ActivityTypeId);

	const TOptional<uint32> PracticeAfter = Registry.GetPersonCapabilityPractice(William, CarpentrySkill);
	const TOptional<uint32> GeneralAfter = Registry.GetPersonGeneralCapability(William, CarpentrySkill);
	TestTrue(TEXT("Task creation grants no practice"),
		PracticeAfter.IsSet() && PracticeAfter.GetValue() == PracticeBefore.GetValue());
	TestTrue(TEXT("Task creation changes no derived general capability"),
		GeneralAfter.IsSet() && GeneralAfter.GetValue() == GeneralBefore.GetValue());
	TestTrue(TEXT("Task creation grants no new capability"),
		Registry.PersonHasCapability(William, CarpentrySkill));
	TestEqual(TEXT("Task creation records no capability relationship"),
		Registry.GetPersonCapabilityCount(), CapabilityCountBefore);

	const TOptional<int32> TimberAfter = Registry.GetQuantity(WorkshopStore, Timber);
	TestTrue(TEXT("Task creation consumes and produces no goods"),
		TimberAfter.IsSet() && TimberAfter.GetValue() == TimberBefore.GetValue());

	TestEqual(TEXT("Task creation creates no person"), Registry.GetPersonCount(), PersonCountBefore);
	TestEqual(TEXT("Task creation creates no inventory"), Registry.GetInventoryCount(), InventoryCountBefore);
	TestEqual(TEXT("Task creation creates no physical site"),
		Registry.GetPhysicalSiteCount(), SiteCountBefore);

	VerifyInvariants(*this, Registry, TEXT("after creating tasks alongside unrelated state"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskInvariantDetectionTest,
	"RealmsUnwritten.Simulation.Task.InvariantDetection", SimulationTestFlags)

bool FSimulationTaskInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		const FTaskTypeId ShapeBeamType =
			Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		Registry.CreateTask(ShapeBeamType);
		VerifyInvariants(*this, Registry, TEXT("before corrupting a task identifier"));

		FSimulationRegistryTestAccess::TaskRecords(Registry)[0].Id = FTaskId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a task slot and identifier mismatch"));
	}

	{
		FSimulationRegistry Registry;
		const FTaskTypeId ShapeBeamType =
			Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		Registry.CreateTask(ShapeBeamType);
		VerifyInvariants(*this, Registry, TEXT("before clearing a task's task type"));

		FSimulationRegistryTestAccess::TaskRecords(Registry)[0].TaskTypeId = FTaskTypeId();
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a task with a default task type"));
	}

	{
		FSimulationRegistry Registry;
		const FTaskTypeId ShapeBeamType =
			Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		Registry.CreateTask(ShapeBeamType);
		VerifyInvariants(*this, Registry, TEXT("before pointing a task past allocated task types"));

		// Nonzero but never allocated: one task type exists, so slot 9 was never handed out.
		FSimulationRegistryTestAccess::TaskRecords(Registry)[0].TaskTypeId = FTaskTypeId(9);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a task naming an unallocated task type"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
