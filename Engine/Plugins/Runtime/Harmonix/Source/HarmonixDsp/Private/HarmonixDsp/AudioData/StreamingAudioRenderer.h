// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/Streaming/TrackChannelInfo.h"
#include "HarmonixDsp/AudioDataRenderer.h"

#include "Templates/SharedPointer.h"

#include "DSP/BufferVectorOperations.h"
#include "DSP/ConvertDeinterleave.h"
#include "DSP/MultichannelBuffer.h"
#include "DSP/MultichannelLinearResampler.h"

namespace HarmonixDsp
{
	class IAudioData;
}

class FSoundWaveProxy;
class FSoundWaveProxyReader;
class FStreamingAudioData;
class FFusionSampler;
class IStretcherAndPitchShifter;

DECLARE_LOG_CATEGORY_EXTERN(LogHarmonixStreamingAudioRenderer, Log, All);

class HARMONIXDSP_API FStreamingAudioRenderer : public IAudioDataRenderer
{
public:

	using FAlignedInt16Buffer = TArray<int16, Audio::FAudioBufferAlignedAllocator>;

	FStreamingAudioRenderer();
	~FStreamingAudioRenderer();

	virtual void Reset() override;
	virtual void SetAudioData(TSharedRef<HarmonixDsp::IAudioData, ESPMode::ThreadSafe> AudioData, const FSettings& InSettings) override;
	void SetAudioData(TSharedRef<FStreamingAudioData, ESPMode::ThreadSafe> StreamingAudioData, const FSettings& InSettings);
	virtual const TSharedPtr<HarmonixDsp::IAudioData, ESPMode::ThreadSafe> GetAudioData() const;

	virtual void MigrateToSampler(const FFusionSampler* InSampler) override;

	virtual void SetFrame(uint32 InFrameNum) override;

	virtual double Render(TAudioBuffer<float>& OutBuffer, double InPos, int32 InMaxFrame, double InResampleInc, double InPitchShift, double InSpeed,
		bool MaintainPitchWhenSpeedChanges, bool InShouldHonorLoopPoints, const FGainMatrix& InGain) override;

	double RenderInternal(TAudioBuffer<float>& OutBuffer, double Pos, int32 MaxFrame, double Inc, bool ShouldHonorLoopPoints, const FGainMatrix& Gain);

	virtual double RenderUnshifted(TAudioBuffer<float>& OutBuffer, double InPos, int32 InMaxFrame, double InInc,
		bool InShouldHonorLoopPoints, const FGainMatrix& InGain) override;

	void GenerateSourceAudio(uint32 StartFrame, Audio::FAlignedFloatBuffer& OutAudio, bool bHonorLoopRegion = false);

private:

	void RenderSimpleUnshifted(TAudioBuffer<float>& OutBuffer, const FLerpData* LerpArray, uint32 InNumFrames, const FGainMatrix& InGain, double InInc, bool ShouldHonorLoopPoints);
	void RenderMultiChannelUnshifted(TAudioBuffer<float>& OutBuffer, const FLerpData* LerpArray, uint32 InNumFrames, const FGainMatrix& InGain, double InInc, bool ShouldHonorLoopPoints);
	void RenderMultiChannelRoutedUnshifted(TAudioBuffer<float>& OutBuffer, const FLerpData* LerpArray, uint32 InNumFrames, const FGainMatrix& InGain, double InInc, bool ShouldHonorLoopPoints);

	void SeekSourceAudioToFrame(uint32 FrameIdx);
	void DecodeSourceAudio(Audio::TCircularAudioBuffer<float>& OutBuffer);

	void GenerateSourceAudioInternal(uint32 StartFrameIndex, float* OutAudioData, uint32 NumSamples);

	uint32 GetSourceAudioFrameIndex();

private:

	const FFusionSampler* MySampler = nullptr;
	const TArray<FTrackChannelInfo>* TrackChannelInfo = nullptr;
	TSharedPtr<IStretcherAndPitchShifter, ESPMode::ThreadSafe> Shifter;

	// Ref to the actual streaming audio data
	// this data is a shared instance of audio data
	// it will get loaded on construction 
	TSharedPtr<FStreamingAudioData, ESPMode::ThreadSafe> StreamingAudioData;

	TUniquePtr<FSoundWaveProxyReader> WaveProxyReader;

	Audio::FAlignedFloatBuffer DecodeBuffer;
	Audio::FAlignedFloatBuffer WorkBuffer;

	Audio::TCircularAudioBuffer<float> InterleavedCircularBuffer;
	int32 NumDeinterleaveChannels;
	

	// needs to be small enough to avoid audio artifacts and syncing issues
	// but also large enough that we're decoding multiple times per block
	static constexpr int32 DeinterleaveBlockSizeInFrames = 256;
};