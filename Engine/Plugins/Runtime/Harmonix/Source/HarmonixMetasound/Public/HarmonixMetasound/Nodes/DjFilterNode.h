// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::DjFilter
{
	const HARMONIXMETASOUND_API Metasound::FNodeClassName& GetClassName();
	
	namespace Inputs
	{
		EXTERN_METASOUND_PARAM(AudioMono);
		EXTERN_METASOUND_PARAM(Amount);
		EXTERN_METASOUND_PARAM(Resonance);
		EXTERN_METASOUND_PARAM(LowPassMinFrequency);
		EXTERN_METASOUND_PARAM(LowPassMaxFrequency);
		EXTERN_METASOUND_PARAM(HighPassMinFrequency);
		EXTERN_METASOUND_PARAM(HighPassMaxFrequency);
		EXTERN_METASOUND_PARAM(DeadZoneSize);
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM(AudioMono);
	}
}
