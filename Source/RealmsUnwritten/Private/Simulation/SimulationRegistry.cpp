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

	/** How many times an identifier appears in a membership list. */
	template <typename TIdType>
	int32 CountOccurrences(const TArray<TIdType>& List, TIdType Id)
	{
		int32 Occurrences = 0;
		for (const TIdType Entry : List)
		{
			if (Entry == Id)
			{
				++Occurrences;
			}
		}

		return Occurrences;
	}

	/** Reports the first identifier listed more than once, if any. */
	template <typename TIdType>
	bool TryFindDuplicate(const TArray<TIdType>& List, TIdType& OutDuplicate)
	{
		TSet<TIdType> SeenIds;
		SeenIds.Reserve(List.Num());

		for (const TIdType Entry : List)
		{
			bool bAlreadySeen = false;
			SeenIds.Add(Entry, &bAlreadySeen);
			if (bAlreadySeen)
			{
				OutDuplicate = Entry;
				return true;
			}
		}

		return false;
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

FSettlementId FSimulationRegistry::CreateSettlement(const FString& Name)
{
	const int32 RecordIndex = SettlementRecords.AddDefaulted();

	FSettlementRecord& SettlementRecord = SettlementRecords[RecordIndex];
	SettlementRecord.Id = FromRecordIndex<FSettlementId>(RecordIndex);
	SettlementRecord.Name = Name;

	return SettlementRecord.Id;
}

FPropertyId FSimulationRegistry::CreateProperty(FSettlementId SettlementId)
{
	// Validated before storage is touched, so a property can never reference a settlement
	// that does not exist, and a rejected creation consumes no identifier.
	if (!ContainsSettlement(SettlementId))
	{
		return FPropertyId();
	}

	const int32 RecordIndex = PropertyRecords.AddDefaulted();

	FPropertyRecord& PropertyRecord = PropertyRecords[RecordIndex];
	PropertyRecord.Id = FromRecordIndex<FPropertyId>(RecordIndex);
	PropertyRecord.SettlementId = SettlementId;

	const FPropertyId NewPropertyId = PropertyRecord.Id;

	FSettlementRecord* SettlementRecord = ResolveSettlementMutable(SettlementId);
	checkf(SettlementRecord != nullptr, TEXT("%s resolved before the property was created but not after."),
		*SettlementId.ToString());
	SettlementRecord->Properties.Add(NewPropertyId);

	return NewPropertyId;
}

bool FSimulationRegistry::ContainsPerson(FPersonId PersonId) const
{
	return ResolvePerson(PersonId) != nullptr;
}

bool FSimulationRegistry::ContainsHousehold(FHouseholdId HouseholdId) const
{
	return ResolveHousehold(HouseholdId) != nullptr;
}

bool FSimulationRegistry::ContainsSettlement(FSettlementId SettlementId) const
{
	return ResolveSettlement(SettlementId) != nullptr;
}

bool FSimulationRegistry::ContainsProperty(FPropertyId PropertyId) const
{
	return ResolveProperty(PropertyId) != nullptr;
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

TOptional<FSettlementRecord> FSimulationRegistry::FindSettlement(FSettlementId SettlementId) const
{
	if (const FSettlementRecord* SettlementRecord = ResolveSettlement(SettlementId))
	{
		return TOptional<FSettlementRecord>(*SettlementRecord);
	}

	return TOptional<FSettlementRecord>();
}

TOptional<FPropertyRecord> FSimulationRegistry::FindProperty(FPropertyId PropertyId) const
{
	if (const FPropertyRecord* PropertyRecord = ResolveProperty(PropertyId))
	{
		return TOptional<FPropertyRecord>(*PropertyRecord);
	}

	return TOptional<FPropertyRecord>();
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

ESettlementMembershipResult FSimulationRegistry::PlaceHouseholdInSettlement(
	FHouseholdId HouseholdId, FSettlementId SettlementId)
{
	FHouseholdRecord* HouseholdRecord = ResolveHouseholdMutable(HouseholdId);
	if (HouseholdRecord == nullptr)
	{
		return ESettlementMembershipResult::UnknownHousehold;
	}

	if (!ContainsSettlement(SettlementId))
	{
		return ESettlementMembershipResult::UnknownSettlement;
	}

	if (HouseholdRecord->SettlementId == SettlementId)
	{
		return ESettlementMembershipResult::AlreadyMember;
	}

	// Relocating while housed would strand the residence in the previous settlement, so the
	// caller vacates first rather than having a property silently emptied here.
	if (HouseholdRecord->ResidenceId.IsValid())
	{
		return ESettlementMembershipResult::StillResident;
	}

	SetHouseholdSettlement(*HouseholdRecord, SettlementId);

	return ESettlementMembershipResult::Success;
}

ESettlementMembershipResult FSimulationRegistry::RemoveHouseholdFromSettlement(
	FHouseholdId HouseholdId, FSettlementId SettlementId)
{
	FHouseholdRecord* HouseholdRecord = ResolveHouseholdMutable(HouseholdId);
	if (HouseholdRecord == nullptr)
	{
		return ESettlementMembershipResult::UnknownHousehold;
	}

	if (!ContainsSettlement(SettlementId))
	{
		return ESettlementMembershipResult::UnknownSettlement;
	}

	if (HouseholdRecord->SettlementId != SettlementId)
	{
		return ESettlementMembershipResult::NotAMember;
	}

	if (HouseholdRecord->ResidenceId.IsValid())
	{
		return ESettlementMembershipResult::StillResident;
	}

	SetHouseholdSettlement(*HouseholdRecord, FSettlementId());

	return ESettlementMembershipResult::Success;
}

EResidenceResult FSimulationRegistry::AssignHouseholdResidence(FHouseholdId HouseholdId, FPropertyId PropertyId)
{
	FHouseholdRecord* HouseholdRecord = ResolveHouseholdMutable(HouseholdId);
	if (HouseholdRecord == nullptr)
	{
		return EResidenceResult::UnknownHousehold;
	}

	const FPropertyRecord* PropertyRecord = ResolveProperty(PropertyId);
	if (PropertyRecord == nullptr)
	{
		return EResidenceResult::UnknownProperty;
	}

	if (HouseholdRecord->ResidenceId == PropertyId)
	{
		return EResidenceResult::AlreadyResident;
	}

	if (!HouseholdRecord->SettlementId.IsValid())
	{
		return EResidenceResult::HouseholdNotInSettlement;
	}

	if (PropertyRecord->SettlementId != HouseholdRecord->SettlementId)
	{
		return EResidenceResult::SettlementMismatch;
	}

	if (PropertyRecord->ResidentHouseholdId.IsValid())
	{
		return EResidenceResult::PropertyOccupied;
	}

	SetHouseholdResidence(*HouseholdRecord, PropertyId);

	return EResidenceResult::Success;
}

EResidenceResult FSimulationRegistry::RemoveHouseholdResidence(FHouseholdId HouseholdId, FPropertyId PropertyId)
{
	FHouseholdRecord* HouseholdRecord = ResolveHouseholdMutable(HouseholdId);
	if (HouseholdRecord == nullptr)
	{
		return EResidenceResult::UnknownHousehold;
	}

	if (!ContainsProperty(PropertyId))
	{
		return EResidenceResult::UnknownProperty;
	}

	if (HouseholdRecord->ResidenceId != PropertyId)
	{
		return EResidenceResult::NotResident;
	}

	SetHouseholdResidence(*HouseholdRecord, FPropertyId());

	return EResidenceResult::Success;
}

bool FSimulationRegistry::ValidateInvariants(FString& OutFailureDescription) const
{
	OutFailureDescription.Reset();

	return ValidatePersonRecords(OutFailureDescription)
		&& ValidateHouseholdRecords(OutFailureDescription)
		&& ValidateSettlementRecords(OutFailureDescription)
		&& ValidatePropertyRecords(OutFailureDescription);
}

bool FSimulationRegistry::ValidatePersonRecords(FString& OutFailureDescription) const
{
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

		const int32 MembershipCount = CountOccurrences(HouseholdRecord->Members, PersonRecord.Id);
		if (MembershipCount != 1)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s claims %s but appears in its member list %d times."),
				*PersonRecord.Id.ToString(), *PersonRecord.HouseholdId.ToString(), MembershipCount);
			return false;
		}
	}

	return true;
}

