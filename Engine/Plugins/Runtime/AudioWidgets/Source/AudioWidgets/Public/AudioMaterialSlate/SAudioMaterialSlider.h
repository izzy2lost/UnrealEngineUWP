// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Framework/SlateDelegates.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SLeafWidget.h"

class UWidget;

/**
 * A simple slate that renders slider in single material and modifies the material on value change.
 *
 */
class AUDIOWIDGETS_API SAudioMaterialSlider : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioMaterialSlider)
	{}

	/** The owner object*/
	SLATE_ARGUMENT(TWeakObjectPtr<UObject>, Owner)

	/** The slider's orientation. */
	SLATE_ARGUMENT(EOrientation, Orientation)

	/** The style used to draw the slider. */
	SLATE_STYLE_ARGUMENT(FAudioMaterialSliderStyle, AudioMaterialSliderStyle)

	/** A value that drives where the slider handle appears. Value is clamped between 0 and 1. */
	SLATE_ATTRIBUTE(float, ValueAttribute)

	/** Called when the value is changed by the slider. */
	SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
	
	/** Called when the value is committed (mouse capture ends) */
	SLATE_EVENT(FOnFloatValueChanged, OnValueCommitted)

	SLATE_END_ARGS()

	/**
	* Construct the widget.
	*/
	void Construct(const FArguments& InArgs);

	// SWidget overrides
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	/** Set the Value attribute */
	void SetValue(TAttribute<float> InValueAttribute);

	/** Apply new material to be used to render the Slate.*/
	void ApplyNewMaterial();

	/** Set the orientation of the slider*/
	void SetOrientation(EOrientation InOrientation);

	//SWidget
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	//~SWidget

public:

	// Holds a delegate that is executed when the slider's value changed.
	FOnFloatValueChanged OnValueChanged;
	
	// Holds a delegate that is executed when the slider's value is committed (mouse capture ends).
	FOnFloatValueChanged OnValueCommitted;

private:

	/**
	* Commits the specified slider value.
	*
	* @param NewValue The value to commit.
	*/
	void CommitValue(float NewValue);

	/**
	* Calculates the new value based on the given absolute coordinates.
	*
	* @param MyGeometry The slider's geometry.
	* @param AbsolutePosition The absolute position of the slider.
	* @return The new value.
	*/
	FVector2D PositionToValue(const FGeometry& MyGeometry, const FVector2D& AbsolutePosition);

private:

	// Optional override for desired size 
	TAttribute<TOptional<FVector2D>> DesiredSizeOverride;

	// Holds the slider's orientation
	EOrientation Orientation;

	// Holds the owner of the Slate
	TWeakObjectPtr<UObject> Owner;

	// Holds the style for the Slate
	const FAudioMaterialSliderStyle* AudioMaterialSliderStyle = nullptr;

	// Holds the Modifiable Material that represent the slider
	mutable TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	//Holds the current value
	TAttribute<float> ValueAttribute = 0.f;

};
