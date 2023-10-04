// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphDeferredPassNode.h"
#include "Graph/Renderers/MovieGraphDeferredPass.h"

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> UMovieGraphDeferredRenderPassNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphDeferredPass>();
}

UMovieGraphDeferredRenderPassNode::UMovieGraphDeferredRenderPassNode()
	: SpatialSampleCount(1)
	, AntiAliasingMethod(EAntiAliasingMethod::AAM_TSR)
	, ViewModeIndex(VMI_Lit)
{
}

#if WITH_EDITOR
FText UMovieGraphDeferredRenderPassNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "DeferredRenderPassGraphNode_Description", "Deferred Renderer");
}
#endif

FString UMovieGraphDeferredRenderPassNode::GetRendererNameImpl() const
{
	static const FString RendererNameImpl(TEXT("Deferred"));
	return RendererNameImpl;
}

EViewModeIndex UMovieGraphDeferredRenderPassNode::GetViewModeIndex() const
{
	return ViewModeIndex;
}