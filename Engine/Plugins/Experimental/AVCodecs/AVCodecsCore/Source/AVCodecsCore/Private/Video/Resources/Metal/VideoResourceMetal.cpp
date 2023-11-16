// Copyright Epic Games, Inc. All Rights Reserved.

#if AVCODECS_USE_METAL

#include "Video/Resources/Metal/VideoResourceMetal.h"

REGISTER_TYPEID(FVideoContextMetal);
REGISTER_TYPEID(FVideoResourceMetal);

static TAVResult<EVideoFormat> ConvertFormat(MTL::PixelFormat Format)
{
	switch (Format)
	{
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB:
		return EVideoFormat::BGRA;
	case MTL::PixelFormatRGB10A2Unorm:
		return EVideoFormat::ABGR10;
	case MTL::PixelFormatR8Unorm:
	case MTL::PixelFormatR8Uint:
		return EVideoFormat::R8;
	default:
		return FAVResult(EAVResult::ErrorUnsupported, FString::Printf(TEXT("MTL::PixelFormat format %d is not supported"), Format), TEXT("Metal"));
	}
}

FVideoContextMetal::FVideoContextMetal(MTL::Device* Device)
	: Device(Device)
{
}

FVideoDescriptor FVideoResourceMetal::GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, MTL::Texture* Raw)
{
    uint32_t Width = Raw->width();
    uint32_t Height = Raw->height();
    TAVResult<EVideoFormat> ConvertedFormat = ConvertFormat(Raw->pixelFormat());
    
	return FVideoDescriptor(ConvertedFormat, Width, Height);
}

FVideoResourceMetal::FVideoResourceMetal(TSharedRef<FAVDevice> const& Device, MTL::Texture* Raw, FAVLayout const& Layout)
	: TVideoResource(Device, Layout, GetDescriptorFrom(Device, Raw))
	, Raw(Raw)
{
}

FAVResult FVideoResourceMetal::Validate() const
{
	if (!Raw)
	{
		return FAVResult(EAVResult::ErrorInvalidState, TEXT("Raw resource is invalid"), TEXT("Metal"));
	}

	return EAVResult::Success;
}

#endif
