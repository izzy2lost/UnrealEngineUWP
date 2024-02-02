// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArray.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VTypeArray)
DEFINE_TRIVIAL_VISIT_REFERENCES(VTypeArray);
TGlobalTrivialEmergentTypePtr<&VTypeArray::StaticCppClassInfo> VTypeArray::GlobalTrivialEmergentType;

DEFINE_DERIVED_VCPPCLASSINFO(VArray);
DEFINE_TRIVIAL_VISIT_REFERENCES(VArray);

VArray& VArray::Concat(FRunningContext Context, VArrayBase& Lhs, VArrayBase& Rhs)
{
	VArray& NewArray = VArray::New(Context, Lhs.Num() + Rhs.Num(), DetermineCombinedType(Lhs.GetArrayType(), Rhs.GetArrayType()));
	if (NewArray.GetArrayType() != EArrayType::VValue)
	{
		FMemory::Memcpy(NewArray.GetData(), Lhs.GetData(), Lhs.ByteLength());
		FMemory::Memcpy(NewArray.GetData<int32>() + Lhs.Num(), Rhs.GetData(), Rhs.ByteLength());
		return NewArray;
	}

	uint32 Index = 0;
	for (int I = 0; I < Lhs.Num(); ++I)
	{
		NewArray.SetValue(Context, Index++, Lhs.GetValue(I));
	}
	for (int J = 0; J < Rhs.Num(); ++J)
	{
		NewArray.SetValue(Context, Index++, Rhs.GetValue(J));
	}
	return NewArray;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
