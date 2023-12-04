// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

struct FLiveLinkHubConfigData;
class FJsonObject;

namespace UE::LiveLinkHub::FileUtilities::Private
{
	/** A key which must be present in json configs and mapped to the saved json version. */
	const FString JsonVersionKey = TEXT("liveLinkHub");
	/** The json version we support. */
	constexpr int32 LiveLinkHubVersion = 1;

	/** The extension of the config file. */
	const FString ConfigExtension = TEXT("json");
	/** The default name of the config file. */
	const FString ConfigDefaultFileName = TEXT("LiveLinkHubConfig");
	/** The description of the config file. */
	const FString ConfigDescription = TEXT("Live Link Hub Config");

	/** Save config data to disk. */
	void SaveConfig(const FLiveLinkHubConfigData& InConfigData, const FString& InFilePath);

	/** Load config data from disk. */
	TSharedPtr<FLiveLinkHubConfigData> LoadConfig(const FString& InFilePath);

	/** Convert config data to json. */
	TSharedPtr<FJsonObject> ToJson(const FLiveLinkHubConfigData& InConfigData);

	/** Convert config data from json. */
	TSharedPtr<FLiveLinkHubConfigData> FromJson(const TSharedPtr<FJsonObject>& InJsonObject);
}