// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{
inline VObject& VObject::New(FAllocationContext Context, VEmergentType& InEmergentType)
{
	// `Data` is a flexible array member of `VObject`, which means it's not accounted for in the `sizeof(VObject)` result.
	// We therefore calculate the actual size based off the number of fields. `TWriteBarrier` just wraps around
	// its templated type, so `sizeof(VRestValue)` is what the actual size required is.
	const uint64 NumIndexedFields = InEmergentType.Shape->GetNumIndexedFields();
	// Cache this so that subsequent queries don't have to iterate the fields again.
	InEmergentType.Shape->NumIndexedFields = NumIndexedFields;
	const uint64 Size = sizeof(VObject) + (NumIndexedFields * sizeof(VRestValue));
	return *new (Context.AllocateFastCell(Size)) VObject(Context, InEmergentType);
}

inline const VValue VObject::LoadField(FAllocationContext Context, const VUniqueString& Name)
{
	const VFields::VEntry* Field = GetEmergentType()->Shape->GetField(Context, Name);
	if (Field == nullptr)
	{
		V_DIE("Field: %hs was not found!", Name.AsCString());
	}
	switch (Field->Type)
	{
		case EFieldType::Offset:
		case EFieldType::Mutable:
			return Data[Field->Index].Get(Context);
		case EFieldType::Constant:
			return Field->Constant.Get().Follow();
		default:
			VERSE_UNREACHABLE();
			break;
	}
}

inline VRestValue& VObject::GetFieldSlot(FAllocationContext Context, VUniqueString& Name)
{
	const VFields::VEntry* Field = GetEmergentType()->Shape->GetField(Context, Name);
	V_DIE_IF(Field == nullptr);
	V_DIE_IF(Field->Type == EFieldType::Constant); // This shouldn't happen since such field's data should be on the shape, not the object.
	return Data[Field->Index];
}

inline void VObject::SetField(FAllocationContext Context, VUniqueString& Name, VValue Value)
{
	const VFields::VEntry* Field = GetEmergentType()->Shape->GetField(Context, Name);
	switch (Field->Type)
	{
		case EFieldType::Mutable:
		case EFieldType::Offset:
			Data[Field->Index].Set(Context, Value);
			break;
		case EFieldType::Constant:
			V_DIE("Attempted to set a value for a non-offset field: %hs!", Name.AsCString());
			break;
		default:
			VERSE_UNREACHABLE();
	}
}

inline uint64 VObject::AllocationSize(const uint64 NumIndexedFields)
{
	return sizeof(VObject) + (NumIndexedFields * sizeof(VRestValue));
}

inline VObject::VObject(FAllocationContext Context, VEmergentType& InEmergentType)
	: VHeapValue(Context, &InEmergentType)
{
	// We only need to allocate space for indexed fields since we are raising constants to the shape
	// and not storing their data on per-object instances.
	const uint64 NumIndexedFields = InEmergentType.Shape->NumIndexedFields; // This should already have been cached.
	for (uint64 Index = 0; Index < NumIndexedFields; ++Index)
	{
		// TODO SOL-4222: Pipe through proper split depth here.
		new (&Data[Index]) VRestValue(0);
	}
}

} // namespace Verse
