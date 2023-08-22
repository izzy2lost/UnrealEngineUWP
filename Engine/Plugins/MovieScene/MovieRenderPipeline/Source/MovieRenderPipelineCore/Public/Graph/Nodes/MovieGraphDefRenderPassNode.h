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
	UMovieGraphDeferredRenderPassNode();

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
#endif

protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override;
	// ~UMovieGraphRenderPassNode Interface

	// UMovieGraphCoreRenderPassNode Interface
	virtual EViewModeIndex GetViewModeIndex() const override;
	// ~UMovieGraphCoreRenderPassNode Interface

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ViewModeIndex : 1;

	/** The view mode index that will be applied to renders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(EditCondition="bOverride_ViewModeIndex", InvalidEnumValues = "VMI_PathTracing"))
	TEnumAsByte<EViewModeIndex> ViewModeIndex;
};