// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMUClass.h"
#include "UObject/Package.h"
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMClass.h"
#endif

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void UVerseVMClass::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	UVerseVMClass* This = static_cast<UVerseVMClass*>(InThis);
	Collector.AddReferencedVerseValue(This->Shape);
	Collector.AddReferencedVerseValue(This->Class);
}
#endif