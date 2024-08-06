// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreSequencerTimeSource.h"

UPropertyAnimatorCoreSequencerTimeSource::FOnAnimatorTimeEvaluated UPropertyAnimatorCoreSequencerTimeSource::OnAnimatorTimeEvaluated;

double UPropertyAnimatorCoreSequencerTimeSource::GetTimeElapsed()
{
	return EvalTime.Get(0);
}

bool UPropertyAnimatorCoreSequencerTimeSource::IsTimeSourceReady() const
{
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

void UPropertyAnimatorCoreSequencerTimeSource::OnSequencerTimeEvaluated(uint8 InChannel, double InTimeEval)
{
	if (ChannelData.Channel == InChannel)
	{
		EvalTime = InTimeEval;
	}
}
