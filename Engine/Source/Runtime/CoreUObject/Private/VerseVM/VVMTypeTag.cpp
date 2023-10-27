// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTypeTag.h"

#include "Containers/UnrealString.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{
FString DebugName(EVerseTypeTag Tag)
{
	switch (Tag)
	{
#define VISIT_OP(Tag, Desc, ...) \
	case EVerseTypeTag::Tag:     \
		return TEXT(Desc);
		VERSE_TYPE_TAGS(VISIT_OP)
#undef VISIT_OP
		default:
			break;
	}
	VERSE_UNREACHABLE();
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)