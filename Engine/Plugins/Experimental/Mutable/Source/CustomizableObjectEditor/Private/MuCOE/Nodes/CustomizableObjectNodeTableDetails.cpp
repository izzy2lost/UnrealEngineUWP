// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTableDetails.h"

#include "Animation/AnimInstance.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "Layout/Visibility.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "SSearchableComboBox.h"
#include "Styling/SlateColor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeTableDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeTableDetails);
}


void FCustomizableObjectNodeTableDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	IDetailCustomization::CustomizeDetails(DetailBuilder);
	
	Node = 0;
	DetailBuilderPtr = DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder->GetDetailsView();
	
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeTable>(DetailsView->GetSelectedObjects()[0].Get());
	}

	if (Node.IsValid())
	{
		IDetailCategoryBuilder& CustomizableObjectCategory = DetailBuilder->EditCategory("TableProperties");
		IDetailCategoryBuilder& CompilationRestrictionsCategory = DetailBuilder->EditCategory("CompilationRestrictions");
		DetailBuilder->HideProperty("VersionColumn");
		IDetailCategoryBuilder& UICategory = DetailBuilder->EditCategory("UI");
		DetailBuilder->HideProperty("ParamUIMetadataColumn");
		DetailBuilder->HideProperty("ThumbnailColumn");
		IDetailCategoryBuilder& AnimationCategory = DetailBuilder->EditCategory("AnimationProperties");
		IDetailCategoryBuilder& LayoutCategory = DetailBuilder->EditCategory("DefaultMeshLayoutEditor");

		// Attaching the Posrecontruct delegate to force a refresh of the details
		Node->PostReconstructNodeDelegate.AddSP(this, &FCustomizableObjectNodeTableDetails::OnNodePinValueChanged);

		GenerateMeshColumnComboBoxOptions();
		TSharedPtr<FString> CurrentMutableMetadataColumn = GenerateMutableMetaDataColumnComboBoxOptions();
		TSharedPtr<FString> CurrentVersionColumn = GenerateVersionColumnComboBoxOptions();
		TSharedPtr<FString> CurrentThumbnailColumn = GenerateThumbnailColumnComboBoxOptions();

		CustomizableObjectCategory.AddProperty("ParameterName");

		CompilationRestrictionsCategory.AddCustomRow(LOCTEXT("VersionColumn_Selector","VersionColumn"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VersionColumn_SelectorText","Version Column"))
			.ToolTipText(LOCTEXT("VersionColumn_SelectorTooltip","Select the column that contains the version of each row."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(VersionColumnsComboBox,STextComboBox)
			.InitiallySelectedItem(CurrentVersionColumn)
			.OptionsSource(&VersionColumnsOptionNames)
			.OnComboBoxOpening(this, &FCustomizableObjectNodeTableDetails::OnOpenVersionColumnComboBox)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(this, &FCustomizableObjectNodeTableDetails::GetVersionColumnComboBoxTextColor, &VersionColumnsOptionNames)
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionReset)));

		UICategory.AddCustomRow(LOCTEXT("MutableUIMetadataColumn_Selector","MutableUIMetadataColumn"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MutableUIMetadataColumn_SelectorText","Options UI Metadata Column"))
			.ToolTipText(LOCTEXT("MutableUIMetadataColumn_SelectorTooltip","Select a column that contains a Parameter UI Metadata for each Parameter Option (table row)."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(MutableMetaDataComboBox,STextComboBox)
			.InitiallySelectedItem(CurrentMutableMetadataColumn)
			.OptionsSource(&MutableMetaDataColumnsOptionNames)
			.OnComboBoxOpening(this, &FCustomizableObjectNodeTableDetails::OnOpenMutableMetadataComboBox)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(this, &FCustomizableObjectNodeTableDetails::GetComboBoxTextColor, &MutableMetaDataColumnsOptionNames, Node->ParamUIMetadataColumn)
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionReset)));
		
		UICategory.AddCustomRow(LOCTEXT("ThumbnailColumn_Selector", "ThumbnailColumn"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ThumbnailColumn_SelectorText", "Options Thumbnail Column"))
			.ToolTipText(LOCTEXT("ThumbnailColumn_SelectorTooltip", "Select a column that contains the assets to use its thumbnails as Option thumbnails."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(ThumbnailComboBox,STextComboBox)
			.InitiallySelectedItem(CurrentThumbnailColumn)
			.OptionsSource(&ThumbnailColumnOptionNames)
			.OnComboBoxOpening(this, &FCustomizableObjectNodeTableDetails::OnOpenThumbnailComboBox)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnThumbnailColumnComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(this, &FCustomizableObjectNodeTableDetails::GetComboBoxTextColor, &MutableMetaDataColumnsOptionNames, Node->ThumbnailColumn)
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnThumbnailColumnComboBoxSelectionReset)));



		// Anim Category -----------------------------------

		// Mesh Column Selector
		AnimationCategory.AddCustomRow(LOCTEXT("AnimationProperties", "Animation Properties"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimMeshColumnText", "Mesh Column: "))
			.ToolTipText(LOCTEXT("AnimMeshColumnTooltip", "Select a mesh column from the Data Table to edit its animation options (Applied to all LODs)."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SAssignNew(AnimMeshColumnComboBox, STextComboBox)
			.OptionsSource(&AnimMeshColumnOptionNames)
			.InitiallySelectedItem(AnimMeshColumnOptionNames[0])
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimMeshColumnComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnAnimMeshCustomRowResetButtonClicked)));


		// AnimBP Column Selector
		AnimationCategory.AddCustomRow(LOCTEXT("AnimationProperties", "Animation Properties"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimBPText", "Animation Blueprint Column: "))
			.ToolTipText(LOCTEXT("AnimBlueprintColumnTooltip", "Select an animation blueprint column from the Data Table that will be applied to the mesh selected"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SAssignNew(AnimComboBox, STextComboBox)
			.OptionsSource(&AnimOptionNames)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimInstanceComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnAnimCustomRowResetButtonClicked, EAnimColumnType::EACT_BluePrintColumn)))
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::AnimWidgetsVisibility));


		// AnimSlot Column Selector
		AnimationCategory.AddCustomRow(LOCTEXT("AnimationProperties", "Animation Properties"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimSlotText", "Animation Slot Column: "))
			.ToolTipText(LOCTEXT("AnimSlotColumnTooltip", "Select an animation slot column from the Data Table that will set to the slot value of the animation blueprint"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SAssignNew(AnimSlotComboBox, STextComboBox)
			.OptionsSource(&AnimSlotOptionNames)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimSlotComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnAnimCustomRowResetButtonClicked, EAnimColumnType::EACT_SlotColumn)))
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::AnimWidgetsVisibility));
		

		// AnimTags Column Selector
		AnimationCategory.AddCustomRow(LOCTEXT("AnimationProperties", "Animation Properties"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimTagsText", "Animation Tags Column: "))
			.ToolTipText(LOCTEXT("AnimTagColumnTooltip", "Select an animation tag column from the Data Table that will set to the animation tags of the animation blueprint"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SAssignNew(AnimTagsComboBox, STextComboBox)
			.OptionsSource(&AnimTagsOptionNames)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimTagsComboBoxSelectionChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnAnimCustomRowResetButtonClicked, EAnimColumnType::EACT_TagsColumn)))
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::AnimWidgetsVisibility));



		// Layout Category -----------------------------------

		//	Layout Selector
		SelectedLayout = nullptr;

		// Mesh selector of the layout editor
		LayoutCategory.AddCustomRow(LOCTEXT("TableLayoutEditor_MeshSelector", "Mesh Selector"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LayoutMeshColumnText", "Mesh Column: "))
			.ToolTipText(LOCTEXT("LayoutMeshColumnTooltip", "Select a mesh from the Data Table to edit its layout blocks."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.9f)
			[
				SAssignNew(LayoutMeshColumnComboBox, STextComboBox)
				.OptionsSource(&LayoutMeshColumnOptionNames)
				.InitiallySelectedItem(LayoutMeshColumnOptionNames[0])
				.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnLayoutMeshColumnComboBoxSelectionChanged)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			
			+ SHorizontalBox::Slot()
			.FillWidth(0.1f)
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Content()
				[
					SNew(SImage)
					.Image(UE_MUTABLE_GET_BRUSH(TEXT("Icons.Info")))
					.ToolTipText(FText(LOCTEXT("LaytoutMeshNoteTooltipText","Note:"
						"\nAs all meshes of a Data Table column share the same layout, the UVs shown"
						"\nin the editor are from the Default Skeletal Mesh of the Structure.")))
				]
			]
			
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnLayoutMeshCustomRowResetButtonClicked)));


		// Layout grid size options
		int32 MaxGridSize = 32;
		for (int32 Size = 1; Size <= MaxGridSize; Size *= 2)
		{
			LayoutGridSizes.Add(MakeShareable(new FString(FString::Printf(TEXT("%d x %d"), Size, Size))));
		}

		// Layout size selector widget
		LayoutCategory.AddCustomRow(LOCTEXT("TableBlocksDetails_SizeSelector", "SizeSelector"))
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::LayoutOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableLayoutGridSizeText", "Grid Size"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(GridSizeComboBox, STextComboBox)
			.OptionsSource(&LayoutGridSizes)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnGridSizeChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];


		// Layout Strategy options. Hardcoded: we should get names and tooltips from the enum property
		{
			LayoutPackingStrategies.Empty();
			LayoutPackingStrategiesTooltips.Empty();

			LayoutPackingStrategies.Add(MakeShareable(new FString("Resizable")));
			LayoutPackingStrategiesTooltips.Add(LOCTEXT("TableDetails_ResizableStrategyTooltip", "In a layout merge, Layout size will increase if blocks don't fit inside."));

			LayoutPackingStrategies.Add(MakeShareable(new FString("Fixed")));
			LayoutPackingStrategiesTooltips.Add(LOCTEXT("TableDetails_FixedStrategyTooltip", "In a layout merge, the layout will increase its size until the maximum layout grid size"
				"\nBlock sizes will be reduced if they don't fit inside the layout."
				"\nSet the reduction priority of each block to control which blocks are reduced first and how they are reduced."));

			LayoutPackingStrategies.Add(MakeShareable(new FString("Overlay")));
			LayoutPackingStrategiesTooltips.Add(LOCTEXT("TableDetails_OverlayStrategyTooltip", "In a layout merge, the layout will not be modified and blocks will be ignored."
				"\nExtend material nodes just add their layouts on top of the base one"));
		}

		// Layout strategy selector group widget
		IDetailGroup* LayoutStrategyOptionsGroup = &LayoutCategory.AddGroup(TEXT("TableLayoutStrategyOptionsGroup"), LOCTEXT("TableLayoutStrategyGroup", "Table Layout Strategy Group"), false, true);
		LayoutStrategyOptionsGroup->HeaderRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::LayoutOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableLayoutStrategy_Text", "Layout Strategy:"))
			.ToolTipText(LOCTEXT("TableLayoutStrategyTooltip", "Selects the packing strategy in case of a layout merge."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(StrategyComboBox, SSearchableComboBox)
			.OptionsSource(&LayoutPackingStrategies)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnLayoutPackingStrategyChanged)
			.OnGenerateWidget(this, &FCustomizableObjectNodeTableDetails::OnGenerateStrategyComboBox)
			.ToolTipText(this, &FCustomizableObjectNodeTableDetails::GetSelectedLayoutStrategyTooltip)
			[
				SNew(STextBlock)
				.Text(this, &FCustomizableObjectNodeTableDetails::GetSelectedLayoutStrategyName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];


		// Max layout size selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::FixedStrategyOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableMaxLayoutSize_Text", "Max Layout Size:"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(MaxGridSizeComboBox, STextComboBox)
			.OptionsSource(&LayoutGridSizes)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnMaxGridSizeChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];


		// Block reduction methods options
		{
			BlockReductionMethods.Empty();
			BlockReductionMethods.Add(MakeShareable(new FString("Halve")));
			BlockReductionMethodsTooltips.Add(LOCTEXT("TableDetails_HalveRedMethodTooltip", "Blocks will be reduced by half each time."));

			BlockReductionMethods.Add(MakeShareable(new FString("Unitary")));
			BlockReductionMethodsTooltips.Add(LOCTEXT("TableDetails_UnitaryRedMethodTooltip", "Blocks will be reduced by one unit each time."));
		}

		// Reduction method selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::FixedStrategyOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableReductionMethod_Text", "Reduction Method:"))
			.ToolTipText(LOCTEXT("TableReduction_Method_Tooltip", "Select how blocks will be reduced in case that they do not fit in the layout:"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(ReductionMethodComboBox, SSearchableComboBox)
			.OptionsSource(&BlockReductionMethods)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnReductionMethodChanged)
			.OnGenerateWidget(this, &FCustomizableObjectNodeTableDetails::OnGenerateReductionMethodComboBox)
			.ToolTipText(this, &FCustomizableObjectNodeTableDetails::GetSelectedLayoutReductionMethodTooltip)
			[
				SNew(STextBlock)
				.Text(this, &FCustomizableObjectNodeTableDetails::GetSelectedLayoutReductionMethodName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

		// Layout blocks editor widget
		LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

		// Block editor Widget
		LayoutCategory.AddCustomRow(LOCTEXT("TableLayoutEditor", "Layout Editor"))
		[
			SNew(SBox)
			.HeightOverride(700.0f)
			.WidthOverride(700.0f)
			[
				LayoutBlocksEditor.ToSharedRef()
			]
		];

		LayoutBlocksEditor->SetCurrentLayout(nullptr);
	}
}


void FCustomizableObjectNodeTableDetails::GenerateMeshColumnComboBoxOptions()
{
	AnimMeshColumnOptionNames.Empty();
	LayoutMeshColumnOptionNames.Empty();

	// Add first element to clear selection
	AnimMeshColumnOptionNames.Add(MakeShareable(new FString("- Nothing Selected -")));
	LayoutMeshColumnOptionNames.Add(MakeShareable(new FString("- Nothing Selected -")));

	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();

	if (!TableStruct)
	{
		return;
	}

	// Get mesh columns only
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
		{
			if (SoftObjectProperty->PropertyClass->IsChildOf(USkeletalMesh::StaticClass())
				|| SoftObjectProperty->PropertyClass->IsChildOf(UStaticMesh::StaticClass()))
			{
				FString MeshColumnName = DataTableUtils::GetPropertyExportName(ColumnProperty);
				AnimMeshColumnOptionNames.Add(MakeShareable(new FString(MeshColumnName)));

				for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
				{
					const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

					if (!PinData || PinData->ColumnName != MeshColumnName || Node->GetPinMeshType(Pin) != ETableMeshPinType::SKELETAL_MESH)
					{
						continue;
					}

					if (PinData && PinData->ColumnName == MeshColumnName)
					{
						if (PinData->Layouts.Num() > 1)
						{
							for (int32 LayoutIndex = 0; LayoutIndex < PinData->Layouts.Num(); ++LayoutIndex)
							{
								FString LayoutName = Pin->PinFriendlyName.ToString() + FString::Printf(TEXT(" UV_%d"), LayoutIndex);
								LayoutMeshColumnOptionNames.Add(MakeShareable(new FString(LayoutName)));
							}
						}
						else
						{
							LayoutMeshColumnOptionNames.Add(MakeShareable(new FString(Pin->PinFriendlyName.ToString())));
						}
					}
				}
			}
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnNodePinValueChanged()
{
	if (IDetailLayoutBuilder* DetailBuilder = DetailBuilderPtr.Pin().Get()) // Raw because we don't want to keep alive the details builder when calling the force refresh details
	{
		DetailBuilder->ForceRefreshDetails();
	}
}


// Anim Category --------------------------------------------------------------------------------

void FCustomizableObjectNodeTableDetails::GenerateAnimInstanceComboBoxOptions()
{
	// Options Reset
	AnimOptionNames.Empty();
	AnimSlotOptionNames.Empty();
	AnimTagsOptionNames.Empty();

	// Selection Reset
	AnimComboBox->ClearSelection();
	AnimSlotComboBox->ClearSelection();
	AnimTagsComboBox->ClearSelection();

	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();

	FString ColumnName;
	FTableNodeColumnData* MeshColumnData = nullptr;

	if (TableStruct && AnimMeshColumnComboBox.IsValid())
	{
		ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));

		MeshColumnData = Node->ColumnDataMap.Find(ColumnId);
	}
	else
	{
		return;
	}

	// Fill in name option arrays and set the selected item if any
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		if (FProperty* ColumnProperty = *It)
		{
			if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(ColumnProperty))
			{
				if (SoftClassProperty->MetaClass->IsChildOf(UAnimInstance::StaticClass()))
				{
					TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
					AnimOptionNames.Add(Option);

					if (MeshColumnData && MeshColumnData->AnimInstanceColumnName == *Option)
					{
						AnimComboBox->SetSelectedItem(Option);
					}
				}
			}

			else if (CastField<FIntProperty>(ColumnProperty) || CastField<FNameProperty>(ColumnProperty))
			{
				TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
				AnimSlotOptionNames.Add(Option);

				if (MeshColumnData && MeshColumnData->AnimSlotColumnName == *Option)
				{
					AnimSlotComboBox->SetSelectedItem(Option);
				}
			}

			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
			{
				if (StructProperty->Struct == TBaseStructure<FGameplayTagContainer>::Get())
				{
					TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
					AnimTagsOptionNames.Add(Option);

					if (MeshColumnData && MeshColumnData->AnimTagColumnName == *Option)
					{
						AnimTagsComboBox->SetSelectedItem(Option);
					}
				}
			}
		}
	}
}


