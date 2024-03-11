// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Framework/SlateDelegates.h"
#include "SAudioInputWidget.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SLeafWidget.h"

class SAudioTextBox;
class SAudioMaterialKnob;
class SVerticalBox;
class UObject;

/**
 * Wraps SAudioMaterialKnob and adds Label text that will show a value text.
 */
class AUDIOWIDGETS_API SAudioMaterialLabeledKnob : public SAudioInputWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioMaterialLabeledKnob)
	{}

	/** The owner object*/
	SLATE_ARGUMENT(TWeakObjectPtr<UObject>, Owner)

	/**Value of the Knob*/
	SLATE_ATTRIBUTE(float, Value)

	/** The style used to draw the knob. */
	SLATE_STYLE_ARGUMENT(FAudioMaterialKnobStyle, Style)

	/** Called when the knob's state changes. */
	SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)

	/** Invoked when the mouse is pressed and a capture begins. */
	SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

	/** Invoked when the mouse is released and a capture ends. */
	SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Set the Value attribute */
	void SetValue(float InValueAttribute);

	//SAudioInputWidget
	virtual const float GetOutputValue(const float InSliderValue) override;
	virtual const float GetSliderValue(const float OutputValue) override;
	virtual const float GetOutputValueForText(const float InSliderValue);
	virtual const float GetSliderValueForText(const float OutputValue);

	/**
	 * Set the knob's linear (0-1 normalized) value.
	 */
	virtual void SetSliderValue(float InSliderValue) override;
	virtual void SetOutputRange(const FVector2D Range) override;
	virtual void SetDesiredSizeOverride(const FVector2D Size) override;
	virtual void SetLabelBackgroundColor(FSlateColor InColor) override;
	virtual void SetUnitsText(const FText Units) override;
	virtual void SetUnitsTextReadOnly(const bool bIsReadOnly) override;
	virtual void SetShowUnitsText(const bool bShowUnitsText) override;
	//~SAudioInputWidget

public:

	// Holds a delegate that is executed when the knob's value changes.
	FOnFloatValueChanged OnValueChanged;

	// Holds a delegate that is executed when the mouse is pressed and a capture begins.
	FSimpleDelegate OnMouseCaptureBegin;

	// Holds a delegate that is executed when the mouse is let up and a capture ends.
	FSimpleDelegate OnMouseCaptureEnd;

protected:

	//SWidget
	virtual FVector2D ComputeDesiredSize(float) const override;
	//~SWidget

private:

	TAttribute<TOptional<FVector2D>> DesiredSizeOverride;

	// Holds the owner of the Slate
	TWeakObjectPtr<UObject> Owner;

	// Holds the style for the Slate
	const FAudioMaterialKnobStyle* Style = nullptr;

	//Holds the knobs current Value
	TAttribute<float> ValueAttribute = 1.0f;

	// Widget components
	TSharedPtr<SAudioMaterialKnob> Knob;
	TSharedPtr<SAudioTextBox> Label;

	/** verticalBox that holds the widgets*/
	TSharedPtr<SVerticalBox> VerticalLayotWidget;

	// Range for output 
	FVector2D OutputRange = FVector2D(0.0f, 1.0f);
	const FVector2D NormalizedLinearSliderRange = FVector2D(0.0f, 1.0f);

};
