// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "VerseVM/Inline/VVMNativeStructInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMShape.h"

namespace Verse
{
inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, FEmergentTypesCacheKeyFuncs::KeyInitType B)
{
	return A == B;
}

inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, const VUniqueStringSet& B)
{
	return *(A.Get()) == B;
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(FEmergentTypesCacheKeyFuncs::KeyInitType Key)
{
	return GetTypeHash(Key);
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(const VUniqueStringSet& Key)
{
	return GetTypeHash(Key);
}

inline VConstructor::VEntry VConstructor::VEntry::Constant(FAllocationContext Context, FUtf8StringView InField, bool bInNative, VPropertyType* InPropertyType, VValue InValue)
{
	return Constant(Context, VUniqueString::New(Context, InField), bInNative, InPropertyType, InValue);
}

inline VConstructor::VEntry VConstructor::VEntry::Constant(FAllocationContext Context, VUniqueString& InField, bool bInNative, VPropertyType* InPropertyType, VValue InValue)
{
	return VEntry{
		{Context,        InField},
		bInNative,
		{Context, InPropertyType},
		{Context,        InValue},
		false
    };
}

inline VFunction* VConstructor::VEntry::Initializer() const
{
	if (bDynamic && Value.Get())
	{
		return &Value.Get().StaticCast<VFunction>();
	}
	else
	{
		return nullptr;
	}
}

inline VConstructor::VEntry VConstructor::VEntry::Field(FAllocationContext Context, VUniqueString& InField, bool bInNative, VPropertyType* InType)
{
	return {
		{Context, InField},
		bInNative,
		{Context, InType},
		{},
		true  // bDynamic
	};
}

inline VConstructor::VEntry VConstructor::VEntry::FieldInitializer(FAllocationContext Context, FUtf8StringView InField, bool bInNative, VPropertyType* InPropertyType, VProcedure& InCode)
{
	return FieldInitializer(Context, VUniqueString::New(Context, InField), bInNative, InPropertyType, InCode);
}

inline VConstructor::VEntry VConstructor::VEntry::FieldInitializer(FAllocationContext Context, VUniqueString& InField, bool bInNative, VPropertyType* InPropertyType, VProcedure& InCode)
{
	return {
		{Context,        InField},
		bInNative,
		{Context, InPropertyType},
		{Context,         InCode},
		true
    };
}

inline VConstructor::VEntry VConstructor::VEntry::Block(FAllocationContext Context, VProcedure& Code)
{
	return {
		{},
		false,
		{},
		{Context, Code},
		true
    };
}

inline UScriptStruct::ICppStructOps& VClass::GetCppStructOps() const
{
	return *CastChecked<UScriptStruct>(AssociatedUStruct.Get().AsUObject())->GetCppStructOps();
}

template <class CppStructType>
inline VNativeStruct& VClass::NewNativeStruct(FAllocationContext Context, CppStructType&& Struct)
{
	V_DIE_UNLESS(IsNativeStruct());

	// Get or create the singleton emergent type for this native struct
	VEmergentType& NewEmergentType = GetOrCreateEmergentTypeForNativeStruct(Context);
	return VNativeStruct::New(Context, NewEmergentType, Forward<CppStructType>(Struct));
}

inline VConstructor& VClass::GetConstructor() const
{
	return *Constructor.Get();
}

inline VClass& VClass::New(FAllocationContext Context, VPackage* Scope, VArray* Name, VArray* UEMangledName, UClass* ImportClass, bool bNative, EKind Kind, const TArray<VClass*>& Inherited, VConstructor& Constructor)
{
	const size_t NumBytes = offsetof(VClass, Inherited) + Inherited.Num() * sizeof(Inherited[0]);
	return *new (Context.AllocateFastCell(NumBytes)) VClass(Context, Scope, Name, UEMangledName, ImportClass, bNative, Kind, Inherited, Constructor);
}

} // namespace Verse
#endif // WITH_VERSE_VM
