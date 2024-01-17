// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundNodeInterface.h"
#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MorphingLFO
{
	template<typename OutputDataType>
	const Metasound::FNodeClassName& GetClassName()
	{
		static const Metasound::FNodeClassName ClassName
		{
			HarmonixNodeNamespace,
			TEXT("MorphingLFO"),
			Metasound::GetMetasoundDataTypeName<OutputDataType>()
		};
		return ClassName;
	}
	
	namespace Inputs
	{
		EXTERN_METASOUND_PARAM(MidiClock);
		EXTERN_METASOUND_PARAM(LFOSyncType);
		EXTERN_METASOUND_PARAM(LFOFrequency);
		EXTERN_METASOUND_PARAM(LFOInvert);
		EXTERN_METASOUND_PARAM(LFOShape)
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM(LFO);
	}
}