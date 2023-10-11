// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSP/InterpolatedMultiTapDelay.h"

#include "DSP/FloatArrayMath.h"
#include "Math/VectorRegister.h"

namespace Audio
{
	void FInterpolatedMultiTapDelay::Init(const int32 InBufferSizeSamples)
	{
		WriteIndex = 0;
		DelayLine.Reset();
		DelayLine.AddZeroed(InBufferSizeSamples);
		WrapBuffer.Reset();
		WrapBuffer.AddZeroed(InBufferSizeSamples);
	}

	void FInterpolatedMultiTapDelay::Advance(const FAlignedFloatBuffer& InBuffer)
	{
		check (InBuffer.Num() % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER == 0);
		const uint32 InNumSamples = InBuffer.Num();
		const uint32 DelayBufferNumSamples = DelayLine.Num();

		if (InNumSamples <= 0)
		{
			return;
		}

		if (InNumSamples + WriteIndex > DelayBufferNumSamples)
		{
			const int32 FirstHalfSize = DelayBufferNumSamples - WriteIndex;
			const int32 SecondHalfSize = InNumSamples - FirstHalfSize;

			if (FirstHalfSize > 0)
			{
				FMemory::Memcpy(&DelayLine[WriteIndex], InBuffer.GetData(), FirstHalfSize * sizeof(float));
			}
			if (SecondHalfSize > 0)
			{
				FMemory::Memcpy(DelayLine.GetData(), &InBuffer[FirstHalfSize], SecondHalfSize * sizeof(float));
			}
			WriteIndex = SecondHalfSize;
		}
		else
		{
			FMemory::Memcpy(&DelayLine[WriteIndex], InBuffer.GetData(), InNumSamples * sizeof(float));
			WriteIndex += InNumSamples;
			WriteIndex = FMath::Wrap<uint32>(WriteIndex, 0, DelayLine.Num());
		}
	}

	uint32 FInterpolatedMultiTapDelay::Read(const uint32 StartNumDelaySamples, const uint32 StartSampleFraction, const uint32 EndNumDelaySamples, FAlignedFloatBuffer& OutBuffer)
	{
		const int32 OutputNumSamples = OutBuffer.Num();
		const int32 DelayBufferNumSamples = DelayLine.Num();

		// likely to only run on the first frame, if not configured with enough memory
		if (OutputNumSamples > DelayBufferNumSamples)
		{
			DelayLine.SetNumZeroed(OutputNumSamples);
		}

		if (OutputNumSamples > WrapBuffer.Num())
		{
			WrapBuffer.SetNumUninitialized(OutputNumSamples);
		}
		
		if (DelayBufferNumSamples <= 0 || OutputNumSamples <= 0)
		{
			return 0;
		}

		int32 StartSample = WriteIndex - StartNumDelaySamples;
		int32 EndSample = WriteIndex + OutputNumSamples - EndNumDelaySamples;

		const float SampleStride = FMath::Clamp((float)(EndSample - StartSample) / (float)OutputNumSamples, 0.25f, 4.f);
		const uint32 FixedSampleRate = (uint32)(SampleStride * 65536.f);

		StartSample = FMath::Wrap(StartSample, 0, DelayBufferNumSamples - 1);
		
		Resampler.CurrentFrameFraction = StartSampleFraction;
		const int32 FramesNeeded = (int32)Resampler.SourceFramesNeeded(OutputNumSamples, FixedSampleRate);

		float* SourceBuffer = &DelayLine[StartSample];
		if (StartSample + FramesNeeded >= DelayBufferNumSamples)
		{
			constexpr int32 NumSafetyBufferSamples = 16;
			
			const int32 NumSamplesToEnd = DelayBufferNumSamples - StartSample;
			const int32 SecondBufferNumSamples = FMath::Min((FramesNeeded - NumSamplesToEnd) + NumSafetyBufferSamples, DelayBufferNumSamples);
			
			FMemory::Memcpy(WrapBuffer.GetData(), &DelayLine[StartSample], NumSamplesToEnd * sizeof(float));
			FMemory::Memcpy(&WrapBuffer[NumSamplesToEnd], DelayLine.GetData(), SecondBufferNumSamples * sizeof(float));

			SourceBuffer = WrapBuffer.GetData();
		}

		Resampler.ResampleMono(OutputNumSamples, FixedSampleRate, SourceBuffer, OutBuffer.GetData());

		return Resampler.CurrentFrameFraction;
	}

	void FInterpolatedMultiTapDelay::Reset()
	{
		const int32 NumElements = DelayLine.Num();
		DelayLine.Reset(NumElements);
		DelayLine.AddZeroed(NumElements);
	}

	bool FInterpolatedMultiTapDelay::IsInitialized() const
	{
		return DelayLine.GetAllocatedSize() > 0;
	}
}