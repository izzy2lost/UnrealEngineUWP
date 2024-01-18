// Copyright Epic Games, Inc. All Rights Reserved.

#include "PipInstall.h"

#include "PyUtil.h"
#include "PythonScriptPluginSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FeedbackContextMarkup.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "PipInstall"

#if WITH_PYTHON

// In order to keep editor startup time fast, check directly for this utils version (make sure to match with wheel version in PythonScriptPlugin/Content/Python/Lib/wheels)
// NOTE: This version must also be changed in PipInstallMode.cs in order to support UBT functionality
const FString FPipInstall::PipInstallUtilsVer = TEXT("0.1.4");

const FString FPipInstall::PluginsListingFilename = TEXT("pyreqs_plugins.list");
const FString FPipInstall::PluginsSitePackageFilename = TEXT("plugin_site_package.pth");
const FString FPipInstall::RequirementsInputFilename = TEXT("merged_requirements.in");
const FString FPipInstall::ExtraUrlsFilename = TEXT("extra_urls.txt");
const FString FPipInstall::ParsedRequirementsFilename = TEXT("merged_requirements.txt");


bool FPipInstall::EnabledOnStartup()
{
	bool bRunOnStartup = GetDefault<UPythonScriptPluginSettings>()->bRunPipInstallOnStartup;
	bool bCmdLineDisable = FParse::Param(FCommandLine::Get(), TEXT("DisablePipInstall"));

	return bRunOnStartup && !bCmdLineDisable;
}

FString FPipInstall::WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::WritePluginsListing);

	const FString PipInstallPath = GetPipInstallPath();

	OutPythonPlugins.Empty();

	// List of plugins with pip dependencies
	TArray<FString> PipPluginPaths;
	// List of enabled plugins' site-packages folders
	TArray<FString> PluginSitePackagePaths;
	for ( const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins() )
	{
		const FString PythonContentPath = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir() / TEXT("Python"));
		const FString PluginPlatformSitePackagesPath = PythonContentPath / TEXT("Lib") / FPlatformMisc::GetUBTPlatform() / TEXT("site-packages");
		const FString PluginGeneralSitePackagesPath = PythonContentPath / TEXT("Lib") / TEXT("site-packages");

		// Write platform/general site-packages paths per-plugin to .pth file to account for packaged python dependencies during pip install
		if (FPaths::DirectoryExists(PluginPlatformSitePackagesPath))
		{
			PluginSitePackagePaths.Add(PluginPlatformSitePackagesPath);
		}

		if (FPaths::DirectoryExists(PluginGeneralSitePackagesPath))
		{
			PluginSitePackagePaths.Add(PluginGeneralSitePackagesPath);
		}

		const FPluginDescriptor& PluginDesc = Plugin->GetDescriptor();
		if (PluginDesc.CachedJson->HasTypedField(TEXT("PythonRequirements"), EJson::Array))
		{
			const FString PluginDescFile = FPaths::ConvertRelativePathToFull(Plugin->GetDescriptorFileName());
			PipPluginPaths.Add(PluginDescFile);
			OutPythonPlugins.Add(Plugin);
		}
	}

	// Create list of plugins that may require pip install dependencies
	const FString PyPluginsListingFile = PipInstallPath / PluginsListingFilename;
	FFileHelper::SaveStringArrayToFile(PipPluginPaths, *PyPluginsListingFile);

	// Create .pth file in PipInstall/Lib/site-packages to account for plugins with packaged dependencies
	const FString PyPluginsSitePackageFile = PipInstallPath / TEXT("Lib") / TEXT("site-packages") / PluginsSitePackageFilename;
	FFileHelper::SaveStringArrayToFile(PluginSitePackagePaths, *PyPluginsSitePackageFile);

    return PyPluginsListingFile;
}

