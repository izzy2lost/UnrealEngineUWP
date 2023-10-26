// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
inline bool VFields::FFieldsMapKeyFuncs::Matches(KeyInitType A, KeyInitType B)
{
	return A == B;
}

inline bool VFields::FFieldsMapKeyFuncs::Matches(KeyInitType A, const VUniqueString& B)
{
	return *(A.Get()) == B;
}

inline uint32 VFields::FFieldsMapKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return GetTypeHash(Key);
}

inline uint32 VFields::FFieldsMapKeyFuncs::GetKeyHash(const VUniqueString& Key)
{
	return GetTypeHash(Key);
}

inline VFields::VEntry::VEntry(const VFields::VEntry& Other)
	: Index(Other.Index)
	, Type(Other.Type)
{
	new (&Constant) TWriteBarrier<VValue>(Other.Constant);
}

inline VFields::VEntry::VEntry(VFields::VEntry&& Other)
	: Index(Other.Index)
	, Type(Other.Type)
{
	new (&Constant) TWriteBarrier<VValue>(MoveTemp(Other.Constant));
}

inline VFields::VEntry::VEntry(const uint64 InIndex, const bool bIsMutable)
	: Index(InIndex)
	, Constant({})
	, Type(bIsMutable ? EFieldType::Mutable : EFieldType::Offset) {}

inline VFields::VEntry::VEntry(FAccessContext Context, VValue InConstant, const EFieldType FieldType)
	: Index(0)
	, Constant(TWriteBarrier<VValue>{Context, InConstant})
	, Type(FieldType)
{
}

inline VFields::VEntry::VEntry(FAccessContext Context, VValue InConstant)
	: VFields::VEntry::VEntry(Context, InConstant, EFieldType::Constant) {}

inline bool VFields::VEntry::operator==(const VFields::VEntry& Other) const
{
	if (Type != Other.Type || Index != Other.Index)
	{
		return false;
	}
	return VValue::Equal(
		FRunningContextPromise(),
		Constant.Get(),
		Other.Constant.Get(),
		[](VValue Left, VValue Right) {
			checkSlow(!Left.IsPlaceholder());
			checkSlow(!Right.IsPlaceholder());
		});
}

inline VFields::VFields(FAllocationContext Context, VFields::FieldsMap&& InFields)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, Fields(MoveTemp(InFields))
{
}

inline VFields& VFields::VFields::New(FAllocationContext Context, VFields::FieldsMap&& InFields)
{
	// We allocate in the destructor space here since we're making `VFields` destructible so that it can
	// destruct its `TMap` member of fields.
	return *new (Context.Allocate(FHeap::DestructorSpace, sizeof(VFields))) VFields(Context, MoveTemp(InFields));
}

inline VFields::FieldsMap& VFields::GetFields()
{
	return Fields;
}

inline const VFields::VEntry* VShape::GetField(FAllocationContext Context, const VUniqueString& Name) const
{
	// NOTE: It should be safe to `const_cast` here because we're really just constructing a `TWriteBarrier`
	// around the string for the sake of lookup; it doesn't need to mutate the actual string itself.
	if (const VFields::VEntry* Field = Fields.FindByHash(GetTypeHash(Name), Name))
	{
		return Field;
	}
	else
	{
		return nullptr;
	}
}

inline uint64 VShape::GetNumFields() const
{
	return Fields.Num();
}

inline bool VShape::operator==(const VShape& Other) const
{
	return Fields.OrderIndependentCompareEqual(Other.Fields);
}

inline uint32 GetTypeHash(const VFields::VEntry& Field)
{
	switch (Field.Type)
	{
		case Verse::EFieldType::Mutable:
		case Verse::EFieldType::Offset:
			return HashCombineFast(::GetTypeHash(static_cast<int8>(Field.Type)), ::GetTypeHash(Field.Index));
		case Verse::EFieldType::Constant:
			return HashCombineFast(::GetTypeHash(static_cast<int8>(Field.Type)), GetTypeHash(Field.Constant.Get()));
		default:
			break;
	}
	VERSE_UNREACHABLE();
}

inline uint32 GetTypeHash(const VShape& Shape)
{
	uint32 Hash = 0;
	for (auto It : Shape.Fields)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(It.Key));
		Hash = HashCombineFast(Hash, GetTypeHash(It.Value));
	}
	return Hash;
}

inline const VFields::FieldsMap& VShape::GetFields() const
{
	return Fields;
}

} // namespace Verse
