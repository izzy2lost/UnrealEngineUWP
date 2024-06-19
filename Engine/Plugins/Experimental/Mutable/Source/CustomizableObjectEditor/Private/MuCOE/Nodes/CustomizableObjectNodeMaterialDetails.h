// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableObjectNodeDetails.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"
#include "Styling/SlateColor.h"

class IDetailLayoutBuilder;
class SSearchableComboBox;
class STextBlock;
class SWidget;
class UCustomizableObjectNodeMaterial;

namespace ESelectInfo { enum Type : int; }

class FCustomizableObjectNodeMaterialDetails : public FCustomizableObjectNodeDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	/** Fills an array of string with all the components names defined in the root CO. */
	void GetComponentNames();

	void OnComponentNameSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> GenerateComponentNameComboEntryWidget(TSharedPtr<FString> InItem) const;

	FText GenerateSelectedComponentNameWidget() const;

	void OnOpenComponentsCombobox();

private:

	// Weak pointer to the material node represented in the details
	TWeakObjectPtr<UCustomizableObjectNodeMaterial> MaterialNode;

	// All component names of the customizable object
	TArray<TSharedPtr<FString>> ComponentNames;

	/** Pointer to store which is the current selected component */
	TSharedPtr<FString> SelectedComponentName;
	
	/** Widget to draw the name of the selected component inside the combobox */
	TSharedPtr<STextBlock> ComponentNameTextWidget;
	
	/** Combobox widget to select a component name */
	TSharedPtr<SSearchableComboBox> ComponentComboBoxWidget;

	/** Color for the default text mode */
	FSlateColor DefaultTextColor;
};
