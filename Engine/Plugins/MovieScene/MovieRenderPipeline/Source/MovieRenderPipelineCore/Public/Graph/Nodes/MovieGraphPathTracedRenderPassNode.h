// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphCoreRenderPassNode.h"

#include "MovieGraphPathTracedRenderPassNode.generated.h"

/** A render node which uses the path tracer. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphPathTracedRenderPassNode : public UMovieGraphCoreRenderPassNode
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
#endif

	// UMovieGraphRenderPassNode Interface
	virtual void SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData) override;
	virtual void TeardownImpl() override;
	// ~UMovieGraphRenderPassNode Interface

protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override;
	// ~UMovieGraphRenderPassNode Interface

	// UMovieGraphCoreRenderPassNode Interface
	virtual EViewModeIndex GetViewModeIndex() const override;
	virtual FEngineShowFlags GetShowFlags() const override;
	// ~UMovieGraphCoreRenderPassNode Interface

private:
	/**
	 * The original value of the "r.PathTracing.ProgressDisplay" cvar before the render starts. The progress display
	 * will be hidden during the render.
	 */
	bool bOriginalProgressDisplayCvarValue = false;
};