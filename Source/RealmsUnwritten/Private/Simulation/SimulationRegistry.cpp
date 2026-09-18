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

	/** Slot of a good type's entry within one inventory, or INDEX_NONE when it holds none. */
	int32 FindEntryIndex(const TArray<FInventoryEntry>& Entries, FGoodTypeId GoodTypeId)
	{
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			if (Entries[EntryIndex].GoodTypeId == GoodTypeId)
			{
				return EntryIndex;
			}
		}

		return INDEX_NONE;
	}

	/** Slot of a recorded (Person, SkillType) pair, or INDEX_NONE when none is recorded. */
	int32 FindPersonCapabilityIndex(
		const TArray<FPersonCapabilityRecord>& Records, FPersonId PersonId, FSkillTypeId SkillTypeId)
	{
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			const FPersonCapabilityRecord& Record = Records[RecordIndex];
			if (Record.PersonId == PersonId && Record.SkillTypeId == SkillTypeId)
			{
				return RecordIndex;
			}
		}

		return INDEX_NONE;
	}

	/** Quantity of a good type held, or zero when the inventory holds none of it. */
	int32 GetEntryQuantity(const TArray<FInventoryEntry>& Entries, FGoodTypeId GoodTypeId)
	{
		const int32 EntryIndex = FindEntryIndex(Entries, GoodTypeId);
		return EntryIndex == INDEX_NONE ? 0 : Entries[EntryIndex].Quantity;
	}

	/**
	 * Whether adding Quantity to ExistingQuantity would exceed the representable maximum.
	 *
	 * The comparison is rearranged so the sum is never formed unless it is known to fit;
	 * signed overflow is undefined behaviour and cannot be detected after the fact.
	 */
	bool WouldExceedMaxQuantity(int32 ExistingQuantity, int32 Quantity)
	{
		return Quantity > MaxGoodQuantity - ExistingQuantity;
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

FPhysicalSiteId FSimulationRegistry::CreatePhysicalSite(
	FPropertyId PropertyId, FName PurposeKey, const FString& DisplayName)
{
	// Validated before storage is touched, so a site can never reference a property that does
	// not exist, and a rejected creation consumes no identifier. The display name is not
	// validated, as nowhere else in the registry validates display data.
	if (!ContainsProperty(PropertyId))
	{
		return FPhysicalSiteId();
	}

	if (PurposeKey.IsNone())
	{
		return FPhysicalSiteId();
	}

	const int32 RecordIndex = PhysicalSiteRecords.AddDefaulted();

	FPhysicalSiteRecord& PhysicalSiteRecord = PhysicalSiteRecords[RecordIndex];
	PhysicalSiteRecord.Id = FromRecordIndex<FPhysicalSiteId>(RecordIndex);
	PhysicalSiteRecord.PropertyId = PropertyId;
	PhysicalSiteRecord.PurposeKey = PurposeKey;
	PhysicalSiteRecord.DisplayName = DisplayName;

	const FPhysicalSiteId NewPhysicalSiteId = PhysicalSiteRecord.Id;

	FPropertyRecord* PropertyRecord = ResolvePropertyMutable(PropertyId);
	checkf(PropertyRecord != nullptr, TEXT("%s resolved before the site was created but not after."),
		*PropertyId.ToString());
	PropertyRecord->PhysicalSites.Add(NewPhysicalSiteId);

	return NewPhysicalSiteId;
}

FGoodTypeId FSimulationRegistry::CreateGoodType(FName AuthoredKey, const FString& DisplayName)
{
	// Validated before anything is stored, so a rejected definition consumes no runtime
	// handle and leaves the registry exactly as it was.
	if (AuthoredKey.IsNone())
	{
		return FGoodTypeId();
	}

	if (FindGoodTypeIdByKey(AuthoredKey).IsSet())
	{
		return FGoodTypeId();
	}

	const int32 RecordIndex = GoodTypeRecords.AddDefaulted();

	FGoodTypeRecord& GoodTypeRecord = GoodTypeRecords[RecordIndex];
	GoodTypeRecord.AuthoredKey = AuthoredKey;
	GoodTypeRecord.Id = FromRecordIndex<FGoodTypeId>(RecordIndex);
	GoodTypeRecord.Name = DisplayName;

	return GoodTypeRecord.Id;
}

FWorkTypeId FSimulationRegistry::CreateWorkType(FName AuthoredKey, const FString& DisplayName)
{
	// Validated before anything is stored, so a rejected definition consumes no runtime
	// handle and leaves the registry exactly as it was. Work-type keys are a separate
	// namespace from good-type keys, so FindWorkTypeIdByKey is the uniqueness check.
	if (AuthoredKey.IsNone())
	{
		return FWorkTypeId();
	}

	if (FindWorkTypeIdByKey(AuthoredKey).IsSet())
	{
		return FWorkTypeId();
	}

	const int32 RecordIndex = WorkTypeRecords.AddDefaulted();

	FWorkTypeRecord& WorkTypeRecord = WorkTypeRecords[RecordIndex];
	WorkTypeRecord.AuthoredKey = AuthoredKey;
	WorkTypeRecord.Id = FromRecordIndex<FWorkTypeId>(RecordIndex);
	WorkTypeRecord.Name = DisplayName;

	return WorkTypeRecord.Id;
}

FSkillTypeId FSimulationRegistry::CreateSkillType(FName AuthoredKey, const FString& DisplayName)
{
	// Validation precedes storage so rejection consumes no runtime handle. Only the skill-type
	// namespace participates in uniqueness; other authored definition families are independent.
	if (AuthoredKey.IsNone())
	{
		return FSkillTypeId();
	}

	if (FindSkillTypeIdByKey(AuthoredKey).IsSet())
	{
		return FSkillTypeId();
	}

	const int32 RecordIndex = SkillTypeRecords.AddDefaulted();

	FSkillTypeRecord& SkillTypeRecord = SkillTypeRecords[RecordIndex];
	SkillTypeRecord.AuthoredKey = AuthoredKey;
	SkillTypeRecord.Id = FromRecordIndex<FSkillTypeId>(RecordIndex);
	SkillTypeRecord.Name = DisplayName;

	return SkillTypeRecord.Id;
}

FInventoryId FSimulationRegistry::CreateInventory()
{
	const int32 RecordIndex = InventoryRecords.AddDefaulted();

	FInventoryRecord& InventoryRecord = InventoryRecords[RecordIndex];
	InventoryRecord.Id = FromRecordIndex<FInventoryId>(RecordIndex);
	InventoryRecord.Location = FInventoryLocation::Nowhere();

	return InventoryRecord.Id;
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

bool FSimulationRegistry::ContainsPhysicalSite(FPhysicalSiteId PhysicalSiteId) const
{
	return ResolvePhysicalSite(PhysicalSiteId) != nullptr;
}

bool FSimulationRegistry::ContainsGoodType(FGoodTypeId GoodTypeId) const
{
	return ResolveGoodType(GoodTypeId) != nullptr;
}

bool FSimulationRegistry::ContainsWorkType(FWorkTypeId WorkTypeId) const
{
	return ResolveWorkType(WorkTypeId) != nullptr;
}

bool FSimulationRegistry::ContainsSkillType(FSkillTypeId SkillTypeId) const
{
	return ResolveSkillType(SkillTypeId) != nullptr;
}

bool FSimulationRegistry::ContainsInventory(FInventoryId InventoryId) const
{
	return ResolveInventory(InventoryId) != nullptr;
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

TOptional<FPhysicalSiteRecord> FSimulationRegistry::FindPhysicalSite(FPhysicalSiteId PhysicalSiteId) const
{
	if (const FPhysicalSiteRecord* PhysicalSiteRecord = ResolvePhysicalSite(PhysicalSiteId))
	{
		return TOptional<FPhysicalSiteRecord>(*PhysicalSiteRecord);
	}

	return TOptional<FPhysicalSiteRecord>();
}

TOptional<FGoodTypeRecord> FSimulationRegistry::FindGoodType(FGoodTypeId GoodTypeId) const
{
	if (const FGoodTypeRecord* GoodTypeRecord = ResolveGoodType(GoodTypeId))
	{
		return TOptional<FGoodTypeRecord>(*GoodTypeRecord);
	}

	return TOptional<FGoodTypeRecord>();
}

TOptional<FGoodTypeId> FSimulationRegistry::FindGoodTypeIdByKey(FName AuthoredKey) const
{
	if (AuthoredKey.IsNone())
	{
		return TOptional<FGoodTypeId>();
	}

	// A scan of the good type records, which are few and which already hold the authored
	// keys. A key-to-handle index would be a second copy of this relationship to keep
	// consistent, and is not worth it until measurement says otherwise.
	for (const FGoodTypeRecord& GoodTypeRecord : GoodTypeRecords)
	{
		if (GoodTypeRecord.AuthoredKey == AuthoredKey)
		{
			return TOptional<FGoodTypeId>(GoodTypeRecord.Id);
		}
	}

	return TOptional<FGoodTypeId>();
}

TOptional<FWorkTypeRecord> FSimulationRegistry::FindWorkType(FWorkTypeId WorkTypeId) const
{
	if (const FWorkTypeRecord* WorkTypeRecord = ResolveWorkType(WorkTypeId))
	{
		return TOptional<FWorkTypeRecord>(*WorkTypeRecord);
	}

	return TOptional<FWorkTypeRecord>();
}

TOptional<FWorkTypeId> FSimulationRegistry::FindWorkTypeIdByKey(FName AuthoredKey) const
{
	if (AuthoredKey.IsNone())
	{
		return TOptional<FWorkTypeId>();
	}

	for (const FWorkTypeRecord& WorkTypeRecord : WorkTypeRecords)
	{
		if (WorkTypeRecord.AuthoredKey == AuthoredKey)
		{
			return TOptional<FWorkTypeId>(WorkTypeRecord.Id);
		}
	}

	return TOptional<FWorkTypeId>();
}

TOptional<FSkillTypeRecord> FSimulationRegistry::FindSkillType(FSkillTypeId SkillTypeId) const
{
	if (const FSkillTypeRecord* SkillTypeRecord = ResolveSkillType(SkillTypeId))
	{
		return TOptional<FSkillTypeRecord>(*SkillTypeRecord);
	}

	return TOptional<FSkillTypeRecord>();
}

TOptional<FSkillTypeId> FSimulationRegistry::FindSkillTypeIdByKey(FName AuthoredKey) const
{
	if (AuthoredKey.IsNone())
	{
		return TOptional<FSkillTypeId>();
	}

	for (const FSkillTypeRecord& SkillTypeRecord : SkillTypeRecords)
	{
		if (SkillTypeRecord.AuthoredKey == AuthoredKey)
		{
			return TOptional<FSkillTypeId>(SkillTypeRecord.Id);
		}
	}

	return TOptional<FSkillTypeId>();
}

TOptional<FInventoryRecord> FSimulationRegistry::FindInventory(FInventoryId InventoryId) const
{
	if (const FInventoryRecord* InventoryRecord = ResolveInventory(InventoryId))
	{
		return TOptional<FInventoryRecord>(*InventoryRecord);
	}

	return TOptional<FInventoryRecord>();
}

TOptional<int32> FSimulationRegistry::GetQuantity(FInventoryId InventoryId, FGoodTypeId GoodTypeId) const
{
	const FInventoryRecord* InventoryRecord = ResolveInventory(InventoryId);
	if (InventoryRecord == nullptr || !ContainsGoodType(GoodTypeId))
	{
		return TOptional<int32>();
	}

	return TOptional<int32>(GetEntryQuantity(InventoryRecord->Entries, GoodTypeId));
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

EInventoryLocationResult FSimulationRegistry::AssignInventoryToSite(
	FInventoryId InventoryId, FPhysicalSiteId PhysicalSiteId)
{
	FInventoryRecord* InventoryRecord = ResolveInventoryMutable(InventoryId);
	if (InventoryRecord == nullptr)
	{
		return EInventoryLocationResult::UnknownInventory;
	}

	if (!ContainsPhysicalSite(PhysicalSiteId))
	{
		return EInventoryLocationResult::UnknownSite;
	}

	const FInventoryLocation& CurrentLocation = InventoryRecord->Location;
	if (CurrentLocation.Kind == EInventoryLocationKind::PhysicalSite
		&& CurrentLocation.PhysicalSiteId == PhysicalSiteId)
	{
		return EInventoryLocationResult::AlreadyAtSite;
	}

	// Whatever the inventory holds comes with it: the goods were in the inventory, and the
	// inventory is now somewhere else. Nothing is created, destroyed, or left behind, so a
	// populated inventory relocates as freely as an empty one.
	SetInventoryLocation(*InventoryRecord, FInventoryLocation::AtPhysicalSite(PhysicalSiteId));

	return EInventoryLocationResult::Success;
}

EInventoryLocationResult FSimulationRegistry::RemoveInventoryLocation(FInventoryId InventoryId)
{
	FInventoryRecord* InventoryRecord = ResolveInventoryMutable(InventoryId);
	if (InventoryRecord == nullptr)
	{
		return EInventoryLocationResult::UnknownInventory;
	}

	if (!InventoryRecord->Location.IsLocated())
	{
		return EInventoryLocationResult::NotLocated;
	}

	// Goods may not be left without a place, so the only way to move a populated inventory is
	// to another site. Emptying it first is the caller's other option.
	if (InventoryRecord->Entries.Num() > 0)
	{
		return EInventoryLocationResult::InventoryNotEmpty;
	}

	SetInventoryLocation(*InventoryRecord, FInventoryLocation::Nowhere());

	return EInventoryLocationResult::Success;
}

EPersonWorkResult FSimulationRegistry::AssignPersonWork(
	FPersonId PersonId, FWorkTypeId WorkTypeId, FPhysicalSiteId PhysicalSiteId)
{
	// Every identifier is resolved before anything is written, so a rejected assignment
	// cannot replace a valid commitment with a dangling one.
	FPersonRecord* PersonRecord = ResolvePersonMutable(PersonId);
	if (PersonRecord == nullptr)
	{
		return EPersonWorkResult::UnknownPerson;
	}

	if (!ContainsWorkType(WorkTypeId))
	{
		return EPersonWorkResult::UnknownWorkType;
	}

	if (!ContainsPhysicalSite(PhysicalSiteId))
	{
		return EPersonWorkResult::UnknownSite;
	}

	const FCurrentWork& CurrentWork = PersonRecord->CurrentWork;
	if (CurrentWork.Kind == ECurrentWorkKind::PhysicalSite
		&& CurrentWork.WorkTypeId == WorkTypeId
		&& CurrentWork.PhysicalSiteId == PhysicalSiteId)
	{
		return EPersonWorkResult::AlreadyAssigned;
	}

	SetPersonWork(*PersonRecord, FCurrentWork::AtPhysicalSite(WorkTypeId, PhysicalSiteId));

	return EPersonWorkResult::Success;
}

EPersonWorkResult FSimulationRegistry::RemovePersonWork(FPersonId PersonId)
{
	FPersonRecord* PersonRecord = ResolvePersonMutable(PersonId);
	if (PersonRecord == nullptr)
	{
		return EPersonWorkResult::UnknownPerson;
	}

	if (!PersonRecord->CurrentWork.IsAssigned())
	{
		return EPersonWorkResult::NotAssigned;
	}

	SetPersonWork(*PersonRecord, FCurrentWork::Nowhere());

	return EPersonWorkResult::Success;
}

EPersonCapabilityResult FSimulationRegistry::AddPersonCapability(
	FPersonId PersonId, FSkillTypeId SkillTypeId)
{
	// All validation precedes any append so a rejected pair leaves storage unchanged.
	if (!ContainsPerson(PersonId))
	{
		return EPersonCapabilityResult::UnknownPerson;
	}

	if (!ContainsSkillType(SkillTypeId))
	{
		return EPersonCapabilityResult::UnknownSkillType;
	}

	if (FindPersonCapabilityIndex(PersonCapabilityRecords, PersonId, SkillTypeId) != INDEX_NONE)
	{
		return EPersonCapabilityResult::AlreadyHasCapability;
	}

	const int32 RecordIndex = PersonCapabilityRecords.AddDefaulted();
	FPersonCapabilityRecord& CapabilityRecord = PersonCapabilityRecords[RecordIndex];
	CapabilityRecord.PersonId = PersonId;
	CapabilityRecord.SkillTypeId = SkillTypeId;
	CapabilityRecord.AccumulatedPractice = 0;

	return EPersonCapabilityResult::Success;
}

EPersonCapabilityPracticeResult FSimulationRegistry::AddPersonCapabilityPractice(
	FPersonId PersonId, FSkillTypeId SkillTypeId, uint32 Amount)
{
	// All validation precedes any write so a rejected increment leaves practice unchanged.
	if (!ContainsPerson(PersonId))
	{
		return EPersonCapabilityPracticeResult::UnknownPerson;
	}

	if (!ContainsSkillType(SkillTypeId))
	{
		return EPersonCapabilityPracticeResult::UnknownSkillType;
	}

	const int32 RecordIndex = FindPersonCapabilityIndex(PersonCapabilityRecords, PersonId, SkillTypeId);
	if (RecordIndex == INDEX_NONE)
	{
		return EPersonCapabilityPracticeResult::MissingCapability;
	}

	if (Amount == 0)
	{
		return EPersonCapabilityPracticeResult::InvalidAmount;
	}

	FPersonCapabilityRecord& CapabilityRecord = PersonCapabilityRecords[RecordIndex];
	if (Amount > MAX_uint32 - CapabilityRecord.AccumulatedPractice)
	{
		return EPersonCapabilityPracticeResult::Overflow;
	}

	CapabilityRecord.AccumulatedPractice += Amount;
	return EPersonCapabilityPracticeResult::Success;
}

bool FSimulationRegistry::PersonHasCapability(FPersonId PersonId, FSkillTypeId SkillTypeId) const
{
	return FindPersonCapabilityIndex(PersonCapabilityRecords, PersonId, SkillTypeId) != INDEX_NONE;
}

TOptional<uint32> FSimulationRegistry::GetPersonCapabilityPractice(
	FPersonId PersonId, FSkillTypeId SkillTypeId) const
{
	const int32 RecordIndex = FindPersonCapabilityIndex(PersonCapabilityRecords, PersonId, SkillTypeId);
	if (RecordIndex == INDEX_NONE)
	{
		return TOptional<uint32>();
	}

	return PersonCapabilityRecords[RecordIndex].AccumulatedPractice;
}

void FSimulationRegistry::ApplyGoodsAddition(
	FInventoryRecord& InventoryRecord, FGoodTypeId GoodTypeId, int32 Quantity)
{
	const int32 EntryIndex = FindEntryIndex(InventoryRecord.Entries, GoodTypeId);
	const int32 ExistingQuantity = EntryIndex == INDEX_NONE ? 0 : InventoryRecord.Entries[EntryIndex].Quantity;

	checkf(Quantity > 0, TEXT("Unvalidated quantity %d reached the inventory addition primitive."), Quantity);
	checkf(!WouldExceedMaxQuantity(ExistingQuantity, Quantity),
		TEXT("Unvalidated addition of %d to %d reached the inventory addition primitive."),
		Quantity, ExistingQuantity);

	if (EntryIndex == INDEX_NONE)
	{
		FInventoryEntry& NewEntry = InventoryRecord.Entries.AddDefaulted_GetRef();
		NewEntry.GoodTypeId = GoodTypeId;
		NewEntry.Quantity = Quantity;
	}
	else
	{
		InventoryRecord.Entries[EntryIndex].Quantity = ExistingQuantity + Quantity;
	}
}

void FSimulationRegistry::ApplyGoodsRemoval(
	FInventoryRecord& InventoryRecord, FGoodTypeId GoodTypeId, int32 Quantity)
{
	const int32 EntryIndex = FindEntryIndex(InventoryRecord.Entries, GoodTypeId);

	checkf(Quantity > 0, TEXT("Unvalidated quantity %d reached the inventory removal primitive."), Quantity);
	checkf(EntryIndex != INDEX_NONE && InventoryRecord.Entries[EntryIndex].Quantity >= Quantity,
		TEXT("Unvalidated removal of %d reached the inventory removal primitive."), Quantity);

	const int32 RemainingQuantity = InventoryRecord.Entries[EntryIndex].Quantity - Quantity;
	if (RemainingQuantity == 0)
	{
		// Order-preserving, so entry order stays insertion order rather than depending on
		// which entry happened to empty.
		InventoryRecord.Entries.RemoveAt(EntryIndex);
	}
	else
	{
		InventoryRecord.Entries[EntryIndex].Quantity = RemainingQuantity;
	}
}

void FSimulationRegistry::RecordGoodsAudit(
	EGoodsAuditAction Action, FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason)
{
	FGoodsAuditRecord& AuditRecord = GoodsAuditRecords.AddDefaulted_GetRef();
	AuditRecord.Action = Action;
	AuditRecord.InventoryId = InventoryId;
	AuditRecord.GoodTypeId = GoodTypeId;
	AuditRecord.Quantity = Quantity;
	AuditRecord.Reason = Reason;
}

TOptional<FGoodsAuditRecord> FSimulationRegistry::GetGoodsAuditRecord(int32 RecordIndex) const
{
	if (!GoodsAuditRecords.IsValidIndex(RecordIndex))
	{
		return TOptional<FGoodsAuditRecord>();
	}

	return TOptional<FGoodsAuditRecord>(GoodsAuditRecords[RecordIndex]);
}

EAddGoodsResult FSimulationRegistry::AddGoods(
	FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason)
{
	FInventoryRecord* InventoryRecord = ResolveInventoryMutable(InventoryId);
	if (InventoryRecord == nullptr)
	{
		return EAddGoodsResult::UnknownInventory;
	}

	if (!ContainsGoodType(GoodTypeId))
	{
		return EAddGoodsResult::UnknownGoodType;
	}

	if (Quantity <= 0)
	{
		return EAddGoodsResult::InvalidQuantity;
	}

	// Quantity may not come into existence anonymously.
	if (Reason.IsNone())
	{
		return EAddGoodsResult::InvalidReason;
	}

	// Nor may it come into existence nowhere. No site is created or assigned to rescue the
	// call: an inventory with no place is a construction step the caller has not finished.
	if (!IsInventoryLocated(*InventoryRecord))
	{
		return EAddGoodsResult::InventoryNotLocated;
	}

	const int32 ExistingQuantity = GetEntryQuantity(InventoryRecord->Entries, GoodTypeId);
	if (WouldExceedMaxQuantity(ExistingQuantity, Quantity))
	{
		return EAddGoodsResult::Overflow;
	}

	ApplyGoodsAddition(*InventoryRecord, GoodTypeId, Quantity);
	RecordGoodsAudit(EGoodsAuditAction::Created, InventoryId, GoodTypeId, Quantity, Reason);

	return EAddGoodsResult::Success;
}

ERemoveGoodsResult FSimulationRegistry::RemoveGoods(
	FInventoryId InventoryId, FGoodTypeId GoodTypeId, int32 Quantity, FName Reason)
{
	FInventoryRecord* InventoryRecord = ResolveInventoryMutable(InventoryId);
	if (InventoryRecord == nullptr)
	{
		return ERemoveGoodsResult::UnknownInventory;
	}

	if (!ContainsGoodType(GoodTypeId))
	{
		return ERemoveGoodsResult::UnknownGoodType;
	}

	if (Quantity <= 0)
	{
		return ERemoveGoodsResult::InvalidQuantity;
	}

	// Quantity may not cease to exist anonymously.
	if (Reason.IsNone())
	{
		return ERemoveGoodsResult::InvalidReason;
	}

	// An inventory holding none of the good type has no entry at all, which is the same
	// rejection as holding too little.
	if (GetEntryQuantity(InventoryRecord->Entries, GoodTypeId) < Quantity)
	{
		return ERemoveGoodsResult::InsufficientQuantity;
	}

	ApplyGoodsRemoval(*InventoryRecord, GoodTypeId, Quantity);
	RecordGoodsAudit(EGoodsAuditAction::Destroyed, InventoryId, GoodTypeId, Quantity, Reason);

	return ERemoveGoodsResult::Success;
}

ETransferGoodsResult FSimulationRegistry::TransferGoods(
	FInventoryId SourceInventoryId,
	FInventoryId DestinationInventoryId,
	FGoodTypeId GoodTypeId,
	int32 Quantity)
{
	const FInventoryRecord* SourceRecord = ResolveInventory(SourceInventoryId);
	if (SourceRecord == nullptr)
	{
		return ETransferGoodsResult::UnknownSourceInventory;
	}

	const FInventoryRecord* DestinationRecord = ResolveInventory(DestinationInventoryId);
	if (DestinationRecord == nullptr)
	{
		return ETransferGoodsResult::UnknownDestinationInventory;
	}

	if (!ContainsGoodType(GoodTypeId))
	{
		return ETransferGoodsResult::UnknownGoodType;
	}

	if (Quantity <= 0)
	{
		return ETransferGoodsResult::InvalidQuantity;
	}

	// Structural rejection before the state-dependent ones, so a transfer to the same
	// inventory reports SameInventory whether or not it could otherwise have succeeded. It
	// is also what lets the two halves below be validated independently.
	if (SourceInventoryId == DestinationInventoryId)
	{
		return ETransferGoodsResult::SameInventory;
	}

	// Goods move from a place to a place. Both ends are checked before the quantity ones, so
	// a caller whose inventory has no place hears that rather than being told it is empty.
	if (!IsInventoryLocated(*SourceRecord))
	{
		return ETransferGoodsResult::SourceInventoryNotLocated;
	}

	if (!IsInventoryLocated(*DestinationRecord))
	{
		return ETransferGoodsResult::DestinationInventoryNotLocated;
	}

	if (GetEntryQuantity(SourceRecord->Entries, GoodTypeId) < Quantity)
	{
		return ETransferGoodsResult::InsufficientQuantity;
	}

	if (WouldExceedMaxQuantity(GetEntryQuantity(DestinationRecord->Entries, GoodTypeId), Quantity))
	{
		return ETransferGoodsResult::Overflow;
	}

	// Nothing is created or destroyed here, so the two internal quantity mutations are
	// applied directly rather than through AddGoods and RemoveGoods. That keeps a transfer
	// out of the audit trail, and it means no future creation or destruction policy on those
	// public operations can refuse half of an already validated transfer.
	//
	// Neither primitive can fail: both halves were validated above, and the inventories are
	// distinct, so applying one cannot invalidate the other's precondition. Each record is
	// resolved immediately before use, so no address is held across a mutation.
	ApplyGoodsRemoval(*ResolveInventoryMutable(SourceInventoryId), GoodTypeId, Quantity);
	ApplyGoodsAddition(*ResolveInventoryMutable(DestinationInventoryId), GoodTypeId, Quantity);

	return ETransferGoodsResult::Success;
}

bool FSimulationRegistry::ValidateInvariants(FString& OutFailureDescription) const
{
	OutFailureDescription.Reset();

	return ValidatePersonRecords(OutFailureDescription)
		&& ValidateHouseholdRecords(OutFailureDescription)
		&& ValidateSettlementRecords(OutFailureDescription)
		&& ValidatePropertyRecords(OutFailureDescription)
		&& ValidatePhysicalSiteRecords(OutFailureDescription)
		&& ValidateGoodTypeRecords(OutFailureDescription)
		&& ValidateWorkTypeRecords(OutFailureDescription)
		&& ValidateSkillTypeRecords(OutFailureDescription)
		&& ValidatePersonCapabilityRecords(OutFailureDescription)
		&& ValidateInventoryRecords(OutFailureDescription);
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

		// Current work is independent of household membership: an unhoused person may hold a
		// valid commitment, and a missing household must not skip these checks.
		const FCurrentWork& CurrentWork = PersonRecord.CurrentWork;
		if (CurrentWork.Kind == ECurrentWorkKind::None)
		{
			if (CurrentWork.WorkTypeId.IsValid())
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s has no work but names %s."),
					*PersonRecord.Id.ToString(), *CurrentWork.WorkTypeId.ToString());
				return false;
			}

			if (CurrentWork.PhysicalSiteId.IsValid())
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s has no work but names %s."),
					*PersonRecord.Id.ToString(), *CurrentWork.PhysicalSiteId.ToString());
				return false;
			}
		}
		else if (CurrentWork.Kind == ECurrentWorkKind::PhysicalSite)
		{
			if (!ContainsWorkType(CurrentWork.WorkTypeId))
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s is assigned unresolvable %s."),
					*PersonRecord.Id.ToString(), *CurrentWork.WorkTypeId.ToString());
				return false;
			}

			if (!ContainsPhysicalSite(CurrentWork.PhysicalSiteId))
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s is assigned work at unresolvable %s."),
					*PersonRecord.Id.ToString(), *CurrentWork.PhysicalSiteId.ToString());
				return false;
			}
		}
		else
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s has unsupported current-work kind %u."),
				*PersonRecord.Id.ToString(), static_cast<uint32>(CurrentWork.Kind));
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

		FPhysicalSiteId DuplicateSiteId;
		if (TryFindDuplicate(PropertyRecord.PhysicalSites, DuplicateSiteId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s lists %s more than once."),
				*PropertyRecord.Id.ToString(), *DuplicateSiteId.ToString());
			return false;
		}

		for (const FPhysicalSiteId PhysicalSiteId : PropertyRecord.PhysicalSites)
		{
			const FPhysicalSiteRecord* PhysicalSiteRecord = ResolvePhysicalSite(PhysicalSiteId);
			if (PhysicalSiteRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists unresolvable %s."),
					*PropertyRecord.Id.ToString(), *PhysicalSiteId.ToString());
				return false;
			}

			if (PhysicalSiteRecord->PropertyId != PropertyRecord.Id)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists %s, but that site belongs to %s."),
					*PropertyRecord.Id.ToString(), *PhysicalSiteId.ToString(),
					*PhysicalSiteRecord->PropertyId.ToString());
				return false;
			}
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

