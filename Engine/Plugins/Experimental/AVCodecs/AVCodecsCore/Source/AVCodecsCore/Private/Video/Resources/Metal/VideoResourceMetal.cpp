// Copyright Epic Games, Inc. All Rights Reserved.

#if AVCODECS_USE_METAL

#include "Video/Resources/Metal/VideoResourceMetal.h"

REGISTER_TYPEID(FVideoContextMetal);
REGISTER_TYPEID(FVideoResourceMetal);

static TAVResult<EVideoFormat> ConvertFormat(mtlpp::PixelFormat Format)
{
	switch (Format)
	{
	case mtlpp::PixelFormat::BGRA8Unorm:
    case mtlpp::PixelFormat::BGRA8Unorm_sRGB:
		return EVideoFormat::BGRA;
	case mtlpp::PixelFormat::RGB10A2Unorm:
		return EVideoFormat::ABGR10;
	case mtlpp::PixelFormat::R8Unorm:
	case mtlpp::PixelFormat::R8Uint:
		return EVideoFormat::R8;	
	default:
		return FAVResult(EAVResult::ErrorUnsupported, FString::Printf(TEXT("mtlpp::PixelFormat format %d is not supported"), Format), TEXT("Metal"));
	}
}

FVideoContextMetal::FVideoContextMetal(mtlpp::Device const& Device)
	: Device(Device)
{
}

FVideoDescriptor FVideoResourceMetal::GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, mtlpp::Texture* Raw)
{
    uint32_t Width = Raw->GetWidth();
    uint32_t Height = Raw->GetHeight();
    TAVResult<EVideoFormat> ConvertedFormat = ConvertFormat(Raw->GetPixelFormat());
    
	return FVideoDescriptor(ConvertedFormat, Width, Height);
}

FVideoResourceMetal::FVideoResourceMetal(TSharedRef<FAVDevice> const& Device, mtlpp::Texture* Raw, FAVLayout const& Layout)
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
