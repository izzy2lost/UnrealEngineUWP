// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "PropertyAnimatorCoreTimeSourceBase.h"
#include "PropertyAnimatorCoreMachineClockTimeSource.generated.h"

/** Enumerates all possible modes for the machine clock time source */
UENUM(BlueprintType)
enum class EPropertyAnimatorCoreMachineClockMode : uint8
{
	/** Local time of the machine */
	LocalTime,
	/** Universal time = Greenwich Mean Time */
	UtcTime,
	/** Specified duration elapsing until it reaches 0 */
	Countdown,
	/** Current time elapsed since the time source is active */
	Stopwatch
};

/** Machine clock time source that support various option */
UCLASS(MinimalAPI)
class UPropertyAnimatorCoreMachineClockTimeSource : public UPropertyAnimatorCoreTimeSourceBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreMachineClockTimeSource()
		: UPropertyAnimatorCoreTimeSourceBase(TEXT("MachineClock"))
	{}

	//~ Begin UPropertyAnimatorTimeSourceBase
	virtual double GetTimeElapsed() override;
	virtual bool IsTimeSourceReady() const override;
	virtual void OnTimeSourceActive() override;
	//~ End UPropertyAnimatorTimeSourceBase

	void SetMode(EPropertyAnimatorCoreMachineClockMode InMode);
	EPropertyAnimatorCoreMachineClockMode GetMode() const
	{
		return Mode;
	}

	void SetCountdownDuration(const FTimespan& InTimeSpan);
	void GetCountdownDuration(FTimespan& OutTimeSpan) const
	{
		OutTimeSpan = CountdownTimeSpan;
	}

	void SetCountdownDuration(const FString& InDuration);
	const FString& GetCountdownDuration() const
	{
		return CountdownDuration;
	}

protected:
	static FTimespan ParseTime(const FString& InFormat);

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	void OnModeChanged();

	/** Machine time mode to use */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator")
	EPropertyAnimatorCoreMachineClockMode Mode = EPropertyAnimatorCoreMachineClockMode::LocalTime;

	/**
	* Countdown duration format : 
	* 120 = 2 minutes
	* 02:00 = 2 minutes
	* 00:02:00 = 2 minutes
	* 2m = 2 minutes
	* 1h = 1 hour
	* 120s = 2 minutes
	*/
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(EditCondition="Mode == EPropertyAnimatorCoreMachineClockMode::Countdown", EditConditionHides))
	FString CountdownDuration = TEXT("1m");

private:
	UPROPERTY(Transient)
	FTimespan CountdownTimeSpan;

	UPROPERTY(Transient)
	FDateTime ActivationTime;
};