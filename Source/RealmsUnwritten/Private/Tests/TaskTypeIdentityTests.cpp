#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for authored Task Type identity.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Task types are created at runtime under authored keys. The keys used here, such as
 * Task.ShapeBeam, are test data only and appear in no production catalog.
 *
 * A task type is a definition only. Nothing here creates a task instance, names a person,
 * place, or target, or touches work, activity, capability, or goods state.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskTypeCreationTest,
	"RealmsUnwritten.Simulation.TaskType.Creation", SimulationTestFlags)

bool FSimulationTaskTypeCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	TestFalse(TEXT("A task type cannot be created without an authored key"),
		Registry.CreateTaskType(NAME_None, TEXT("Nameless")).IsValid());
	TestEqual(TEXT("A rejected nameless definition stores no task type"), Registry.GetTaskTypeCount(), 0);

	const FTaskTypeId ShapeBeam = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	TestTrue(TEXT("A valid authored key creates a task type"), ShapeBeam.IsValid());
	TestEqual(TEXT("The rejected nameless definition consumed no runtime handle"), ShapeBeam.GetValue(), 1u);
	TestTrue(TEXT("The created task type is contained"), Registry.ContainsTaskType(ShapeBeam));
	TestEqual(TEXT("The created task type is counted"), Registry.GetTaskTypeCount(), 1);

	const TOptional<FTaskTypeRecord> ShapeBeamRecord = Registry.FindTaskType(ShapeBeam);
	if (!ShapeBeamRecord.IsSet())
	{
		AddError(TEXT("The created task type should resolve by identifier"));
		return false;
	}

	TestTrue(TEXT("The record preserves its typed identifier"), ShapeBeamRecord->Id == ShapeBeam);
	TestTrue(TEXT("The record preserves its authored key"),
		ShapeBeamRecord->AuthoredKey == FName(TEXT("Task.ShapeBeam")));
	TestEqual(TEXT("The record preserves its display name"),
		ShapeBeamRecord->Name, FString(TEXT("Shape Beam")));

	const TOptional<FTaskTypeId> ShapeBeamByKey = Registry.FindTaskTypeIdByKey(TEXT("Task.ShapeBeam"));
	TestTrue(TEXT("A known authored key resolves to its typed identifier"),
		ShapeBeamByKey.IsSet() && ShapeBeamByKey.GetValue() == ShapeBeam);
	TestFalse(TEXT("An unknown authored key does not resolve"),
		Registry.FindTaskTypeIdByKey(TEXT("Task.Unknown")).IsSet());
	TestFalse(TEXT("NAME_None does not resolve as an authored key"),
		Registry.FindTaskTypeIdByKey(NAME_None).IsSet());

	const int32 CountBeforeDuplicate = Registry.GetTaskTypeCount();
	const FTaskTypeId Duplicate = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Altered Beam"));
	TestFalse(TEXT("A duplicate authored key is rejected"), Duplicate.IsValid());
	TestEqual(TEXT("A duplicate authored key stores no additional record"),
		Registry.GetTaskTypeCount(), CountBeforeDuplicate);

	const TOptional<FTaskTypeRecord> OriginalAfterDuplicate = Registry.FindTaskType(ShapeBeam);
	TestTrue(TEXT("Duplicate rejection leaves the original authoritative record unchanged"),
		OriginalAfterDuplicate.IsSet()
			&& OriginalAfterDuplicate->AuthoredKey == FName(TEXT("Task.ShapeBeam"))
			&& OriginalAfterDuplicate->Name == TEXT("Shape Beam"));

	const FTaskTypeId HarvestWheat = Registry.CreateTaskType(TEXT("Task.HarvestWheat"), TEXT("Harvest Wheat"));
	TestEqual(TEXT("A rejected duplicate consumed no runtime handle"),
		HarvestWheat.GetValue(), static_cast<uint32>(CountBeforeDuplicate + 1));
	const FTaskTypeId OtherShapeBeam =
		Registry.CreateTaskType(TEXT("Task.OtherShapeBeam"), TEXT("Shape Beam"));
	TestTrue(TEXT("A duplicate display name under a distinct key is allowed"), OtherShapeBeam.IsValid());
	TestTrue(TEXT("The duplicate display name resolves to its own authored key"),
		Registry.FindTaskTypeIdByKey(TEXT("Task.OtherShapeBeam")).GetValue() == OtherShapeBeam);
	TestEqual(TEXT("Only the accepted definitions are counted"), Registry.GetTaskTypeCount(), 3);

	const uint32 UnresolvableValues[] = {
		0u, OtherShapeBeam.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FTaskTypeId UnknownId(UnresolvableValue);
		TestFalse(FString::Printf(TEXT("Unknown task type %u is not contained"), UnresolvableValue),
			Registry.ContainsTaskType(UnknownId));
		TestFalse(FString::Printf(TEXT("Unknown task type %u does not resolve"), UnresolvableValue),
			Registry.FindTaskType(UnknownId).IsSet());
	}

	VerifyInvariants(*this, Registry, TEXT("after task-type creation and rejection"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskTypeNamespaceIndependenceTest,
	"RealmsUnwritten.Simulation.TaskType.NamespaceIndependence", SimulationTestFlags)

bool FSimulationTaskTypeNamespaceIndependenceTest::RunTest(const FString& Parameters)
{
	static_assert(!SimulationIdContract::bInterchangeable<FTaskTypeId, FGoodTypeId>,
		"Task-type and good-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FTaskTypeId, FWorkTypeId>,
		"Task-type and work-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FTaskTypeId, FSkillTypeId>,
		"Task-type and skill-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FTaskTypeId, FActivityTypeId>,
		"Task-type and activity-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FTaskTypeId, FPersonId>,
		"Task-type and person identifiers must not be interchangeable.");

	FSimulationRegistry Registry;
	const FName SharedKey(TEXT("Working"));
	const FGoodTypeId GoodTypeId = Registry.CreateGoodType(SharedKey, TEXT("Working Good"));
	const FWorkTypeId WorkTypeId = Registry.CreateWorkType(SharedKey, TEXT("Working Work"));
	const FSkillTypeId SkillTypeId = Registry.CreateSkillType(SharedKey, TEXT("Working Skill"));
	const FActivityTypeId ActivityTypeId = Registry.CreateActivityType(SharedKey, TEXT("Working Activity"));
	const FTaskTypeId TaskTypeId = Registry.CreateTaskType(SharedKey, TEXT("Working Task"));

	TestTrue(TEXT("The Good Type accepts the exact shared key Working"), GoodTypeId.IsValid());
	TestTrue(TEXT("The Work Type accepts the exact shared key Working"), WorkTypeId.IsValid());
	TestTrue(TEXT("The Skill Type accepts the exact shared key Working"), SkillTypeId.IsValid());
	TestTrue(TEXT("The Activity Type accepts the exact shared key Working"), ActivityTypeId.IsValid());
	TestTrue(TEXT("The Task Type accepts the exact shared key Working"), TaskTypeId.IsValid());
	TestEqual(TEXT("The Good Type registry holds exactly its registration"), Registry.GetGoodTypeCount(), 1);
	TestEqual(TEXT("The Work Type registry holds exactly its registration"), Registry.GetWorkTypeCount(), 1);
	TestEqual(TEXT("The Skill Type registry holds exactly its registration"), Registry.GetSkillTypeCount(), 1);
	TestEqual(TEXT("The Activity Type registry holds exactly its registration"),
		Registry.GetActivityTypeCount(), 1);
	TestEqual(TEXT("The Task Type registry holds exactly its registration"), Registry.GetTaskTypeCount(), 1);

	const TOptional<FGoodTypeId> ResolvedGood = Registry.FindGoodTypeIdByKey(SharedKey);
	const TOptional<FWorkTypeId> ResolvedWork = Registry.FindWorkTypeIdByKey(SharedKey);
	const TOptional<FSkillTypeId> ResolvedSkill = Registry.FindSkillTypeIdByKey(SharedKey);
	const TOptional<FActivityTypeId> ResolvedActivity = Registry.FindActivityTypeIdByKey(SharedKey);
	const TOptional<FTaskTypeId> ResolvedTask = Registry.FindTaskTypeIdByKey(SharedKey);
	TestTrue(TEXT("The Good Type registry independently resolves Working"),
		ResolvedGood.IsSet() && ResolvedGood.GetValue() == GoodTypeId);
	TestTrue(TEXT("The Work Type registry independently resolves Working"),
		ResolvedWork.IsSet() && ResolvedWork.GetValue() == WorkTypeId);
	TestTrue(TEXT("The Skill Type registry independently resolves Working"),
		ResolvedSkill.IsSet() && ResolvedSkill.GetValue() == SkillTypeId);
	TestTrue(TEXT("The Activity Type registry independently resolves Working"),
		ResolvedActivity.IsSet() && ResolvedActivity.GetValue() == ActivityTypeId);
	TestTrue(TEXT("The Task Type registry independently resolves Working"),
		ResolvedTask.IsSet() && ResolvedTask.GetValue() == TaskTypeId);

	const TOptional<FTaskTypeRecord> TaskRecord = Registry.FindTaskType(TaskTypeId);
	TestTrue(TEXT("The Task Type record carries only its own display name"),
		TaskRecord.IsSet() && TaskRecord->Name == TEXT("Working Task"));

	VerifyInvariants(*this, Registry, TEXT("with one identical key in five definition namespaces"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskTypeSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.TaskType.SnapshotRegression", SimulationTestFlags)

bool FSimulationTaskTypeSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;
	const FTaskTypeId ShapeBeam = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	TOptional<FTaskTypeRecord> Snapshot = Registry.FindTaskType(ShapeBeam);
	if (!Snapshot.IsSet())
	{
		AddError(TEXT("The task type snapshot should resolve"));
		return false;
	}

	for (int32 FillerIndex = 0; FillerIndex < 16; ++FillerIndex)
	{
		Registry.CreateTaskType(
			FName(*FString::Printf(TEXT("Task.Filler%d"), FillerIndex)), TEXT("Filler"));
	}

	TestTrue(TEXT("The held snapshot survives storage growth with its identifier intact"),
		Snapshot->Id == ShapeBeam);
	TestTrue(TEXT("The held snapshot survives storage growth with its authored key intact"),
		Snapshot->AuthoredKey == FName(TEXT("Task.ShapeBeam")));
	TestEqual(TEXT("The held snapshot survives storage growth with its display name intact"),
		Snapshot->Name, FString(TEXT("Shape Beam")));

	const TOptional<FTaskTypeRecord> AuthoritativeBeforeMutation = Registry.FindTaskType(ShapeBeam);
	if (!AuthoritativeBeforeMutation.IsSet())
	{
		AddError(TEXT("The authoritative task type should resolve before snapshot mutation"));
		return false;
	}

	TestTrue(TEXT("The authoritative record holds Task.ShapeBeam before the snapshot is edited"),
		AuthoritativeBeforeMutation->Id == ShapeBeam
			&& AuthoritativeBeforeMutation->AuthoredKey == FName(TEXT("Task.ShapeBeam"))
			&& AuthoritativeBeforeMutation->Name == TEXT("Shape Beam"));

	Snapshot->Id = FTaskTypeId(UInt32MaxIdValue);
	Snapshot->AuthoredKey = FName(TEXT("Task.Tampered"));
	Snapshot->Name = TEXT("Tampered");

	const TOptional<FTaskTypeRecord> AuthoritativeRecord = Registry.FindTaskType(ShapeBeam);
	if (!AuthoritativeRecord.IsSet())
	{
		AddError(TEXT("The authoritative task type should still resolve after snapshot mutation"));
		return false;
	}

	TestTrue(TEXT("Snapshot mutation does not change the authoritative identifier"),
		AuthoritativeRecord->Id == ShapeBeam);
	TestTrue(TEXT("Snapshot mutation does not change the authoritative key"),
		AuthoritativeRecord->AuthoredKey == FName(TEXT("Task.ShapeBeam")));
	TestEqual(TEXT("Snapshot mutation does not change the authoritative display name"),
		AuthoritativeRecord->Name, FString(TEXT("Shape Beam")));
	TestFalse(TEXT("The tampered snapshot key was not registered"),
		Registry.FindTaskTypeIdByKey(TEXT("Task.Tampered")).IsSet());

	VerifyInvariants(*this, Registry, TEXT("after mutating a detached task-type snapshot"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationTaskTypeInvariantDetectionTest,
	"RealmsUnwritten.Simulation.TaskType.InvariantDetection", SimulationTestFlags)

bool FSimulationTaskTypeInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		VerifyInvariants(*this, Registry, TEXT("before corrupting a task type identifier"));

		FSimulationRegistryTestAccess::TaskTypeRecords(Registry)[0].Id = FTaskTypeId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a task-type slot and identifier mismatch"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		FSimulationRegistryTestAccess::TaskTypeRecords(Registry)[0].AuthoredKey = NAME_None;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a task type with no authored key"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		Registry.CreateTaskType(TEXT("Task.HarvestWheat"), TEXT("Harvest Wheat"));
		FSimulationRegistryTestAccess::TaskTypeRecords(Registry)[1].AuthoredKey =
			FName(TEXT("Task.ShapeBeam"));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("duplicate task-type authored keys"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
