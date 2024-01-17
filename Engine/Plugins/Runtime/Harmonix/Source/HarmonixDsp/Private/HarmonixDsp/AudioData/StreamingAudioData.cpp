// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/AudioData/StreamingAudioData.h"

#include "AudioDecompress.h"
#include "AudioStreaming.h"
#include "HarmonixDsp/AudioUtility.h"
#include "HarmonixDsp/Conversions.h"
#include "HarmonixDsp/GainMatrix.h"
#include "HarmonixDsp/PannerDetails.h"
#include "Interfaces/IAudioFormat.h"
#include "Math/UnrealMathUtility.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProxyReader.h"
#include "StreamingAudioData.h"

LLM_DEFINE_TAG(Harmonix_StreamingAudioData);

DEFINE_LOG_CATEGORY(LogHarmonixStreamingAudioData);

const FName& FStreamingAudioData::GetName() const
{
	return WaveProxy->GetFName();
}

bool FStreamingAudioData::GetHasTailSection() const
{
	if (!GetHasLoopSection())
		return false;

	int32 LastFrame = GetNumFrames() - 1;

	return GetLoopEndFrame() != LastFrame;
}

bool FStreamingAudioData::GetHasLoopSection() const
{
	return WaveProxy->GetLoopRegions().Num() > 0;
}

uint32 FStreamingAudioData::GetLoopStartFrame() const
{
	check(WaveProxy->GetLoopRegions().Num() > 0);
	return WaveProxy->GetLoopRegions()[0].FramePosition;
}

uint32 FStreamingAudioData::GetLoopEndFrame() const
{
	check(WaveProxy->GetLoopRegions().Num() > 0);
	const FSoundWaveCuePoint& LoopRegion = WaveProxy->GetLoopRegions()[0];
	return LoopRegion.FramePosition + LoopRegion.FrameLength;
}

bool FStreamingAudioData::Failed() const
{
	return false;
}

TUniquePtr<FSoundWaveProxyReader> FStreamingAudioData::CreateWaveProxyReader() const
{
	FSoundWaveProxyReader::FSettings WaveReaderSettings;
	WaveReaderSettings.MaxDecodeSizeInFrames = MaxDecodeSizeInFrames;
	WaveReaderSettings.StartTimeInSeconds = 0.0f;
	WaveReaderSettings.bMaintainAudioSync = true;
	return FSoundWaveProxyReader::Create(WaveProxy, WaveReaderSettings);
}

FStreamingAudioData::FStreamingAudioData(FSoundWaveProxyRef InWaveProxy, const FSettings& InSettings)
	: HarmonixDsp::IAudioData()
	, WaveProxy(InWaveProxy)
{
	ChannelMask = GetChannelMaskForNumChannels(GetNumChannels());
	ChannelLayout = HarmonixDsp::FAudioBuffer::GetDefaultChannelLayoutForChannelCount(GetNumChannels());
	MaxDecodeSizeInFrames = InSettings.MaxDecodeSizeFrames;

	if (WaveProxy->GetLoadingBehavior() == ESoundWaveLoadingBehavior::ForceInline)
	{
		IStreamingManager::Get().GetAudioStreamingManager().AddForceInlineSoundWave(WaveProxy.ToSharedPtr());
	}
}

FStreamingAudioData::~FStreamingAudioData()
{
	if (WaveProxy->GetLoadingBehavior() == ESoundWaveLoadingBehavior::ForceInline)
	{
		IStreamingManager::Get().GetAudioStreamingManager().RemoveForceInlineSoundWave(WaveProxy.ToSharedPtr());
	}
}

float FStreamingAudioData::GetSampleRate() const
{
	return WaveProxy->GetSampleRate();
}

uint32 FStreamingAudioData::GetNumFrames() const
{
	return WaveProxy->GetNumFrames();
}

int32 FStreamingAudioData::GetNumChannels() const
{
	return WaveProxy->GetNumChannels();
}

float FStreamingAudioData::GetDurationSeconds() const
{
	return WaveProxy->GetDuration();
}

uint32 FStreamingAudioData::GetChannelMask() const
{
	return ChannelMask;
}

EAudioBufferChannelLayout FStreamingAudioData::GetChannelLayout() const
{
	return ChannelLayout;
}