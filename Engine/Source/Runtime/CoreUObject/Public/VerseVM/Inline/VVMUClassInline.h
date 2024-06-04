// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "UObject/VerseValueProperty.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMUClass.h"

FORCEINLINE_DEBUGGABLE Verse::VValue UVerseVMClass::LoadField(Verse::FAllocationContext Context, UObject* Object, Verse::VUniqueString& FieldName)
{
	using namespace Verse;

	const UVerseVMClass* Class = CastChecked<UVerseVMClass>(Object->GetClass());
	const VShape::VEntry* Field = Class->Shape->GetField(FieldName);

	switch (Field->Type)
	{
		case EFieldType::FProperty:
		{
			FVRestValueProperty* FieldProperty = CastFieldChecked<FVRestValueProperty>(Field->UProperty);
			return FieldProperty->ContainerPtrToValuePtr<Verse::VRestValue>(Object)->Get(Context);
		}
		case EFieldType::Constant:
		{
			VValue FieldValue = Field->Value.Get();
			if (FieldValue.IsCellOfType<VProcedure>())
			{
				return VFunction::New(Context, FieldValue.StaticCast<VProcedure>(), Object);
			}
			else if (FieldValue.IsCellOfType<VNativeFunction>())
			{
				return FieldValue.StaticCast<VNativeFunction>().Bind(Context, Object);
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

#endif // WITH_VERSE_VM
