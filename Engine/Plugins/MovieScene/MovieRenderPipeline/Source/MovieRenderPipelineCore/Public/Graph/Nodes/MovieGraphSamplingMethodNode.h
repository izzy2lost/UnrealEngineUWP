// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphSamplingMethodNode.generated.h"

/** A node which configures sampling properties for renderers. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphSamplingMethodNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphSamplingMethodNode();

	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	uint8 bOverride_SamplingMethodClass : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	uint8 bOverride_TemporalSampleCount : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	uint8 bOverride_SampleCount : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	uint8 bOverride_NoiseThreshold : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	uint8 bOverride_MaxWallTime : 1;

	/** The type of sampling the render should use. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", NoClear, meta = (EditCondition = "bOverride_SamplingMethodClass", DisplayName = "Sampling Method", MetaClass = "/Script/MovieRenderPipelineCore.MovieGraphTimeStepBase", ShowDisplayNames))
	FSoftClassPath SamplingMethodClass;

	/** The number of temporal samples which should be taken on one frame. Applies only to linear sampling. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Linear Sampling", meta = (UIMin = 1, ClampMin = 1, EditCondition = "bOverride_TemporalSampleCount"))
	int32 TemporalSampleCount;

	/**
	 * The number of samples which should be taken on one frame. The number of samples taken may be less if the max
	 * wall time or noise threshold is reached. Applies only to stochastic sampling.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Stochastic Sampling", meta = (UIMin = 1, ClampMin = 1, EditCondition = "bOverride_SampleCount"))
	int32 SampleCount;

	/**
	 * [UNIMPLEMENTED] The noise threshold that must be reached on the current frame before samples stop being collected.
	 * Samples may stop being collected before the noise threshold is reached if the max wall time is met, or the sample
	 * count is set too low. Applies only to stochastic sampling.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Stochastic Sampling", meta = (UIMin = 0, ClampMin = 0, EditCondition = "bOverride_NoiseThreshold"))
	float NoiseThreshold;

	/**
	 * [UNIMPLEMENTED] The maximum amount of aggregate time (seconds) that can be spent rendering all samples for one frame.
	 * Applies only to stochastic sampling.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Stochastic Sampling", meta = (UIMin = 0, ClampMin = 0, Unit = "s", EditCondition = "bOverride_MaxWallTime"))
	float MaxWallTime;
};