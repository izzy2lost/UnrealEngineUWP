// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MetaHumanSDKSettings.generated.h"

/**
 *
 */
UCLASS(Config = MetaHumanProjectUtilities)
class METAHUMANSDKEDITOR_API UMetaHumanSDKSettings : public UObject
{
	GENERATED_BODY()

public:
	// The URL for fetching version information and release notes
	UPROPERTY(Config)
	FString VersionServiceBaseUrl;
};
