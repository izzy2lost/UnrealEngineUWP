// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Clients/LiveLinkHubUEClientInfo.h"
#include "CoreTypes.h"
#include "Engine/TimecodeProvider.h"
#include "LiveLinkPresetTypes.h"
#include "Misc/Guid.h"
#include "Subjects/LiveLinkHubSubjectSessionConfig.h"


#include "LiveLinkHubSessionData.generated.h"

/** Live link hub session data that can be saved to disk. */
UCLASS()
class ULiveLinkHubSessionData : public UObject
{
public:
	GENERATED_BODY()

	ULiveLinkHubSessionData()
	{
		SubjectsConfig = CreateDefaultSubobject<ULiveLinkHubSubjectSessionConfig>(TEXT("SubjectsConfig"));

        if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
        {
        	SubjectsConfig->Initialize();
        }
	}

	ULiveLinkHubSessionData(ULiveLinkHubSubjectSessionConfig* InSubjectsConfig)
	{
		SubjectsConfig = InSubjectsConfig;
	}

	/** Live link hub sources. */
	UPROPERTY()
	TArray<FLiveLinkSourcePreset> Sources;

	/** Live link hub subjects. */
	UPROPERTY()
	TArray<FLiveLinkSubjectPreset> Subjects;

	/** Live link hub client info. */
	UPROPERTY()
	TArray<FLiveLinkHubUEClientInfo> Clients;

	/** Timecode settings for the live link hub. */
	UPROPERTY()
	FLiveLinkHubTimecodeSettings TimecodeSettings;

	/** Subject configs for this session. */
	UPROPERTY(Instanced)
	TObjectPtr<ULiveLinkHubSubjectSessionConfig> SubjectsConfig;
};