bool FSimulationRegistry::ValidateHouseholdRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < HouseholdRecords.Num(); ++RecordIndex)
	{
		const FHouseholdRecord& HouseholdRecord = HouseholdRecords[RecordIndex];

		if (ToRecordIndex(HouseholdRecord.Id, HouseholdRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Household slot %d holds identifier %s."), RecordIndex, *HouseholdRecord.Id.ToString());
			return false;
		}

		FPersonId DuplicateMemberId;
		if (TryFindDuplicate(HouseholdRecord.Members, DuplicateMemberId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s lists %s more than once."),
				*HouseholdRecord.Id.ToString(), *DuplicateMemberId.ToString());
			return false;
		}

		for (const FPersonId MemberId : HouseholdRecord.Members)
		{
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

		if (HouseholdRecord.SettlementId.IsValid())
		{
			const FSettlementRecord* SettlementRecord = ResolveSettlement(HouseholdRecord.SettlementId);
			if (SettlementRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s references unresolvable %s."),
					*HouseholdRecord.Id.ToString(), *HouseholdRecord.SettlementId.ToString());
				return false;
			}

			const int32 ListedCount = CountOccurrences(SettlementRecord->Households, HouseholdRecord.Id);
			if (ListedCount != 1)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s claims %s but appears in its household list %d times."),
					*HouseholdRecord.Id.ToString(), *HouseholdRecord.SettlementId.ToString(), ListedCount);
				return false;
			}
		}

		if (!HouseholdRecord.ResidenceId.IsValid())
		{
			continue;
		}

		const FPropertyRecord* PropertyRecord = ResolveProperty(HouseholdRecord.ResidenceId);
		if (PropertyRecord == nullptr)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s resides on unresolvable %s."),
				*HouseholdRecord.Id.ToString(), *HouseholdRecord.ResidenceId.ToString());
			return false;
		}

		if (PropertyRecord->ResidentHouseholdId != HouseholdRecord.Id)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s resides on %s, but that property is occupied by %s."),
				*HouseholdRecord.Id.ToString(), *HouseholdRecord.ResidenceId.ToString(),
				*PropertyRecord->ResidentHouseholdId.ToString());
			return false;
		}

		if (PropertyRecord->SettlementId != HouseholdRecord.SettlementId)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s is in %s but resides on %s, which belongs to %s."),
				*HouseholdRecord.Id.ToString(), *HouseholdRecord.SettlementId.ToString(),
				*HouseholdRecord.ResidenceId.ToString(), *PropertyRecord->SettlementId.ToString());
			return false;
		}
	}

	return true;
}

