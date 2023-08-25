// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

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
	const uint64 Size = sizeof(VObject) + (InEmergentType.Shape->GetNumFields() * sizeof(VRestValue));
	return *new (Context.AllocateFastCell(Size)) VObject(Context, InEmergentType);
}

inline const VValue VObject::LoadField(FAllocationContext Context, VUniqueString& Name)
{
	const VShape::VEntry* Field = GetEmergentType()->Shape->GetField(Context, Name);
	V_DIE_IF(Field == nullptr);
	switch (Field->Type)
	{
		case EFieldType::Offset:
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
	const VShape::VEntry* Field = GetEmergentType()->Shape->GetField(Context, Name);
	V_DIE_IF(Field == nullptr);
	V_DIE_UNLESS(Field->Type == EFieldType::Offset);
	return Data[Field->Index];
}

inline void VObject::SetField(FAllocationContext Context, VUniqueString& Name, VValue Value)
{
	const VShape::VEntry* Field = GetEmergentType()->Shape->GetField(Context, Name);
	if (Field->Type == EFieldType::Offset)
	{
		Data[Field->Index].Set(Context, Value);
	}
	else
	{
		V_DIE("Attempted to set a value for a non-offset field: %hs!", Name.AsCString());
	}
}

inline VObject::VObject(FAllocationContext Context, VEmergentType& InEmergentType)
	: VHeapValue(Context, &InEmergentType)
{
	const uint64 NumFields = InEmergentType.Shape->GetNumFields();
	for (uint64 Index = 0; Index < NumFields; ++Index)
	{
		// TODO SOL-4222: Pipe through proper split depth here.
		new (&Data[Index]) VRestValue(0);
	}
}
} // namespace Verse
