// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNextRuntimeTest.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::AnimNext
{
	FScopedClearNodeTemplateRegistry::FScopedClearNodeTemplateRegistry()
	{
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();
		Swap(Registry, TmpRegistry);
	}

	FScopedClearNodeTemplateRegistry::~FScopedClearNodeTemplateRegistry()
	{
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();
		Swap(Registry, TmpRegistry);
	}
}
#endif
