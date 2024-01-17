// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::Peak
{
	const HARMONIXMETASOUND_API Metasound::FNodeClassName& GetClassName();
	
	namespace Inputs
	{
		EXTERN_METASOUND_PARAM(AudioMono);
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM(Peak);
	}
}
