// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace UE::AnimNext::UncookedOnly
{
	class FModule : public IModuleInterface
	{
	private:
		virtual void StartupModule() override
		{
		}

		virtual void ShutdownModule() override
		{
		}
	};
}

IMPLEMENT_MODULE(UE::AnimNext::UncookedOnly::FModule, AnimNextAnimGraphUncookedOnly);
