// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocksDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"
#include "UObject/Package.h"

class FString;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeRemoveMeshBlocksDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeRemoveMeshBlocksDetails );
}


void FCustomizableObjectNodeRemoveMeshBlocksDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FCustomizableObjectNodeEditMaterialBaseDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeRemoveMeshBlocks>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& LayoutCategory = DetailBuilder.EditCategory("LayoutOptions");
	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory("Blocks");

	if (!Node)
	{
		BlocksCategory.AddCustomRow(LOCTEXT("BlocksDetails_NodeNotFound", "NodeNotFound"))
			[
				SNew(STextBlock)
					.Text(LOCTEXT("Node not found", "Node not found"))
			];
		return;
	}

	// Grid size combo
	{
		int32 MaxGridSize = 128;
		LayoutGridSizes.Empty();

		TSharedPtr<FString> CurrentSize;
		for (int32 Size = 1; Size <= MaxGridSize; Size *= 2)
		{
			LayoutGridSizes.Add(MakeShareable(new FString(FString::Printf(TEXT("%d x %d"), Size, Size))));

			if (Node->Layout->GetGridSize() == FIntPoint(Size))
			{
				CurrentSize = LayoutGridSizes.Last();
			}
		}

		IDetailGroup& LayoutOptionsGroup = LayoutCategory.AddGroup(TEXT("LayoutOptionsGroup"), LOCTEXT("LayoutGroup", "Layout Group"), false, true);
		LayoutOptionsGroup.HeaderRow()
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("LayoutGridSizeText", "Grid Size"))
					.Font(DetailBuilder.GetDetailFont())
			]
			.ValueContent()
			[
				SNew(STextComboBox)
					.InitiallySelectedItem(CurrentSize)
					.OptionsSource(&LayoutGridSizes)
					.OnSelectionChanged(this, &FCustomizableObjectNodeRemoveMeshBlocksDetails::OnGridSizeChanged)
					.Font(DetailBuilder.GetDetailFont())
			];
	}

	// Block editor
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

	UpdateLayout();
}


void FCustomizableObjectNodeRemoveMeshBlocksDetails::UpdateLayout()
{
	// Try to find the parent layout, because we want to show its UVs in the widget
	UCustomizableObjectLayout* ParentLayout = nullptr;
	if (UCustomizableObjectNodeMaterialBase* ParentMaterialNode = Node->GetParentMaterialNode())
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (!Layouts.IsValidIndex(Node->ParentLayoutIndex))
		{
			UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
				*Node->GetOutermost()->GetName(), Node->ParentLayoutIndex, Layouts.Num());
		}
		else
		{
			ParentLayout = Layouts[Node->ParentLayoutIndex];
		}
	}

	LayoutBlocksEditor->SetCurrentLayout(Node->Layout, ParentLayout);

}


void FCustomizableObjectNodeRemoveMeshBlocksDetails::OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!Node->Layout)
	{
		return;
	}

	int32 Size = 1 << LayoutGridSizes.Find(NewSelection);

	if (Node->Layout->GetGridSize().X != Size || Node->Layout->GetGridSize().Y != Size)
	{
		Node->Layout->SetGridSize(FIntPoint(Size));

		Node->MarkPackageDirty();

		UpdateLayout();
	}
}


#undef LOCTEXT_NAMESPACE
