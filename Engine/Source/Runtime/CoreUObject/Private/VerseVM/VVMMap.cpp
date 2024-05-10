// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMap.h"
#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VMapBase);

template <typename TVisitor>
void VMapBase::VisitReferencesImpl(TVisitor& Visitor)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	if constexpr (std::is_same_v<TVisitor, FMarkStackVisitor>)
	{
		Visitor.VisitAux(Data.Get().GetPtr(), TEXT("Data")); // Visit the buffer we allocated for the array as Aux memory
		Visitor.VisitAux(SequenceData.Get().GetPtr(), TEXT("SequenceTable"));
		for (auto MapIt : *this)
		{
			::Verse::Visit(Visitor, MapIt.Key, TEXT("Key"));
			::Verse::Visit(Visitor, MapIt.Value, TEXT("Value"));
		}
	}
	else
	{
		uint64 ScratchNumElements = NumElements;
		Visitor.BeginMap(TEXT("Values"), ScratchNumElements);
		for (auto MapIt : *this)
		{
			Visitor.BeginObject();
			::Verse::Visit(Visitor, MapIt.Key, TEXT("Key"));
			::Verse::Visit(Visitor, MapIt.Value, TEXT("Value"));
			Visitor.EndObject();
		}
		Visitor.EndMap();
	}
}
inline VValue FindInPairDataByHashWithSlot(FAllocationContext Context, VMapBase::PairType* PairData, uint32 Capacity, uint32 Hash, VValue Key, uint32* OutSlot)
{
	check(Capacity > 0);
	uint32 HashMask = Capacity - 1;
	uint32 Slot = Hash & HashMask;
	uint32 LoopCount = 0;
	while (!PairData[Slot].Key.Get().IsUninitialized() && LoopCount++ < Capacity)
	{
		if (VValue::Equal(Context, PairData[Slot].Key.Get(), Key, [](VValue Left, VValue Right) {}))
		{
			*OutSlot = Slot;
			return PairData[Slot].Value.Get();
		}
		Slot = (Slot + 1) & HashMask; // dumb linear probe, @TODO: something better
	}
	*OutSlot = Slot;
	return VValue();
}

VValue VMapBase::FindByHashWithSlot(FAllocationContext Context, uint32 Hash, VValue Key, uint32* OutSlot)
{
	return FindInPairDataByHashWithSlot(Context, GetPairTable(), Capacity, Hash, Key, OutSlot);
}

uint32 VMapBase::GetTypeHashImpl()
{
	uint32 Result = 0;
	for (auto MapIt : *this)
	{
		Result = ::HashCombineFast(Result, ::HashCombineFast(GetTypeHash(MapIt.Key), GetTypeHash(MapIt.Value)));
	}
	return Result;
}

void VMapBase::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	uint32 Count = 0;
	for (auto MapIt : *this)
	{
		if (Count > 0)
		{
			Builder.Append(TEXT(", "));
		}
		++Count;
		MapIt.Key.ToString(Builder, Context, Formatter);
		Builder.Append(TEXT(" => "));
		MapIt.Value.ToString(Builder, Context, Formatter);
	}
}

