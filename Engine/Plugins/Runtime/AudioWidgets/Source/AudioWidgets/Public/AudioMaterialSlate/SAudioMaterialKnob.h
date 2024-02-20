// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Framework/SlateDelegates.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SLeafWidget.h"

class UObject;

/**
 * A simple slate that renders a knob in single material and modifies the material on value change.
 *
 */
class AUDIOWIDGETS_API SAudioMaterialKnob : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioMaterialKnob)
	{}

	/** The owner object*/
	SLATE_ARGUMENT(TWeakObjectPtr<UObject>, Owner)

	/**Value of the Knob*/
	SLATE_ATTRIBUTE(float, Value)

	/** The style used to draw the button. */
	SLATE_STYLE_ARGUMENT(FAudioMaterialKnobStyle, AudioMaterialKnobStyle)

	/** Called when the button's state changes. */
	SLATE_EVENT(FOnFloatValueChanged, OnFloatValueChanged)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Set the Value attribute */
	void SetValue(float InValueAttribute);

	/** Apply new material to be used to render the Slate.*/
	void ApplyNewMaterial();

public:

	FOnFloatValueChanged OnValueChanged;

protected:

	// SWidget overrides
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)override;

private:

	/**Commits new value*/
	void CommitValue(float NewValue);

private:

	// Holds the owner of the Slate
	TWeakObjectPtr<UObject> Owner;

	// Holds the style for the Slate
	const FAudioMaterialKnobStyle* AudioMaterialKnobStyle = nullptr;

	//Current Value
	TAttribute<float> ValueAttribute = 1.0;

	// The position of the mouse when it pushed down and started rotating the knob
	FVector2D MouseDownPosition;

	// the value when the mouse was pushed down
	float MouseDownValue = 0.f;

	// the max pixels to go to min or max value (clamped to 0 or 1) in one drag period
	int32 PixelDelta = 50;

};
