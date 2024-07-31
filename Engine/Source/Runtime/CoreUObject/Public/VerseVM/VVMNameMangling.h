// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

namespace Verse
{

// The following method are intended to take a case sensitive name (which maybe already adorned with package information)
// and convert it into a case insensitive name.

COREUOBJECT_API bool MangleCasedName(const FString& Name, FString& MangledName);
COREUOBJECT_API bool MangleCasedNameCheck(const FString& Name, FName& MangledName);
COREUOBJECT_API FString UnmangleCasedName(const FName MaybeMangledName, bool* bOutNameWasMangled = nullptr);

} // namespace Verse