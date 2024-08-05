// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieSceneExecutionTokens.h"
#include "TimeSources/PropertyAnimatorCoreSequencerTimeSource.h"

struct FMovieSceneAnimatorExecutionToken : IMovieSceneExecutionToken
{
	FMovieSceneAnimatorExecutionToken(uint8 InChannel)
		: Channel(InChannel)
	{}

	//~ Begin IMovieSceneExecutionToken
	virtual void Execute(const FMovieSceneContext& InContext, const FMovieSceneEvaluationOperand& InOperand, FPersistentEvaluationData& InPersistentData, IMovieScenePlayer& InPlayer) override
	{
		const double EvaluatedTime = InContext.GetTime().AsDecimal() / InContext.GetFrameRate().AsDecimal();
		UPropertyAnimatorCoreSequencerTimeSource::OnAnimatorTimeEvaluated.Broadcast(Channel, EvaluatedTime);
	}
	//~ End IMovieSceneExecutionToken

private:
	uint8 Channel = 0;
};
