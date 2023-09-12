// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AlignedBuffer.h"

namespace Audio
{
	/**
	 *  Delay line supporting multiple interpolated tap reads before advancing
	 *  InBufferSizeSamples must be >= the size of the output buffer passed into Read
	 *  If the delay length can be less than the output buffer size, Advance should be called before Read
	 */
	class FInterpolatedMultiTapDelay
	{
	public:
		FInterpolatedMultiTapDelay() = default;
		
		SIGNALPROCESSING_API void Init(const int32 InBufferSizeSamples, const float InSampleRate);

		SIGNALPROCESSING_API void Advance(const FAlignedFloatBuffer& InSamples);
		SIGNALPROCESSING_API void Read(const float StartDelayMSec, const float EndDelayMSec, FAlignedFloatBuffer& OutSamples);
		SIGNALPROCESSING_API void Reset();
		SIGNALPROCESSING_API bool IsInitialized() const;

	private:
		void ReadBlockInternal(const int32 StartSample, const int32 SamplesToRead, const int32 NumOutputSamples, float* OutSamples);
		FORCEINLINE int32 AlignIndex(const int32 InIndex) const;
		
		int32 WriteIndex = 0;
		float MsToSamples = 1.f;

		FAlignedFloatBuffer DelayLine;
	};	
}

