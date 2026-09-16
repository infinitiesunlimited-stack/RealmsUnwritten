#include "Simulation/SimulationRegistry.h"

namespace
{
	/**
	 * Dense storage slot for an identifier, or INDEX_NONE when the identifier is invalid or
	 * was never allocated.
	 *
	 * The unsigned value is range-checked before any narrowing, so every value a caller can
	 * construct - including 0x80000000 and 0xFFFFFFFF, which do not survive a signed cast -
	 * resolves to INDEX_NONE rather than an overflowed or out-of-range index.
	 */
	template <typename TIdType>
	int32 ToRecordIndex(TIdType Id, int32 RecordCount)
	{
		using ValueType = typename TIdType::ValueType;

		const ValueType Value = Id.GetValue();
		if (Value == TIdType::InvalidValue || Value > static_cast<ValueType>(RecordCount))
		{
			return INDEX_NONE;
		}

		return static_cast<int32>(Value - 1);
	}

	/** Identifier for a newly appended record in the given slot. */
	template <typename TIdType>
	TIdType FromRecordIndex(int32 RecordIndex)
	{
		return TIdType(static_cast<typename TIdType::ValueType>(RecordIndex) + 1);
	}
}

FPersonId FSimulationRegistry::CreatePerson(const FPersonCreationParams& Params)
{
	// Validated before storage is touched, so a rejected creation cannot leave a partial
	// record behind or consume an identifier.
	if (Params.AgeYears < 0)
	{
		return FPersonId();
	}

	const int32 RecordIndex = PersonRecords.AddDefaulted();

	FPersonRecord& PersonRecord = PersonRecords[RecordIndex];
	PersonRecord.Id = FromRecordIndex<FPersonId>(RecordIndex);
	PersonRecord.GivenName = Params.GivenName;
	PersonRecord.FamilyName = Params.FamilyName;
	PersonRecord.AgeYears = Params.AgeYears;
	PersonRecord.LifeState = EPersonLifeState::Alive;

	return PersonRecord.Id;
}

FHouseholdId FSimulationRegistry::CreateHousehold(const FString& Name)
{
	const int32 RecordIndex = HouseholdRecords.AddDefaulted();

	FHouseholdRecord& HouseholdRecord = HouseholdRecords[RecordIndex];
	HouseholdRecord.Id = FromRecordIndex<FHouseholdId>(RecordIndex);
	HouseholdRecord.Name = Name;

	return HouseholdRecord.Id;
}

bool FSimulationRegistry::ContainsPerson(FPersonId PersonId) const
{
	return ResolvePerson(PersonId) != nullptr;
}

bool FSimulationRegistry::ContainsHousehold(FHouseholdId HouseholdId) const
{
	return ResolveHousehold(HouseholdId) != nullptr;
}

TOptional<FPersonRecord> FSimulationRegistry::FindPerson(FPersonId PersonId) const
{
	if (const FPersonRecord* PersonRecord = ResolvePerson(PersonId))
	{
		return TOptional<FPersonRecord>(*PersonRecord);
	}

	return TOptional<FPersonRecord>();
}

TOptional<FHouseholdRecord> FSimulationRegistry::FindHousehold(FHouseholdId HouseholdId) const
{
	if (const FHouseholdRecord* HouseholdRecord = ResolveHousehold(HouseholdId))
	{
		return TOptional<FHouseholdRecord>(*HouseholdRecord);
	}

	return TOptional<FHouseholdRecord>();
}

EHouseholdMembershipResult FSimulationRegistry::AddPersonToHousehold(FPersonId PersonId, FHouseholdId HouseholdId)
{
	FPersonRecord* PersonRecord = ResolvePersonMutable(PersonId);
	if (PersonRecord == nullptr)
	{
		return EHouseholdMembershipResult::UnknownPerson;
	}

	if (!ContainsHousehold(HouseholdId))
	{
		return EHouseholdMembershipResult::UnknownHousehold;
	}

	if (PersonRecord->HouseholdId == HouseholdId)
	{
		return EHouseholdMembershipResult::AlreadyMember;
	}

	SetPersonHousehold(*PersonRecord, HouseholdId);

	return EHouseholdMembershipResult::Success;
}

EHouseholdMembershipResult FSimulationRegistry::RemovePersonFromHousehold(FPersonId PersonId, FHouseholdId HouseholdId)
{
	FPersonRecord* PersonRecord = ResolvePersonMutable(PersonId);
	if (PersonRecord == nullptr)
	{
		return EHouseholdMembershipResult::UnknownPerson;
	}

	if (!ContainsHousehold(HouseholdId))
	{
		return EHouseholdMembershipResult::UnknownHousehold;
	}

	if (PersonRecord->HouseholdId != HouseholdId)
	{
		return EHouseholdMembershipResult::NotAMember;
	}

	SetPersonHousehold(*PersonRecord, FHouseholdId());

	return EHouseholdMembershipResult::Success;
}

