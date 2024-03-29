// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::StepSequencePlayer
{
	HARMONIXMETASOUND_API Metasound::FNodeClassName GetClassName();
	HARMONIXMETASOUND_API int32 GetCurrentMajorVersion();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_EXTERN(SequenceAsset, "Step Sequence Asset", "Step sequence to play.");
		DECLARE_METASOUND_PARAM_EXTERN(VelocityMultiplier, "Velocity Multiplier", "Multiplies the current note velocity by this number");
		DECLARE_METASOUND_PARAM_EXTERN(MaxColumns, "Max Columns", "The Maximinum Number of cells to play per step sequence row.");
		DECLARE_METASOUND_PARAM_EXTERN(AdditionalOctaves, "Additional Octaves", "The number of octaves to add to the authored step sequence note.");
		DECLARE_METASOUND_PARAM_EXTERN(StepSizeQuarterNotes, "Step Size Quarter Notes", "The size, in quarter notes, of each step");
		DECLARE_METASOUND_PARAM_EXTERN(ActivePage, "Active Page", "The page of the step sequence to play (1 indexed)");
		DECLARE_METASOUND_PARAM_EXTERN(AutoPage, "Auto Page", "Whether to calculate the page of the step sequence based on current position");
		DECLARE_METASOUND_PARAM_EXTERN(AutoPagePlaysBlankPages, "Auto Page Plays Blank Pages", "If autopaging, should blank pages be played?");
		DECLARE_METASOUND_PARAM_ALIAS(Transport);
		DECLARE_METASOUND_PARAM_ALIAS(MidiClock);
		DECLARE_METASOUND_PARAM_ALIAS(Speed);
		DECLARE_METASOUND_PARAM_ALIAS(Loop);
		DECLARE_METASOUND_PARAM_ALIAS(Enabled);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_ALIAS(MidiStream);
	}
}