EVisibility FCustomizableObjectNodeTableDetails::AnimWidgetsVisibility() const
{
	if (AnimMeshColumnComboBox.IsValid() && AnimMeshColumnComboBox->GetSelectedItem() != AnimMeshColumnOptionNames[0])
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}


void FCustomizableObjectNodeTableDetails::OnAnimMeshColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		GenerateAnimInstanceComboBoxOptions();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimInstanceComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	bool bIsMeshSelectionValid =  AnimMeshColumnComboBox->GetSelectedItem() != AnimMeshColumnOptionNames[0] && AnimMeshColumnComboBox->GetSelectedItem().IsValid();

	if (bIsMeshSelectionValid && Selection.IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
		FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);
		
		if (MeshColumnData)
		{
			MeshColumnData->AnimInstanceColumnName = *Selection;
		}
		else if(ColumnId.IsValid())
		{
			FTableNodeColumnData NewMeshColumnData;
			NewMeshColumnData.AnimInstanceColumnName = *Selection;

			Node->ColumnDataMap.Add(ColumnId, NewMeshColumnData);
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimSlotComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	bool bIsMeshSelectionValid = AnimMeshColumnComboBox->GetSelectedItem() != AnimMeshColumnOptionNames[0] && AnimMeshColumnComboBox->GetSelectedItem().IsValid();

	if (bIsMeshSelectionValid && Selection.IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
		FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

		if (MeshColumnData)
		{
			MeshColumnData->AnimSlotColumnName = *Selection;
		}
		else if (ColumnId.IsValid())
		{
			FTableNodeColumnData NewMeshColumnData;
			NewMeshColumnData.AnimSlotColumnName = *Selection;

			Node->ColumnDataMap.Add(ColumnId, NewMeshColumnData);
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimTagsComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	bool bIsMeshSelectionValid = AnimMeshColumnComboBox->GetSelectedItem() != AnimMeshColumnOptionNames[0] && AnimMeshColumnComboBox->GetSelectedItem().IsValid();

	if (bIsMeshSelectionValid && Selection.IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
		FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

		MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

		if (MeshColumnData)
		{
			MeshColumnData->AnimTagColumnName = *Selection;
		}
		else if (ColumnId.IsValid())
		{
			FTableNodeColumnData NewMeshColumnData;
			NewMeshColumnData.AnimTagColumnName = *Selection;

			Node->ColumnDataMap.Add(ColumnId, NewMeshColumnData);
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimMeshCustomRowResetButtonClicked()
{
	if (AnimMeshColumnOptionNames.Num())
	{
		AnimMeshColumnComboBox->SetSelectedItem(AnimMeshColumnOptionNames[0]);
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimCustomRowResetButtonClicked(EAnimColumnType ColumnType)
{
	if (!AnimMeshColumnComboBox->GetSelectedItem().IsValid())
	{
		return;
	}

	FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
	FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
	FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

	if (!MeshColumnData)
	{
		return;
	}

	switch (ColumnType)
	{
	case EAnimColumnType::EACT_BluePrintColumn:
	{
		MeshColumnData->AnimInstanceColumnName.Reset();
		AnimComboBox->ClearSelection();

		break;
	}
	case EAnimColumnType::EACT_SlotColumn:
	{
		MeshColumnData->AnimSlotColumnName.Reset();
		AnimSlotComboBox->ClearSelection();

		break;
	}
	case EAnimColumnType::EACT_TagsColumn:
	{
		MeshColumnData->AnimTagColumnName.Reset();
		AnimTagsComboBox->ClearSelection();

		break;
	}
	default:
		break;
	}

	Node->MarkPackageDirty();
}


// Layout Category --------------------------------------------------------------------------------

void FCustomizableObjectNodeTableDetails::OnLayoutMeshCustomRowResetButtonClicked()
{
	if (LayoutMeshColumnComboBox.IsValid() && LayoutMeshColumnOptionNames.Num())
	{
		LayoutMeshColumnComboBox->SetSelectedItem(LayoutMeshColumnOptionNames[0]);
		SelectedLayout = nullptr;
		LayoutBlocksEditor->SetCurrentLayout(nullptr);
	}
}


void FCustomizableObjectNodeTableDetails::OnLayoutMeshColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && Selection != LayoutMeshColumnOptionNames[0])
	{
		FString ColumnName;
		(*Selection).Split(" LOD_", &ColumnName, NULL);

		for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
		{
			const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

			if (PinData && PinData->ColumnName == ColumnName)
			{
				for (int32 LayoutIndex = 0; LayoutIndex < PinData->Layouts.Num(); ++LayoutIndex)
				{
					if (PinData->Layouts[LayoutIndex]->GetLayoutName() == *Selection)
					{
						LayoutBlocksEditor->SetCurrentLayout(PinData->Layouts[LayoutIndex]);
						SelectedLayout = PinData->Layouts[LayoutIndex];

						FillLayoutComboBoxOptions();
					}
				}
			}
		}
	}
	else
	{
		SelectedLayout = nullptr;
		LayoutBlocksEditor->SetCurrentLayout(nullptr);
	}
}


EVisibility FCustomizableObjectNodeTableDetails::LayoutOptionsVisibility() const
{
	return SelectedLayout.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}


EVisibility FCustomizableObjectNodeTableDetails::FixedStrategyOptionsVisibility() const
{
	return (SelectedLayout.IsValid() && SelectedLayout->GetPackingStrategy() == ECustomizableObjectTextureLayoutPackingStrategy::Fixed) ? EVisibility::Visible : EVisibility::Collapsed;
}


void FCustomizableObjectNodeTableDetails::FillLayoutComboBoxOptions()
{
	if (SelectedLayout.IsValid() && GridSizeComboBox.IsValid() && StrategyComboBox.IsValid()
		&& MaxGridSizeComboBox.IsValid() && ReductionMethodComboBox.IsValid())
	{
		for (int32 OptionIndex = 0; OptionIndex < LayoutGridSizes.Num(); ++OptionIndex)
		{
			int32 Size = 1 << OptionIndex;

			if (SelectedLayout->GetGridSize() == FIntPoint(Size))
			{
				GridSizeComboBox->SetSelectedItem(LayoutGridSizes[OptionIndex]);
			}

			if (SelectedLayout->GetMaxGridSize() == FIntPoint(Size))
			{
				MaxGridSizeComboBox->SetSelectedItem(LayoutGridSizes[OptionIndex]);
			}
		}

		StrategyComboBox->SetSelectedItem(LayoutPackingStrategies[(uint32)SelectedLayout->GetPackingStrategy()]);
		ReductionMethodComboBox->SetSelectedItem(BlockReductionMethods[(uint32)SelectedLayout->GetBlockReductionMethod()]);
	}
}


TSharedRef<SWidget> FCustomizableObjectNodeTableDetails::OnGenerateStrategyComboBox(TSharedPtr<FString> InItem) const
{
	FText Tooltip;

	if (InItem.IsValid())
	{
		int32 TooltipIndex = LayoutPackingStrategies.IndexOfByKey(InItem);

		if (LayoutPackingStrategiesTooltips.IsValidIndex(TooltipIndex))
		{
			//A list of tool tips should have been populated in a 1 to 1 correspondance
			check(LayoutPackingStrategies.Num() == LayoutPackingStrategiesTooltips.Num());
			Tooltip = LayoutPackingStrategiesTooltips[TooltipIndex];
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(*InItem.Get()))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(Tooltip);
}


TSharedRef<SWidget> FCustomizableObjectNodeTableDetails::OnGenerateReductionMethodComboBox(TSharedPtr<FString> InItem) const
{
	FText Tooltip;

	if (InItem.IsValid())
	{
		int32 TooltipIndex = BlockReductionMethods.IndexOfByKey(InItem);

		if (BlockReductionMethodsTooltips.IsValidIndex(TooltipIndex))
		{
			//A list of tool tips should have been populated in a 1 to 1 correspondance
			check(BlockReductionMethods.Num() == BlockReductionMethodsTooltips.Num());
			Tooltip = BlockReductionMethodsTooltips[TooltipIndex];
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(*InItem.Get()))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(Tooltip);
}


FText FCustomizableObjectNodeTableDetails::GetSelectedLayoutStrategyName() const
{
	if (SelectedLayout.IsValid())
	{
		return FText::FromString(*LayoutPackingStrategies[(uint32)SelectedLayout->GetPackingStrategy()]);
	}

	return FText();
}


FText FCustomizableObjectNodeTableDetails::GetSelectedLayoutReductionMethodName() const
{
	if (SelectedLayout.IsValid())
	{
		return FText::FromString(*BlockReductionMethods[(uint32)SelectedLayout->GetBlockReductionMethod()]);
	}

	return FText();
}


void FCustomizableObjectNodeTableDetails::OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		int Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (SelectedLayout->GetGridSize().X != Size || SelectedLayout->GetGridSize().Y != Size)
		{
			SelectedLayout->SetGridSize(FIntPoint(Size));

			// Adjust all the blocks sizes
			for (int b = 0; b < SelectedLayout->Blocks.Num(); ++b)
			{
				SelectedLayout->Blocks[b].Min.X = FMath::Min(SelectedLayout->Blocks[b].Min.X, Size - 1);
				SelectedLayout->Blocks[b].Min.Y = FMath::Min(SelectedLayout->Blocks[b].Min.Y, Size - 1);
				SelectedLayout->Blocks[b].Max.X = FMath::Min(SelectedLayout->Blocks[b].Max.X, Size);
				SelectedLayout->Blocks[b].Max.Y = FMath::Min(SelectedLayout->Blocks[b].Max.Y, Size);
			}

			Node->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		uint32 selection = LayoutPackingStrategies.IndexOfByKey(NewSelection);

		if (SelectedLayout->GetPackingStrategy() != (ECustomizableObjectTextureLayoutPackingStrategy)selection)
		{
			SelectedLayout->SetPackingStrategy((ECustomizableObjectTextureLayoutPackingStrategy)selection);
			Node->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		int Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (SelectedLayout->GetMaxGridSize().X != Size || SelectedLayout->GetMaxGridSize().Y != Size)
		{
			SelectedLayout->SetMaxGridSize(FIntPoint(Size));
			SelectedLayout->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		uint32 selection = BlockReductionMethods.IndexOfByKey(NewSelection);

		if (SelectedLayout->GetBlockReductionMethod() != (ECustomizableObjectLayoutBlockReductionMethod)selection)
		{
			SelectedLayout->SetBlockReductionMethod((ECustomizableObjectLayoutBlockReductionMethod)selection);
			Node->MarkPackageDirty();
		}
	}
}


FText FCustomizableObjectNodeTableDetails::GetSelectedLayoutStrategyTooltip() const
{
	if (SelectedLayout.IsValid())
	{
		//A list of tool tips should have been populated in a 1 to 1 correspondance
		check(LayoutPackingStrategies.Num() == LayoutPackingStrategiesTooltips.Num());

		return LayoutPackingStrategiesTooltips[(uint32)SelectedLayout->GetPackingStrategy()];
	}

	return FText();
}


FText FCustomizableObjectNodeTableDetails::GetSelectedLayoutReductionMethodTooltip() const
{
	if (SelectedLayout.IsValid())
	{
		//A list of tool tips should have been populated in a 1 to 1 correspondance
		check(BlockReductionMethods.Num() == BlockReductionMethodsTooltips.Num());

		return BlockReductionMethodsTooltips[(uint32)SelectedLayout->GetBlockReductionMethod()];
	}

	return FText();
}


// Metadata Category --------------------------------------------------------------------------------

TSharedPtr<FString> FCustomizableObjectNodeTableDetails::GenerateMutableMetaDataColumnComboBoxOptions()
{
	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();
	TSharedPtr<FString> CurrentSelection;
	MutableMetaDataColumnsOptionNames.Reset();

	if (!TableStruct)
	{
		return CurrentSelection;
	}

	// Iterating struct Options
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
		{
			if (StructProperty->Struct == FMutableParamUIMetadata::StaticStruct())
			{
				TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
				MutableMetaDataColumnsOptionNames.Add(Option);

				if (*Option == Node->ParamUIMetadataColumn)
				{
					CurrentSelection = MutableMetaDataColumnsOptionNames.Last();
				}
			}
		}
	}

	if (!Node->ParamUIMetadataColumn.IsNone() && !CurrentSelection)
	{
		MutableMetaDataColumnsOptionNames.Add(MakeShareable(new FString(Node->ParamUIMetadataColumn.ToString())));
		CurrentSelection = MutableMetaDataColumnsOptionNames.Last();
	}

	return CurrentSelection;
}


void FCustomizableObjectNodeTableDetails::OnOpenMutableMetadataComboBox()
{
	TSharedPtr<FString> CurrentSelection = GenerateMutableMetaDataColumnComboBoxOptions();

	if (MutableMetaDataComboBox.IsValid())
	{
		MutableMetaDataComboBox->ClearSelection();
		MutableMetaDataComboBox->RefreshOptions();
		MutableMetaDataComboBox->SetSelectedItem(CurrentSelection);
	}
}


void FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection && Node->ParamUIMetadataColumn != FName(*Selection) 
		&& (SelectInfo == ESelectInfo::OnKeyPress || SelectInfo == ESelectInfo::OnMouseClick))
	{
		Node->ParamUIMetadataColumn = FName(*Selection);
		Node->MarkPackageDirty();
	}
}


FSlateColor FCustomizableObjectNodeTableDetails::GetComboBoxTextColor(TArray<TSharedPtr<FString>>* CurrentOptions, const FName ColumnName) const
{	
	if (Node->FindTableProperty(Node->GetTableNodeStruct(), ColumnName) || ColumnName.IsNone())
	{
		return FSlateColor::UseForeground();
	}

	// Table Struct null or does not contain the selected property anymore
	return FSlateColor(FLinearColor(0.9f, 0.05f, 0.05f, 1.0f));
}


void FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionReset()
{
	Node->ParamUIMetadataColumn = NAME_None;

	if (MutableMetaDataComboBox.IsValid())
	{
		GenerateMutableMetaDataColumnComboBoxOptions();
		MutableMetaDataComboBox->ClearSelection();
		MutableMetaDataComboBox->RefreshOptions();
	}
}


TSharedPtr<FString> FCustomizableObjectNodeTableDetails::GenerateThumbnailColumnComboBoxOptions()
{
	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();
	TSharedPtr<FString> CurrentSelection;
	ThumbnailColumnOptionNames.Reset();

	if (!TableStruct)
	{
		return CurrentSelection;
	}

	// Iterating struct Options
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		if (const FSoftObjectProperty* ObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
		{
			TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
			ThumbnailColumnOptionNames.Add(Option);

			if (*Option == Node->ThumbnailColumn)
			{
				CurrentSelection = ThumbnailColumnOptionNames.Last();
			}
		}
	}

	if (!Node->ThumbnailColumn.IsNone() && !CurrentSelection)
	{
		ThumbnailColumnOptionNames.Add(MakeShareable(new FString(Node->ThumbnailColumn.ToString())));
		CurrentSelection = ThumbnailColumnOptionNames.Last();
	}

	return CurrentSelection;
}


void FCustomizableObjectNodeTableDetails::OnOpenThumbnailComboBox()
{
	TSharedPtr<FString> CurrentSelection = GenerateThumbnailColumnComboBoxOptions();

	if (ThumbnailComboBox.IsValid())
	{
		ThumbnailComboBox->ClearSelection();
		ThumbnailComboBox->RefreshOptions();
		ThumbnailComboBox->SetSelectedItem(CurrentSelection);
	}
}


void FCustomizableObjectNodeTableDetails::OnThumbnailColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection && Node->ThumbnailColumn != FName(*Selection)
		&& (SelectInfo == ESelectInfo::OnKeyPress || SelectInfo == ESelectInfo::OnMouseClick))
	{
		Node->ThumbnailColumn = FName(*Selection);
		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnThumbnailColumnComboBoxSelectionReset()
{
	Node->ThumbnailColumn = NAME_None;

	if (ThumbnailComboBox.IsValid())
	{
		GenerateThumbnailColumnComboBoxOptions();
		ThumbnailComboBox->ClearSelection();
		ThumbnailComboBox->RefreshOptions();
	}
}


// Version Bridge Category --------------------------------------------------------------------------------

TSharedPtr<FString> FCustomizableObjectNodeTableDetails::GenerateVersionColumnComboBoxOptions()
{
	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();
	TSharedPtr<FString> CurrentSelection;
	VersionColumnsOptionNames.Reset();

	if (!TableStruct)
	{
		return CurrentSelection;
	}

	// Iterating struct Options
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
		VersionColumnsOptionNames.Add(Option);

		if (*Option == Node->VersionColumn)
		{
			CurrentSelection = VersionColumnsOptionNames.Last();
		}
	}

	if (!Node->VersionColumn.IsNone() && !CurrentSelection)
	{
		VersionColumnsOptionNames.Add(MakeShareable(new FString(Node->VersionColumn.ToString())));
		CurrentSelection = VersionColumnsOptionNames.Last();
	}

	return CurrentSelection;
}


void FCustomizableObjectNodeTableDetails::OnOpenVersionColumnComboBox()
{
	TSharedPtr<FString> CurrentSelection = GenerateVersionColumnComboBoxOptions();

	if (VersionColumnsComboBox.IsValid())
	{
		VersionColumnsComboBox->ClearSelection();
		VersionColumnsComboBox->RefreshOptions();
		VersionColumnsComboBox->SetSelectedItem(CurrentSelection);
	}
}


void FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection && Node->VersionColumn != FName(*Selection)
		&& (SelectInfo == ESelectInfo::OnKeyPress || SelectInfo == ESelectInfo::OnMouseClick))
	{
		Node->VersionColumn = FName(*Selection);
		Node->MarkPackageDirty();
	}
}


FSlateColor FCustomizableObjectNodeTableDetails::GetVersionColumnComboBoxTextColor(TArray<TSharedPtr<FString>>* CurrentOptions) const
{
	if (Node->FindTableProperty(Node->GetTableNodeStruct(), Node->VersionColumn) || Node->VersionColumn.IsNone())
	{
		return FSlateColor::UseForeground();
	}

	// Table Struct null or does not contain the selected property anymore
	return FSlateColor(FLinearColor(0.9f, 0.05f, 0.05f, 1.0f));
}


void FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionReset()
{
	Node->VersionColumn = NAME_None;

	if (VersionColumnsComboBox.IsValid())
	{
		GenerateVersionColumnComboBoxOptions();
		VersionColumnsComboBox->ClearSelection();
		VersionColumnsComboBox->RefreshOptions();
	}
}


#undef LOCTEXT_NAMESPACE
