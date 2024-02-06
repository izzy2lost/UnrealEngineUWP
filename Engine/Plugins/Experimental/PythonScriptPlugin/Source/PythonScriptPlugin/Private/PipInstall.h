// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Interfaces/IPluginManager.h"
#include "Templates/SharedPointer.h"

#if WITH_PYTHON

class FJsonObject;
class FFeedbackContext;

class IProgressParser;

class FPipInstall
{
public:
	static FPipInstall& Get();

	bool IsEnabled();
	bool IsCmdLineDisabled();

	void CheckRemoveOrphanedPackages(const FString& SitePackagesPath);
	void CheckInvalidPipEnv();

	FString WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins);
	FString WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls);

	void SetupPipEnv(FFeedbackContext* Context, bool bForceRebuild = false);
	void RemoveParsedDependencyFiles();
	FString ParsePluginDependencies(const FString& MergedInRequirementsFile, FFeedbackContext* Context);
	bool RunPipInstall(FFeedbackContext* Context, bool bOfflineOnly = false, const FString& ForceIndexUrl = TEXT(""));

	int NumPackagesToInstall();

	FString GetPipInstallPath();
	FString GetPipSitePackagesPath();

private:
	FPipInstall();

	void WriteSitePackagePthFile();
	void SetupPipInstallUtils(FFeedbackContext* Context);
	bool CheckPipInstallUtils(FFeedbackContext* Context);
	static int32 RunPythonCmd(const FText& Description, const FString& PythonInterp, const FString& Cmd, FFeedbackContext* Context, TSharedPtr<IProgressParser> CmdParser = nullptr);
	static bool RunLoggedSubprocess(int32* OutExitCode, const FText& Description, const FString& URL, const FString& Params, FFeedbackContext* Context, TSharedPtr<IProgressParser> CmdParser);

	FString ParseVenvVersion();

	static FString GetPythonScriptPluginPath();
	static FString GetVenvInterpreter(const FString& InstallPath);
	static bool CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName);

	int CountInstallLines(const TArray<FString>& RequirementLines);

	bool bRunOnStartup;
	bool bCmdLineDisable;

	FString PipInstallPath;
	FString VenvInterp;

	static const FString PipInstallUtilsVer;
	static const FString PluginsListingFilename;
	static const FString PluginsSitePackageFilename;
	static const FString RequirementsInputFilename;
	static const FString ExtraUrlsFilename;
	static const FString ParsedRequirementsFilename;
};

#endif //WITH_PYTHON
