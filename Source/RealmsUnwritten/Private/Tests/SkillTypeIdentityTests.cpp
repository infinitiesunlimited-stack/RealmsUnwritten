#include "Misc/AutomationTest.h"
#include "Simulation/SimulationRegistry.h"
#include "Tests/SimulationRegistryTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationSkillTypeCreationTest,
	"RealmsUnwritten.Simulation.SkillType.Creation", SimulationTestFlags)

bool FSimulationSkillTypeCreationTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;

	TestFalse(TEXT("A skill type cannot be created without an authored key"),
		Registry.CreateSkillType(NAME_None, TEXT("Nameless")).IsValid());
	TestEqual(TEXT("A rejected nameless definition stores no skill type"), Registry.GetSkillTypeCount(), 0);

	const FSkillTypeId Masonry = Registry.CreateSkillType(TEXT("Skill.StoneMasonry"), TEXT("Stone Masonry"));
	TestTrue(TEXT("A valid authored key creates a skill type"), Masonry.IsValid());
	TestEqual(TEXT("The rejected nameless definition consumed no runtime handle"), Masonry.GetValue(), 1u);

	const TOptional<FSkillTypeRecord> MasonryRecord = Registry.FindSkillType(Masonry);
	if (!MasonryRecord.IsSet())
	{
		AddError(TEXT("The created skill type should resolve by identifier"));
		return false;
	}

	TestTrue(TEXT("The record preserves its typed identifier"), MasonryRecord->Id == Masonry);
	TestTrue(TEXT("The record preserves its authored key"),
		MasonryRecord->AuthoredKey == FName(TEXT("Skill.StoneMasonry")));
	TestEqual(TEXT("The record preserves its display name"),
		MasonryRecord->Name, FString(TEXT("Stone Masonry")));

	const TOptional<FSkillTypeId> MasonryByKey =
		Registry.FindSkillTypeIdByKey(TEXT("Skill.StoneMasonry"));
	TestTrue(TEXT("A known authored key resolves to its typed identifier"),
		MasonryByKey.IsSet() && MasonryByKey.GetValue() == Masonry);
	TestFalse(TEXT("An unknown authored key does not resolve"),
		Registry.FindSkillTypeIdByKey(TEXT("Skill.Unknown")).IsSet());
	TestFalse(TEXT("NAME_None does not resolve as an authored key"),
		Registry.FindSkillTypeIdByKey(NAME_None).IsSet());

	const int32 CountBeforeDuplicate = Registry.GetSkillTypeCount();
	const FSkillTypeId Duplicate =
		Registry.CreateSkillType(TEXT("Skill.StoneMasonry"), TEXT("Altered Masonry"));
	TestFalse(TEXT("A duplicate authored key is rejected"), Duplicate.IsValid());
	TestEqual(TEXT("A duplicate authored key stores no additional record"),
		Registry.GetSkillTypeCount(), CountBeforeDuplicate);

	const TOptional<FSkillTypeRecord> OriginalAfterDuplicate = Registry.FindSkillType(Masonry);
	TestTrue(TEXT("Duplicate rejection leaves the original authoritative record unchanged"),
		OriginalAfterDuplicate.IsSet()
			&& OriginalAfterDuplicate->AuthoredKey == FName(TEXT("Skill.StoneMasonry"))
			&& OriginalAfterDuplicate->Name == TEXT("Stone Masonry"));

	const FSkillTypeId Carpentry = Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
	TestEqual(TEXT("A rejected duplicate consumed no runtime handle"),
		Carpentry.GetValue(), static_cast<uint32>(CountBeforeDuplicate + 1));
	const FSkillTypeId OtherMasonry =
		Registry.CreateSkillType(TEXT("Skill.OtherMasonry"), TEXT("Stone Masonry"));
	TestTrue(TEXT("A duplicate display name under a distinct key is allowed"), OtherMasonry.IsValid());

	const uint32 UnresolvableValues[] = {
		0u, OtherMasonry.GetValue() + 1u, Int32MaxIdValue, SignBitIdValue, UInt32MaxIdValue };
	for (const uint32 UnresolvableValue : UnresolvableValues)
	{
		const FSkillTypeId UnknownId(UnresolvableValue);
		TestFalse(FString::Printf(TEXT("Unknown skill type %u is not contained"), UnresolvableValue),
			Registry.ContainsSkillType(UnknownId));
		TestFalse(FString::Printf(TEXT("Unknown skill type %u does not resolve"), UnresolvableValue),
			Registry.FindSkillType(UnknownId).IsSet());
	}

	VerifyInvariants(*this, Registry, TEXT("after skill-type creation and rejection"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationSkillTypeNamespaceIndependenceTest,
	"RealmsUnwritten.Simulation.SkillType.NamespaceIndependence", SimulationTestFlags)

bool FSimulationSkillTypeNamespaceIndependenceTest::RunTest(const FString& Parameters)
{
	static_assert(!SimulationIdContract::bInterchangeable<FSkillTypeId, FGoodTypeId>,
		"Skill-type and good-type identifiers must not be interchangeable.");
	static_assert(!SimulationIdContract::bInterchangeable<FSkillTypeId, FWorkTypeId>,
		"Skill-type and work-type identifiers must not be interchangeable.");

	FSimulationRegistry Registry;
	const FName SharedKey(TEXT("Wheat"));
	const FGoodTypeId GoodTypeId = Registry.CreateGoodType(SharedKey, TEXT("Wheat Good"));
	const FWorkTypeId WorkTypeId = Registry.CreateWorkType(SharedKey, TEXT("Wheat Work"));
	const FSkillTypeId SkillTypeId = Registry.CreateSkillType(SharedKey, TEXT("Wheat Skill"));

	TestTrue(TEXT("The Good Type accepts the exact shared key Wheat"), GoodTypeId.IsValid());
	TestTrue(TEXT("The Work Type accepts the exact shared key Wheat"), WorkTypeId.IsValid());
	TestTrue(TEXT("The Skill Type accepts the exact shared key Wheat"), SkillTypeId.IsValid());
	TestEqual(TEXT("The Good Type registry holds exactly its registration"), Registry.GetGoodTypeCount(), 1);
	TestEqual(TEXT("The Work Type registry holds exactly its registration"), Registry.GetWorkTypeCount(), 1);
	TestEqual(TEXT("The Skill Type registry holds exactly its registration"), Registry.GetSkillTypeCount(), 1);

	const TOptional<FGoodTypeId> ResolvedGood = Registry.FindGoodTypeIdByKey(SharedKey);
	const TOptional<FWorkTypeId> ResolvedWork = Registry.FindWorkTypeIdByKey(SharedKey);
	const TOptional<FSkillTypeId> ResolvedSkill = Registry.FindSkillTypeIdByKey(SharedKey);
	TestTrue(TEXT("The Good Type registry independently resolves Wheat"),
		ResolvedGood.IsSet() && ResolvedGood.GetValue() == GoodTypeId);
	TestTrue(TEXT("The Work Type registry independently resolves Wheat"),
		ResolvedWork.IsSet() && ResolvedWork.GetValue() == WorkTypeId);
	TestTrue(TEXT("The Skill Type registry independently resolves Wheat"),
		ResolvedSkill.IsSet() && ResolvedSkill.GetValue() == SkillTypeId);

	VerifyInvariants(*this, Registry, TEXT("with one identical key in three definition namespaces"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationSkillTypeSnapshotRegressionTest,
	"RealmsUnwritten.Simulation.SkillType.SnapshotRegression", SimulationTestFlags)

bool FSimulationSkillTypeSnapshotRegressionTest::RunTest(const FString& Parameters)
{
	FSimulationRegistry Registry;
	const FSkillTypeId Masonry = Registry.CreateSkillType(TEXT("Skill.StoneMasonry"), TEXT("Stone Masonry"));
	TOptional<FSkillTypeRecord> Snapshot = Registry.FindSkillType(Masonry);
	if (!Snapshot.IsSet())
	{
		AddError(TEXT("The skill type snapshot should resolve"));
		return false;
	}

	for (int32 FillerIndex = 0; FillerIndex < 16; ++FillerIndex)
	{
		Registry.CreateSkillType(
			FName(*FString::Printf(TEXT("Skill.Filler%d"), FillerIndex)), TEXT("Filler"));
	}

	Snapshot->Id = FSkillTypeId(UInt32MaxIdValue);
	Snapshot->AuthoredKey = FName(TEXT("Skill.Tampered"));
	Snapshot->Name = TEXT("Tampered");

	const TOptional<FSkillTypeRecord> AuthoritativeRecord = Registry.FindSkillType(Masonry);
	if (!AuthoritativeRecord.IsSet())
	{
		AddError(TEXT("The authoritative skill type should still resolve after snapshot mutation"));
		return false;
	}

	TestTrue(TEXT("Snapshot mutation does not change the authoritative identifier"),
		AuthoritativeRecord->Id == Masonry);
	TestTrue(TEXT("Snapshot mutation does not change the authoritative key"),
		AuthoritativeRecord->AuthoredKey == FName(TEXT("Skill.StoneMasonry")));
	TestEqual(TEXT("Snapshot mutation does not change the authoritative display name"),
		AuthoritativeRecord->Name, FString(TEXT("Stone Masonry")));
	TestFalse(TEXT("The tampered snapshot key was not registered"),
		Registry.FindSkillTypeIdByKey(TEXT("Skill.Tampered")).IsSet());

	VerifyInvariants(*this, Registry, TEXT("after mutating a detached skill-type snapshot"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationSkillTypeInvariantDetectionTest,
	"RealmsUnwritten.Simulation.SkillType.InvariantDetection", SimulationTestFlags)

bool FSimulationSkillTypeInvariantDetectionTest::RunTest(const FString& Parameters)
{
	{
		FSimulationRegistry Registry;
		Registry.CreateSkillType(TEXT("Skill.StoneMasonry"), TEXT("Stone Masonry"));
		FSimulationRegistryTestAccess::SkillTypeRecords(Registry)[0].Id = FSkillTypeId(7);
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a skill-type slot and identifier mismatch"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateSkillType(TEXT("Skill.StoneMasonry"), TEXT("Stone Masonry"));
		FSimulationRegistryTestAccess::SkillTypeRecords(Registry)[0].AuthoredKey = NAME_None;
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("a skill type with no authored key"));
	}

	{
		FSimulationRegistry Registry;
		Registry.CreateSkillType(TEXT("Skill.StoneMasonry"), TEXT("Stone Masonry"));
		Registry.CreateSkillType(TEXT("Skill.Carpentry"), TEXT("Carpentry"));
		FSimulationRegistryTestAccess::SkillTypeRecords(Registry)[1].AuthoredKey =
			FName(TEXT("Skill.StoneMasonry"));
		VerifyInvariantsDetectFailure(*this, Registry, TEXT("duplicate skill-type authored keys"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
