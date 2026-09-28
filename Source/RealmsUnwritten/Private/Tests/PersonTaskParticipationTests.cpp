#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests for Person–Task Participation: recognized membership of a Person in a Task.
 *
 * Every test builds its own FSimulationRegistry on the stack. No Actor is spawned, no
 * UObject is created, and no map is opened or required.
 *
 * Participation is relationship existence only. These tests do not cover assignment,
 * CurrentTask, presence, execution, contribution, roles, removal, or reverse Task-to-person
 * search.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonTaskParticipationAdditionTest,
	"RealmsUnwritten.Simulation.PersonTaskParticipation.Addition", SimulationTestFlags)

bool FSimulationPersonTaskParticipationAdditionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);
	TestTrue(TEXT("A task instance exists before any participation is recorded"), ShapeBeam.IsValid());
	TestEqual(TEXT("No participation relationships exist until one is added"),
		Registry.GetPersonTaskParticipationCount(), 0);

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
	TestFalse(TEXT("William does not yet participate in the existing task"),
		Registry.HasPersonTaskParticipation(William, ShapeBeam));

	TestTrue(TEXT("Recording William + Shape Beam succeeds"),
		Registry.AddPersonTaskParticipation(William, ShapeBeam)
			== EPersonTaskParticipationResult::Success);
	TestEqual(TEXT("One sparse relationship is stored"), Registry.GetPersonTaskParticipationCount(), 1);
	TestTrue(TEXT("William participates in Shape Beam after addition"),
		Registry.HasPersonTaskParticipation(William, ShapeBeam));

	const TOptional<FPersonRecord> WilliamRecord = Registry.FindPerson(William);
	const TOptional<FTaskRecord> ShapeBeamRecord = Registry.FindTask(ShapeBeam);
	if (!WilliamRecord.IsSet() || !ShapeBeamRecord.IsSet())
	{
		AddError(TEXT("William and the task should still resolve after participation addition"));
		return false;
	}

	TestTrue(TEXT("Participation addition does not change person identity"), WilliamRecord->Id == William);
	TestEqual(TEXT("Participation addition does not change the person's given name"),
		WilliamRecord->GivenName, FString(TEXT("William")));
	TestTrue(TEXT("Participation addition does not change task identity"), ShapeBeamRecord->Id == ShapeBeam);
	TestTrue(TEXT("Participation addition does not change the task's type"),
		ShapeBeamRecord->TaskTypeId == ShapeBeamType);

	VerifyInvariants(*this, Registry, TEXT("after recording a person-task participation"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonTaskParticipationCardinalityTest,
	"RealmsUnwritten.Simulation.PersonTaskParticipation.Cardinality", SimulationTestFlags)

bool FSimulationPersonTaskParticipationCardinalityTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
	const FPersonId Thomas = Registry.CreatePerson(MakePersonParams(TEXT("Thomas"), TEXT("Miller"), 22));
	const FPersonId John = Registry.CreatePerson(MakePersonParams(TEXT("John"), TEXT("Baker"), 41));
	const FPersonId Robert = Registry.CreatePerson(MakePersonParams(TEXT("Robert"), TEXT("Cooper"), 29));

	const FTaskTypeId RaiseFrameType =
		Registry.CreateTaskType(TEXT("Task.RaiseTimberFrame"), TEXT("Raise Timber Frame"));
	const FTaskTypeId RepairType =
		Registry.CreateTaskType(TEXT("Task.RepairWorkshop"), TEXT("Repair Workshop"));
	const FTaskTypeId BuildCartType = Registry.CreateTaskType(TEXT("Task.BuildCart"), TEXT("Build Cart"));
	const FTaskId TaskA = Registry.CreateTask(RaiseFrameType);
	const FTaskId TaskB = Registry.CreateTask(RepairType);
	const FTaskId TaskC = Registry.CreateTask(BuildCartType);

	TestTrue(TEXT("William participates in Task A"),
		Registry.AddPersonTaskParticipation(William, TaskA) == EPersonTaskParticipationResult::Success);
	TestTrue(TEXT("Thomas participates in Task A"),
		Registry.AddPersonTaskParticipation(Thomas, TaskA) == EPersonTaskParticipationResult::Success);
	TestTrue(TEXT("John participates in Task A"),
		Registry.AddPersonTaskParticipation(John, TaskA) == EPersonTaskParticipationResult::Success);
	TestTrue(TEXT("Robert participates in Task A"),
		Registry.AddPersonTaskParticipation(Robert, TaskA) == EPersonTaskParticipationResult::Success);

	TestTrue(TEXT("All four people participate in Task A at once"),
		Registry.HasPersonTaskParticipation(William, TaskA)
			&& Registry.HasPersonTaskParticipation(Thomas, TaskA)
			&& Registry.HasPersonTaskParticipation(John, TaskA)
			&& Registry.HasPersonTaskParticipation(Robert, TaskA));
	TestEqual(TEXT("Four people on one task store four distinct pairs"),
		Registry.GetPersonTaskParticipationCount(), 4);

	TestTrue(TEXT("William also participates in Task B"),
		Registry.AddPersonTaskParticipation(William, TaskB) == EPersonTaskParticipationResult::Success);
	TestTrue(TEXT("William also participates in Task C"),
		Registry.AddPersonTaskParticipation(William, TaskC) == EPersonTaskParticipationResult::Success);

	TestTrue(TEXT("William participates in Task A, Task B, and Task C at once"),
		Registry.HasPersonTaskParticipation(William, TaskA)
			&& Registry.HasPersonTaskParticipation(William, TaskB)
			&& Registry.HasPersonTaskParticipation(William, TaskC));
	TestFalse(TEXT("Thomas does not participate in William's additional tasks"),
		Registry.HasPersonTaskParticipation(Thomas, TaskB)
			|| Registry.HasPersonTaskParticipation(Thomas, TaskC));
	TestEqual(TEXT("Six recorded pairs, not a dense Person x Task matrix"),
		Registry.GetPersonTaskParticipationCount(), 6);

	VerifyInvariants(*this, Registry, TEXT("after recording many-to-many participation"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonTaskParticipationRejectionTest,
	"RealmsUnwritten.Simulation.PersonTaskParticipation.Rejection", SimulationTestFlags)

bool FSimulationPersonTaskParticipationRejectionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
	const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);
	TestTrue(TEXT("The original pair is recorded before rejection cases"),
		Registry.AddPersonTaskParticipation(William, ShapeBeam)
			== EPersonTaskParticipationResult::Success);

	const int32 CountAfterSuccess = Registry.GetPersonTaskParticipationCount();
	const TOptional<FPersonRecord> WilliamBefore = Registry.FindPerson(William);
	const TOptional<FTaskRecord> TaskBefore = Registry.FindTask(ShapeBeam);
	if (!WilliamBefore.IsSet() || !TaskBefore.IsSet())
	{
		AddError(TEXT("William and the task should resolve before rejection cases"));
		return false;
	}

	TestTrue(TEXT("A duplicate pair is rejected as already participating"),
		Registry.AddPersonTaskParticipation(William, ShapeBeam)
			== EPersonTaskParticipationResult::AlreadyParticipating);
	TestEqual(TEXT("Duplicate rejection stores no additional relationship"),
		Registry.GetPersonTaskParticipationCount(), CountAfterSuccess);

	const uint32 UnresolvableValues[] = {
		0u, William.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		TestTrue(FString::Printf(TEXT("Unknown person %u is UnknownPerson"), UnresolvableValue),
			Registry.AddPersonTaskParticipation(FPersonId(UnresolvableValue), ShapeBeam)
				== EPersonTaskParticipationResult::UnknownPerson);
		TestTrue(FString::Printf(TEXT("Unknown task %u is UnknownTask"), UnresolvableValue),
			Registry.AddPersonTaskParticipation(William, FTaskId(UnresolvableValue))
				== EPersonTaskParticipationResult::UnknownTask);
		TestTrue(FString::Printf(TEXT("Unknown person %u wins when the task is also unknown"),
				UnresolvableValue),
			Registry.AddPersonTaskParticipation(FPersonId(UnresolvableValue), FTaskId(UnresolvableValue))
				== EPersonTaskParticipationResult::UnknownPerson);
		TestFalse(FString::Printf(TEXT("Unknown pair %u is not recorded as participating"), UnresolvableValue),
			Registry.HasPersonTaskParticipation(FPersonId(UnresolvableValue), FTaskId(UnresolvableValue)));
	}

	TestEqual(TEXT("Rejected calls store no additional relationship"),
		Registry.GetPersonTaskParticipationCount(), CountAfterSuccess);
	TestTrue(TEXT("The original pair still exists after rejection"),
		Registry.HasPersonTaskParticipation(William, ShapeBeam));

	const TOptional<FPersonRecord> WilliamAfter = Registry.FindPerson(William);
	const TOptional<FTaskRecord> TaskAfter = Registry.FindTask(ShapeBeam);
	TestTrue(TEXT("Rejection does not change person identity or names"),
		WilliamAfter.IsSet()
			&& WilliamAfter->Id == WilliamBefore->Id
			&& WilliamAfter->GivenName == WilliamBefore->GivenName
			&& WilliamAfter->FamilyName == WilliamBefore->FamilyName);
	TestTrue(TEXT("Rejection does not change task identity or type"),
		TaskAfter.IsSet()
			&& TaskAfter->Id == TaskBefore->Id
			&& TaskAfter->TaskTypeId == TaskBefore->TaskTypeId);

	VerifyInvariants(*this, Registry, TEXT("after participation rejection"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonTaskParticipationIndependenceTest,
	"RealmsUnwritten.Simulation.PersonTaskParticipation.Independence", SimulationTestFlags)

bool FSimulationPersonTaskParticipationIndependenceTest::RunTest(const FString& Parameters)
{
	// A regression boundary, not a dependency: the fixture exists so that recording
	// participation has something it could wrongly disturb. Participation needs none of it.
	FSimulationRegistry Registry;

	const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
	const FPersonId Thomas = Registry.CreatePerson(MakePersonParams(TEXT("Thomas"), TEXT("Miller"), 22));
	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	TestTrue(TEXT("William has Carpentry so practice can be proven unchanged"),
		Registry.AddPersonCapability(William, Carpentry) == EPersonCapabilityResult::Success);
	TestTrue(TEXT("William has quantified practice before participation"),
		Registry.AddPersonCapabilityPractice(William, Carpentry, 40u)
			== EPersonCapabilityPracticeResult::Success);
	TestFalse(TEXT("Thomas has no Carpentry capability"),
		Registry.PersonHasCapability(Thomas, Carpentry));

	const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
	const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);

	VerifyInvariants(*this, Registry, TEXT("with the pre-participation fixture in place"));

	const TOptional<FPersonRecord> WilliamBefore = Registry.FindPerson(William);
	const TOptional<FPersonRecord> ThomasBefore = Registry.FindPerson(Thomas);
	const TOptional<FTaskRecord> TaskBefore = Registry.FindTask(ShapeBeam);
	const TOptional<uint32> PracticeBefore = Registry.GetPersonCapabilityPractice(William, Carpentry);
	const TOptional<uint32> GeneralBefore = Registry.GetPersonGeneralCapability(William, Carpentry);
	if (!WilliamBefore.IsSet() || !ThomasBefore.IsSet() || !TaskBefore.IsSet()
		|| !PracticeBefore.IsSet() || !GeneralBefore.IsSet())
	{
		AddError(TEXT("The fixture preconditions should all resolve before any participation exists"));
		return false;
	}

	TestTrue(TEXT("William has no household, no current work, and no current activity"),
		!WilliamBefore->HouseholdId.IsValid()
			&& WilliamBefore->CurrentWork.Kind == ECurrentWorkKind::None
			&& WilliamBefore->CurrentActivity.Kind == ECurrentActivityKind::None);

	const int32 CapabilityCountBefore = Registry.GetPersonCapabilityCount();
	const int32 PersonCountBefore = Registry.GetPersonCount();
	const int32 TaskCountBefore = Registry.GetTaskCount();

	TestTrue(TEXT("William participates without work, household, or recorded activity"),
		Registry.AddPersonTaskParticipation(William, ShapeBeam)
			== EPersonTaskParticipationResult::Success);
	TestTrue(TEXT("Thomas participates in Shape Beam without Carpentry"),
		Registry.AddPersonTaskParticipation(Thomas, ShapeBeam)
			== EPersonTaskParticipationResult::Success);

	const TOptional<FPersonRecord> WilliamAfter = Registry.FindPerson(William);
	const TOptional<FPersonRecord> ThomasAfter = Registry.FindPerson(Thomas);
	const TOptional<FTaskRecord> TaskAfter = Registry.FindTask(ShapeBeam);
	if (!WilliamAfter.IsSet() || !ThomasAfter.IsSet() || !TaskAfter.IsSet())
	{
		AddError(TEXT("People and the task should still resolve after participation"));
		return false;
	}

	TestTrue(TEXT("Participation does not change William's identity, household, work, or activity"),
		WilliamAfter->Id == WilliamBefore->Id
			&& WilliamAfter->HouseholdId == WilliamBefore->HouseholdId
			&& WilliamAfter->CurrentWork.Kind == WilliamBefore->CurrentWork.Kind
			&& WilliamAfter->CurrentWork.WorkTypeId == WilliamBefore->CurrentWork.WorkTypeId
			&& WilliamAfter->CurrentActivity.Kind == WilliamBefore->CurrentActivity.Kind
			&& WilliamAfter->CurrentActivity.ActivityTypeId == WilliamBefore->CurrentActivity.ActivityTypeId);
	TestTrue(TEXT("Participation does not change Thomas's identity, household, work, or activity"),
		ThomasAfter->Id == ThomasBefore->Id
			&& ThomasAfter->HouseholdId == ThomasBefore->HouseholdId
			&& ThomasAfter->CurrentWork.Kind == ThomasBefore->CurrentWork.Kind
			&& ThomasAfter->CurrentActivity.Kind == ThomasBefore->CurrentActivity.Kind);
	TestTrue(TEXT("Participation does not change the task identity or type"),
		TaskAfter->Id == TaskBefore->Id && TaskAfter->TaskTypeId == TaskBefore->TaskTypeId);

	TestTrue(TEXT("Participation grants Thomas no Carpentry"),
		!Registry.PersonHasCapability(Thomas, Carpentry));
	TestEqual(TEXT("Participation records no capability relationship"),
		Registry.GetPersonCapabilityCount(), CapabilityCountBefore);
	const TOptional<uint32> PracticeAfter = Registry.GetPersonCapabilityPractice(William, Carpentry);
	const TOptional<uint32> GeneralAfter = Registry.GetPersonGeneralCapability(William, Carpentry);
	TestTrue(TEXT("Participation generates no practice"),
		PracticeAfter.IsSet() && PracticeAfter.GetValue() == PracticeBefore.GetValue());
	TestTrue(TEXT("Participation changes no derived general capability"),
		GeneralAfter.IsSet() && GeneralAfter.GetValue() == GeneralBefore.GetValue());
	TestEqual(TEXT("Participation creates no person"), Registry.GetPersonCount(), PersonCountBefore);
	TestEqual(TEXT("Participation creates no task"), Registry.GetTaskCount(), TaskCountBefore);

	VerifyInvariants(*this, Registry, TEXT("after recording participation alongside unrelated state"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationPersonTaskParticipationInvariantDetectionTest,
	"RealmsUnwritten.Simulation.PersonTaskParticipation.InvariantDetection", SimulationTestFlags)

bool FSimulationPersonTaskParticipationInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
		const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);
		TestTrue(TEXT("A valid pair is recorded before corrupting the person"),
			Registry.AddPersonTaskParticipation(William, ShapeBeam)
				== EPersonTaskParticipationResult::Success);
		VerifyInvariants(*this, Registry, TEXT("before corrupting a participation person"));

		FSimulationRegistryTestAccess::PersonTaskParticipationRecords(Registry)[0].PersonId = FPersonId(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a participation naming an unresolvable person"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
		const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);
		TestTrue(TEXT("A valid pair is recorded before corrupting the task"),
			Registry.AddPersonTaskParticipation(William, ShapeBeam)
				== EPersonTaskParticipationResult::Success);
		VerifyInvariants(*this, Registry, TEXT("before corrupting a participation task"));

		FSimulationRegistryTestAccess::PersonTaskParticipationRecords(Registry)[0].TaskId = FTaskId(99);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a participation naming an unresolvable task"));
	}

	{
		FSimulationRegistry Registry;
		const FPersonId William = Registry.CreatePerson(MakePersonParams(TEXT("William"), TEXT("Carter"), 34));
		const FPersonId Thomas = Registry.CreatePerson(MakePersonParams(TEXT("Thomas"), TEXT("Miller"), 22));
		const FTaskTypeId ShapeBeamType = Registry.CreateTaskType(TEXT("Task.ShapeBeam"), TEXT("Shape Beam"));
		const FTaskTypeId HarvestType =
			Registry.CreateTaskType(TEXT("Task.HarvestWheat"), TEXT("Harvest Wheat"));
		const FTaskId ShapeBeam = Registry.CreateTask(ShapeBeamType);
		const FTaskId Harvest = Registry.CreateTask(HarvestType);
		TestTrue(TEXT("Two distinct pairs are recorded before duplicating one"),
			Registry.AddPersonTaskParticipation(William, ShapeBeam)
					== EPersonTaskParticipationResult::Success
				&& Registry.AddPersonTaskParticipation(Thomas, Harvest)
					== EPersonTaskParticipationResult::Success);
		VerifyInvariants(*this, Registry, TEXT("before duplicating a participation pair"));

		FSimulationRegistryTestAccess::PersonTaskParticipationRecords(Registry)[1].PersonId = William;
		FSimulationRegistryTestAccess::PersonTaskParticipationRecords(Registry)[1].TaskId = ShapeBeam;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("duplicate person-task participation pairs"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
