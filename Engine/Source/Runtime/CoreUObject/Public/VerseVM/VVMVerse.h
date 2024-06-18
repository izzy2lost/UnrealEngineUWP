// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "CoreMinimal.h"

namespace Verse
{
class IEngineEnvironment;

class VerseVM
{
public:
	COREUOBJECT_API static void Startup();
	COREUOBJECT_API static void Shutdown();
	COREUOBJECT_API static IEngineEnvironment* GetEngineEnvironment();
	COREUOBJECT_API static void SetEngineEnvironment(IEngineEnvironment* Environment);
};
} // namespace Verse
#endif // WITH_VERSE_VM
