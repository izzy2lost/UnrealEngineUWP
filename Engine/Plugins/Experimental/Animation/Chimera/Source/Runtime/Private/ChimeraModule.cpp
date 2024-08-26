// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FChimeraRuntimeModule"

namespace UE::Chimera
{

class FChimeraRuntimeModule : public IModuleInterface
{
public:
	//~IModuleInterface
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
	//~End of IModuleInterface
};

} // namespace UE::Chimera

IMPLEMENT_MODULE(UE::Chimera::FChimeraRuntimeModule, Chimera)

#undef LOCTEXT_NAMESPACE

