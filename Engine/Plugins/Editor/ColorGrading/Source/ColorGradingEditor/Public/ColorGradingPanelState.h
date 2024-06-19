// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

/** Modes that colors can be displayed in for the color grading panel */
enum class COLORGRADINGEDITOR_API EColorGradingColorDisplayMode
{
	RGB,
	HSV
};

/**
 * Stores the state of the color grading panel UI that can be reloaded in cases where the panel or any of its elements
 * are reloaded (such as when the containing drawer is reopened or docked)
 */
struct COLORGRADINGEDITOR_API FColorGradingPanelState
{
	/** The objects that are selected in the list */
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	/** The color grading group that is selected */
	int32 SelectedColorGradingGroup = INDEX_NONE;

	/** The color grading element that is selected */
	int32 SelectedColorGradingElement = INDEX_NONE;

	/** Indicates which color wheels are hidden */
	TArray<bool> HiddenColorWheels;

	/** The selected orientation of the color wheels */
	EOrientation ColorWheelOrientation = EOrientation::Orient_Vertical;

	/** The color display mode of the color wheels */
	EColorGradingColorDisplayMode ColorDisplayMode;

	/** Indicates which subsections were selected for each section in the details panel */
	TArray<int32> SelectedDetailsSubsections;
};