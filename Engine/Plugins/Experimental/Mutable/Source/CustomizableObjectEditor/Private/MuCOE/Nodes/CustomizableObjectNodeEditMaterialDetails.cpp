// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "MuCOE/PinViewer/SPinViewer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/Package.h"

class FString;

#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeEditMaterialDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeEditMaterialDetails);
}


void FCustomizableObjectNodeEditMaterialDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FCustomizableObjectNodeEditMaterialBaseDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeEditMaterial>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Blocks" );

	if (Node)
	{
		// Add blocks selector
		LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

		BlocksCategory.AddCustomRow(LOCTEXT("BlocksDetails_BlockInstructions", "BlockInstructions"))
		[
			SNew(SBox)
			.HeightOverride(700.0f)
			.WidthOverride(700.0f)
			[
				LayoutBlocksEditor.ToSharedRef()
			]
		];

		// Try to find the parent layout, because we want to show its UVs in the widget
		UCustomizableObjectLayout* ParentLayout = nullptr;
		if (UCustomizableObjectNodeMaterialBase* ParentMaterialNode = Node->GetParentMaterialNode())
		{
			TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

			if (!Layouts.IsValidIndex(Node->ParentLayoutIndex))
			{
				UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeEditMaterial refers to an invalid texture layout index %d. Parent node has %d layouts."),
					*Node->GetOutermost()->GetName(), Node->ParentLayoutIndex, Layouts.Num());
			}
			else
			{
				ParentLayout = Layouts[Node->ParentLayoutIndex];
			}
		}

		LayoutBlocksEditor->SetCurrentLayout(Node->Layout, ParentLayout);
	}
	else
	{
		BlocksCategory.AddCustomRow( LOCTEXT("BlocksDetails_NodeNotFound", "NodeNotFound") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "Node not found", "Node not found" ) )
		];
	}
}

#undef LOCTEXT_NAMESPACE
