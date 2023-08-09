// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphPathTracedRenderPassNode.h"

#include "Engine/EngineBaseTypes.h"
#include "ShowFlags.h"

#if WITH_EDITOR
FText UMovieGraphPathTracedRenderPassNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "PathTracedRenderPassGraphNode_Description", "Path Traced Renderer");
}
#endif

void UMovieGraphPathTracedRenderPassNode::SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData)
{
	Super::SetupImpl(InSetupData);
	
	// Hide the progress display during the render
	if (IConsoleVariable* ProgressDisplayCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.ProgressDisplay")))
	{
		bOriginalProgressDisplayCvarValue = ProgressDisplayCvar->GetBool();
		ProgressDisplayCvar->Set(false);
	}
}

void UMovieGraphPathTracedRenderPassNode::TeardownImpl()
{
	Super::TeardownImpl();

	// Restore the original setting for the progress display
	if (IConsoleVariable* ProgressDisplayCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.ProgressDisplay")))
	{
		ProgressDisplayCvar->Set(bOriginalProgressDisplayCvarValue);
	}
}

FString UMovieGraphPathTracedRenderPassNode::GetRendererNameImpl() const
{
	static const FString RendererNameImpl(TEXT("PathTraced"));
	return RendererNameImpl;
}

EViewModeIndex UMovieGraphPathTracedRenderPassNode::GetViewModeIndex() const
{
	return VMI_PathTracing;
}

FEngineShowFlags UMovieGraphPathTracedRenderPassNode::GetShowFlags() const
{
	FEngineShowFlags ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
	ShowFlags.SetPathTracing(true);
	// TODO: SetMotionBlur()?
	
	return ShowFlags;
}