// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MidiNoteTranspose
{
	HARMONIXMETASOUND_API const Metasound::FNodeClassName& GetClassName();
	HARMONIXMETASOUND_API int32 GetCurrentMajorVersion();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(Enable);
		DECLARE_METASOUND_PARAM_EXTERN(MidiStream);
		DECLARE_METASOUND_PARAM_EXTERN(Transposition);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(MidiStream);
	}
}
