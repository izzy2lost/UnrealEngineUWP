// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphImagePassBaseNode.h"
#include "Graph/MoviePipelineRenderLayerSubsystem.h" 

UMovieGraphImagePassBaseNode::UMovieGraphImagePassBaseNode()
{
	ShowFlags = CreateDefaultSubobject<UMovieGraphShowFlags>(TEXT("ShowFlags"));
}

void UMovieGraphImagePassBaseNode::SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData)
{
	// To make the implementation simpler, we make one instance of FMovieGraphDeferredRenderPas
	// per camera, and per render layer. These objects can pull from common pools to share state,
	// which gives us a better overview of how many resources are being used by MRQ.
	for (const FMovieGraphRenderPassLayerData& LayerData : InSetupData.Layers)
	{
		// Each node can change which renderer instance actually gets created.
		TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> RendererInstance = CreateInstance();
		if(RendererInstance)
		{
			RendererInstance->Setup(InSetupData.Renderer, this, LayerData);
			CurrentInstances.Add(MoveTemp(RendererInstance));
		}
	}
}

void UMovieGraphImagePassBaseNode::TeardownImpl()
{
	// We don't need to flush the rendering commands as we assume the MovieGraph
	// Renderer has already done that once, so all data for all passes should
	// have been submitted to the GPU (and subsequently read back) by now.
	for (TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase>& Instance : CurrentInstances)
	{
		Instance->Teardown();
	}
	CurrentInstances.Reset();
}

void UMovieGraphImagePassBaseNode::RenderImpl(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData)
{
	for (const TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase>& Instance : CurrentInstances)
	{
		UMoviePipelineRenderLayerSubsystem* LayerSubsystem =
			Instance->GetRenderer()->GetWorld()->GetSubsystem<UMoviePipelineRenderLayerSubsystem>();
		
		// Apply all modifiers in the evaluated graph
		if (LayerSubsystem)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MRQ::DeferredRender::ActivateRenderLayer);
			LayerSubsystem->SetActiveRenderLayerByName(Instance->GetBranchName());
		}
		
		Instance->Render(InFrameTraversalContext, InTimeData);

		// Revert all modifiers
		if (LayerSubsystem)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MRQ::DeferredRender::RevertRenderLayer);
			LayerSubsystem->ClearActiveRenderLayer();
		}
	}
}

void UMovieGraphImagePassBaseNode::GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const
{
	for (const TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase>& Instance : CurrentInstances)
	{
		Instance->GatherOutputPasses(OutExpectedPasses);
	}
}

EViewModeIndex UMovieGraphImagePassBaseNode::GetViewModeIndex() const
{
	return VMI_Lit;
}

FEngineShowFlags UMovieGraphImagePassBaseNode::GetShowFlags() const
{
	return ShowFlags->GetShowFlags();
}

TArray<uint32> UMovieGraphImagePassBaseNode::GetDefaultShowFlags() const
{
	return TArray<uint32>();
}

void UMovieGraphImagePassBaseNode::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	
	UMovieGraphImagePassBaseNode* This = CastChecked<UMovieGraphImagePassBaseNode>(InThis);
	for (TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase>& Instance : This->CurrentInstances)
	{
		Instance->AddReferencedObjects(Collector);
	}
}