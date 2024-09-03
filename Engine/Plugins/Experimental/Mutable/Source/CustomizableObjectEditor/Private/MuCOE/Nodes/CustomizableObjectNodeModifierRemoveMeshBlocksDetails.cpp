// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMeshBlocksDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMeshBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"
#include "UObject/Package.h"

class FString;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeModifierRemoveMeshBlocksDetails );
}


void FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FCustomizableObjectNodeModifierBaseDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeModifierRemoveMeshBlocks>(DetailsView->GetSelectedObjects()[0].Get());
	}

	// This property is not relevant for this node
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeModifierWithMaterial, ReferenceMaterial), UCustomizableObjectNodeModifierWithMaterial::StaticClass());

	IDetailCategoryBuilder& LayoutCategory = DetailBuilder.EditCategory("LayoutOptions");
	LayoutCategory.SetSortOrder(10000);

	if (!Node)
	{
		LayoutCategory.AddCustomRow(LOCTEXT("BlocksDetails_NodeNotFound", "NodeNotFound"))
			[
				SNew(STextBlock)
					.Text(LOCTEXT("Node not found", "Node not found"))
			];
		return;
	}

	// UV Channel combo (for now hardcoded to a maximum of 4)
	{
		int32 MaxGridSize = 128;
		UVChannelOptions.Empty();

		TSharedPtr<FString> CurrentUVChannel;
		for (int32 Index = 0; Index<4; ++Index)
		{
			UVChannelOptions.Add(MakeShareable(new FString(FString::Printf(TEXT("%d"), Index))));

			if (Node->ParentLayoutIndex == Index)
			{
				CurrentUVChannel = UVChannelOptions.Last();
			}
		}

		IDetailGroup& LayoutOptionsGroup = LayoutCategory.AddGroup(TEXT("LayoutOptionsGroup"), LOCTEXT("LayoutGroup", "Layout Group"), false, true);
		LayoutOptionsGroup.HeaderRow()
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("UVChannel", "UV Channel"))
					.Font(DetailBuilder.GetDetailFont())
			]
			.ValueContent()
			[
				SNew(STextComboBox)
					.InitiallySelectedItem(CurrentUVChannel)
					.OptionsSource(&UVChannelOptions)
					.OnSelectionChanged(this, &FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::OnUVChannelChanged)
					.Font(DetailBuilder.GetDetailFont())
			];
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
					.OnSelectionChanged(this, &FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::OnGridSizeChanged)
					.Font(DetailBuilder.GetDetailFont())
			];
	}

	// Block editor
	LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

	LayoutCategory.AddCustomRow(LOCTEXT("BlocksDetails_BlockInstructions", "BlockInstructions"))
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


void FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::OnRequiredTagsPropertyChanged()
{
	FCustomizableObjectNodeModifierBaseDetails::OnRequiredTagsPropertyChanged();
	UpdateLayout();
}


void FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::UpdateLayout()
{
	// Try to find the parent layout, because we want to show its UVs in the widget
	UCustomizableObjectLayout* ParentLayout = Node->GetPossibleParentLayout();

	LayoutBlocksEditor->SetCurrentLayout(Node->Layout, ParentLayout);

}


void FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
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


void FCustomizableObjectNodeModifierRemoveMeshBlocksDetails::OnUVChannelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!Node->Layout)
	{
		return;
	}

	int32 Index = UVChannelOptions.Find(NewSelection);

	if (Node->ParentLayoutIndex != Index)
	{
		Node->ParentLayoutIndex = Index;
		Node->MarkPackageDirty();
		UpdateLayout();
	}
}


#undef LOCTEXT_NAMESPACE
