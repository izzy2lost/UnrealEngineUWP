// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Param/ParameterPickerArgs.h"
#include "EdGraph/EdGraphPin.h"

struct FAnimNextParamInstanceIdentifier;
struct FAnimNextParamType;

namespace UE::AnimNext::Editor
{
	class SParameterPicker;
	class SGraphPinParam;
}

namespace UE::AnimNext::Editor
{

/** Retrieves the parameter name to display */
using FOnGetParameterName = TDelegate<FName(void)>;

/** Retrieves the parameter type to display */
using FOnGetParameterType = TDelegate<FAnimNextParamType(void)>;

/** Retrieves the parameter scope to display */
using FOnGetParameterInstanceId = TDelegate<TInstancedStruct<FAnimNextParamInstanceIdentifier>(void)>;

class SParameterPickerCombo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SParameterPickerCombo) {}

	/** Arguments for configuring the picker in the dropdown menu */
	SLATE_ARGUMENT(FParameterPickerArgs, PickerArgs)

	/** Retrieves the parameter name to display */
	SLATE_EVENT(FOnGetParameterName, OnGetParameterName)

	/** Retrieves the parameter type to display */
	SLATE_EVENT(FOnGetParameterType, OnGetParameterType)

	/** Retrieves the parameter instance ID to display */
	SLATE_EVENT(FOnGetParameterInstanceId, OnGetParameterInstanceId)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RequestRefresh();

	// Retrieves the parameter name to display
	FOnGetParameterName OnGetParameterNameDelegate;

	// Retrieves the parameter type to display
	FOnGetParameterType OnGetParameterTypeDelegate;

	// Retrieves the parameter instance ID to display
	FOnGetParameterInstanceId OnGetParameterInstanceIdDelegate;

	// Cached pin type
	FEdGraphPinType PinType;

	// Cached name
	FName ParameterName;

	// Cached display name
	FText ParameterNameText;

	// Cached tooltip
	FText ParameterNameTooltipText;

	// Cached parameter type
	FAnimNextParamType ParameterType;

	// Cached parameter instance ID
	TInstancedStruct<FAnimNextParamInstanceIdentifier> ParameterInstanceId;

	// Cached icon
	const FSlateBrush* Icon = nullptr;

	// Cached color
	FSlateColor IconColor = FLinearColor::Gray;

	// Picker widget used to focus after the popup is displayed
	TWeakPtr<SParameterPicker> PickerWidget;

	// Arguments for the picker popup
	FParameterPickerArgs PickerArgs;

	bool bRefreshRequested = false;

	friend class SGraphPinParam;
	friend class SGraphPinParamName;
};

}