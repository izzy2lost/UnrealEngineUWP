// Copyright Epic Games, Inc. All Rights Reserved.

#include "PipInstall.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


#if WITH_PYTHON

FString FPipInstall::WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::WritePluginsListing)

	const FString PipInstallPath = FPaths::ProjectIntermediateDir() / TEXT("PipInstall");

	OutPythonPlugins.Empty();

	TArray<FString> PythonPluginPaths;
	for ( const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins() )
	{
		const FPluginDescriptor& PluginDesc = Plugin->GetDescriptor();
		if (PluginDesc.CachedJson->HasTypedField(TEXT("PythonRequirements"), EJson::Array))
		{
			const FString PluginDescFile = FPaths::ConvertRelativePathToFull(Plugin->GetDescriptorFileName());
			PythonPluginPaths.Add(PluginDescFile);
			OutPythonPlugins.Add(Plugin);
		}
	}

	const FString PyPluginsListingFile = PipInstallPath / TEXT("pyreqs_plugins.list");
	FFileHelper::SaveStringArrayToFile(PythonPluginPaths, *PyPluginsListingFile);

    return PyPluginsListingFile;
}

FString FPipInstall::WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::WritePluginDependencies)

	const FString PipInstallPath = FPaths::ProjectIntermediateDir() / TEXT("PipInstall");

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

	const FString MergedReqsFile = PipInstallPath / TEXT("merged_requirements.in");
	const FString ExtraUrlsFile = PipInstallPath / TEXT("extra_urls.txt");

	FFileHelper::SaveStringArrayToFile(OutRequirements, *MergedReqsFile);
	FFileHelper::SaveStringArrayToFile(OutExtraUrls, *ExtraUrlsFile);

	return MergedReqsFile;
}

bool FPipInstall::CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName)
{
	FString JsonPlatform;

	return !JsonObject->TryGetStringField(TEXT("Platform"), JsonPlatform) || JsonPlatform.Equals(TEXT("All"), ESearchCase::IgnoreCase) || JsonPlatform.Equals(PlatformName, ESearchCase::IgnoreCase);
}

#endif //WITH_PYTHON
