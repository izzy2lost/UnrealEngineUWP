// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/*
 * An "Audio Consumer" is an object that is responsible for outputting the audio received from a peer. For example, by
 * passing the audio into a UE submix.
 */
class PIXELSTREAMING2_API IPixelStreaming2AudioConsumer
{
public:
	virtual ~IPixelStreaming2AudioConsumer() = default;

	virtual void ConsumeRawPCM(const int16_t* AudioData, int InSampleRate, size_t NChannels, size_t NFrames) = 0;
	virtual void OnConsumerAdded() = 0;
	virtual void OnConsumerRemoved() = 0;
};