FString FPipInstall::WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::WritePluginDependencies);

	const FString PipInstallPath = GetPipInstallPath();

	OutRequirements.Empty();
	OutExtraUrls.Empty();

	for (const TSharedRef<IPlugin>& Plugin : PythonPlugins)
	{
		const FPluginDescriptor& PluginDesc = Plugin->GetDescriptor();
		for (const TSharedPtr<FJsonValue>& JsonVal : PluginDesc.CachedJson->GetArrayField(TEXT("PythonRequirements")))
		{
			const TSharedPtr<FJsonObject>& JsonObj = JsonVal->AsObject();
			if (!CheckCompatiblePlatform(JsonObj, FPlatformMisc::GetUBTPlatform()))
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* PyReqs;
			if (JsonObj->TryGetArrayField(TEXT("Requirements"), PyReqs))
			{
				for (const TSharedPtr<FJsonValue>& JsonReqVal : *PyReqs)
				{
					OutRequirements.Add(JsonReqVal->AsString());
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* PyUrls;
			if (JsonObj->TryGetArrayField(TEXT("ExtraIndexUrls"), PyUrls))
			{
				for (const TSharedPtr<FJsonValue>& JsonUrlVal : *PyUrls)
				{
					OutExtraUrls.Add(JsonUrlVal->AsString());
				}
			}
		}
	}

	const FString MergedReqsFile = FPaths::ConvertRelativePathToFull(PipInstallPath / RequirementsInputFilename);
	const FString ExtraUrlsFile = FPaths::ConvertRelativePathToFull(PipInstallPath / ExtraUrlsFilename);

	FFileHelper::SaveStringArrayToFile(OutRequirements, *MergedReqsFile);
	FFileHelper::SaveStringArrayToFile(OutExtraUrls, *ExtraUrlsFile);

	return MergedReqsFile;
}

void FPipInstall::CheckInvalidPipEnv()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::CheckInvalidPipEnv);

	const FString PipInstallPath = GetPipInstallPath();
	if (!FPaths::DirectoryExists(PipInstallPath))
	{
		return;
	}

	const FString VenvVersion = ParseVenvVersion(PipInstallPath);
	if (VenvVersion == TEXT(PY_VERSION))
	{
		return;
	}

	UE_LOG(LogPython, Display, TEXT("Engine python version (%s) incompatible with venv (%s), recreating..."), TEXT(PY_VERSION), *VenvVersion);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteDirectoryRecursively(*PipInstallPath);
}

void FPipInstall::SetupPipEnv(FFeedbackContext* Context, bool bForceRebuild /* = false */)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::SetupPipEnv);

	const FString PipInstallPath = GetPipInstallPath();
	const FString VenvInterp = GetVenvInterpreter(PipInstallPath);

	if (!bForceRebuild && FPaths::FileExists(VenvInterp))
	{
		SetupPipInstallUtils(VenvInterp, Context);
		return;
	}

	if (bForceRebuild && FPaths::DirectoryExists(PipInstallPath))
	{
		// TODO: Need to cache generated files before deleting the directory (or only delete subdirs)
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteDirectoryRecursively(*PipInstallPath);
	}

	const FString EngineInterp = PyUtil::GetInterpreterExecutablePath();
	const FString VenvCmd = FString::Printf(TEXT("-m venv \"%s\""), *FPaths::ConvertRelativePathToFull(PipInstallPath));
	int32 Res = RunPythonCmd(LOCTEXT("PipInstall.SetupVenv", "Setting up pip install environment..."), EngineInterp, VenvCmd, Context);
	if (Res != 0)
	{
		UE_LOG(LogPython, Error, TEXT("Unable to create pip install environment (%d)"), Res);
		return;
	}

	SetupPipInstallUtils(VenvInterp, Context);
}

FString FPipInstall::ParsePluginDependencies(const FString& MergedInRequirementsFile, FFeedbackContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::ParsePluginDependencies);

	const FString PipInstallPath = GetPipInstallPath();
	const FString VenvInterp = GetVenvInterpreter(PipInstallPath);

	const FString ParsedReqsFile = PipInstallPath / ParsedRequirementsFilename;

	const FString Cmd = FString::Printf(TEXT("-m ue_parse_plugin_reqs -vv \"%s\" \"%s\""), *MergedInRequirementsFile, *ParsedReqsFile);
	RunPythonCmd(LOCTEXT("PipInstall.ParseRequirements", "Parsing pip requirements..."), VenvInterp, Cmd, Context);

	return FPaths::ConvertRelativePathToFull(ParsedReqsFile);
}

bool FPipInstall::HasInstallLines(const TArray<FString>& RequirementLines)
{
	for (const FStringView Line : RequirementLines)
	{
		bool bCommentLine = Line.TrimStart().StartsWith(TCHAR('#'));
		if (!bCommentLine && !Line.Contains(TEXT("# [pkg:check]")))
		{
			return true;
		}
	}

	return false;
}

FString FPipInstall::GetPipInstallPath()
{
	return FPaths::ProjectIntermediateDir() / TEXT("PipInstall");
}


void FPipInstall::SetupPipInstallUtils(const FString& VenvInterp, FFeedbackContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::SetupPipInstallUtils);

	if (CheckPipInstallUtils(VenvInterp, Context))
	{
		return;
	}

	const FString PipInstallPath = GetPipInstallPath();
	const FString PythonScriptDir = GetPythonScriptPluginPath();
	if (PythonScriptDir.IsEmpty())
	{
		return;
	}

	const FString PipWheelsDir = PythonScriptDir / TEXT("Content/Python/Lib/wheels");
	const FString InstallRequirements = PythonScriptDir / TEXT("Content/Python/PipInstallUtils/requirements.txt");

	const FString PipInstallReq = TEXT("ue-pipinstall-utils==") + PipInstallUtilsVer;
	const FString Cmd = FString::Printf(TEXT("-m pip install --upgrade --no-index --find-links \"%s\" -r \"%s\" %s"), *PipWheelsDir, *InstallRequirements, *PipInstallReq);

	RunPythonCmd(LOCTEXT("PipInstall.SetupPipInstallUtils", "Setting up pip install utils"), VenvInterp, Cmd, Context);
}


