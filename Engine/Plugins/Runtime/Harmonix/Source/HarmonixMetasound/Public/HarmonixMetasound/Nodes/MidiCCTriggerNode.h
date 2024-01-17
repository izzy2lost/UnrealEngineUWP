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
		EXTERN_METASOUND_PARAM(Enable);
		EXTERN_METASOUND_PARAM(MidiTrackNumber);
		EXTERN_METASOUND_PARAM(MidiChannelNumber);
		EXTERN_METASOUND_PARAM(InputMidiControllerID);
		EXTERN_METASOUND_PARAM(MidiStream);
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM(OutputControlChangeValueInt32);
		EXTERN_METASOUND_PARAM(OutputControlChangeValueFloat);
		EXTERN_METASOUND_PARAM(OutputTrigger);
	}
}