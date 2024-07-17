// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphLinearTimeStep.h"
#include "MovieRenderPipelineCoreModule.h"

#include "Graph/Nodes/MovieGraphSamplingMethodNode.h"

int32 UMovieGraphLinearTimeStep::GetNextTemporalRangeIndex() const
{
	// Linear time step just steps through the temporal ranges in order
	return CurrentFrameData.TemporalSampleIndex;
}

int32 UMovieGraphLinearTimeStep::GetTemporalSampleCount() const
{
	constexpr bool bIncludeCDOs = true;
	const UMovieGraphSamplingMethodNode* SamplingMethod =
		CurrentFrameData.EvaluatedConfig->GetSettingForBranch<UMovieGraphSamplingMethodNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs);
	if (SamplingMethod->TemporalSampleCount <= 0)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Sampling Method > Temporal Sample Count was zero, this is not allowed. Forcing value to 1!"));
	}

	return FMath::Max(SamplingMethod->TemporalSampleCount, 1);
}
