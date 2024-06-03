// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphFileOutputNode.h"

#include "Graph/MovieGraphConfig.h"

int32 UMovieGraphFileOutputNode::GetNumFileOutputNodes(const UMovieGraphEvaluatedConfig& InEvaluatedConfig, const FName& InBranchName)
{
	return InEvaluatedConfig.GetSettingsForBranch(UMovieGraphFileOutputNode::StaticClass(), InBranchName, false /*bIncludeCDOs*/, false /*bExactMatch*/).Num();
}

TArray<FMovieGraphPassData> UMovieGraphFileOutputNode::GetCompositedPasses(UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData)
{
	// Gather the passes that need to be composited
	TArray<FMovieGraphPassData> CompositedPasses;

	for (const FMovieGraphPassData& RenderData : InRawFrameData->ImageOutputData)
	{
		const UE::MovieGraph::FMovieGraphSampleState* Payload = RenderData.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		check(Payload);
		if (!Payload->bCompositeOnOtherRenders)
		{
			continue;
		}

		FMovieGraphPassData CompositePass;
		CompositePass.Key = RenderData.Key;
		CompositePass.Value = RenderData.Value->CopyImageData();
		CompositedPasses.Add(MoveTemp(CompositePass));
	}

	// Sort composited passes if multiple were found. Passes with a higher sort order go to the end of the array so they
	// get composited on top of passes with a lower sort order.
	CompositedPasses.Sort([](const FMovieGraphPassData& PassA, const FMovieGraphPassData& PassB)
	{
		const UE::MovieGraph::FMovieGraphSampleState* PayloadA = PassA.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		const UE::MovieGraph::FMovieGraphSampleState* PayloadB = PassB.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		check(PayloadA);
		check(PayloadB);

		return PayloadA->CompositingSortOrder < PayloadB->CompositingSortOrder;
	});

	return CompositedPasses;
}