bool VMapBase::EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VMapBase>())
	{
		return false;
	}

	VMapBase& OtherMap = Other->StaticCast<VMapBase>();
	if (Num() != OtherMap.Num())
	{
		return false;
	}

	for (int32 i = 0; i < Num(); ++i)
	{
		VValue K0 = GetKey(i);
		VValue K1 = OtherMap.GetKey(i);
		VValue V0 = GetValue(i);
		VValue V1 = OtherMap.GetValue(i);
		if (!VValue::Equal(Context, K0, K1, HandlePlaceholder) || !VValue::Equal(Context, V0, V1, HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

void VMapBase::Reserve(FAllocationContext Context, uint32 InCapacity)
{
	uint32 NewCapacity = FMath::RoundUpToPowerOfTwo(InCapacity < 8 ? 8 : InCapacity);
	int32 NumElementsInsertedIntoNewData = 0;
	if (NewCapacity <= Capacity)
	{
		return; // should we support shrinking?
	}
	TAux<void> NewData = TAux<void>(FAllocationContext(Context).AllocateAuxCell(GetPairTableSizeForCapacity(NewCapacity)));
	TAux<SequenceType> NewSequenceData = TAux<SequenceType>(FAllocationContext(Context).AllocateAuxCell(GetSequenceTableSizeForCapacity(NewCapacity)));

	FMemory::Memzero(NewData.GetPtr(), GetPairTableSizeForCapacity(NewCapacity));

	if (Data)
	{
		PairType* OldPairTable = GetPairTable();
		SequenceType* OldSequenceTable = GetSequenceTable();

		PairType* NewPairTable = static_cast<PairType*>(NewData.GetPtr());
		SequenceType* NewSequenceTable = static_cast<SequenceType*>(NewSequenceData.GetPtr());

		for (int32 ElemIdx = 0; ElemIdx < NumElements; ++ElemIdx)
		{
			PairType* OldPair = OldPairTable + OldSequenceTable[ElemIdx];
			uint32 NewSlot;
			VValue ExistingValInNewTable = FindInPairDataByHashWithSlot(Context, NewPairTable, NewCapacity, GetTypeHash(OldPair->Key), OldPair->Key.Get(), &NewSlot);
			check(ExistingValInNewTable.IsUninitialized()); // duplicate keys should be impossible since we're building from an existing set of data
			while (!NewPairTable[NewSlot].Key.Get().IsUninitialized())
			{
				NewSlot = (NewSlot + 1) & (NewCapacity - 1); // dumb linear probe, @TODO: something better
			}
			NewPairTable[NewSlot] = {OldPair->Key, OldPair->Value};
			NewSequenceTable[NumElementsInsertedIntoNewData++] = NewSlot;
		}
	}
	Data = {Context, NewData};
	SequenceData = {Context, NewSequenceData};
	Capacity = NewCapacity;
}

VMapBase::~VMapBase()
{
}

template <typename MapType, typename TranslationFunc>
VValue VMapBase::FreezeMeltImpl(FAllocationContext Context, TranslationFunc&& Func)
{
	VMapBase& MapCopy = VMapBase::New<MapType>(Context, Num());

	UE::FExternalMutex ExternalMutex(MapCopy.Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	PairType* PairTable = GetPairTable();
	SequenceType* SequenceTable = GetSequenceTable();
	for (int i = 0; i < NumElements; ++i)
	{
		PairType* Pair = PairTable + SequenceTable[i];
		VValue Key = Pair->Key.Get(); // Func(Context, Pair->Key.Get());
		// if (Key.IsPlaceholder())
		//{
		//	return Key;
		// }
		VValue Val = Func(Context, Pair->Value.Get());
		if (Val.IsPlaceholder())
		{
			return Val;
		}
		MapCopy.AddWithoutLocking(Context, GetTypeHash(Key), Key, Val);
	}
	return MapCopy;
}

VValue VMapBase::MeltImpl(FAllocationContext Context)
{
	return FreezeMeltImpl<VMutableMap>(Context, [](FAllocationContext Context, VValue Value) { return VValue::Melt(Context, Value); });
}

VValue VMutableMap::FreezeImpl(FAllocationContext Context)
{
	return FreezeMeltImpl<VMap>(Context, [](FAllocationContext Context, VValue Value) { return VValue::Freeze(Context, Value); });
}

DEFINE_DERIVED_VCPPCLASSINFO(VMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMap);
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

DEFINE_DERIVED_VCPPCLASSINFO(VMutableMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMutableMap);
TGlobalTrivialEmergentTypePtr<&VMutableMap::StaticCppClassInfo> VMutableMap::GlobalTrivialEmergentType;

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
