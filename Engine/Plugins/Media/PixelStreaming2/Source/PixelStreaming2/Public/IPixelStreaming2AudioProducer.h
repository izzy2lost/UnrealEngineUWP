// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/*
 * A "Audio Producer" is an object that you use to push audio frames into the Pixel Streaming system.
 *
 * Example usage:
 *
 * (1) Each new frame you want to push you call `MyAudioProducer->PushFrame(Buffer, NumSamples, NumChannels, SampleRate)`
 *
 * Note: Any audio you push in with `PushAudio` will be mixed with other audio producers before being streamed
 */
class PIXELSTREAMING2_API IPixelStreaming2AudioProducer
{
public:
	IPixelStreaming2AudioProducer() = default;
	virtual ~IPixelStreaming2AudioProducer() = default;

    /* Pushes audio into the PS2 audio pipeline which will mix with other audio producers before broadcasting. */
    virtual void PushAudio(const float* InBuffer, int32 NumSamples, int32 NumChannels, int32 SampleRate) = 0;
};
