// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreaming2StreamerVideoProducer.h"
#include "PixelStreaming2StreamerVideoProducerBackBuffer.generated.h"

UCLASS(NotBlueprintType, NotBlueprintable, Category = "PixelStreaming2", META = (DisplayName = "Back Buffer Streamer Video Input"))
class PIXELSTREAMING2BLUEPRINT_API UPixelStreaming2StreamerVideoProducerBackBuffer : public UPixelStreaming2StreamerVideoProducer
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual TSharedPtr<IPixelStreaming2VideoProducer> GetVideoProducer() override;
};