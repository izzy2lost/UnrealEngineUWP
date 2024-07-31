// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkPreset.h"
#include "Misc/FrameRate.h"
#include "UObject/Object.h"

#include "LiveLinkRecording.generated.h"

UCLASS(Abstract)
class ULiveLinkRecording : public UObject
{
public:
	GENERATED_BODY()

	ULiveLinkRecording()
	{
		RecordingPreset = CreateDefaultSubobject<ULiveLinkPreset>(TEXT("RecordingPreset"));
	}

	/** True if this asset has all data loaded. */
	virtual bool IsFullyLoaded() const { return false; }

	/** True while recording data is being written to bulk data. */
	virtual bool IsSavingRecordingData() const { return false; }

	/** LiveLink Preset used to save the initial state of the sources and subjects at the time of recording. */
	UPROPERTY(Instanced)
	TObjectPtr<ULiveLinkPreset> RecordingPreset = nullptr;

	/** Length of the recording. */
	UPROPERTY()
	double LengthInSeconds = 0.0;

	/** The framerate of the recording. */
	UPROPERTY()
	FFrameRate FrameRate;
};