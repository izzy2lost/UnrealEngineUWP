// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2StreamerVideoProducerRenderTarget.h"
#include "VideoProducerRenderTarget.h"

UPixelStreaming2StreamerVideoProducerRenderTarget::UPixelStreaming2StreamerVideoProducerRenderTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<IPixelStreaming2VideoProducer> UPixelStreaming2StreamerVideoProducerRenderTarget::GetVideoProducer()
{
	if (!VideoProducer)
	{
		VideoProducer = UE::PixelStreaming2::FVideoProducerRenderTarget::Create(Target);
	}
	return VideoProducer;
}
