// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.h"
#include "PropertyAnimatorCoreSequencerTimeSource.generated.h"

USTRUCT()
struct FPropertyAnimatorCoreSequencerTimeSourceChannel
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, Category="Animator")
	uint8 Channel = 0;
};

/** Sequencer time source that sync with animator track channel */
UCLASS(MinimalAPI)
class UPropertyAnimatorCoreSequencerTimeSource : public UPropertyAnimatorCoreTimeSourceBase
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnimatorTimeEvaluated, uint8 /** Channel */, double /** EvalTime */)
	static FOnAnimatorTimeEvaluated OnAnimatorTimeEvaluated;

	UPropertyAnimatorCoreSequencerTimeSource()
		: UPropertyAnimatorCoreTimeSourceBase(TEXT("Sequencer"))
	{}

	//~ Begin UPropertyAnimatorTimeSourceBase
	virtual double GetTimeElapsed() override;
	virtual bool IsTimeSourceReady() const override;
	virtual void OnTimeSourceActive() override;
	virtual void OnTimeSourceInactive() override;
	//~ End UPropertyAnimatorTimeSourceBase

	void SetChannel(uint8 InChannel);
	uint8 GetChannel() const
	{
		return ChannelData.Channel;
	}

protected:
	void OnSequencerTimeEvaluated(uint8 InChannel, double InTimeEval);

	/** Channel to sample time from */
	UPROPERTY(EditInstanceOnly, DisplayName="Channel", Category="Animator")
	FPropertyAnimatorCoreSequencerTimeSourceChannel ChannelData;

	/** Last evaluated time received */
	TOptional<double> EvalTime;
};