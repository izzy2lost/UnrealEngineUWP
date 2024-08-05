// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneAnimatorExecutionToken.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneAnimatorEvalTemplate.generated.h"

USTRUCT()
struct FMovieSceneAnimatorEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneAnimatorEvalTemplate() = default;

	explicit FMovieSceneAnimatorEvalTemplate(uint8 InChannel)
		: Channel(InChannel)
	{}

	//~ Begin FMovieSceneEvalTemplate
	virtual UScriptStruct& GetScriptStructImpl() const override
	{
		return *StaticStruct();
	}

	virtual void Evaluate(const FMovieSceneEvaluationOperand& InOperand, const FMovieSceneContext& InContext, const FPersistentEvaluationData& InPersistentData, FMovieSceneExecutionTokens& InExecutionTokens) const override
	{
		FMovieSceneAnimatorExecutionToken ExecutionToken(Channel);
		InExecutionTokens.Add(MoveTemp(ExecutionToken));
	}
	//~ End FMovieSceneEvalTemplate

private:
	uint8 Channel = 0;
};
