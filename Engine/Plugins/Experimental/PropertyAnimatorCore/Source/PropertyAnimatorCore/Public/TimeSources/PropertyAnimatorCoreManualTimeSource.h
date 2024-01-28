// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.h"
#include "PropertyAnimatorCoreManualTimeSource.generated.h"

UCLASS()
class PROPERTYANIMATORCORE_API UPropertyAnimatorCoreManualTimeSource : public UPropertyAnimatorCoreTimeSourceBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreManualTimeSource()
		: UPropertyAnimatorCoreTimeSourceBase(TEXT("Manual"))
	{}

	UFUNCTION()
	void SetCustomTime(float InTime);
	float GetCustomTime() const
	{
		return CustomTime;
	}

	//~ Begin UPropertyAnimatorTimeSourceBase
	virtual double GetTimeElapsed() override;
	virtual bool IsTimeSourceReady() const override;
	//~ End UPropertyAnimatorTimeSourceBase

protected:
	/** Allows you to drive controllers with this float */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCustomTime", Getter="GetCustomTime", Category="Animator")
	float CustomTime = 0.f;
};