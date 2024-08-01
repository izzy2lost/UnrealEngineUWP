// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "UObject/UnrealType.h" // For FProperty
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{

inline VValue VObject::LoadField(FAllocationContext Context, const VCppClassInfo& CppClassInfo, const VShape::VEntry* Field)
{
	V_DIE_IF(Field == nullptr);

	switch (Field->Type)
	{
		case EFieldType::Offset:
			return GetFieldData(CppClassInfo)[Field->Index].Get(Context);
		case EFieldType::FProperty:
			check(Field->UProperty->IsA<FVRestValueProperty>());
			return Field->UProperty->ContainerPtrToValuePtr<VRestValue>(GetData(CppClassInfo))->Get(Context);
		case EFieldType::Constant:
		{
			VValue FieldValue = Field->Value.Get();
			V_DIE_IF(FieldValue.IsCellOfType<VProcedure>());
			if (VFunction* Function = FieldValue.DynamicCast<VFunction>(); Function && !Function->HasSelf())
			{
				// NOTE: (yiliang.siew) Update the function-without-`Self` to point to the current object instance.
				// We only do this if the function doesn't already have a `Self` bound - in the case of fields that
				// are pointing to functions, we don't want to overwrite that `Self` which was already previously-bound.
				return Function->Bind(Context, *this);
			}
			else if (VNativeFunction* NativeFunction = FieldValue.DynamicCast<VNativeFunction>(); NativeFunction && !NativeFunction->HasSelf())
			{
				return NativeFunction->Bind(Context, *this);
			}
			else
			{
				return FieldValue;
			}
		}
		default:
			VERSE_UNREACHABLE();
			break;
	}
}

inline VValue VObject::LoadField(FAllocationContext Context, const VUniqueString& Name)
{
	const VEmergentType* EmergentType = GetEmergentType();
	return LoadField(Context, *EmergentType->CppClassInfo, EmergentType->Shape->GetField(Name));
}

inline void VObject::SetField(FAllocationContext Context, const VShape& Shape, const VUniqueString& Name, void* Data, VValue Value)
{
	const VShape::VEntry* Field = Shape.GetField(Name);
	V_DIE_IF(Field == nullptr);
	switch (Field->Type)
	{
		case EFieldType::Offset:
			return BitCast<VRestValue*>(Data)[Field->Index].Set(Context, Value);
		case EFieldType::FProperty:
			check(Field->UProperty->IsA<FVRestValueProperty>());
			return Field->UProperty->ContainerPtrToValuePtr<VRestValue>(Data)->Set(Context, Value);
		case EFieldType::Constant:
		default:
			VERSE_UNREACHABLE(); // This shouldn't happen since such field's data should be on the shape, not the object.
			break;
	}
}

inline void VObject::SetField(FAllocationContext Context, const VUniqueString& Name, VValue Value)
{
	const VEmergentType* EmergentType = GetEmergentType();
	SetField(Context, *EmergentType->Shape, Name, GetData(*EmergentType->CppClassInfo), Value);
}

inline VObject::VObject(FAllocationContext Context, VEmergentType& InEmergentType)
	: VHeapValue(Context, &InEmergentType)
{
	// Leave initialization of the data to the subclasses
}

FORCEINLINE size_t VObject::DataOffset(const VCppClassInfo& CppClassInfo)
{
	return Align(CppClassInfo.SizeWithoutFields, DataAlignment);
}

FORCEINLINE void* VObject::GetData(const VCppClassInfo& CppClassInfo)
{
	return BitCast<uint8*>(this) + DataOffset(CppClassInfo);
}

FORCEINLINE VRestValue* VObject::GetFieldData(const VCppClassInfo& CppClassInfo)
{
	return BitCast<VRestValue*>(GetData(CppClassInfo));
}

} // namespace Verse
#endif // WITH_VERSE_VM
