// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreSequencerTimeSource.h"

UPropertyAnimatorCoreSequencerTimeSource::FOnAnimatorTimeEvaluated UPropertyAnimatorCoreSequencerTimeSource::OnAnimatorTimeEvaluated;

bool UPropertyAnimatorCoreSequencerTimeSource::UpdateEvaluationData(FPropertyAnimatorCoreTimeSourceEvaluationData& OutData)
{
	if (!EvalTime.IsSet() || !EvalMagnitude.IsSet())
	{
		return false;
	}

	OutData.TimeElapsed = EvalTime.GetValue();
	OutData.Magnitude = EvalMagnitude.GetValue();

	return true;
}

void UPropertyAnimatorCoreSequencerTimeSource::OnTimeSourceActive()
{
	Super::OnTimeSourceActive();

	OnAnimatorTimeEvaluated.AddUObject(this, &UPropertyAnimatorCoreSequencerTimeSource::OnSequencerTimeEvaluated);
}

void UPropertyAnimatorCoreSequencerTimeSource::OnTimeSourceInactive()
{
	Super::OnTimeSourceInactive();

	OnAnimatorTimeEvaluated.RemoveAll(this);
}

void UPropertyAnimatorCoreSequencerTimeSource::SetChannel(uint8 InChannel)
{
	ChannelData.Channel = InChannel;
}

void UPropertyAnimatorCoreSequencerTimeSource::OnSequencerTimeEvaluated(uint8 InChannel, const TOptional<double>& InTimeEval, const TOptional<float>& InMagnitudeEval)
{
	if (ChannelData.Channel == InChannel)
	{
		EvalTime = InTimeEval;
		EvalMagnitude = InMagnitudeEval;
	}
}
