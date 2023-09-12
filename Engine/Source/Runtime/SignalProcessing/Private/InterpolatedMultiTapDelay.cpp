// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSP/InterpolatedMultiTapDelay.h"

#include "DSP/FloatArrayMath.h"
#include "Math/VectorRegister.h"

namespace Audio
{
	void FInterpolatedMultiTapDelay::Init(const int32 InBufferSizeSamples, const float InSampleRate)
	{
		WriteIndex = 0;
		MsToSamples = InSampleRate / 1000.f;
		DelayLine.Reset();
		DelayLine.AddZeroed(AlignIndex(InBufferSizeSamples));
	}

	void FInterpolatedMultiTapDelay::Advance(const FAlignedFloatBuffer& InSamples)
	{
		check (InSamples.Num() % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER == 0);
		const int32 InNumSamples = InSamples.Num();

		if (InNumSamples <= 0)
		{
			return;
		}

		if (InNumSamples + WriteIndex > DelayLine.Num())
		{
			const int32 FirstHalfSize = DelayLine.Num() - WriteIndex;
			const int32 SecondHalfSize = InNumSamples - FirstHalfSize;

			if (FirstHalfSize > 0)
			{
				FMemory::Memcpy(&DelayLine[WriteIndex], InSamples.GetData(), FirstHalfSize * sizeof(float));
			}
			if (SecondHalfSize > 0)
			{
				FMemory::Memcpy(DelayLine.GetData(), &InSamples[FirstHalfSize], SecondHalfSize * sizeof(float));
			}
			WriteIndex = SecondHalfSize;
		}
		else
		{
			FMemory::Memcpy(&DelayLine[WriteIndex], InSamples.GetData(), InNumSamples * sizeof(float));
			WriteIndex += InNumSamples;
			WriteIndex = FMath::Wrap(WriteIndex, 0, DelayLine.Num());
		}
	}

	void FInterpolatedMultiTapDelay::Read(const float StartDelayMSec, const float EndDelayMSec, FAlignedFloatBuffer& OutSamples)
	{
		const int32 OutputSamples = OutSamples.Num();
		check (OutputSamples % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER == 0);

		// likely to only run on the first frame, if not configured with enough memory
		if (OutputSamples > DelayLine.Num())
		{
			DelayLine.SetNumZeroed(OutputSamples);
		}

		const int32 BufferSize = DelayLine.Num();
		const int32 StartSample = AlignIndex(FMath::Wrap(WriteIndex - FMath::FloorToInt32(StartDelayMSec * MsToSamples), 0, BufferSize - 1));
		const int32 EndSample = AlignIndex(FMath::Wrap(WriteIndex + OutputSamples - FMath::CeilToInt32(EndDelayMSec * MsToSamples), 0, BufferSize - 1));

		float* OutputPtr = OutSamples.GetData();
		
		if (StartSample > EndSample)
		{
			const int32 DelaySamples = EndSample - (StartSample - BufferSize);

			// block wraps
			const int32 FirstBlockDelaySamples = BufferSize - StartSample;
			const int32 SecondBlockDelaySamples = DelaySamples - FirstBlockDelaySamples;
			const int32 FirstBlockOutputSamples = AlignIndex(((float)FirstBlockDelaySamples / (float)DelaySamples) * OutputSamples);
			const int32 SecondBlockOuptutSamples = OutputSamples - FirstBlockOutputSamples;
		
			ReadBlockInternal(StartSample, FirstBlockDelaySamples, FirstBlockOutputSamples, OutputPtr);
			ReadBlockInternal(0, SecondBlockDelaySamples, SecondBlockOuptutSamples, OutputPtr + FirstBlockOutputSamples);
		}
		else
		{
			ReadBlockInternal(StartSample, EndSample - StartSample, OutputSamples, OutputPtr);
		}
	}

	void FInterpolatedMultiTapDelay::Reset()
	{
		const int32 NumElements = DelayLine.Num();
		DelayLine.Reset(NumElements);
		DelayLine.AddZeroed(NumElements);
	}

	bool FInterpolatedMultiTapDelay::IsInitialized() const
	{
		return DelayLine.GetAllocatedSize() > 0 && MsToSamples > 1.f;
	}

	void FInterpolatedMultiTapDelay::ReadBlockInternal(const int32 StartSample, const int32 SamplesToRead, const int32 NumOutputSamples, float* OutSamples)
	{
		if (NumOutputSamples == 0 || SamplesToRead == 0)
		{
			return;
		}
		
		check(StartSample + SamplesToRead <= DelayLine.Num())
		check(NumOutputSamples % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER == 0)

		const float SampleStride = (float)SamplesToRead / (float)NumOutputSamples;

		// protect against overflows while wrapping without branching in the main loop
		int32 MainLoopOutputSamples = NumOutputSamples;
		int32 MainLoopReadSamples = SamplesToRead;
		
		if (StartSample + SamplesToRead == DelayLine.Num())
		{
			MainLoopOutputSamples -= AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER;
			MainLoopReadSamples = FMath::RoundToInt32(MainLoopOutputSamples / SampleStride);
		}

		ArrayInterpolate(&DelayLine[StartSample], OutSamples, MainLoopReadSamples, MainLoopOutputSamples);

		float SampleIndex = MainLoopReadSamples;
		
		// wrap in the loop only if close to wrapping: protects against rounding up past last index
		for (int32 OutputIndex = MainLoopOutputSamples; OutputIndex < NumOutputSamples; OutputIndex++)
		{
			const int32 LeftSample = FMath::FloorToInt32(SampleIndex);
			int32 RightSample = FMath::CeilToInt32(SampleIndex);
			
			if (RightSample >= DelayLine.Num())
			{
				RightSample -= DelayLine.Num();
			}
			
			const float Frac = SampleIndex - LeftSample;
			OutSamples[OutputIndex] = (Frac * DelayLine[LeftSample]) + ((1.f - Frac) * DelayLine[RightSample]);
			
			SampleIndex += SampleStride;
		}
	}

	int32 FInterpolatedMultiTapDelay::AlignIndex(const int32 InIndex) const
	{
		return InIndex - (InIndex % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER);
	}
}