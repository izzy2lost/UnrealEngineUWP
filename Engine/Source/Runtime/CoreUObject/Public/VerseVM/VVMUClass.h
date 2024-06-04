// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/Casts.h"
#include "UObject/Class.h"
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMShape.h"
#endif
#include "VVMUClass.generated.h"

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
namespace Verse
{
struct VClass;
}
#endif

// Class used for all VerseVM generated classes
UCLASS(within = Package, Config = Engine)
class COREUOBJECT_API UVerseVMClass : public UClass
{
	GENERATED_BODY()

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
public:
	static Verse::VValue LoadField(Verse::FAllocationContext Context, UObject* Object, Verse::VUniqueString& FieldName);
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	//~ Begin UObject interface
	virtual bool IsAsset() const override { return true; }
	//~ End UObject interface

	Verse::TWriteBarrier<Verse::VShape> Shape;
	Verse::TWriteBarrier<Verse::VClass> Class;
#endif
};

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
namespace Verse
{
inline UClass* VClass::GetOrCreateUClass(FAllocationContext Context)
{
	return AssociatedUClass ? CastChecked<UClass>(AssociatedUClass.Get().AsUObject()) : CreateUClass(Context);
}
} // namespace Verse
#endif