bool FSimulationRegistry::ValidatePhysicalSiteRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < PhysicalSiteRecords.Num(); ++RecordIndex)
	{
		const FPhysicalSiteRecord& PhysicalSiteRecord = PhysicalSiteRecords[RecordIndex];

		if (ToRecordIndex(PhysicalSiteRecord.Id, PhysicalSiteRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Physical site slot %d holds identifier %s."),
				RecordIndex, *PhysicalSiteRecord.Id.ToString());
			return false;
		}

		if (PhysicalSiteRecord.PurposeKey.IsNone())
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s has no purpose key."), *PhysicalSiteRecord.Id.ToString());
			return false;
		}

		const FPropertyRecord* PropertyRecord = ResolveProperty(PhysicalSiteRecord.PropertyId);
		if (PropertyRecord == nullptr)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s belongs to unresolvable %s."),
				*PhysicalSiteRecord.Id.ToString(), *PhysicalSiteRecord.PropertyId.ToString());
			return false;
		}

		const int32 ListedCount = CountOccurrences(PropertyRecord->PhysicalSites, PhysicalSiteRecord.Id);
		if (ListedCount != 1)
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s belongs to %s but appears in its site list %d times."),
				*PhysicalSiteRecord.Id.ToString(), *PhysicalSiteRecord.PropertyId.ToString(), ListedCount);
			return false;
		}

		FInventoryId DuplicateInventoryId;
		if (TryFindDuplicate(PhysicalSiteRecord.Inventories, DuplicateInventoryId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s lists %s more than once."),
				*PhysicalSiteRecord.Id.ToString(), *DuplicateInventoryId.ToString());
			return false;
		}

		for (const FInventoryId InventoryId : PhysicalSiteRecord.Inventories)
		{
			const FInventoryRecord* InventoryRecord = ResolveInventory(InventoryId);
			if (InventoryRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists unresolvable %s."),
					*PhysicalSiteRecord.Id.ToString(), *InventoryId.ToString());
				return false;
			}

			// An inventory names only one site, so this is also what catches the same
			// inventory being listed by two of them: at most one of the two can agree.
			const FInventoryLocation& Location = InventoryRecord->Location;
			if (Location.Kind != EInventoryLocationKind::PhysicalSite
				|| Location.PhysicalSiteId != PhysicalSiteRecord.Id)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s lists %s, but that inventory is not located there."),
					*PhysicalSiteRecord.Id.ToString(), *InventoryId.ToString());
				return false;
			}
		}
	}

	return true;
}

