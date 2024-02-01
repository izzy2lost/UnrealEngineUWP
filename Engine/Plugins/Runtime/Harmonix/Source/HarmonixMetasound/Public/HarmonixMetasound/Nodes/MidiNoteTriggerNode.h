// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MidiNoteTriggerNode
{
	const HARMONIXMETASOUND_API Metasound::FNodeClassName& GetClassName();
	HARMONIXMETASOUND_API int32 GetCurrentMajorVersion();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(Enable);
		DECLARE_METASOUND_PARAM_EXTERN(MidiStream);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(NoteOnTrigger);
		DECLARE_METASOUND_PARAM_EXTERN(NoteOffTrigger);
		DECLARE_METASOUND_PARAM_EXTERN(MidiNoteNumber);
		DECLARE_METASOUND_PARAM_EXTERN(MidiVelocity);
	}
}