// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Misc/Timespan.h"
#include "PropertyAnimatorClock.generated.h"

/** Mode supported for properties value */
UENUM(BlueprintType)
enum class EPropertyAnimatorClockMode : uint8
{
	/** Local time of the machine */
	LocalTime,
	/** Specified duration elapsing until it reaches 0 */
	Countdown,
	/** Shows the current time elapsed */
	Stopwatch
};

/** Animate supported string properties to display time */
UCLASS(MinimalAPI, AutoExpandCategories=("Animator"))
class UPropertyAnimatorClock : public UPropertyAnimatorCoreBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultControllerName = TEXT("Clock");

	UPropertyAnimatorClock();

	PROPERTYANIMATOR_API void SetDisplayFormat(const FString& InDisplayFormat);
	const FString& GetDisplayFormat() const
	{
		return DisplayFormat;
	}

protected:
	//~ Begin UPropertyAnimatorCoreBase
	virtual EPropertyAnimatorPropertySupport IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const override;
	virtual void EvaluateProperties(FInstancedPropertyBag& InParameters) override;
	virtual void OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty, EPropertyAnimatorPropertySupport InSupport) override;
	//~ End UPropertyAnimatorCoreBase

	/**
	 * Display date time format : 
	 * %a - Weekday, eg) Sun
	 * %A - Weekday, eg) Sunday
	 * %w - Weekday, 0-6 (Sunday is 0)
	 * %y - Year, YY
	 * %Y - Year, YYYY
	 * %b - Month, eg) Jan
	 * %B - Month, eg) January
	 * %m - Month, 01-12
	 * %d - Day, 01-31
	 * %e - Day, 1-31
	 * %l - 12h Hour, 1-12
	 * %I - 12h Hour, 01-12
	 * %H - 24h Hour, 00-23
	 * %M - Minute, 00-59
	 * %S - Second, 00-60
	 * %p - AM or PM
	 * %P - am or PM
	 * %j - Day of the Year, 001-366
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="Animator")
	FString DisplayFormat = TEXT("%H:%M:%S");
};