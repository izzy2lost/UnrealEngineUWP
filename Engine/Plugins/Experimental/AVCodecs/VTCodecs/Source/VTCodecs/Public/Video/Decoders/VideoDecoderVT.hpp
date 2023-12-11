// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/Resources/Metal/VideoResourceMetal.h"
#include "Video/Util/NaluRewriter.h"

THIRD_PARTY_INCLUDES_START
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CMSync.h>
THIRD_PARTY_INCLUDES_END

#define CONDITIONAL_RELEASE(x)          \
    if (x)                              \
    {                                   \
        CFRelease(x);                   \
        x = nullptr;                    \
    }

template <typename TResource>
TVideoDecoderVT<TResource>::~TVideoDecoderVT()
{
	Close();
}

template <typename TResource>
bool TVideoDecoderVT<TResource>::IsOpen() const
{
	return bIsOpen;
}

template <typename TResource>
FAVResult TVideoDecoderVT<TResource>::Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
{
	Close();

	TVideoDecoder<TResource, FVideoDecoderConfigVT>::Open(NewDevice, NewInstance);

    MemoryPool = CMMemoryPoolCreate(nullptr);

	FrameCount = 0;

    bIsOpen = true;

	return EAVResult::Success;
}

template <typename TResource>
void TVideoDecoderVT<TResource>::Close()
{
    DestroyDecompressionSession();
    SetVideoFormat(nullptr);

    if(MemoryPool)
    {
        CMMemoryPoolInvalidate(MemoryPool);
        CFRelease(MemoryPool);
    }

    bIsOpen = false;
}

template <typename TResource>
void TVideoDecoderVT<TResource>::ResetDecompressionSession()
{
    DestroyDecompressionSession();

    if(!VideoFormat)
    {
        FAVResult::Log(EAVResult::PendingInput, TEXT("Waiting for VideoFormat"), TEXT("VT"));                   
        return; 
    }
    
    // Set source image buffer attributes. These attributes will be present on
    // buffers retrieved from the decoder's pixel buffer pool.
    const size_t AttributesSize = 3;
    CFTypeRef Keys[AttributesSize] = 
    {
        kCVPixelBufferOpenGLCompatibilityKey,
        kCVPixelBufferIOSurfacePropertiesKey,
        kCVPixelBufferPixelFormatTypeKey
    };

    CFDictionaryRef IOSurfaceValue = CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    // TODO (belchy06): This should support more than ARGB8 pixel format
    int64_t PixelType = kCVPixelFormatType_32BGRA;
    CFNumberRef PixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &PixelType);

    CFTypeRef Values[AttributesSize] =
    { 
        kCFBooleanTrue, 
        IOSurfaceValue, 
        PixelFormat 
    };

    CFDictionaryRef Attributes = CFDictionaryCreate(kCFAllocatorDefault, Keys, Values, AttributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CONDITIONAL_RELEASE(IOSurfaceValue);
    CONDITIONAL_RELEASE(PixelFormat);

    VTDecompressionOutputCallbackRecord Record = 
    {
        Internal::VTDecompressionOutputCallback, 
        this
    };
                
    OSStatus Result = VTDecompressionSessionCreate(kCFAllocatorDefault, VideoFormat, nullptr, Attributes, &Record, &Decoder);

    if(Result != 0)
    {
        DestroyDecompressionSession();
        FAVResult::Log(EAVResult::ErrorCreating, TEXT("Failed to create VTDecompressionSession"), TEXT("VT"), Result);
    }

    CONDITIONAL_RELEASE(Attributes);

    ConfigureDecompressionSession();
}

template <typename TResource>
void TVideoDecoderVT<TResource>::DestroyDecompressionSession()
{
    if (Decoder) 
    {
        VTDecompressionSessionInvalidate(Decoder);	
        CFRelease(Decoder);
        Decoder = nullptr;
    }
}

