// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMEmergentTypeInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/VVMNativeStruct.h"

namespace Verse
{

template <class CppStructType>
FORCEINLINE CppStructType& VNativeStruct::GetStruct(const VCppClassInfo& CppClassInfo)
{
	checkSlow(sizeof(CppStructType) == GetEmergentType()->GetCppStructOps().GetSize());

	return *BitCast<CppStructType*>(VObject::GetData(CppClassInfo));
}

template <class CppStructType>
FORCEINLINE CppStructType& VNativeStruct::GetStruct()
{
	return GetStruct<CppStructType>(*GetEmergentType()->CppClassInfo);
}

template <class CppStructType>
inline VNativeStruct& VNativeStruct::New(FAllocationContext Context, VEmergentType& InEmergentType, CppStructType&& InStruct)
{
	return *new (AllocateCell(Context, InEmergentType)) VNativeStruct(Context, InEmergentType, Forward<CppStructType>(InStruct));
}

inline VNativeStruct& VNativeStruct::NewUninitialized(FAllocationContext Context, VEmergentType& InEmergentType, bool bRunCppConstructor)
{
	return *new (AllocateCell(Context, InEmergentType)) VNativeStruct(Context, InEmergentType, bRunCppConstructor);
}

inline std::byte* VNativeStruct::AllocateCell(FAllocationContext Context, VEmergentType& InEmergentType)
{
	UScriptStruct::ICppStructOps& CppStructOps = InEmergentType.GetCppStructOps();
	const size_t ByteSize = DataOffset(*InEmergentType.CppClassInfo) + CppStructOps.GetSize();
	const bool bHasDestructor = CppStructOps.HasDestructor();
	return bHasDestructor ? Context.Allocate(FHeap::DestructorSpace, ByteSize) : Context.AllocateFastCell(ByteSize);
}

template <class CppStructType>
inline VNativeStruct::VNativeStruct(FAllocationContext Context, VEmergentType& InEmergentType, CppStructType&& InStruct)
	: VObject(Context, InEmergentType)
{
	using StructType = typename TDecay<CppStructType>::Type;
	checkSlow(sizeof(StructType) == InEmergentType.GetCppStructOps().GetSize());

	SetIsStruct();
	void* Data = GetData(*InEmergentType.CppClassInfo);
	new (Data) StructType(Forward<CppStructType>(InStruct));
}

inline VNativeStruct::VNativeStruct(FAllocationContext Context, VEmergentType& InEmergentType, bool bRunCppConstructor)
	: VObject(Context, InEmergentType)
{
	SetIsStruct();
	if (bRunCppConstructor)
	{
		UScriptStruct::ICppStructOps& CppStructOps = InEmergentType.GetCppStructOps();
		void* Data = GetData(*InEmergentType.CppClassInfo);
		if (CppStructOps.HasZeroConstructor())
		{
			memset(Data, 0, CppStructOps.GetSize());
		}
		else
		{
			CppStructOps.Construct(Data);
		}
	}
}

inline VNativeStruct::~VNativeStruct()
{
	const VEmergentType* EmergentType = GetEmergentType();
	UScriptStruct::ICppStructOps& CppStructOps = EmergentType->GetCppStructOps();
	if (CppStructOps.HasDestructor())
	{
		CppStructOps.Destruct(VObject::GetData(*EmergentType->CppClassInfo));
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
