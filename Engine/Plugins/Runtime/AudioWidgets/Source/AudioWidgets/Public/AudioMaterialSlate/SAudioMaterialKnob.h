// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Framework/SlateDelegates.h"
#include "SAudioInputWidget.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SLeafWidget.h"

class UObject;

/**
 * A simple slate that renders a knob in single material and modifies the material on value change.
 *
 */
class AUDIOWIDGETS_API SAudioMaterialKnob : public SAudioInputWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioMaterialKnob)
	{}

	/** The owner object*/
	SLATE_ARGUMENT(TWeakObjectPtr<UObject>, Owner)

	/**Value of the Knob*/
	SLATE_ATTRIBUTE(float, Value)

	/** The style used to draw the knob. */
	SLATE_STYLE_ARGUMENT(FAudioMaterialKnobStyle, AudioMaterialKnobStyle)

	/** Called when the knob's state changes. */
	SLATE_EVENT(FOnFloatValueChanged, OnFloatValueChanged)

	/** Invoked when the mouse is pressed and a capture begins. */
	SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

	/** Invoked when the mouse is released and a capture ends. */
	SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Set the Value attribute */
	void SetValue(float InValueAttribute);

	/** Apply new material to be used to render the Slate.*/
	UMaterialInstanceDynamic* ApplyNewMaterial();

	//SAudioInputWidget
	virtual const float GetOutputValue(const float InSliderValue) override;
	virtual const float GetSliderValue(const float OutputValue) override;

	/**
	 * Set the knob's linear (0-1 normalized) value.
	 */
	virtual void SetSliderValue(float InSliderValue) override;
	virtual void SetOutputRange(const FVector2D Range) override;

	virtual void SetDesiredSizeOverride(const FVector2D Size) override;

	//These are pure virtual functions in the parent class and are implemented properly in this class later.
	virtual void SetLabelBackgroundColor(FSlateColor InColor) override {};
	virtual void SetUnitsText(const FText Units) override {};
	virtual void SetUnitsTextReadOnly(const bool bIsReadOnly) override {};
	virtual void SetShowUnitsText(const bool bShowUnitsText) override {};
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
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)override;
	//~SWidget

protected:

	// Range for output 
	FVector2D OutputRange = FVector2D(0.0f, 1.0f);
	const FVector2D NormalizedLinearSliderRange = FVector2D(0.0f, 1.0f);

private:

	TAttribute<TOptional<FVector2D>> DesiredSizeOverride;

	/**Commits new value*/
	void CommitValue(float NewValue);

private:

	// Holds the owner of the Slate
	TWeakObjectPtr<UObject> Owner;

	// Holds the style for the Slate
	const FAudioMaterialKnobStyle* AudioMaterialKnobStyle = nullptr;

	// Holds the Modifiable Material that represent the Knob
	mutable TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	//Holds the knobs current Value
	TAttribute<float> ValueAttribute = 1.0;

	// The position of the mouse when it pushed down and started rotating the knob
	FVector2D MouseDownPosition;

	// the value when the mouse was pushed down
	float MouseDownValue = 0.f;

	// Holds the initial cursor in case a custom cursor has been specified, so we can restore it after dragging the slider
	EMouseCursor::Type CachedCursor;

	// the max pixels to go to min or max value (clamped to 0 or 1) in one drag period
	int32 PixelDelta = 50;

};
