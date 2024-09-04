// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2StreamerVideoProducerBackBuffer.h"
#include "VideoProducerPIEViewport.h"
#include "VideoProducerBackBuffer.h"

UPixelStreaming2StreamerVideoProducerBackBuffer::UPixelStreaming2StreamerVideoProducerBackBuffer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<IPixelStreaming2VideoProducer> UPixelStreaming2StreamerVideoProducerBackBuffer::GetVideoProducer()
{
	if (!VideoProducer)
	{
		// detect if we're in PIE mode or not
		bool IsGame = false;
		FParse::Bool(FCommandLine::Get(), TEXT("game"), IsGame);
		if (GIsEditor && !IsGame)
		{
			VideoProducer = UE::PixelStreaming2::FVideoProducerPIEViewport::Create();
		}
		else
		{
			VideoProducer = UE::PixelStreaming2::FVideoProducerBackBuffer::Create();
		}
	}

	return VideoProducer;
}
