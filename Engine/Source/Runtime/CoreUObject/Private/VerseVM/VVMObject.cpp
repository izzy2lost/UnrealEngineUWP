// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMObject.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMVar.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VObject);

template <typename TVisitor>
void VObject::VisitReferencesImpl(TVisitor& Visitor)
{
	const VEmergentType* EmergentType = GetEmergentType();
	VRestValue* Data = GetData(*EmergentType->CppClassInfo);
	uint64 NumIndexedFields = EmergentType->Shape->NumIndexedFields;
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		Visitor.BeginArray(TEXT("Data"), NumIndexedFields);
		Visitor.Visit(Data, Data + NumIndexedFields);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(Data, Data + NumIndexedFields);
	}
}

bool VObject::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!IsStruct())
	{
		return this == Other;
	}

	if (!Other->IsA<VObject>())
	{
		return false;
	}

	if (GetEmergentType()->Type != Other->GetEmergentType()->Type)
	{
		return false;
	}

	if (GetEmergentType()->Shape->Fields.Num() != Other->GetEmergentType()->Shape->Fields.Num())
	{
		return false;
	}

	// TODO: Optimize for when objects share emergent type
	VObject& OtherObject = Other->StaticCast<VObject>();
	for (VShape::FieldsMap::TConstIterator It = GetEmergentType()->Shape->Fields; It; ++It)
	{
		VValue FieldValue = OtherObject.LoadField(Context, *It.Key().Get());
		if (!FieldValue)
		{
			return false;
		}

		if (!VValue::Equal(Context, LoadField(Context, *It.Key().Get()), FieldValue, HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

// TODO: Make this (And all other container TypeHash funcs) handle placeholders appropriately
uint32 VObject::GetTypeHashImpl()
{
	if (!IsStruct())
	{
		return PointerHash(this);
	}

	const VEmergentType* EmergentType = GetEmergentType();
	VRestValue* Data = GetData(*EmergentType->CppClassInfo);

	// Hash nominal type
	uint32 Result = PointerHash(EmergentType->Type.Get());
	for (VShape::FieldsMap::TConstIterator It = EmergentType->Shape->Fields; It; ++It)
	{
		// Hash Field Name
		Result = ::HashCombineFast(Result, GetTypeHash(It.Key()));

		// Hash Value
		if (It.Value().Type == EFieldType::Constant)
		{
			Result = ::HashCombineFast(Result, GetTypeHash(It.Value().Value));
		}
		else
		{
			Result = ::HashCombineFast(Result, GetTypeHash(Data[It.Value().Index]));
		}
	}
	return Result;
}

VValue VObject::MeltImpl(FRunningContext Context)
{
	V_DIE_UNLESS(IsStruct());

	VEmergentType& EmergentType = *GetEmergentType();
	VEmergentType& NewEmergentType = EmergentType.GetOrCreateMeltTransition(Context);

	VObject& NewObject = VObject::NewUninitialized(Context, NewEmergentType);
	NewObject.SetIsStruct();
	if (&EmergentType == &NewEmergentType)
	{
		VRestValue* Data = GetData(*EmergentType.CppClassInfo);
		VRestValue* TargetData = NewObject.GetData(*EmergentType.CppClassInfo);
		uint64 NumIndexedFields = EmergentType.Shape->NumIndexedFields;
		for (uint64 I = 0; I < NumIndexedFields; ++I)
		{
			VValue MeltResult = VValue::Melt(Context, Data[I].Get(Context));
			if (MeltResult.IsPlaceholder())
			{
				return MeltResult;
			}

			TargetData[I].Set(Context, MeltResult);
		}
	}
	else
	{
		for (auto It = EmergentType.Shape->CreateFieldsIterator(); It; ++It)
		{
			VUniqueString& Key = *It->Key.Get();
			VValue MeltResult = VValue::Melt(Context, LoadField(Context, Key));
			if (MeltResult.IsPlaceholder())
			{
				return MeltResult;
			}
			NewObject.SetField(Context, Key, MeltResult);
		}
	}

	return VValue(NewObject);
}

VValue VObject::FreezeImpl(FRunningContext Context)
{
	V_DIE_UNLESS(IsStruct());

	VEmergentType& EmergentType = *GetEmergentType();
	VObject& NewObject = VObject::NewUninitialized(Context, EmergentType);
	NewObject.SetIsStruct();

	// Mutable structs have all fields as indexed fields in the object.
	uint64 NumIndexedFields = EmergentType.Shape->NumIndexedFields;
	V_DIE_UNLESS(NumIndexedFields == EmergentType.Shape->GetNumFields());

	VRestValue* Data = GetData(*EmergentType.CppClassInfo);
	VRestValue* TargetData = NewObject.GetData(*EmergentType.CppClassInfo);
	for (uint64 I = 0; I < NumIndexedFields; ++I)
	{
		TargetData[I].Set(Context, VValue::Freeze(Context, Data[I].Get(Context)));
	}
	return VValue(NewObject);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
