// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieSceneExecutionTokens.h"
#include "Sequencer/MovieSceneAnimatorSection.h"
#include "Sequencer/MovieSceneAnimatorTypes.h"
#include "TimeSources/PropertyAnimatorCoreSequencerTimeSource.h"

/** Used to restore state back to previous when outside section */
struct FMovieSceneAnimatorPreAnimatedTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
{
	explicit FMovieSceneAnimatorPreAnimatedTokenProducer(uint8 InChannel)
		: Channel(InChannel)
	{}

	//~ Begin IMovieScenePreAnimatedGlobalTokenProducer
	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
	{
		struct FMovieSceneAnimatorPreAnimatedGlobalToken : IMovieScenePreAnimatedGlobalToken
		{
			FMovieSceneAnimatorPreAnimatedGlobalToken(uint8 InChannel)
				: Channel(InChannel)
			{}

			//~ Begin IMovieScenePreAnimatedGlobalToken
			virtual void RestoreState(const UE::MovieScene::FRestoreStateParams& InParams) override
			{
				UPropertyAnimatorCoreSequencerTimeSource::OnAnimatorTimeEvaluated.Broadcast(Channel, TOptional<double>(), TOptional<float>());
			}
			//~ End IMovieScenePreAnimatedGlobalToken

		private:
			uint8 Channel = 0;
		};

		return FMovieSceneAnimatorPreAnimatedGlobalToken(Channel);
	}
	//~ End IMovieScenePreAnimatedGlobalTokenProducer

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FMovieSceneAnimatorPreAnimatedTokenProducer>();
	}

private:
	uint8 Channel = 0;
};

/** Used to evaluate active section */
struct FMovieSceneAnimatorExecutionToken : IMovieSceneExecutionToken
{
	FMovieSceneAnimatorExecutionToken(const FMovieSceneAnimatorSectionData& InSectionData)
		: SectionData(InSectionData)
	{}

	//~ Begin IMovieSceneExecutionToken
	virtual void Execute(const FMovieSceneContext& InContext, const FMovieSceneEvaluationOperand& InOperand, FPersistentEvaluationData& InPersistentData, IMovieScenePlayer& InPlayer) override
	{
		InPlayer.SavePreAnimatedState(FMovieSceneAnimatorPreAnimatedTokenProducer::GetAnimTypeID(), FMovieSceneAnimatorPreAnimatedTokenProducer(SectionData.Channel));

		double EvaluatedTime = InContext.GetTime().AsDecimal();

		if (SectionData.bUseSectionTime && SectionData.Section)
		{
			EvaluatedTime -= SectionData.Section->GetInclusiveStartFrame().Value;
		}

		EvaluatedTime /= InContext.GetFrameRate().AsDecimal();

		const float Magnitude = SectionData.Section ? SectionData.Section->EvaluateEasing(InContext.GetTime()) : 1;

		UPropertyAnimatorCoreSequencerTimeSource::OnAnimatorTimeEvaluated.Broadcast(SectionData.Channel, EvaluatedTime, Magnitude);
	}
	//~ End IMovieSceneExecutionToken

private:
	FMovieSceneAnimatorSectionData SectionData;
};
