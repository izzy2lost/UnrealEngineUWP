// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/ViewDensity.h"
#include "HAL/IConsoleManager.h"

namespace UE::Sequencer
{

float GSequencerOutlinerRelaxedHeight = 22.f;
FAutoConsoleVariableRef CVarSequencerOutlinerRelaxedHeight(
	TEXT("Sequencer.Outliner.RelaxedHeight"),
	GSequencerOutlinerRelaxedHeight,
	TEXT("(Default: 22.f. Defines the height of outliner items when in relaxed mode.")
	);
float GSequencerOutlinerExpandedHeight = 28.f;
FAutoConsoleVariableRef CVarSequencerOutlinerExpandedHeight(
	TEXT("Sequencer.Outliner.ExpandedHeight"),
	GSequencerOutlinerExpandedHeight,
	TEXT("(Default: 28.f. Defines the height of outliner items when in expanded mode.")
	);


FViewDensityInfo::FViewDensityInfo()
	: UniformHeight(GetUniformHeight(EViewDensity::Relaxed))
	, Density(EViewDensity::Relaxed)
{}

FViewDensityInfo::FViewDensityInfo(EViewDensity InViewDensity)
	: UniformHeight(GetUniformHeight(InViewDensity))
	, Density(InViewDensity)
{}

TOptional<float> FViewDensityInfo::GetUniformHeight(EViewDensity Density)
{
	switch(Density)
	{
		case EViewDensity::Relaxed:  return GSequencerOutlinerRelaxedHeight;
		case EViewDensity::Expanded: return GSequencerOutlinerExpandedHeight;
		default:                     return TOptional<float>();
	}
}


} // namespace UE::Sequencer