bool FSimulationRegistry::ValidateGoodTypeRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < GoodTypeRecords.Num(); ++RecordIndex)
	{
		const FGoodTypeRecord& GoodTypeRecord = GoodTypeRecords[RecordIndex];

		if (ToRecordIndex(GoodTypeRecord.Id, GoodTypeRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Good type slot %d holds identifier %s."), RecordIndex, *GoodTypeRecord.Id.ToString());
			return false;
		}

		if (GoodTypeRecord.AuthoredKey.IsNone())
		{
			OutFailureDescription = FString::Printf(
				TEXT("Good type slot %d has no authored key."), RecordIndex);
			return false;
		}

		// The authored key is a definition identity, so it must identify exactly one good
		// type. Compared against earlier slots only, so the first duplicate is reported once.
		for (int32 EarlierIndex = 0; EarlierIndex < RecordIndex; ++EarlierIndex)
		{
			if (GoodTypeRecords[EarlierIndex].AuthoredKey == GoodTypeRecord.AuthoredKey)
			{
				OutFailureDescription = FString::Printf(
					TEXT("Good type slots %d and %d share the authored key '%s'."),
					EarlierIndex, RecordIndex, *GoodTypeRecord.AuthoredKey.ToString());
				return false;
			}
		}
	}

	return true;
}

