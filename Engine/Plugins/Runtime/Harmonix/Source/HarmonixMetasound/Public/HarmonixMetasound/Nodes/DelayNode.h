// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::DelayNode
{
	namespace Constants
	{
		inline constexpr int32 NumChannels = 2;
		inline constexpr float MaxDelayTime = 2500.0f;
	}

	namespace Inputs
	{
		EXTERN_METASOUND_PARAM(AudioLeft);
		EXTERN_METASOUND_PARAM(AudioRight);
		EXTERN_METASOUND_PARAM(MidiClock)
		EXTERN_METASOUND_PARAM(DelayTimeType);
		EXTERN_METASOUND_PARAM(DelayTime);
		EXTERN_METASOUND_PARAM(Feedback);
		EXTERN_METASOUND_PARAM(DryLevel);
		EXTERN_METASOUND_PARAM(WetLevel);
		EXTERN_METASOUND_PARAM(WetFilterEnabled);
		EXTERN_METASOUND_PARAM(FeedbackFilterEnabled);
		EXTERN_METASOUND_PARAM(FilterType);
		EXTERN_METASOUND_PARAM(FilterCutoff);
		EXTERN_METASOUND_PARAM(FilterQ);
		EXTERN_METASOUND_PARAM(LFOEnabled);
		EXTERN_METASOUND_PARAM(LFOTimeType);
		EXTERN_METASOUND_PARAM(LFOFrequency);
		EXTERN_METASOUND_PARAM(LFODepth);
		EXTERN_METASOUND_PARAM(StereoType);
		EXTERN_METASOUND_PARAM(StereoSpreadLeft);
		EXTERN_METASOUND_PARAM(StereoSpreadRight);
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM(AudioLeft);
		EXTERN_METASOUND_PARAM(AudioRight);
	}
}
