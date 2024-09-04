// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2BlueprintPrivate.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogPixelStreaming2Blueprint);

class FPixelStreaming2BlueprintModule : public IModuleInterface
{
public:
private:
	/** IModuleInterface implementation */
	void StartupModule() override {}
	void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FPixelStreaming2BlueprintModule, PixelStreaming2Blueprint)
