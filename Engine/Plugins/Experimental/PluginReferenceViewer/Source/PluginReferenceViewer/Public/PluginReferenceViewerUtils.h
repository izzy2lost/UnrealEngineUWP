// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPluginReferenceViewerUtils
{
public:
	/* Exports the number of references from each plugin to their plugin dependencies. References include; assets, scripts and named references*/
	static void ExportPlugins(const TArray<FString>& InPlugins, const FString& InFilename);

private:
	FPluginReferenceViewerUtils() = delete;
	FPluginReferenceViewerUtils(const FPluginReferenceViewerUtils&) = delete;
};