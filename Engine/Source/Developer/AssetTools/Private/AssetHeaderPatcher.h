// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/Task.h"
#include "Containers/ContainersFwd.h"

struct FAssetHeaderPatcher
{
	enum class EResult
	{
		None,
		Success,
		ErrorFailedToLoadSourceAsset,
		ErrorFailedToDeserializeSourceAsset,
		ErrorUnexpectedSectionOrder,
		ErrorBadOffset,
		ErrorUnkownSection,
		ErrorFailedToOpenDestinationFile,
		ErrorFailedToWriteToDestinationFile,
		ErrorEmptyRequireSection,
	};

	static UE::Tasks::TTask<EResult> Start(FString InSrcAsset, FString InDstAsset,
		TMap<FString, FString> InSearchAndReplace);

	static UE::Tasks::TTask<EResult> Start(TUniquePtr<FArchive> InSrcReader, FString InDstAsset,
		TMap<FString, FString> InSearchAndReplace);

	static EResult Test_DoPatch(FArchive& InSrcReader, FArchive& InDstWriter,
		TMap<FString, FString> InSearchAndReplace);
};
