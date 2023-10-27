// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCppClassInfo.h"
#include "Containers/UnrealString.h"

namespace Verse
{
FString VCppClassInfo::DebugName() const
{
	return Name;
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)