bool FSimulationRegistry::ValidateWorkTypeRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < WorkTypeRecords.Num(); ++RecordIndex)
	{
		const FWorkTypeRecord& WorkTypeRecord = WorkTypeRecords[RecordIndex];

		if (ToRecordIndex(WorkTypeRecord.Id, WorkTypeRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Work type slot %d holds identifier %s."), RecordIndex, *WorkTypeRecord.Id.ToString());
			return false;
		}

		if (WorkTypeRecord.AuthoredKey.IsNone())
		{
			OutFailureDescription = FString::Printf(
				TEXT("Work type slot %d has no authored key."), RecordIndex);
			return false;
		}

		for (int32 EarlierIndex = 0; EarlierIndex < RecordIndex; ++EarlierIndex)
		{
			if (WorkTypeRecords[EarlierIndex].AuthoredKey == WorkTypeRecord.AuthoredKey)
			{
				OutFailureDescription = FString::Printf(
					TEXT("Work type slots %d and %d share the authored key '%s'."),
					EarlierIndex, RecordIndex, *WorkTypeRecord.AuthoredKey.ToString());
				return false;
			}
		}
	}

	return true;
}

bool FSimulationRegistry::ValidateSkillTypeRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < SkillTypeRecords.Num(); ++RecordIndex)
	{
		const FSkillTypeRecord& SkillTypeRecord = SkillTypeRecords[RecordIndex];

		if (ToRecordIndex(SkillTypeRecord.Id, SkillTypeRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Skill type slot %d holds identifier %s."), RecordIndex, *SkillTypeRecord.Id.ToString());
			return false;
		}

		if (SkillTypeRecord.AuthoredKey.IsNone())
		{
			OutFailureDescription = FString::Printf(
				TEXT("Skill type slot %d has no authored key."), RecordIndex);
			return false;
		}

		for (int32 EarlierIndex = 0; EarlierIndex < RecordIndex; ++EarlierIndex)
		{
			if (SkillTypeRecords[EarlierIndex].AuthoredKey == SkillTypeRecord.AuthoredKey)
			{
				OutFailureDescription = FString::Printf(
					TEXT("Skill type slots %d and %d share the authored key '%s'."),
					EarlierIndex, RecordIndex, *SkillTypeRecord.AuthoredKey.ToString());
				return false;
			}
		}
	}

	return true;
}

