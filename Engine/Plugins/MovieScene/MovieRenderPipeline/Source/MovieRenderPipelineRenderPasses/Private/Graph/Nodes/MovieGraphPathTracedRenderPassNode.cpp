// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphPathTracedRenderPassNode.h"
#include "Graph/Renderers/MovieGraphPathTracerPass.h"
#include "Engine/EngineBaseTypes.h"
#include "ShowFlags.h"

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> UMovieGraphPathTracedRenderPassNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphPathTracerPass>();
}

UMovieGraphPathTracedRenderPassNode::UMovieGraphPathTracedRenderPassNode()
	: SpatialSampleCount(1)
	, bDisableToneCurve(false)
	, bAllowOCIO(true)
	, bWriteAllSamples(false)
{
	ShowFlags->ApplyDefaultShowFlagValue(VMI_PathTracing, true);
	// TODO: Showflag for SetMotionBlur()?
}

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

bool UMovieGraphPathTracedRenderPassNode::GetWriteAllSamples() const
{
	return bWriteAllSamples;
}

TArray<FMoviePipelinePostProcessPass> UMovieGraphPathTracedRenderPassNode::GetAdditionalPostProcessMaterials() const
{
	return AdditionalPostProcessMaterials;
}

int32 UMovieGraphPathTracedRenderPassNode::GetNumSpatialSamples() const
{
	return SpatialSampleCount;
}

bool UMovieGraphPathTracedRenderPassNode::GetDisableToneCurve() const
{
	return bDisableToneCurve;
}

bool UMovieGraphPathTracedRenderPassNode::GetAllowOCIO() const
{
	return bAllowOCIO;
}
