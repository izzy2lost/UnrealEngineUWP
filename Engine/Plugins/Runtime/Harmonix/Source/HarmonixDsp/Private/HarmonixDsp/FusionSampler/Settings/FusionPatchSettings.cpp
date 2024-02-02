// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/FusionSampler/Settings/FusionPatchSettings.h"

FFusionPatchSettings::FFusionPatchSettings()
	: VolumeDb(0.0f)
	, DownPitchBendCents(-200.0f)
	, UpPitchBendCents(200.0f)
	, FineTuneCents(0.0f)
	, StartPointOffsetMs(0.0f)
	, MaxVoices(32)
	, KeyzoneSelectMode(EKeyzoneSelectMode::Layers)
{
	Lfos.Pan().Target = ELfoTarget::Pan;
	Lfos.Pitch().Target = ELfoTarget::Pitch;
}