bool FSimulationRegistry::ValidateInvariants(FString& OutFailureDescription) const
{
	OutFailureDescription.Reset();

	for (int32 RecordIndex = 0; RecordIndex < PersonRecords.Num(); ++RecordIndex)
	{
		const FPersonRecord& PersonRecord = PersonRecords[RecordIndex];

		if (ToRecordIndex(PersonRecord.Id, PersonRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Person slot %d holds identifier %s."), RecordIndex, *PersonRecord.Id.ToString());
			return false;
		}

		if (PersonRecord.AgeYears < 0)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s stores a negative age of %d years."),
				*PersonRecord.Id.ToString(), PersonRecord.AgeYears);
			return false;
		}

		if (!PersonRecord.HouseholdId.IsValid())
		{
			continue;
		}

		const FHouseholdRecord* HouseholdRecord = ResolveHousehold(PersonRecord.HouseholdId);
		if (HouseholdRecord == nullptr)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s references unresolvable %s."),
				*PersonRecord.Id.ToString(), *PersonRecord.HouseholdId.ToString());
			return false;
		}

		int32 MembershipCount = 0;
		for (const FPersonId MemberId : HouseholdRecord->Members)
		{
			if (MemberId == PersonRecord.Id)
			{
				++MembershipCount;
			}
		}

		if (MembershipCount != 1)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s claims %s but appears in its member list %d times."),
				*PersonRecord.Id.ToString(), *PersonRecord.HouseholdId.ToString(), MembershipCount);
			return false;
		}
	}

	for (int32 RecordIndex = 0; RecordIndex < HouseholdRecords.Num(); ++RecordIndex)
	{
		const FHouseholdRecord& HouseholdRecord = HouseholdRecords[RecordIndex];

		if (ToRecordIndex(HouseholdRecord.Id, HouseholdRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Household slot %d holds identifier %s."), RecordIndex, *HouseholdRecord.Id.ToString());
			return false;
		}

		TSet<FPersonId> SeenMemberIds;
		SeenMemberIds.Reserve(HouseholdRecord.Members.Num());

		for (const FPersonId MemberId : HouseholdRecord.Members)
		{
			bool bAlreadySeen = false;
			SeenMemberIds.Add(MemberId, &bAlreadySeen);
			if (bAlreadySeen)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists %s more than once."),
					*HouseholdRecord.Id.ToString(), *MemberId.ToString());
				return false;
			}

			const FPersonRecord* MemberRecord = ResolvePerson(MemberId);
			if (MemberRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists unresolvable %s."),
					*HouseholdRecord.Id.ToString(), *MemberId.ToString());
				return false;
			}

			if (MemberRecord->HouseholdId != HouseholdRecord.Id)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists %s, but that person claims %s."),
					*HouseholdRecord.Id.ToString(), *MemberId.ToString(),
					*MemberRecord->HouseholdId.ToString());
				return false;
			}
		}
	}

	return true;
}

const FPersonRecord* FSimulationRegistry::ResolvePerson(FPersonId PersonId) const
{
	const int32 RecordIndex = ToRecordIndex(PersonId, PersonRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &PersonRecords[RecordIndex];
}

const FHouseholdRecord* FSimulationRegistry::ResolveHousehold(FHouseholdId HouseholdId) const
{
	const int32 RecordIndex = ToRecordIndex(HouseholdId, HouseholdRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &HouseholdRecords[RecordIndex];
}

FPersonRecord* FSimulationRegistry::ResolvePersonMutable(FPersonId PersonId)
{
	return const_cast<FPersonRecord*>(ResolvePerson(PersonId));
}

FHouseholdRecord* FSimulationRegistry::ResolveHouseholdMutable(FHouseholdId HouseholdId)
{
	return const_cast<FHouseholdRecord*>(ResolveHousehold(HouseholdId));
}

void FSimulationRegistry::SetPersonHousehold(FPersonRecord& PersonRecord, FHouseholdId NewHouseholdId)
{
	if (PersonRecord.HouseholdId == NewHouseholdId)
	{
		return;
	}

	if (FHouseholdRecord* PreviousHousehold = ResolveHouseholdMutable(PersonRecord.HouseholdId))
	{
		PreviousHousehold->Members.RemoveSingle(PersonRecord.Id);
	}

	PersonRecord.HouseholdId = NewHouseholdId;

	if (FHouseholdRecord* NewHousehold = ResolveHouseholdMutable(NewHouseholdId))
	{
		checkf(!NewHousehold->Members.Contains(PersonRecord.Id),
			TEXT("%s was already listed by %s before joining it."),
			*PersonRecord.Id.ToString(), *NewHouseholdId.ToString());

		NewHousehold->Members.Add(PersonRecord.Id);
	}
}