bool FSimulationRegistry::ValidateSettlementRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < SettlementRecords.Num(); ++RecordIndex)
	{
		const FSettlementRecord& SettlementRecord = SettlementRecords[RecordIndex];

		if (ToRecordIndex(SettlementRecord.Id, SettlementRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Settlement slot %d holds identifier %s."), RecordIndex, *SettlementRecord.Id.ToString());
			return false;
		}

		FPropertyId DuplicatePropertyId;
		if (TryFindDuplicate(SettlementRecord.Properties, DuplicatePropertyId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s lists %s more than once."),
				*SettlementRecord.Id.ToString(), *DuplicatePropertyId.ToString());
			return false;
		}

		for (const FPropertyId PropertyId : SettlementRecord.Properties)
		{
			const FPropertyRecord* PropertyRecord = ResolveProperty(PropertyId);
			if (PropertyRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists unresolvable %s."),
					*SettlementRecord.Id.ToString(), *PropertyId.ToString());
				return false;
			}

			if (PropertyRecord->SettlementId != SettlementRecord.Id)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists %s, but that property belongs to %s."),
					*SettlementRecord.Id.ToString(), *PropertyId.ToString(),
					*PropertyRecord->SettlementId.ToString());
				return false;
			}
		}

		FHouseholdId DuplicateHouseholdId;
		if (TryFindDuplicate(SettlementRecord.Households, DuplicateHouseholdId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s lists %s more than once."),
				*SettlementRecord.Id.ToString(), *DuplicateHouseholdId.ToString());
			return false;
		}

		for (const FHouseholdId HouseholdId : SettlementRecord.Households)
		{
			const FHouseholdRecord* HouseholdRecord = ResolveHousehold(HouseholdId);
			if (HouseholdRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists unresolvable %s."),
					*SettlementRecord.Id.ToString(), *HouseholdId.ToString());
				return false;
			}

			if (HouseholdRecord->SettlementId != SettlementRecord.Id)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists %s, but that household claims %s."),
					*SettlementRecord.Id.ToString(), *HouseholdId.ToString(),
					*HouseholdRecord->SettlementId.ToString());
				return false;
			}
		}
	}

	return true;
}

