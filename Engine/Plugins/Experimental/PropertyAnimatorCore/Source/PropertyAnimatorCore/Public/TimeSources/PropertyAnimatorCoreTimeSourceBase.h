// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.generated.h"

class UPropertyAnimatorCoreBase;

/**
 * Abstract base class for time source used by property animators
 * Can be transient or saved to disk if contains user set data
 */
UCLASS(MinimalAPI, Abstract)
class UPropertyAnimatorCoreTimeSourceBase : public UObject
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreSubsystem;

public:
	UPropertyAnimatorCoreTimeSourceBase()
		: UPropertyAnimatorCoreTimeSourceBase(NAME_None)
	{}

	UPropertyAnimatorCoreTimeSourceBase(const FName& InSourceName)
		: TimeSourceName(InSourceName)
	{}

	void ActivateTimeSource();
	void DeactivateTimeSource();

	bool IsTimeSourceActive() const
	{
		return bTimeSourceActive;
	}

	TOptional<double> GetConditionalTimeElapsed();

	/** Get the animator, this time source is on */
	PROPERTYANIMATORCORE_API UPropertyAnimatorCoreBase* GetAnimator() const;

	FName GetTimeSourceName() const
	{
		return TimeSourceName;
	}

	void SetFrameRate(float InFrameRate);
	float GetFrameRate() const
	{
		return FrameRate;
	}

	void SetUseFrameRate(bool bInUseFrameRate);
	bool GetUseFrameRate() const
	{
		return bUseFrameRate;
	}

protected:
	/** Returns the time elapsed for animators */
	virtual double GetTimeElapsed()
	{
		return 0;
	}

	/** Checks if this time source is ready to be used by the animator */
	virtual bool IsTimeSourceReady() const
	{
		return false;
	}

	/** Check if the time elapsed is valid based on the context */
	PROPERTYANIMATORCORE_API virtual bool IsValidTimeElapsed(double InTimeElapsed) const;

	/** Time source CDO is registered by subsystem */
	virtual void OnTimeSourceRegistered() {}

	/** Time source CDO is unregistered by subsystem */
	virtual void OnTimeSourceUnregistered() {}

	/** Time source is active on the animator */
	virtual void OnTimeSourceActive() {}

	/** Time source is inactive on the animator */
	virtual void OnTimeSourceInactive() {}

private:
	/** Use a specific framerate */
	UPROPERTY(EditInstanceOnly, Setter="SetUseFrameRate", Getter="GetUseFrameRate", Category="Animator", meta=(InlineEditConditionToggle))
	bool bUseFrameRate = false;

	/** The frame rate to target for the animator effect */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(ClampMin="0", EditCondition="bUseFrameRate"))
	float FrameRate = 30.f;

	/** Name used to display this time source to the user */
	UPROPERTY(Transient)
	FName TimeSourceName;

	/** Cached time elapsed */
	double LastTimeElapsed = 0;

	/** Is this time source active on the animator */
	bool bTimeSourceActive = false;
};