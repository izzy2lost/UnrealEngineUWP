// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlateStyles.h"
#include "Components/Widget.h"
#include "Delegates/Delegate.h"
#include "AudioMaterialSlider.generated.h"

class SAudioMaterialSlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSliderFloatValueChangedEvent, float, Value);

/**
 * A simple widget that shows a sliding bar with a handle that allows you to control the value between 0..1.
 * Slider is rendered by using material instead of texture.
 *
 * * No Children
 */
UCLASS()
class AUDIOWIDGETS_API UAudioMaterialSlider : public UWidget
{
	GENERATED_BODY()

public:

	/** The slider's style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (DisplayName = "Style"))
	FAudioMaterialSliderStyle WidgetStyle;

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

	/** Gets the current value of the slider.*/
	UFUNCTION(BlueprintPure, Category = "Behavior")
	float GetValue() const;

	/** Sets the current value of the slider. InValue is Clamped between 0.f - 1.f */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetValue(float InValue);

public:

	/** Called when the value is changed by slider. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnSliderFloatValueChangedEvent OnValueChanged;

protected:

	// UWidget
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnValueChanged(float InValue);

private:

	/**Default Value of the slider*/
	UPROPERTY(EditAnywhere, BlueprintSetter = SetValue, BlueprintGetter = GetValue, Category = "Appearance", meta = (UIMin = "0", UIMax = "1"))
	float Value = 1.f;

private:

	/** Native Slate Widget */
	TSharedPtr<SAudioMaterialSlider> Slider;
};
