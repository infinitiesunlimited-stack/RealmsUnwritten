#pragma once

#include "CoreMinimal.h"

#include <type_traits>

/**
 * Stable identity for one authoritative simulation entity.
 *
 * The tag parameter gives every entity family its own distinct type, so a household
 * identifier cannot be passed where a person identifier is expected, in either direction
 * and not even explicitly.
 *
 * The numeric value carries no meaning to callers, and only the owning registry allocates
 * meaningful values. The raw-value constructor is deliberately available, though explicit,
 * because validation and boundary tests need to build arbitrary values on purpose: an
 * identifier the registry never allocated simply fails to resolve, for every possible
 * value. Constructing one therefore grants no capability.
 *
 * Value 0 always means "no entity" and never resolves to a record.
 */
template <typename TEntityTag>
struct TSimulationId
{
	using ValueType = uint32;

	static constexpr ValueType InvalidValue = 0;

	TSimulationId() = default;

	explicit constexpr TSimulationId(ValueType InValue)
		: Value(InValue)
	{
	}

	constexpr bool IsValid() const { return Value != InvalidValue; }

	constexpr ValueType GetValue() const { return Value; }

	constexpr bool operator==(TSimulationId Other) const { return Value == Other.Value; }
	constexpr bool operator!=(TSimulationId Other) const { return Value != Other.Value; }

	FString ToString() const { return FString::Printf(TEXT("%s#%u"), TEntityTag::DebugName, Value); }

	friend uint32 GetTypeHash(TSimulationId Id) { return ::GetTypeHash(Id.Value); }

private:
	ValueType Value = InvalidValue;
};

struct FPersonIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("Person");
};

struct FHouseholdIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("Household");
};

struct FSettlementIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("Settlement");
};

struct FPropertyIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("Property");
};

struct FGoodTypeIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("GoodType");
};

struct FInventoryIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("Inventory");
};

struct FPhysicalSiteIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("PhysicalSite");
};

struct FWorkTypeIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("WorkType");
};

struct FSkillTypeIdTag
{
	static constexpr const TCHAR* DebugName = TEXT("SkillType");
};

using FPersonId = TSimulationId<FPersonIdTag>;
using FHouseholdId = TSimulationId<FHouseholdIdTag>;
using FSettlementId = TSimulationId<FSettlementIdTag>;
using FPropertyId = TSimulationId<FPropertyIdTag>;
using FGoodTypeId = TSimulationId<FGoodTypeIdTag>;
using FInventoryId = TSimulationId<FInventoryIdTag>;
using FPhysicalSiteId = TSimulationId<FPhysicalSiteIdTag>;
using FWorkTypeId = TSimulationId<FWorkTypeIdTag>;
using FSkillTypeId = TSimulationId<FSkillTypeIdTag>;

namespace SimulationIdContract
{
	/** Whether either identifier type could stand in for the other, by any means. */
	template <typename TLeft, typename TRight>
	inline constexpr bool bInterchangeable =
		std::is_same_v<TLeft, TRight> ||
		std::is_convertible_v<TLeft, TRight> ||
		std::is_convertible_v<TRight, TLeft> ||
		std::is_constructible_v<TLeft, TRight> ||
		std::is_constructible_v<TRight, TLeft>;

	template <typename... TIds>
	struct TMutuallyDistinct : std::true_type
	{
	};

	template <typename THead, typename... TTail>
	struct TMutuallyDistinct<THead, TTail...>
		: std::bool_constant<(... && !bInterchangeable<THead, TTail>) && TMutuallyDistinct<TTail...>::value>
	{
	};
}

// No identifier family may ever stand in for another, in either direction, whether
// implicitly or through explicit construction. One assertion covers every pair, so a future
// entity family only has to be added to this list. The contract lives with the types rather
// than only in the tests.
static_assert(
	SimulationIdContract::TMutuallyDistinct<
		FPersonId, FHouseholdId, FSettlementId, FPropertyId, FGoodTypeId, FInventoryId, FPhysicalSiteId,
		FWorkTypeId, FSkillTypeId>::value,
	"Simulation identifier families must never be interchangeable with one another.");
