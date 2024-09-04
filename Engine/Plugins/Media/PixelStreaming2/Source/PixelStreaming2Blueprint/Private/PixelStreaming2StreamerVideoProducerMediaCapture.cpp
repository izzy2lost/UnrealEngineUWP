// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2StreamerVideoProducerMediaCapture.h"
#include "VideoProducerPIEViewport.h"
#include "VideoProducerMediaCapture.h"

UPixelStreaming2StreamerVideoProducerMediaCapture::UPixelStreaming2StreamerVideoProducerMediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<IPixelStreaming2VideoProducer> UPixelStreaming2StreamerVideoProducerMediaCapture::GetVideoProducer()
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
			VideoProducer = UE::PixelStreaming2::FVideoProducerMediaCapture::CreateActiveViewportCapture();
		}
	}

	return VideoProducer;
}
