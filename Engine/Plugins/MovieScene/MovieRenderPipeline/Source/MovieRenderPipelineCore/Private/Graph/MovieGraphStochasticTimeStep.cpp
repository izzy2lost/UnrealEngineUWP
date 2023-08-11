// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphStochasticTimeStep.h"

#include "Graph/Nodes/MovieGraphSamplingMethodNode.h"

int32 UMovieGraphStochasticTimeStep::GetNextTemporalRangeIndex() const
{
	// Instead of going forwards through the temporal ranges, iterate them in reverse (as an example)
	// TODO: Actual implementation
	return (CurrentFrameData.TemporalRanges.Num() - 1) - CurrentFrameData.TemporalSampleIndex;
}

int32 UMovieGraphStochasticTimeStep::GetTemporalSampleCount() const
{
	constexpr bool bIncludeCDOs = true;
	const UMovieGraphSamplingMethodNode* SamplingMethod =
		CurrentFrameData.EvaluatedConfig->GetSettingForBranch<UMovieGraphSamplingMethodNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs);

	return SamplingMethod->SampleCount;
}

bool UMovieGraphStochasticTimeStep::IsLastTemporalSample() const
{
	// TODO: Return early based on wall time or noise threshold?
	return Super::IsLastTemporalSample();
}