template <typename TResource>
void TVideoDecoderVT<TResource>::ConfigureDecompressionSession()
{
    VTSessionSetProperty(Decoder, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
}

template <typename TResource>
bool TVideoDecoderVT<TResource>::IsInitialized() const
{
	return Decoder != nullptr;
}

template <typename TResource>
FAVResult TVideoDecoderVT<TResource>::ApplyConfig()
{
	if (IsOpen())
	{
		FVideoDecoderConfigVT const& PendingConfig = this->GetPendingConfig();
		if (this->AppliedConfig != PendingConfig)
		{
			if (IsInitialized())
			{
                if(this->AppliedConfig.Codec == PendingConfig.Codec)
                {
                    // TODO (belchy06): Reconfiguration
                }
                else
                {
                    if (Decoder) 
                    {
                        DestroyDecompressionSession();
                        FAVResult::Log(EAVResult::Success, TEXT("Re-initializing decoding session"), TEXT("VT"));
                    }
                }
			}

			if (!IsInitialized())
			{
                ResetDecompressionSession();
			}
		}

		return TVideoDecoder<TResource, FVideoDecoderConfigVT>::ApplyConfig();
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("VT"));
}

template <typename TResource>
FAVResult TVideoDecoderVT<TResource>::SendPacket(FVideoPacket const& Packet)
{
	if (IsOpen())
	{
        // We've received our first call to decode a frame, we can now parse the information from
        // the bitstream, configure our config and initialize the session
		if(!IsInitialized())
        {
            // Applying the config must be done before parsing the format as we need to know the codec
            FAVResult AVResult = ApplyConfig();
		    if (AVResult.IsNotSuccess())
		    {
    			return AVResult;
		    }

            CMVideoFormatDescriptionRef InputFormat = nullptr;
            if(this->AppliedConfig.Codec == kCMVideoCodecType_H264)
            {
                InputFormat = NaluRewriter::CreateH264VideoFormatDescription(Packet.DataPtr.Get(), Packet.DataSize);
            }
            else if(this->AppliedConfig.Codec == kCMVideoCodecType_HEVC)
            {
                InputFormat = NaluRewriter::CreateH265VideoFormatDescription(Packet.DataPtr.Get(), Packet.DataSize);
            }
            else if (this->AppliedConfig.Codec == kCMVideoCodecType_VP9)
            {
                InputFormat = NaluRewriter::CreateVP9VideoFormatDescription(Packet.DataPtr.Get(), Packet.DataSize);
            }
            else
            {
                return FAVResult(EAVResult::Error, TEXT("Unsupported codec"), TEXT("VT"));
            }

            if(InputFormat)
            {
                if(!CMFormatDescriptionEqual(InputFormat, VideoFormat))
                {
                    SetVideoFormat(InputFormat);
                    ResetDecompressionSession();
                }
            }

            CONDITIONAL_RELEASE(InputFormat);
        }

        if(!VideoFormat)
        {
            return FAVResult(EAVResult::WarningInvalidState, TEXT("Missing video format. Frame with sps/pps required."), TEXT("VT"));
        }

        CMSampleBufferRef SampleBuffer = nullptr;
        if(this->AppliedConfig.Codec == kCMVideoCodecType_H264)
        {
            if(!NaluRewriter::H264AnnexBBufferToCMSampleBuffer(Packet.DataPtr.Get(), Packet.DataSize, VideoFormat, &SampleBuffer, MemoryPool))
            {
                return FAVResult(EAVResult::Error, TEXT("Failed to get SampleBuffer"), TEXT("VT"));
            }
        }
        else if(this->AppliedConfig.Codec == kCMVideoCodecType_HEVC)
        {
            if(!NaluRewriter::H265AnnexBBufferToCMSampleBuffer(Packet.DataPtr.Get(), Packet.DataSize, VideoFormat, &SampleBuffer, MemoryPool))
            {
                return FAVResult(EAVResult::Error, TEXT("Failed to get SampleBuffer"), TEXT("VT"));
            }
        }
        else if (this->AppliedConfig.Codec == kCMVideoCodecType_VP9)
        {
            if(!NaluRewriter::VP9BufferToCMSampleBuffer(Packet.DataPtr.Get(), Packet.DataSize, VideoFormat, &SampleBuffer, MemoryPool))
            {
                return FAVResult(EAVResult::Error, TEXT("Failed to get SampleBuffer"), TEXT("VT"));
            }
        }
        else
        {
            return FAVResult(EAVResult::Error, TEXT("Unsupported codec"), TEXT("VT"));
        }
        
        if(SampleBuffer == nullptr)
        {
            return FAVResult(EAVResult::Error, TEXT("SampleBuffer is nullptr"), TEXT("VT"));
        }

        TUniquePtr<DecodeParams> FrameDecodeParams;
        FrameDecodeParams.Reset(new DecodeParams());

        OSStatus Result = VTDecompressionSessionDecodeFrame(Decoder, SampleBuffer, 0, FrameDecodeParams.Release(), nullptr);

        CFRelease(SampleBuffer);

        if(Result != 0)
        {
            return FAVResult(EAVResult::Error, TEXT("Failed to decode frame"), TEXT("VT"), Result);
        }

        return EAVResult::Success;
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("VT"));
}

template <typename TResource>
FAVResult TVideoDecoderVT<TResource>::ReceiveFrame(TResolvableVideoResource<TResource>& InOutResource)
{
    if(IsOpen())
    {
        if(TSharedPtr<FFrame> Frame = *Frames.Peek())
        {
            size_t Width = CVPixelBufferGetWidth(Frame->ImageBuffer);
            size_t Height = CVPixelBufferGetHeight(Frame->ImageBuffer);

            OSType PixelFormat = CVPixelBufferGetPixelFormatType(Frame->ImageBuffer);

            FVideoDescriptor ResourceDescriptor = FVideoDescriptor(EVideoFormat::BGRA, Width, Height);
            if (!InOutResource.Resolve(this->GetDevice(), ResourceDescriptor))
			{
				return FAVResult(EAVResult::ErrorResolving, TEXT("Failed to resolve frame resource"), TEXT("VT"));
			}

            CVPixelBufferLockBaseAddress(Frame->ImageBuffer, kCVPixelBufferLock_ReadOnly);

            void* PixelPtr = CVPixelBufferGetBaseAddress(Frame->ImageBuffer);

            // Do copy into the VideoResource
//            InOutResource->GetRaw()->replaceRegion(MTL::Region(0, 0, Width, Height), 0, PixelPtr, Width * 4);

            CVPixelBufferUnlockBaseAddress(Frame->ImageBuffer, kCVPixelBufferLock_ReadOnly);

            Frames.Pop();
            
            return EAVResult::Success;
        }

        return EAVResult::PendingInput;
    }

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("VT"));
}

