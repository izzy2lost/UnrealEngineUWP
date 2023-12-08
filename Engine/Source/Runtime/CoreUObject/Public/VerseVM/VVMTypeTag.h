// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"

class FString;

#define VERSE_TYPE_TAGS(v) \
	v(Trivial, "trivial")  \
	v(Array, "array")      \
	v(Object, "object")    \
	v(Class, "class")

namespace Verse
{
using FVerseTypeTagInt = uint8;

enum class EVerseTypeTag : FVerseTypeTagInt
{
#define VISIT_OP(Name, ...) Name,
	VERSE_TYPE_TAGS(VISIT_OP)
#undef VISIT_OP
};

COREUOBJECT_API FString DebugName(EVerseTypeTag Tag);
} // namespace Verse
#endif // WITH_VERSE_VM
