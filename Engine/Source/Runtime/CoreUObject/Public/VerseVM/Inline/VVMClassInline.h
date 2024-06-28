// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "VerseVM/Inline/VVMNativeStructInline.h"
#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMTypeCreator.h"

namespace Verse
{
struct VShape;

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
	return VNativeStruct::New(Context, NewEmergentType, MoveTemp(Struct));
}

inline VClass& VClass::New(FAllocationContext Context, VPackage* Scope, VArray* Name, VArray* UEMangledName, EKind Kind, bool bNative, const TArray<VClass*>& Inherited, VConstructor& Constructor, UClass* ImportClass)
{
	const size_t NumBytes = offsetof(VClass, Inherited) + Inherited.Num() * sizeof(Inherited[0]);
	return *new (Context.AllocateFastCell(NumBytes)) VClass(Context, Scope, Name, UEMangledName, Kind, bNative, Inherited, Constructor, ImportClass);
}

inline VClass::VClass(FAllocationContext Context, VPackage* InScope, VArray* InName, VArray* InUEMangledName, EKind InKind, bool bInNative, const TArray<VClass*>& InInherited, VConstructor& InConstructor, UClass* InImportClass)
	: VType(Context, &GlobalTrivialEmergentType.Get(Context))
	, ClassName(Context, InName)
	, UEMangledName(Context, InUEMangledName)
	, Scope(Context, InScope)
	, Kind(InKind)
	, bNative(bInNative)
	, NumInherited(InInherited.Num())
{
	if (InImportClass != nullptr)
	{
		AssociatedUStruct.Set(Context, InImportClass);
	}

	if (InInherited.IsEmpty())
	{
		Constructor.Set(Context, InConstructor);
	}
	else
	{
		// Elements of this class override later superclasses, which override earlier superclasses.
		TSet<VUniqueString*> Fields;
		TArray<VConstructor::VEntry> Entries;
		Entries.Reserve(InConstructor.NumEntries);
		Extend(Fields, Entries, InConstructor);
		for (int32 Index = 0; Index < InInherited.Num(); ++Index)
		{
			V_DIE_IF(Index != 0 && InInherited[Index]->Kind == EKind::Class);
			Extend(Fields, Entries, *InInherited[Index]->Constructor.Get());
		}
		Constructor.Set(Context, VConstructor::New(Context, Entries));
	}

	for (uint32 Index = 0; Index < NumInherited; ++Index)
	{
		new (&Inherited[Index]) TWriteBarrier<VClass>(Context, InInherited[Index]);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
