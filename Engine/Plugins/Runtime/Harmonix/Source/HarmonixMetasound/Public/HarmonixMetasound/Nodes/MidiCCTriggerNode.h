// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundNodeInterface.h"
#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MidiCCTriggerNode
{
	const HARMONIXMETASOUND_API Metasound::FNodeClassName& GetClassName();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(Enable);
		DECLARE_METASOUND_PARAM_EXTERN(MidiTrackNumber);
		DECLARE_METASOUND_PARAM_EXTERN(MidiChannelNumber);
		DECLARE_METASOUND_PARAM_EXTERN(InputMidiControllerID);
		DECLARE_METASOUND_PARAM_EXTERN(MidiStream);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(OutputControlChangeValueInt32);
		DECLARE_METASOUND_PARAM_EXTERN(OutputControlChangeValueFloat);
		DECLARE_METASOUND_PARAM_EXTERN(OutputTrigger);
	}
}