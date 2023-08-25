// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

inline const VShape::VEntry* VShape::GetField(FAllocationContext Context, VUniqueString& Name) const
{
	if (const VShape::VEntry* Field = Fields.Find({Context, Name}))
	{
		return Field;
	}
	else
	{
		return nullptr;
	}
}

inline uint64 VShape::GetNumFields() const
{
	return Fields.Num();
}

inline bool VShape::operator==(const VShape& Other) const
{
	return Fields.OrderIndependentCompareEqual(Other.Fields);
}

inline uint32 GetTypeHash(const VShape::VEntry& Field)
{
	switch (Field.Type)
	{
		case Verse::EFieldType::Offset:
			return HashCombineFast(::GetTypeHash(static_cast<int8>(Field.Type)), ::GetTypeHash(Field.Index));
		case Verse::EFieldType::Constant:
			return HashCombineFast(::GetTypeHash(static_cast<int8>(Field.Type)), GetTypeHash(Field.Constant.Get()));
		default:
			break;
	}
	VERSE_UNREACHABLE();
}

inline uint32 GetTypeHash(const VShape& Shape)
{
	uint32 Hash = 0;
	for (auto It : Shape.Fields)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(It.Key));
		Hash = HashCombineFast(Hash, GetTypeHash(It.Value));
	}
	return Hash;
}
} // namespace Verse
