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
		DECLARE_METASOUND_PARAM_EXTERN(MidiClock);
		DECLARE_METASOUND_PARAM_EXTERN(LFOSyncType);
		DECLARE_METASOUND_PARAM_EXTERN(LFOFrequency);
		DECLARE_METASOUND_PARAM_EXTERN(LFOInvert);
		DECLARE_METASOUND_PARAM_EXTERN(LFOShape)
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(LFO);
	}
}