bool FSimulationRegistry::ValidatePersonCapabilityRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < PersonCapabilityRecords.Num(); ++RecordIndex)
	{
		const FPersonCapabilityRecord& CapabilityRecord = PersonCapabilityRecords[RecordIndex];

		if (!ContainsPerson(CapabilityRecord.PersonId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("Person capability slot %d names unresolvable person %s."),
				RecordIndex, *CapabilityRecord.PersonId.ToString());
			return false;
		}

		if (!ContainsSkillType(CapabilityRecord.SkillTypeId))
		{
			OutFailureDescription = FString::Printf(
				TEXT("Person capability slot %d names unresolvable skill type %s."),
				RecordIndex, *CapabilityRecord.SkillTypeId.ToString());
			return false;
		}

		for (int32 EarlierIndex = 0; EarlierIndex < RecordIndex; ++EarlierIndex)
		{
			const FPersonCapabilityRecord& EarlierRecord = PersonCapabilityRecords[EarlierIndex];
			if (EarlierRecord.PersonId == CapabilityRecord.PersonId
				&& EarlierRecord.SkillTypeId == CapabilityRecord.SkillTypeId)
			{
				OutFailureDescription = FString::Printf(
					TEXT("Person capability slots %d and %d share person %s and skill type %s."),
					EarlierIndex,
					RecordIndex,
					*CapabilityRecord.PersonId.ToString(),
					*CapabilityRecord.SkillTypeId.ToString());
				return false;
			}
		}
	}

	return true;
}

