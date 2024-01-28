// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.h"
#include "PropertyAnimatorCoreRangeTimeSource.generated.h"

/**
 * Abstract range time source allowing to start or stop based on time elapsed
 */
UCLASS(Abstract)
class PROPERTYANIMATORCORE_API UPropertyAnimatorCoreRangeTimeSource : public UPropertyAnimatorCoreTimeSourceBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreRangeTimeSource()
		: UPropertyAnimatorCoreTimeSourceBase(NAME_None)
	{}

	UPropertyAnimatorCoreRangeTimeSource(const FName& InSourceName)
		: UPropertyAnimatorCoreTimeSourceBase(InSourceName)
	{}

	void SetUseStartTime(bool bInUse);

	bool GetUseStartTime() const
	{
		return bUseStartTime;
	}

	void SetStartTime(double InStartTime);

	double GetStartTime() const
	{
		return StartTime;
	}

	void SetUseStopTime(bool bInUse);

	bool GetUseStopTime() const
	{
		return bUseStopTime;
	}

	void SetStopTime(double InStopTime);

	double GetStopTime() const
	{
		return StopTime;
	}

	//~ Begin UPropertyAnimatorTimeSourceBase
	virtual bool IsValidTimeElapsed(double InTimeElapsed) const override;
	//~ End UPropertyAnimatorTimeSourceBase

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetUseStartTime", Getter="GetUseStartTime", Category="Animator")
	bool bUseStartTime = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="Animator", meta=(EditCondition="bUseStartTime", EditConditionHides))
	double StartTime = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetUseStopTime", Getter="GetUseStopTime", Category="Animator")
	bool bUseStopTime = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="Animator", meta=(EditCondition="bUseStopTime", EditConditionHides))
	double StopTime = 0;
};