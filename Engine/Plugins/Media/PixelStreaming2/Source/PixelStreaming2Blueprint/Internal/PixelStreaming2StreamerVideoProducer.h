// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture2DDynamic.h"
#include "IPixelStreaming2VideoProducer.h"
#include "PixelStreaming2StreamerVideoProducer.generated.h"

UCLASS(NotBlueprintType, NotBlueprintable, Category = "PixelStreaming2", META = (DisplayName = "Video Input"))
class PIXELSTREAMING2BLUEPRINT_API UPixelStreaming2StreamerVideoProducer : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual TSharedPtr<IPixelStreaming2VideoProducer> GetVideoProducer() { return VideoProducer; }
	
protected:
	TSharedPtr<IPixelStreaming2VideoProducer> VideoProducer;
};