bool FSimulationRegistry::ValidatePropertyRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < PropertyRecords.Num(); ++RecordIndex)
	{
		const FPropertyRecord& PropertyRecord = PropertyRecords[RecordIndex];

		if (ToRecordIndex(PropertyRecord.Id, PropertyRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Property slot %d holds identifier %s."), RecordIndex, *PropertyRecord.Id.ToString());
			return false;
		}

		const FSettlementRecord* SettlementRecord = ResolveSettlement(PropertyRecord.SettlementId);
		if (SettlementRecord == nullptr)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s belongs to unresolvable %s."),
				*PropertyRecord.Id.ToString(), *PropertyRecord.SettlementId.ToString());
			return false;
		}

		const int32 ListedCount = CountOccurrences(SettlementRecord->Properties, PropertyRecord.Id);
		if (ListedCount != 1)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s belongs to %s but appears in its property list %d times."),
				*PropertyRecord.Id.ToString(), *PropertyRecord.SettlementId.ToString(), ListedCount);
			return false;
		}

		if (!PropertyRecord.ResidentHouseholdId.IsValid())
		{
			continue;
		}

		const FHouseholdRecord* HouseholdRecord = ResolveHousehold(PropertyRecord.ResidentHouseholdId);
		if (HouseholdRecord == nullptr)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s is occupied by unresolvable %s."),
				*PropertyRecord.Id.ToString(), *PropertyRecord.ResidentHouseholdId.ToString());
			return false;
		}

		if (HouseholdRecord->ResidenceId != PropertyRecord.Id)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s is occupied by %s, but that household resides on %s."),
				*PropertyRecord.Id.ToString(), *PropertyRecord.ResidentHouseholdId.ToString(),
				*HouseholdRecord->ResidenceId.ToString());
			return false;
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

const FSettlementRecord* FSimulationRegistry::ResolveSettlement(FSettlementId SettlementId) const
{
	const int32 RecordIndex = ToRecordIndex(SettlementId, SettlementRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &SettlementRecords[RecordIndex];
}

const FPropertyRecord* FSimulationRegistry::ResolveProperty(FPropertyId PropertyId) const
{
	const int32 RecordIndex = ToRecordIndex(PropertyId, PropertyRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &PropertyRecords[RecordIndex];
}

FPersonRecord* FSimulationRegistry::ResolvePersonMutable(FPersonId PersonId)
{
	return const_cast<FPersonRecord*>(ResolvePerson(PersonId));
}

FHouseholdRecord* FSimulationRegistry::ResolveHouseholdMutable(FHouseholdId HouseholdId)
{
	return const_cast<FHouseholdRecord*>(ResolveHousehold(HouseholdId));
}

FSettlementRecord* FSimulationRegistry::ResolveSettlementMutable(FSettlementId SettlementId)
{
	return const_cast<FSettlementRecord*>(ResolveSettlement(SettlementId));
}

FPropertyRecord* FSimulationRegistry::ResolvePropertyMutable(FPropertyId PropertyId)
{
	return const_cast<FPropertyRecord*>(ResolveProperty(PropertyId));
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

void FSimulationRegistry::SetHouseholdSettlement(FHouseholdRecord& HouseholdRecord, FSettlementId NewSettlementId)
{
	if (HouseholdRecord.SettlementId == NewSettlementId)
	{
		return;
	}

	if (FSettlementRecord* PreviousSettlement = ResolveSettlementMutable(HouseholdRecord.SettlementId))
	{
		PreviousSettlement->Households.RemoveSingle(HouseholdRecord.Id);
	}

	HouseholdRecord.SettlementId = NewSettlementId;

	if (FSettlementRecord* NewSettlement = ResolveSettlementMutable(NewSettlementId))
	{
		checkf(!NewSettlement->Households.Contains(HouseholdRecord.Id),
			TEXT("%s was already listed by %s before being placed in it."),
			*HouseholdRecord.Id.ToString(), *NewSettlementId.ToString());

		NewSettlement->Households.Add(HouseholdRecord.Id);
	}
}

void FSimulationRegistry::SetHouseholdResidence(FHouseholdRecord& HouseholdRecord, FPropertyId NewPropertyId)
{
	if (HouseholdRecord.ResidenceId == NewPropertyId)
	{
		return;
	}

	if (FPropertyRecord* PreviousProperty = ResolvePropertyMutable(HouseholdRecord.ResidenceId))
	{
		PreviousProperty->ResidentHouseholdId = FHouseholdId();
	}

	HouseholdRecord.ResidenceId = NewPropertyId;

	if (FPropertyRecord* NewProperty = ResolvePropertyMutable(NewPropertyId))
	{
		checkf(!NewProperty->ResidentHouseholdId.IsValid(),
			TEXT("%s was already occupied by %s before %s moved in."),
			*NewPropertyId.ToString(), *NewProperty->ResidentHouseholdId.ToString(),
			*HouseholdRecord.Id.ToString());

		NewProperty->ResidentHouseholdId = HouseholdRecord.Id;
	}
}
