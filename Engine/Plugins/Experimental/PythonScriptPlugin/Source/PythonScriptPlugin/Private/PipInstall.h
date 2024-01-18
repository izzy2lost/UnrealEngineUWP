// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Interfaces/IPluginManager.h"
#include "Templates/SharedPointer.h"

#if WITH_PYTHON

class FJsonObject;
class FFeedbackContext;

class FPipInstall
{
public:
	static const FString PipInstallUtilsVer;
	static const FString PluginsListingFilename;
	static const FString PluginsSitePackageFilename;
	static const FString RequirementsInputFilename;
	static const FString ExtraUrlsFilename;
	static const FString ParsedRequirementsFilename;

	static bool EnabledOnStartup();

	static void CheckInvalidPipEnv();

	static FString WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins);
	static FString WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls);

	static void SetupPipEnv(FFeedbackContext* Context, bool bForceRebuild = false);
	static FString ParsePluginDependencies(const FString& MergedInRequirementsFile, FFeedbackContext* Context);

	static bool HasInstallLines(const TArray<FString>& RequirementLines);

	static FString GetPipInstallPath();

private:
	static void SetupPipInstallUtils(const FString& VenvInterp, FFeedbackContext* Context);
	static bool CheckPipInstallUtils(const FString& VenvInterp, FFeedbackContext* Context);
	static int32 RunPythonCmd(const FText& Description, const FString& VenvInterp, const FString& Cmd, FFeedbackContext* Context);
	static bool RunLoggedSubprocess(const FText& Description, const FString& URL, const FString& Params, FFeedbackContext* Context, int32* OutExitCode);\

	static FString GetPythonScriptPluginPath();
	static FString ParseVenvVersion(const FString& InstallPath);
	static FString GetVenvInterpreter(const FString& InstallPath);
	static bool CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName);
};

#endif //WITH_PYTHON
