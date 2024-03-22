// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{

inline VObject& VObject::NewUninitialized(FAllocationContext Context, VEmergentType& InEmergentType)
{
	return *new (AllocateFastCell(Context, InEmergentType)) VObject(Context, InEmergentType);
}

inline const VValue VObject::LoadField(FAllocationContext Context, VUniqueString& Name)
{
	const VEmergentType* EmergentType = GetEmergentType();
	const VShape::VEntry* Field = EmergentType->Shape->GetField(Context, Name);
	if (Field == nullptr)
	{
		return VValue();
	}
	switch (Field->Type)
	{
		case EFieldType::Offset:
			return GetData(*EmergentType->CppClassInfo)[Field->Index].Get(Context);
		case EFieldType::Constant:
			return Field->Value.Get().Follow();
		default:
			VERSE_UNREACHABLE();
			break;
	}
}

inline VRestValue& VObject::GetFieldSlot(FAllocationContext Context, VUniqueString& Name)
{
	const VEmergentType* EmergentType = GetEmergentType();
	const VShape::VEntry* Field = EmergentType->Shape->GetField(Context, Name);
	V_DIE_IF(Field == nullptr);
	V_DIE_IF(Field->Type == EFieldType::Constant); // This shouldn't happen since such field's data should be on the shape, not the object.
	return GetData(*EmergentType->CppClassInfo)[Field->Index];
}

inline void VObject::SetField(FAllocationContext Context, VUniqueString& Name, VValue Value)
{
	const VEmergentType* EmergentType = GetEmergentType();
	const VShape::VEntry* Field = EmergentType->Shape->GetField(Context, Name);
	switch (Field->Type)
	{
		case EFieldType::Offset:
			GetData(*EmergentType->CppClassInfo)[Field->Index].Set(Context, Value);
			break;
		case EFieldType::Constant:
			V_DIE("Attempted to set a value for a non-offset field: %hs!", Name.AsCString());
			break;
		default:
			VERSE_UNREACHABLE();
	}
}

inline VObject::VObject(FAllocationContext Context, VEmergentType& InEmergentType)
	: VHeapValue(Context, &InEmergentType)
{
	// We only need to allocate space for indexed fields since we are raising constants to the shape
	// and not storing their data on per-object instances.
	VRestValue* Data = GetData(*InEmergentType.CppClassInfo);
	const uint64 NumIndexedFields = InEmergentType.Shape->NumIndexedFields;
	for (uint64 Index = 0; Index < NumIndexedFields; ++Index)
	{
		// TODO SOL-4222: Pipe through proper split depth here.
		new (&Data[Index]) VRestValue(0);
	}
}

inline size_t VObject::FieldsOffset(const VCppClassInfo& CppClassInfo)
{
	return Align(CppClassInfo.SizeWithoutFields, alignof(VRestValue));
}

inline std::byte* VObject::AllocateFastCell(FAllocationContext Context, VEmergentType& InEmergentType)
{
	const uint64 NumIndexedFields = InEmergentType.Shape->NumIndexedFields;
	return Context.AllocateFastCell(FieldsOffset(*InEmergentType.CppClassInfo) + NumIndexedFields * sizeof(VRestValue));
}

inline VRestValue* VObject::GetData(const VCppClassInfo& CppClassInfo)
{
	return BitCast<VRestValue*>(BitCast<uint8*>(this) + FieldsOffset(CppClassInfo));
}

} // namespace Verse
#endif // WITH_VERSE_VM
