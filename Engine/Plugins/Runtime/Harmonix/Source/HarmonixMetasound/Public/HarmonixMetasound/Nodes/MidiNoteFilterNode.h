// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MidiNoteFilter
{
	HARMONIXMETASOUND_API Metasound::FNodeClassName GetClassName();
	HARMONIXMETASOUND_API int32 GetCurrentMajorVersion();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(Enable);
		DECLARE_METASOUND_PARAM_EXTERN(MidiStream);
		DECLARE_METASOUND_PARAM_EXTERN(MinNoteNumber);
		DECLARE_METASOUND_PARAM_EXTERN(MaxNoteNumber);
		DECLARE_METASOUND_PARAM_EXTERN(MinVelocity);
		DECLARE_METASOUND_PARAM_EXTERN(MaxVelocity);
		DECLARE_METASOUND_PARAM_EXTERN(IncludeOtherEvents);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(MidiStream);
	}
}
