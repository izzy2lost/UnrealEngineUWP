// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintPin.h"
#include "MVVMPropertyPath.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SMVVMFieldSelectorMenu.h"

class SGraphPin;
class UK2Node_CallFunction;
class UWidgetBlueprint;

class UMVVMBlueprintViewEvent;

enum class ECheckBoxState : uint8;

namespace UE::MVVM
{

class SFieldSelector;

class SEventParameter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEventParameter) {}
		SLATE_ARGUMENT_DEFAULT(UMVVMBlueprintViewEvent*, Event) =  nullptr;
		SLATE_ARGUMENT(FMVVMBlueprintPinId, ParameterId)
		SLATE_ARGUMENT_DEFAULT(bool, AllowDefault) = true;
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWidgetBlueprint* WidgetBlueprint);

private:
	ECheckBoxState OnGetIsBindArgumentChecked() const;
	void OnBindArgumentChecked(ECheckBoxState Checked);

	FMVVMBlueprintPropertyPath OnGetSelectedField() const;
	void SetSelectedField(const FMVVMBlueprintPropertyPath& Path);

	void HandleFieldSelectionChanged(FMVVMBlueprintPropertyPath SelectedField, const UFunction* Function);
	FFieldSelectionContext GetSelectedSelectionContext() const;

	int32 GetCurrentWidgetIndex() const;

private:
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprint;
	TWeakObjectPtr<UMVVMBlueprintViewEvent> ViewEvent;
	FMVVMBlueprintPinId ParameterId;
	/** This reference is just to keep the default value widget alive. */
	TSharedPtr<SGraphPin> GraphPin;

	FMVVMBlueprintPropertyPath PreviousSelectedField;

	bool bAllowDefault = true;
	bool bDefaultValueVisible = true;
};

}
