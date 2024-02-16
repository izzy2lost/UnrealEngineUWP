// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Components/Widget.h"
#include "Delegates/Delegate.h"
#include "AudioMaterialKnob.generated.h"

class SAudioMaterialKnob;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKnobValueChangedEvent, float, Value);


/**
 * A simple widget that shows a turning Knob that allows you to control the value between 0..1.
 * Knob is rendered by using material instead of texture.
 *
 * * No Children
 */
UCLASS()
class AUDIOWIDGETS_API UAudioMaterialKnob : public UWidget
{
	GENERATED_BODY()

public:

	UAudioMaterialKnob();

	/** The button's style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (DisplayName = "Style", ShowOnlyInnerProperties))
	FAudioMaterialKnobStyle WidgetStyle;

public:
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif // WITH_EDITOR

	// UWidget
	virtual void SynchronizeProperties() override;
	// End of UWidget

	// UVisual
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual

	/** Gets the current value of the knob.*/
	UFUNCTION(BlueprintPure, Category = "Behavior")
	float GetValue();

	/** Sets the current value of the knob. InValue is Clamped between 0.f - 1.f */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetValue(float InValue);

public:

	/** Called when the value is changed by knob. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnKnobValueChangedEvent OnKnobValueChanged;

protected:

	// UWidget
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnKnobValueChanged(float InValue);

private:

	/**Default Value of the Knob*/
	UPROPERTY(EditAnywhere, BlueprintSetter = SetValue, BlueprintGetter = GetValue, Category = "Appearance", meta = (UIMin = "0", UIMax = "1"))
	float Value = 1.f;

private:

	/** Native Slate Widget */
	TSharedPtr<SAudioMaterialKnob> Knob;

};
