// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreaming2StreamerVideoProducer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PixelStreaming2StreamerVideoProducerRenderTarget.generated.h"

UCLASS(NotBlueprintType, NotBlueprintable, Category = "PixelStreaming2", META = (DisplayName = "Render Target Streamer Video Input"))
class PIXELSTREAMING2BLUEPRINT_API UPixelStreaming2StreamerVideoProducerRenderTarget : public UPixelStreaming2StreamerVideoProducer
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video, AssetRegistrySearchable)
	TObjectPtr<UTextureRenderTarget2D> Target;
	
	virtual TSharedPtr<IPixelStreaming2VideoProducer> GetVideoProducer() override;
};