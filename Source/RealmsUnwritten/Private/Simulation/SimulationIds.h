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

using FPersonId = TSimulationId<FPersonIdTag>;
using FHouseholdId = TSimulationId<FHouseholdIdTag>;

// The two identifier families must never be interchangeable, in either direction, whether
// implicitly or through explicit construction. These assertions are the contract, so they
// live with the types rather than only in the tests.
static_assert(!std::is_same_v<FPersonId, FHouseholdId>,
	"Person and household identifiers must be distinct types.");
static_assert(!std::is_convertible_v<FPersonId, FHouseholdId>,
	"A person identifier must not convert to a household identifier.");
static_assert(!std::is_convertible_v<FHouseholdId, FPersonId>,
	"A household identifier must not convert to a person identifier.");
static_assert(!std::is_constructible_v<FHouseholdId, FPersonId>,
	"A household identifier must not be constructible from a person identifier.");
static_assert(!std::is_constructible_v<FPersonId, FHouseholdId>,
	"A person identifier must not be constructible from a household identifier.");
