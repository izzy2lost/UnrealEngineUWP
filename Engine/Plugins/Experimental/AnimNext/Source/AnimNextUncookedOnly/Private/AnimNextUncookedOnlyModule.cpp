// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace UE::AnimNext::UncookedOnly
{
	class FModule : public IModuleInterface
	{
	public:

	private:
		virtual void StartupModule() override;
	};

	void FModule::StartupModule()
	{
	}
}

IMPLEMENT_MODULE(UE::AnimNext::UncookedOnly::FModule, AnimNextUncookedOnly);