bool FSimulationRegistry::ValidateInventoryRecords(FString& OutFailureDescription) const
{
	for (int32 RecordIndex = 0; RecordIndex < InventoryRecords.Num(); ++RecordIndex)
	{
		const FInventoryRecord& InventoryRecord = InventoryRecords[RecordIndex];

		if (ToRecordIndex(InventoryRecord.Id, InventoryRecords.Num()) != RecordIndex)
		{
			OutFailureDescription = FString::Printf(
				TEXT("Inventory slot %d holds identifier %s."), RecordIndex, *InventoryRecord.Id.ToString());
			return false;
		}

		const FInventoryLocation& Location = InventoryRecord.Location;
		if (Location.Kind == EInventoryLocationKind::None)
		{
			// The tag and its payload must agree, or the location says two things at once.
			if (Location.PhysicalSiteId.IsValid())
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s is nowhere but names %s."),
					*InventoryRecord.Id.ToString(), *Location.PhysicalSiteId.ToString());
				return false;
			}

			// The rule the whole slice exists for: goods are never placeless.
			if (InventoryRecord.Entries.Num() > 0)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s holds %d good type(s) but is nowhere."),
					*InventoryRecord.Id.ToString(), InventoryRecord.Entries.Num());
				return false;
			}
		}
		else if (Location.Kind == EInventoryLocationKind::PhysicalSite)
		{
			const FPhysicalSiteRecord* PhysicalSiteRecord = ResolvePhysicalSite(Location.PhysicalSiteId);
			if (PhysicalSiteRecord == nullptr)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s is located at unresolvable %s."),
					*InventoryRecord.Id.ToString(), *Location.PhysicalSiteId.ToString());
				return false;
			}

			const int32 ListedCount = CountOccurrences(PhysicalSiteRecord->Inventories, InventoryRecord.Id);
			if (ListedCount != 1)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s is located at %s but appears in its inventory list %d times."),
					*InventoryRecord.Id.ToString(), *Location.PhysicalSiteId.ToString(), ListedCount);
				return false;
			}
		}
		else
		{
			OutFailureDescription = FString::Printf(
				TEXT("%s has unsupported location kind %u."),
				*InventoryRecord.Id.ToString(), static_cast<uint32>(Location.Kind));
			return false;
		}

		TSet<FGoodTypeId> SeenGoodTypeIds;
		SeenGoodTypeIds.Reserve(InventoryRecord.Entries.Num());

		for (const FInventoryEntry& Entry : InventoryRecord.Entries)
		{
			bool bAlreadySeen = false;
			SeenGoodTypeIds.Add(Entry.GoodTypeId, &bAlreadySeen);
			if (bAlreadySeen)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s holds more than one entry for %s."),
					*InventoryRecord.Id.ToString(), *Entry.GoodTypeId.ToString());
				return false;
			}

			if (!ContainsGoodType(Entry.GoodTypeId))
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s holds unresolvable %s."),
					*InventoryRecord.Id.ToString(), *Entry.GoodTypeId.ToString());
				return false;
			}

			// Quantities are int32 and capped at the type's own maximum, so the only
			// unrepresentable stored values are these.
			if (Entry.Quantity <= 0)
			{
				OutFailureDescription = FString::Printf(
					TEXT("%s stores a quantity of %d for %s; stored quantities must be positive."),
					*InventoryRecord.Id.ToString(), Entry.Quantity, *Entry.GoodTypeId.ToString());
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

const FPhysicalSiteRecord* FSimulationRegistry::ResolvePhysicalSite(FPhysicalSiteId PhysicalSiteId) const
{
	const int32 RecordIndex = ToRecordIndex(PhysicalSiteId, PhysicalSiteRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &PhysicalSiteRecords[RecordIndex];
}

const FGoodTypeRecord* FSimulationRegistry::ResolveGoodType(FGoodTypeId GoodTypeId) const
{
	const int32 RecordIndex = ToRecordIndex(GoodTypeId, GoodTypeRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &GoodTypeRecords[RecordIndex];
}

const FWorkTypeRecord* FSimulationRegistry::ResolveWorkType(FWorkTypeId WorkTypeId) const
{
	const int32 RecordIndex = ToRecordIndex(WorkTypeId, WorkTypeRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &WorkTypeRecords[RecordIndex];
}

const FSkillTypeRecord* FSimulationRegistry::ResolveSkillType(FSkillTypeId SkillTypeId) const
{
	const int32 RecordIndex = ToRecordIndex(SkillTypeId, SkillTypeRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &SkillTypeRecords[RecordIndex];
}

const FInventoryRecord* FSimulationRegistry::ResolveInventory(FInventoryId InventoryId) const
{
	const int32 RecordIndex = ToRecordIndex(InventoryId, InventoryRecords.Num());
	return RecordIndex == INDEX_NONE ? nullptr : &InventoryRecords[RecordIndex];
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

FPhysicalSiteRecord* FSimulationRegistry::ResolvePhysicalSiteMutable(FPhysicalSiteId PhysicalSiteId)
{
	return const_cast<FPhysicalSiteRecord*>(ResolvePhysicalSite(PhysicalSiteId));
}

FInventoryRecord* FSimulationRegistry::ResolveInventoryMutable(FInventoryId InventoryId)
{
	return const_cast<FInventoryRecord*>(ResolveInventory(InventoryId));
}

bool FSimulationRegistry::IsInventoryLocated(const FInventoryRecord& InventoryRecord) const
{
	const FInventoryLocation& Location = InventoryRecord.Location;

	return Location.Kind == EInventoryLocationKind::PhysicalSite
		&& ContainsPhysicalSite(Location.PhysicalSiteId);
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

void FSimulationRegistry::SetInventoryLocation(
	FInventoryRecord& InventoryRecord, FInventoryLocation NewLocation)
{
	const FInventoryLocation& CurrentLocation = InventoryRecord.Location;
	if (CurrentLocation.Kind == NewLocation.Kind
		&& CurrentLocation.PhysicalSiteId == NewLocation.PhysicalSiteId)
	{
		return;
	}

	if (CurrentLocation.Kind == EInventoryLocationKind::PhysicalSite)
	{
		if (FPhysicalSiteRecord* PreviousSite = ResolvePhysicalSiteMutable(CurrentLocation.PhysicalSiteId))
		{
			PreviousSite->Inventories.RemoveSingle(InventoryRecord.Id);
		}
	}

	InventoryRecord.Location = NewLocation;

	if (NewLocation.Kind == EInventoryLocationKind::PhysicalSite)
	{
		if (FPhysicalSiteRecord* NewSite = ResolvePhysicalSiteMutable(NewLocation.PhysicalSiteId))
		{
			checkf(!NewSite->Inventories.Contains(InventoryRecord.Id),
				TEXT("%s was already listed by %s before being placed there."),
				*InventoryRecord.Id.ToString(), *NewLocation.PhysicalSiteId.ToString());

			NewSite->Inventories.Add(InventoryRecord.Id);
		}
	}
}

void FSimulationRegistry::SetPersonWork(FPersonRecord& PersonRecord, const FCurrentWork& NewWork)
{
	PersonRecord.CurrentWork = NewWork;
}
