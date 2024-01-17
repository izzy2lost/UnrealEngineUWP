// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixDsp/Streaming/TrackChannelInfo.h"
#include "HarmonixDsp/AudioData.h"

#include "Templates/SharedPointer.h"

#include "ContentStreaming.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/ConvertDeinterleave.h"
#include "DSP/MultichannelBuffer.h"
#include "DSP/MultichannelLinearResampler.h"

#include "Logging/LogMacros.h"

#include "Async/Future.h"

#include "HAL/LowLevelMemTracker.h"

LLM_DECLARE_TAG_API(Harmonix_StreamingAudioData, HARMONIXDSP_API);

class FSoundWaveProxy;
class FSoundWaveProxyReader;

DECLARE_LOG_CATEGORY_EXTERN(LogHarmonixStreamingAudioData, Log, All);

using FOnAudioLoadComplete = TFunction<void(uint8)>;

class HARMONIXDSP_API FStreamingAudioData : public HarmonixDsp::IAudioData
{
public:

	struct FSettings
	{
		uint32 MaxDecodeSizeFrames = 8192;
	};

	using FSoundWaveProxyRef = TSharedRef<FSoundWaveProxy, ESPMode::ThreadSafe>;

	FStreamingAudioData(FSoundWaveProxyRef InWaveProxy, const FSettings& InSettings);
	virtual ~FStreamingAudioData();

	virtual const FName& GetName() const;
	virtual EAudioEncodedFormat GetFormat() const { return EAudioEncodedFormat::Float32; }
	virtual bool IsStreaming() const { return true; }

	virtual bool GetHasTailSection() const;
	virtual bool GetHasLoopSection() const;
	virtual uint32 GetLoopStartFrame() const;
	virtual uint32 GetLoopEndFrame() const;

	virtual float GetSampleRate() const;
	virtual uint32 GetNumFrames() const;
	virtual int32 GetNumChannels() const;
	virtual float GetDurationSeconds() const;

	// what speaker channels are represented in the data?
	virtual uint32 GetChannelMask() const;

	// what speaker layout was this authored for?
	virtual EAudioBufferChannelLayout GetChannelLayout() const;

	virtual bool Failed() const;

	TUniquePtr<FSoundWaveProxyReader> CreateWaveProxyReader() const;

private:
	
	FSoundWaveProxyRef WaveProxy;

	uint32 ChannelMask;
	EAudioBufferChannelLayout ChannelLayout;

	FSettings Settings;
	uint32 MaxDecodeSizeInFrames = 1024;

};