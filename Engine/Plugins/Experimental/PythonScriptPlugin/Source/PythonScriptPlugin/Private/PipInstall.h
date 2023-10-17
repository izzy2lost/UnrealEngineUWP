// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Interfaces/IPluginManager.h"
#include "Templates/SharedPointer.h"

#if WITH_PYTHON

class FJsonObject;

class FPipInstall
{
public:
	static FString WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins);
    static FString WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls);

private:
	static bool CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName);
};

#endif //WITH_PYTHON
