// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CustomizableObjectNodeEditMaterialBaseDetails.h"
#include "IDetailCustomization.h"

class FString;
class IDetailLayoutBuilder;
class UCustomizableObjectNodeEditMaterial;
class SCustomizableObjectNodeLayoutBlocksEditor;


class FCustomizableObjectNodeEditMaterialDetails : public FCustomizableObjectNodeEditMaterialBaseDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	UCustomizableObjectNodeEditMaterial* Node;

	// Layout block editor widget
	TSharedPtr<SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	/** List of available layout grid sizes. */
	TArray< TSharedPtr< FString > > LayoutGridSizes;

	/** Layout Options Callbacks */
	void OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Reset the layout in the widget to force a refresh. */
	void UpdateLayout();
};
