// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPlugin;

class PLUGINREFERENCEVIEWER_API FPluginReferenceViewerUtils
{
public:
	/* Exports the number of references from each plugin to their plugin dependencies. References include; assets, scripts and named references*/
	static void ExportPlugins(const TArray<FString>& InPlugins, const FString& InFilename);

	/* Exports to a .csv file the list of asset references that exist between the plugin and one of it's plugin dependencies */
	static void ExportReference(const FString& InPlugin, const FString& InReference, const FString& InFilename);

	/* Returns the list of plugins where the supplied gameplay tag is declated */
	static TArray<TSharedRef<IPlugin>> FindGameplayTagSourcePlugins(FName TagName);

private:
	FPluginReferenceViewerUtils() = delete;
	FPluginReferenceViewerUtils(const FPluginReferenceViewerUtils&) = delete;
};