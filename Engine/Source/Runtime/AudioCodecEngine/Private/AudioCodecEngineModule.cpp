// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IAudioCodecRegistry.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FAudioCodecEngineModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
	}
	void ShutdownModule() override
	{
	}
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

IMPLEMENT_MODULE(FAudioCodecEngineModule, AudioCodecEngine);
