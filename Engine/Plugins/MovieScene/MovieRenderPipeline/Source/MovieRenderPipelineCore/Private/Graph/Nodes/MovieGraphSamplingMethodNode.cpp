// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphSamplingMethodNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphLinearTimeStep.h"
#include "Styling/AppStyle.h"

UMovieGraphSamplingMethodNode::UMovieGraphSamplingMethodNode()
	: SamplingMethodClass(UMovieGraphLinearTimeStep::StaticClass())
	, TemporalSampleCount(1)
	, SampleCount(1)
{
}

EMovieGraphBranchRestriction UMovieGraphSamplingMethodNode::GetBranchRestriction() const
{
	return EMovieGraphBranchRestriction::Globals;
}

#if WITH_EDITOR
FText UMovieGraphSamplingMethodNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText SamplingMethodNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_SamplingMethod", "Sampling Method");
	return SamplingMethodNodeName;
}

FText UMovieGraphSamplingMethodNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "SamplingMethodGraphNode_Category", "Rendering");
}

FLinearColor UMovieGraphSamplingMethodNode::GetNodeTitleColor() const
{
	static const FLinearColor SamplingMethodNodeColor = FLinearColor(0.572f, 0.274f, 1.f);
	return SamplingMethodNodeColor;
}

FSlateIcon UMovieGraphSamplingMethodNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon SamplingMethodPresetIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "DerivedData.Cache.Statistics");

	OutColor = FLinearColor::White;
	return SamplingMethodPresetIcon;
}
#endif // WITH_EDITOR