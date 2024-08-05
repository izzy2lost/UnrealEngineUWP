// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CustomizableObjectNodeDetails.h"
#include "CustomizableObjectNodeEditMaterialBaseDetails.h"
#include "IDetailCustomization.h"

class FString;
class IDetailLayoutBuilder;
class UCustomizableObjectNodeRemoveMeshBlocks;
class SCustomizableObjectNodeLayoutBlocksEditor;


class FCustomizableObjectNodeRemoveMeshBlocksDetails : public FCustomizableObjectNodeEditMaterialBaseDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	UCustomizableObjectNodeRemoveMeshBlocks* Node;

	// Layout block editor widget
	TSharedPtr<SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;
};
