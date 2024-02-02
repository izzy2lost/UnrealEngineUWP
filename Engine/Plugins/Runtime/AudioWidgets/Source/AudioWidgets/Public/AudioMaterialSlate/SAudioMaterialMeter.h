// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateStyles.h"
#include "Framework/SlateDelegates.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SLeafWidget.h"

class UWidget;

/**
 * A simple slate that renders the meter in single material and modifies the material on value change.
 *
 */
class AUDIOWIDGETS_API SAudioMaterialMeter : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioMaterialMeter)
	{}

	/** The owner object*/
	SLATE_ARGUMENT(TWeakObjectPtr<UObject>, Owner)

	/** A value that drives how the Meter is rendered*/
	SLATE_ATTRIBUTE(float, ValueAttribute)

	/** The style used to draw the meter. */
	SLATE_STYLE_ARGUMENT(FAudioMaterialMeterStyle, AudioMaterialMeterStyle)

	/** Called when the value is changed in the Meter */
	SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)

	SLATE_END_ARGS()

	/**
	* Construct the widget.
	*/
	void Construct(const FArguments& InArgs);

	// SWidget overrides
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	/** Set the Value attribute */
	void SetValue(float InValueAttribute);

	/** Apply new material to be used to render the Slate.*/
	void ApplyNewMaterial();

public:

	// Holds a delegate that is executed when the slider's value changed.
	FOnFloatValueChanged OnValueChanged;	

private:

	/**
	* Commits the specified meter value.
	*
	* @param NewValue The value to commit.
	*/
	void CommitValue(float NewValue);

private:

	// Holds the owner of the Slate
	TWeakObjectPtr<UObject> Owner;

	// Holds the style for the Slate
	const FAudioMaterialMeterStyle* AudioMaterialMeterStyle = nullptr;

	//Holds the current value
	TAttribute<float> ValueAttribute = 0.f;

	//Width & height
	//TODO: Currently hardcoded for testing, make more dynamic.
	float SlateWidth = 32.f;
	float SlateHeight = 512.f;

};