template <typename TResource>
void TVideoDecoderVT<TResource>::SetVideoFormat(CMVideoFormatDescriptionRef Format)
{
    if(VideoFormat == Format)
    {
        return;
    }

    CONDITIONAL_RELEASE(VideoFormat);

    VideoFormat = Format;
    if(VideoFormat)
    {
        CFRetain(VideoFormat);
    }
}

template <typename TResource>
FAVResult TVideoDecoderVT<TResource>::HandleFrame(void* Params, OSStatus Status, VTDecodeInfoFlags InfoFlags, CVImageBufferRef ImageBuffer, CMTime Timestamp, CMTime Duration)
{
    if(IsOpen())
    {
        if(Status != 0)
        {
            return FAVResult(EAVResult::Error, TEXT("Failed to decode"), TEXT("VT"), Status);
        }

        if(!ImageBuffer)
        {
            return FAVResult(EAVResult::Error, TEXT("No output image buffer"), TEXT("VT"), Status);
        }
        
        // The destructor for FFrame releases the ImageBuffer so we need to make sure it's not release until after it's been used
        TSharedPtr<FFrame> Frame = MakeShareable(new FFrame(ImageBuffer, Timestamp, Duration));
        Frames.Enqueue(Frame);

        return EAVResult::Success;
    }
    
    return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("VT"));
}