bool FPipInstall::CheckPipInstallUtils(const FString& VenvInterp, FFeedbackContext* Context)
{
	// Verify that correct version of pip install utils is already available
	const FString Cmd = FString::Printf(TEXT("-c \"import pkg_resources;dist=pkg_resources.working_set.find(pkg_resources.Requirement.parse('ue-pipinstall-utils'));exit(dist.version!='%s' if dist is not None else 1)\""), *PipInstallUtilsVer);
	return (RunPythonCmd(LOCTEXT("PipInstall.CheckPipInstallUtils", "Check pip install utils installed"), VenvInterp, Cmd, Context) == 0);
}

int32 FPipInstall::RunPythonCmd(const FText& Description, const FString& PythonInterp, const FString& Cmd, FFeedbackContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::RunPythonCmd);

	UE_LOG(LogPython, Log, TEXT("Running python command: python %s"), *Cmd);

	int32 Result = 0;
	RunLoggedSubprocess(Description, FPaths::ConvertRelativePathToFull(PythonInterp), Cmd, Context, &Result);

	return Result;
}

bool FPipInstall::RunLoggedSubprocess(const FText& Description, const FString& URL, const FString& Params, FFeedbackContext* Context, int32* OutExitCode)
{
	FScopedSlowTask SubprocessTask(0, Description, true, *Context);

	// Create a read and write pipe for the child process
	void* StdOutPipeRead = nullptr;
	void* StdOutPipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(StdOutPipeRead, StdOutPipeWrite));

	ON_SCOPE_EXIT
	{
		FPlatformProcess::ClosePipe(StdOutPipeRead, StdOutPipeWrite);
	};

	// Create the process
	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*URL, *Params, false, true, true, nullptr, 0, nullptr, StdOutPipeWrite, nullptr);
	if (ProcessHandle.IsValid())
	{
		FString BufferedText;
		for (bool bProcessFinished = false; !bProcessFinished; )
		{
			bProcessFinished = FPlatformProcess::GetProcReturnCode(ProcessHandle, OutExitCode);
			BufferedText += FPlatformProcess::ReadPipe(StdOutPipeRead);

			int32 EndOfLineIdx;
			while (BufferedText.FindChar(TEXT('\n'), EndOfLineIdx))
			{
				FString Line = BufferedText.Left(EndOfLineIdx);
				Line.RemoveFromEnd(TEXT("\r"), ESearchCase::CaseSensitive);

				Context->Log(LogPython.GetCategoryName(), ELogVerbosity::Log, Line);
				BufferedText.MidInline(EndOfLineIdx + 1, MAX_int32, false);
			}

			FPlatformProcess::Sleep(0.1f);
		}
		ProcessHandle.Reset();
		return true;
	}
	else
	{
		Context->CategorizedLogf(LogPython.GetCategoryName(), ELogVerbosity::Warning, TEXT("Couldn't create process '%s'"), *URL);
		return false;
	}
}


FString FPipInstall::GetPythonScriptPluginPath()
{
	TSharedPtr<IPlugin> PythonPlugin = IPluginManager::Get().FindPlugin("PythonScriptPlugin");
	if (!PythonPlugin)
	{
		return TEXT("");
	}

	return PythonPlugin->GetBaseDir();
}

FString FPipInstall::ParseVenvVersion(const FString& InstallPath)
{
	FString VenvConfig = InstallPath / TEXT("pyvenv.cfg");
	if (!FPaths::FileExists(VenvConfig))
	{
		return TEXT("");
	}

	TArray<FString> ConfigLines;
	if (!FFileHelper::LoadFileToStringArray(ConfigLines, *VenvConfig))
	{
		return TEXT("");
	}

	for (FStringView Line : ConfigLines)
	{
		FStringView ChkLine = Line.TrimStartAndEnd();
		if (!ChkLine.StartsWith(TEXT("version =")))
		{
			continue;
		}

		FStringView Version = ChkLine.RightChop(9).TrimStart();
		return FString(Version);
	}

	return TEXT("");
}

FString FPipInstall::GetVenvInterpreter(const FString& InstallPath)
{
#if PLATFORM_WINDOWS
	return InstallPath / TEXT("Scripts/python.exe");
#elif PLATFORM_MAC || PLATFORM_LINUX
	return InstallPath / TEXT("bin/python3");
#else
	static_assert(false, "Python not supported on this platform!");
#endif
}

bool FPipInstall::CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName)
{
	FString JsonPlatform;

	return !JsonObject->TryGetStringField(TEXT("Platform"), JsonPlatform) || JsonPlatform.Equals(TEXT("All"), ESearchCase::IgnoreCase) || JsonPlatform.Equals(PlatformName, ESearchCase::IgnoreCase);
}

#endif //WITH_PYTHON

#undef LOCTEXT_NAMESPACE
