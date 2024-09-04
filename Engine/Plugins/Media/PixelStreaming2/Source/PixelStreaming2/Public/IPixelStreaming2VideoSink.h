// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPixelStreaming2VideoConsumer;

// Interface for a sink that collects video coming in from the browser and passes into into UE.
class PIXELSTREAMING2_API IPixelStreaming2VideoSink
{
public:
	virtual ~IPixelStreaming2VideoSink() = default;

	virtual void AddVideoConsumer(IPixelStreaming2VideoConsumer* VideoConsumer) = 0;
	virtual void RemoveVideoConsumer(IPixelStreaming2VideoConsumer* VideoConsumer) = 0;
};
