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
};
