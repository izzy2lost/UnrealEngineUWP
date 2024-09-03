// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierEditMeshSectionDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierEditMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierExtendMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "MuCOE/PinViewer/SPinViewer.h"
#include "MuR/MutableTrace.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"
#include "UObject/Package.h"

class FString;

#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeModifierEditMeshSectionDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeModifierEditMeshSectionDetails);
}


void FCustomizableObjectNodeModifierEditMeshSectionDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FCustomizableObjectNodeModifierBaseDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeModifierEditMeshSection>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& LayoutCategory = DetailBuilder.EditCategory("LayoutOptions");

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
		for (int32 Index = 0; Index < 4; ++Index)
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
					.OnSelectionChanged(this, &FCustomizableObjectNodeModifierEditMeshSectionDetails::OnUVChannelChanged)
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
					.OnSelectionChanged(this, &FCustomizableObjectNodeModifierEditMeshSectionDetails::OnGridSizeChanged)
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


void FCustomizableObjectNodeModifierEditMeshSectionDetails::OnRequiredTagsPropertyChanged()
{
	FCustomizableObjectNodeModifierBaseDetails::OnRequiredTagsPropertyChanged();
	UpdateLayout();
}


void FCustomizableObjectNodeModifierEditMeshSectionDetails::UpdateLayout()
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectNodeModifierEditMeshSectionDetails_UpdateLayout);

	// Try to find the parent layout, because we want to show its UVs in the widget
	UCustomizableObjectLayout* ParentLayout = Node->GetPossibleParentLayout();

	LayoutBlocksEditor->SetCurrentLayout(Node->Layout, ParentLayout);
}


void FCustomizableObjectNodeModifierEditMeshSectionDetails::OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
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


void FCustomizableObjectNodeModifierEditMeshSectionDetails::OnUVChannelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!Node->Layout)
	{
		return;
	}

	int32 Index = UVChannelOptions.Find(NewSelection);

	if (Node->ParentLayoutIndex != Index)
	{
		Node->ParentLayoutIndex = Index;
		Node->Modify();
		UpdateLayout();
	}
}

#undef LOCTEXT_NAMESPACE
