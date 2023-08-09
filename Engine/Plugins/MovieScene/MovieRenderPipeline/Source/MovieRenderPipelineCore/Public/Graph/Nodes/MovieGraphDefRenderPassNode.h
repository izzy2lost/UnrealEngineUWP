// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphCoreRenderPassNode.h"

#include "MovieGraphDefRenderPassNode.generated.h"

/** A render node which uses the deferred renderer. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphDeferredRenderPassNode : public UMovieGraphCoreRenderPassNode
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
#endif

protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override;
	// ~UMovieGraphRenderPassNode Interface

	// UMovieGraphCoreRenderPassNode Interface
	virtual EViewModeIndex GetViewModeIndex() const override;
	virtual FEngineShowFlags GetShowFlags() const override;
	// ~UMovieGraphCoreRenderPassNode Interface
};