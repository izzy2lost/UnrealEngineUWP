// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentType.h"

namespace Verse
{

inline UScriptStruct::ICppStructOps& VEmergentType::GetCppStructOps() const
{
	return Type->StaticCast<VClass>().GetCppStructOps();
}

} // namespace Verse
#endif // WITH_VERSE_VM
