// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

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
