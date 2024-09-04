// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreaming2StreamerVideoProducer.h"
#include "PixelStreaming2StreamerVideoProducerMediaCapture.generated.h"

// Equivalent to UPixelStreaming2StreamerVideoProducerBackBuffer but uses the MediaIO Framework to capture the frame rather than Pixel Capture
UCLASS(NotBlueprintType, NotBlueprintable, Category = "PixelStreaming2", META = (DisplayName = "Media Capture Streamer Video Input"))
class PIXELSTREAMING2BLUEPRINT_API UPixelStreaming2StreamerVideoProducerMediaCapture : public UPixelStreaming2StreamerVideoProducer
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual TSharedPtr<IPixelStreaming2VideoProducer> GetVideoProducer() override;
};