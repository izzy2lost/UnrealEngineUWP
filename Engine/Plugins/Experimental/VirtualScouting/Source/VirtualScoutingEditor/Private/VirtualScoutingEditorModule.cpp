// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualScoutingEditorModule.h"
#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY(LogVirtualScoutingEditor);


class FVirtualScoutingEditorModule : public IModuleInterface
{
private:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};


IMPLEMENT_MODULE(FVirtualScoutingEditorModule, VirtualScoutingEditor);
