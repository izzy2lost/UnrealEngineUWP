// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieGraphCoreTimeStep.h"

#include "MovieGraphStochasticTimeStep.generated.h"

/**
 * Advances time in a non-linear (stochastic) manner within the range of time that represents the current frame. Useful for
 * path traced images which have a varying amount of noise (and thus need a varying amount of samples) based on their
 * content.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Stochastic Time Step"))
class MOVIERENDERPIPELINECORE_API UMovieGraphStochasticTimeStep : public UMovieGraphCoreTimeStep
{
	GENERATED_BODY()

public:
	UMovieGraphStochasticTimeStep() = default;

protected:
	virtual int32 GetNextTemporalRangeIndex() const override;
	virtual int32 GetTemporalSampleCount() const override;
	virtual bool IsLastTemporalSample() const override;
};
