// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMap.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMap);
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

void VMap::SerializeImpl(VMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint64 ScratchNumValues = 0;
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		This = &VMap::New(Context, (uint32)ScratchNumValues);
		for (uint32 Index = (uint32)ScratchNumValues; Index != 0; --Index)
		{
			VValue Key, Value;
			Visitor.BeginObject();
			Visitor.Visit(Key, TEXT("Key"));
			Visitor.Visit(Value, TEXT("Value"));
			Visitor.EndObject();
			This->Add(Context, Key, Value);
		}
		Visitor.EndArray();
	}
	else
	{
		uint64 ScratchNumValues = This->Num();
		Visitor.BeginMap(TEXT("Values"), ScratchNumValues);
		for (TTuple<VValue, VValue> Kvp : *This)
		{
			Visitor.BeginObject();
			Visitor.Visit(Kvp.Key, TEXT("Key"));
			Visitor.Visit(Kvp.Value, TEXT("Value"));
			Visitor.EndObject();
		}
		Visitor.EndMap();
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
