// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundEnvelopeFollowerTypes.h"
#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MultibandAnalyzer
{
	const HARMONIXMETASOUND_API Metasound::FNodeClassName& GetClassName();

	struct FSettings
	{
		bool Enable{ true };
		TArray<float> CrossoverFrequencies{ 200, 400, 800 };
		bool ApplySmoothing{ true };
		Metasound::FTime AttackTime = Metasound::FTime::FromMilliseconds(10);
		Metasound::FTime ReleaseTime = Metasound::FTime::FromMilliseconds(10);
		Metasound::EEnvelopePeakMode PeakMode = Metasound::EEnvelopePeakMode::Peak;
	};
	
	namespace Inputs
	{
		EXTERN_METASOUND_PARAM(Enable);
		EXTERN_METASOUND_PARAM(AudioMono);
		EXTERN_METASOUND_PARAM(CrossoverFrequencies);
		EXTERN_METASOUND_PARAM(ApplySmoothing);
		EXTERN_METASOUND_PARAM(AttackTime);
		EXTERN_METASOUND_PARAM(ReleaseTime);
		EXTERN_METASOUND_PARAM(PeakMode);
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM(BandLevels);
	}
}
