// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMShape.h"

namespace Verse
{
struct VClass;
}

// Class used for all VerseVM generated classes
class UVerseVMClass : public UClass
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UVerseVMClass, UClass, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UVerseVMClass, COREUOBJECT_API)
	DECLARE_WITHIN_UPACKAGE()

public:
	COREUOBJECT_API UVerseVMClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FVRestValueProperty* GetPropertyForField(Verse::FAllocationContext Context, Verse::VUniqueString& FieldName) const;

	Verse::TWriteBarrier<Verse::VShape> Shape;
	Verse::TWriteBarrier<Verse::VClass> Class;
};

namespace Verse
{
inline UClass* VClass::GetOrCreateUClass(FAllocationContext Context)
{
	return AssociatedUClass ? CastChecked<UClass>(AssociatedUClass.Get().AsUObject()) : CreateUClass(Context);
}
} // namespace Verse
#endif
