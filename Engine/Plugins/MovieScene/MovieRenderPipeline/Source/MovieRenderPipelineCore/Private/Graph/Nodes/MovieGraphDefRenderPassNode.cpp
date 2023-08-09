// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphDefRenderPassNode.h"

#include "Engine/EngineBaseTypes.h"
#include "ShowFlags.h"

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
	return VMI_Lit;
}

FEngineShowFlags UMovieGraphDeferredRenderPassNode::GetShowFlags() const
{
	FEngineShowFlags ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
	return ShowFlags;
}