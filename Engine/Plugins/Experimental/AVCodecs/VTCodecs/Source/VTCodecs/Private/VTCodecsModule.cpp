// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Encoders/VideoEncoderVT.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH265.h"

#include "Video/Decoders/VideoDecoderVT.h"
#include "Video/Decoders/Configs/VideoDecoderConfigVP9.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH265.h"

#include "Video/Resources/Metal/VideoResourceMetal.h"

THIRD_PARTY_INCLUDES_START
#include <VideoToolbox/VideoToolbox.h>
THIRD_PARTY_INCLUDES_END

class FVTModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{       
        VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);

        FVideoEncoder::RegisterPermutationsOf<TVideoEncoderVT<FVideoResourceMetal>>
            ::With<FVideoResourceMetal>
            ::And<FVideoEncoderConfigVT, FVideoEncoderConfigH264, FVideoEncoderConfigH265>(
                [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                {
                    return FAPI::Get<FVT>().IsValid();
                });

        if(VTIsHardwareDecodeSupported(kCMVideoCodecType_H264))
        {
            FVideoDecoder::RegisterPermutationsOf<TVideoDecoderVT<FVideoResourceMetal>>
                ::With<FVideoResourceMetal>
                ::And<FVideoDecoderConfigVT, FVideoDecoderConfigH264>(
                    [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                    {
                        return FAPI::Get<FVT>().IsValid();
                    });
        }

        if(VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))
        {
            FVideoDecoder::RegisterPermutationsOf<TVideoDecoderVT<FVideoResourceMetal>>
                ::With<FVideoResourceMetal>
                ::And<FVideoDecoderConfigVT, FVideoDecoderConfigH265>(
                    [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                    {
                        return FAPI::Get<FVT>().IsValid();
                    });
        }

        if(VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))
        {
            FVideoDecoder::RegisterPermutationsOf<TVideoDecoderVT<FVideoResourceMetal>>
                ::With<FVideoResourceMetal>
                ::And<FVideoDecoderConfigVT, FVideoDecoderConfigVP9>(
                    [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                    {
                        return FAPI::Get<FVT>().IsValid();
                    });
        }
	}
};

IMPLEMENT_MODULE(FVTModule, VTCodecs);
