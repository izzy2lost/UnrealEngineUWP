// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Optional.h"

namespace UE::Sequencer
{

enum class EViewDensity
{
	/** The most dense view type */
	Compact,

	/** A relaxed view type with larger, uniform heights */
	Relaxed,

	/** An expanded view type with even larger, uniform heights and additional information */
	Expanded
};


struct FViewDensityInfo
{
	SEQUENCERCORE_API FViewDensityInfo();
	SEQUENCERCORE_API FViewDensityInfo(EViewDensity InViewDensity);

	SEQUENCERCORE_API static TOptional<float> GetUniformHeight(EViewDensity Density);

	TOptional<float> UniformHeight;

	EViewDensity Density;
};


} // namespace UE::Sequencer