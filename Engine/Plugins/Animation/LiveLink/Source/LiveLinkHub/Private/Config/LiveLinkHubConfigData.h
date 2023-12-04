// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Clients/LiveLinkHubUEClientInfo.h"
#include "CoreTypes.h"
#include "LiveLinkPresetTypes.h"
#include "Misc/Guid.h"

#include "LiveLinkHubConfigData.generated.h"

/** Live link hub config data which can be serialized to disk. */
USTRUCT()
struct FLiveLinkHubConfigData
{
	GENERATED_BODY()

	/** Live link hub sources. */
	UPROPERTY()
	TArray<FLiveLinkSourcePreset> Sources;

	/** Live link hub subjects. */
	UPROPERTY()
	TArray<FLiveLinkSubjectPreset> Subjects;

	/** Live link hub client info. */
	UPROPERTY()
	TArray<FLiveLinkHubUEClientInfo> Clients;
};