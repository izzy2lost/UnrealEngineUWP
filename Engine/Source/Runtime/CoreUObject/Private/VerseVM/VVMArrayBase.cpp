// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArrayBase.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VArrayBase);
TGlobalTrivialEmergentTypePtr<&VArrayBase::StaticCppClassInfo> VArrayBase::GlobalTrivialEmergentType;

bool VArrayBase::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VArrayBase>())
	{
		return false;
	}

	VArrayBase& OtherArray = Other->StaticCast<VArrayBase>();
	if (Num() != OtherArray.Num())
	{
		return false;
	}
	for (uint32 Index = 0, End = Num(); Index < End; ++Index)
	{
		if (!VValue::Equal(Context, GetValue(Index), OtherArray.GetValue(Index), HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

uint32 VArrayBase::GetTypeHashImpl()
{
	const TWriteBarrier<VValue>* Ptr = GetData();
	const uint32 Size = Num();
	return ::GetArrayHash(Ptr, Size);
}

void VArrayBase::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Builder.Append(TEXT("Array("));
	for (int I = 0; I < Num(); ++I)
	{
		if (I > 0)
		{
			Builder.Append(TEXT(", "));
		}
		GetValue(I).ToString(Builder, Context, Formatter);
	}
	Builder.Append(TEXT(")"));
}

VArrayBase::FConstIterator VArrayBase::begin() const
{
	return GetData();
}

VArrayBase::FConstIterator VArrayBase::end() const
{
	return GetData() + NumValues;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
