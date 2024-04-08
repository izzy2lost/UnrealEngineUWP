// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMArray.h"
#include "VVMArrayBase.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{
struct FOpResult;

struct VMutableArray : VArrayBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VArrayBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

private:
	uint32 Capacity;

public:
	void SetValue(FAllocationContext Context, uint32 Index, VValue Value)
	{
		Super::SetValue(Context, Index, Value, &Capacity);
	}

	void AddValue(FAllocationContext Context, VValue Value);

	template <typename T>
	void Append(FAllocationContext Context, VArrayBase& Array);
	void Append(FAllocationContext Context, VArrayBase& Array);

	static VMutableArray& Concat(FAllocationContext Context, VArrayBase& Lhs, VArrayBase& Rhs);

	void InPlaceMakeImmutable(FAllocationContext Context)
	{
		static_assert(std::is_base_of_v<VArrayBase, VArray>);
		static_assert(sizeof(VArray) == sizeof(VArrayBase));
		SetEmergentType(Context, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, NumValues), &VArray::StaticCppClassInfo));
	}

	static VMutableArray& New(FAllocationContext Context, uint32 InitialCapacity, EArrayType ArrayType = EArrayType::None)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, InitialCapacity, ArrayType);
	}

	static VMutableArray& New(FAllocationContext Context, std::initializer_list<VValue> InitList)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, InitList);
	}

	template <typename InitIndexFunc>
	static VMutableArray& New(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, InNumValues, InitFunc);
	}

	static VMutableArray& New(FAllocationContext Context, FUtf8StringView String)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, String);
	}

	static void SerializeImpl(VMutableArray*& This, FAllocationContext Context, FAbstractVisitor& Visitor) { Serialize(This, Context, Visitor); }

	COREUOBJECT_API FOpResult FreezeImpl(FRunningContext Context);

private:
	VMutableArray(FAllocationContext Context, uint32 InitialCapacity, EArrayType ArrayType)
		: VArrayBase(Context, InitialCapacity, ArrayType, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(InitialCapacity)
	{
		NumValues = 0;
	}

	VMutableArray(FAllocationContext Context, std::initializer_list<VValue> InitList)
		: VArrayBase(Context, InitList, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(static_cast<uint32>(InitList.size())) {}

	template <typename InitIndexFunc>
	VMutableArray(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc)
		: VArrayBase(Context, InNumValues, InitFunc, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(InNumValues) {}

	VMutableArray(FAllocationContext Context, FUtf8StringView String)
		: VArrayBase(Context, String, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(static_cast<uint32>(String.Len())) {}
};

} // namespace Verse
#endif // WITH_VERSE_VM
