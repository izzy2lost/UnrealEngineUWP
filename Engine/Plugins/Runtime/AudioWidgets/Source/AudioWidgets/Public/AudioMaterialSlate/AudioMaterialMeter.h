// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Components/Widget.h"
#include "Delegates/Delegate.h"
#include "AudioMaterialMeter.generated.h"

class SAudioMaterialMeter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterValueChangedEvent, float, Value);

/**
 * Meter is rendered by using material instead of texture.
 *
 * * No Children
 */
UCLASS()
class AUDIOWIDGETS_API UAudioMaterialMeter : public UWidget
{
	GENERATED_BODY()

public:

	UAudioMaterialMeter();

	/** The meter's style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (DisplayName = "Style", ShowOnlyInnerProperties))
	FAudioMaterialMeterStyle WidgetStyle;

public:

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	// UWidget
	virtual void SynchronizeProperties() override;
	// End of UWidget

	// UVisual
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual

	/** Get the current value of the meter.*/
	UFUNCTION(BlueprintPure, Category = "Behavior")
	float GetValue() const;

	/** Set the current value of the Meter.*/
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetValue(float InValue);

public:

	/** Called when the meter value changes. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnMeterValueChangedEvent OnValueChanged;

protected:

	// UWidget
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnValueChanged(float InValue);

private:

	/**Current Value of the Meter*/
	UPROPERTY(EditAnywhere, BlueprintSetter = SetValue, BlueprintGetter = GetValue, Category = "Appearance", meta = (UIMin = "0", UIMax = "1"))
	float MeterValue = 1.f;

	/** Native Slate Widget */
	TSharedPtr<SAudioMaterialMeter> Meter;

};
