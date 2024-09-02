// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CustomizableObjectNodeDetails.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"

class FString;
class IDetailLayoutBuilder;
class UCustomizableObjectNodeModifierRemoveMeshBlocks;
class SCustomizableObjectNodeLayoutBlocksEditor;


class FCustomizableObjectNodeModifierRemoveMeshBlocksDetails : public FCustomizableObjectNodeDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	UCustomizableObjectNodeModifierRemoveMeshBlocks* Node;

	// Layout block editor widget
	TSharedPtr<SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	/** List of available layout grid sizes. */
	TArray< TSharedPtr< FString > > LayoutGridSizes;
	TArray< TSharedPtr< FString > > UVChannelOptions;

	/** Layout Options Callbacks */
	void OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnUVChannelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Reset the layout in the widget to force a refresh. */
	void UpdateLayout();

};
