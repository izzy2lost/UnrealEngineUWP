// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapability.h"

struct CAPTURESOURCEFRAMEWORK_API FRecordingEvent : public FCaptureEvent
{
	static const FString Name;

	FRecordingEvent(bool bInIsRecording)
		: FCaptureEvent(Name)
		, bIsRecording(bInIsRecording)
	{
	}

	bool bIsRecording;
};

class CAPTURESOURCEFRAMEWORK_API FRecordingCapabilityBase : public FCaptureSourceCapability
{
public:
	static const FString Name;
	static const FString SlateName;
	static const FString TakeNumber;
	static const FString StartRecordingCmd;
	static const FString StopRecordingCmd;

	FRecordingCapabilityBase();
	virtual ~FRecordingCapabilityBase() = default;

	virtual void StartRecording(FString InSlateName, int64 InTakeNumber) = 0;
	virtual void StopRecording() = 0;

	virtual void SetStartRecordingHandler(FCommandHandler InCommandHandler) = 0;
	virtual void SetStopRecordingHandler(FCommandHandler InCommandHandler) = 0;
};

class CAPTURESOURCEFRAMEWORK_API FRecordingCapability : public FRecordingCapabilityBase
{
public:

	FRecordingCapability();

	void StartRecording(FString InSlateName, int64 InTakeNumber);
	void StopRecording();

	void SetStartRecordingHandler(FCommandHandler InCommandHandler);
	void SetStopRecordingHandler(FCommandHandler InCommandHandler);

	void PublishRecordingEvent(bool bIsRecording);
};