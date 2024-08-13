// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocksDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "Layout/Visibility.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "PropertyCustomizationHelpers.h"
#include "SSearchableComboBox.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"

class FString;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeLayoutBlocksDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeLayoutBlocksDetails );
}


void FCustomizableObjectNodeLayoutBlocksDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeLayoutBlocks>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& CustomizableObjectCategory = DetailBuilder.EditCategory( "LayoutOptions" );
	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "LayoutBlocksEditor" );

	if (Node.IsValid())
	{
		LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

		TSharedPtr<FString> CurrentSize, CurrentStrategy, CurrentAutoBlocks, CurrentAutoBlocksMerge, CurrentMaxSize, CurrentReductionMethod;
		FillComboBoxOptionsArrays(CurrentSize, CurrentStrategy, CurrentAutoBlocks, CurrentAutoBlocksMerge, CurrentMaxSize, CurrentReductionMethod);

		// Layout strategy selector group widget
		IDetailGroup* LayoutStrategyOptionsGroup = &CustomizableObjectCategory.AddGroup(TEXT("LayoutStrategyOptionsGroup"), LOCTEXT("LayoutStrategyGroup", "Layout Strategy Group"), false, true);
		LayoutStrategyOptionsGroup->HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LayoutStrategy_Text", "Layout Strategy"))
			.ToolTipText(LOCTEXT("LayoutStrategyTooltip", "Selects the UV packing strategy in case of a mesh merge or clip."))
			.Font(DetailBuilder.GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SSearchableComboBox)
			.InitiallySelectedItem(CurrentStrategy)
			.OptionsSource(&LayoutPackingStrategies)
			.OnSelectionChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnLayoutPackingStrategyChanged)
			.OnGenerateWidget(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateStrategyComboBox)
			.ToolTipText(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutStrategyTooltip)
			[
				SNew(STextBlock)
				.Text(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutStrategyName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

		// Automatic block generation strategy
		LayoutStrategyOptionsGroup->AddWidgetRow()
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					return Node->Layout->PackingStrategy != ECustomizableObjectTextureLayoutPackingStrategy::Overlay ? EVisibility::Visible : EVisibility::Collapsed;
				}))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("AutoBlockStrategy_Text", "Automatic Blocks Strategy"))
					.ToolTipText(LOCTEXT("AutoBlockStrategyTooltip", "Selects the strategy to create layout blocks from unassigned UVs."))
					.Font(DetailBuilder.GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SSearchableComboBox)
					.InitiallySelectedItem(CurrentAutoBlocks)
					.OptionsSource(&AutoBlocksStrategies)
					.OnSelectionChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnAutoBlocksChanged)
					.OnGenerateWidget(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateAutoBlocksComboBox)
					.ToolTipText(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksTooltip)
					[
						SNew(STextBlock)
							.Text(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksName)
							.Font(IDetailLayoutBuilder::GetDetailFont())
					]
			];

		// Option to merge child automatic blocks
		LayoutStrategyOptionsGroup->AddWidgetRow()
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					if (Node->Layout->PackingStrategy == ECustomizableObjectTextureLayoutPackingStrategy::Overlay
						||
						Node->Layout->AutomaticBlocksStrategy != ECustomizableObjectLayoutAutomaticBlocksStrategy::UVIslands)
					{
						return EVisibility::Collapsed;
					}
					return EVisibility::Visible;
				}))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("AutoBlockMergeStrategy_Text", "Automatic Blocks Merge Strategy"))
					.ToolTipText(LOCTEXT("AutoBlockMergeStrategyTooltip", "Selects the strategy to merge blocks during automatic generation."))
					.Font(DetailBuilder.GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SSearchableComboBox)
					.InitiallySelectedItem(CurrentAutoBlocksMerge)
					.OptionsSource(&AutoBlocksMergeStrategies)
					.OnSelectionChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnAutoBlocksMergeChanged)
					.OnGenerateWidget(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateAutoBlocksMergeComboBox)
					.ToolTipText(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksMergeTooltip)
					[
						SNew(STextBlock)
							.Text(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksMergeName)
							.Font(IDetailLayoutBuilder::GetDetailFont())
					]
			];

		// Layout size selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					if (Node->Layout->PackingStrategy == ECustomizableObjectTextureLayoutPackingStrategy::Overlay)
					{
						return EVisibility::Collapsed;
					}
					return EVisibility::Visible;
				}))
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
					.OnSelectionChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnGridSizeChanged)
					.Font(DetailBuilder.GetDetailFont())
			];


		// Max layout size selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeLayoutBlocksDetails::FixedStrategyOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MaxLayoutSize_Text", "Max Layout Size"))
			.Font(DetailBuilder.GetDetailFont())
		]
		.ValueContent()
		[
			SNew(STextComboBox)
			.InitiallySelectedItem(CurrentMaxSize)
			.OptionsSource(&LayoutGridSizes)
			.OnSelectionChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnMaxGridSizeChanged)
			.Font(DetailBuilder.GetDetailFont())
		];


		// Block reduction methods options
		{
			BlockReductionMethods.Empty();
			BlockReductionMethods.Add(MakeShareable(new FString("Halve")));
			BlockReductionMethodsTooltips.Add(LOCTEXT("LayoutDetails_HalveRedMethodTooltip", "Blocks will be reduced by half each time."));

			BlockReductionMethods.Add(MakeShareable(new FString("Unitary")));
			BlockReductionMethodsTooltips.Add(LOCTEXT("LayoutDetails_UnitaryRedMethodTooltip", "Blocks will be reduced by one unit each time."));
		}

		// Reduction method selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeLayoutBlocksDetails::FixedStrategyOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ReductionMethod_Text", "Reduction Method"))
			.ToolTipText(LOCTEXT("Reduction_Method_Tooltip", "Select how blocks will be reduced in case that they do not fit in the layout."))
			.Font(DetailBuilder.GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SSearchableComboBox)
			.InitiallySelectedItem(CurrentReductionMethod)
			.OptionsSource(&BlockReductionMethods)
			.OnSelectionChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnReductionMethodChanged)
			.OnGenerateWidget(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateReductionMethodComboBox)
			.ToolTipText(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutReductionMethodTooltip)
			[
				SNew(STextBlock)
					.Text(this, &FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutReductionMethodName)
					.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

		// Warning selector group widget
		IDetailGroup* IgnoreWarningsGroup = &CustomizableObjectCategory.AddGroup(TEXT("IgnoreWarningsOptionsGroup"), LOCTEXT("IgnoreWarningsOptions", "Ignore Unassigned Vertives Warning group"), false, true);
		IgnoreWarningsGroup->HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LayoutOptions_IgnoreLodsCheckBox_Text", "Ignore Unassigned Vertices Warning"))
			.ToolTipText(LOCTEXT("LayoutOptions_IgnoreLodsCheckBox_Tooltip",
				"If true, warning message \"Source mesh has vertices not assigned to any layout block\" will be ignored."
				"\n Note:"
				"\n This warning can appear when a CO has more than one LOD using the same Layout Block node and these LODs have been generated using the automatic LOD generation."
				"\n (At high LODs, some vertices may have been displaced from their original position which means they could have been displaced outside their layout blocks.)"
				"\n Ignoring these warnings can cause some visual artifacts that may or may not be visually important at higher LODs."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked((Node->Layout && Node->Layout->GetIgnoreVertexLayoutWarnings()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnIgnoreErrorsCheckStateChanged)
		];
		
		// LOD selector widget
		IgnoreWarningsGroup->AddWidgetRow()
		.NameContent()
		[
			SAssignNew(LODSelectorTextWidget, STextBlock)
			.Text(LOCTEXT("LayoutOptions_IgnoreLod_Text", "First LOD to ignore"))
			.ToolTipText(LOCTEXT("LayoutOptions_IgnoreLod_Tooltip", "LOD from which vertex warning messages will be ignored."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(Node->Layout ? Node->Layout->GetIgnoreVertexLayoutWarnings() : false)
		]
		.ValueContent()
		[
			SAssignNew(LODSelectorWidget, SSpinBox<int32>)
			.Value_Lambda([this]()->int32
			{
				if (Node->Layout)
				{
					return Node->Layout->GetFirstLODToIgnoreWarnings();
				}

				return 0;
			})
			.IsEnabled(Node->Layout ? Node->Layout->GetIgnoreVertexLayoutWarnings() : false)
			.OnValueChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnLODBoxValueChanged)
			.MinValue(0)
			.Delta(1)
			.AlwaysUsesDeltaSnap(true)
			.MinDesiredWidth(40.0f)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		BlocksCategory.AddCustomRow( LOCTEXT("BlocksDetails_BlockInstructions", "BlockInstructions") )
		[
			SNew(SBox)
			.HeightOverride(700.0f)
			.WidthOverride(700.0f)
			[
				LayoutBlocksEditor.ToSharedRef()
			]
		];
		
		LayoutBlocksEditor->SetCurrentLayout(Node->Layout);
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


void FCustomizableObjectNodeLayoutBlocksDetails::OnIgnoreErrorsCheckStateChanged(ECheckBoxState State)
{
	if (Node->Layout)
	{
		bool bStateBool = State == ECheckBoxState::Checked;
		Node->Layout->SetIgnoreVertexLayoutWarnings(bStateBool);

		LODSelectorWidget->SetEnabled(bStateBool);
		LODSelectorTextWidget->SetEnabled(bStateBool);
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnLODBoxValueChanged(int32 Value)
{
	if (Node->Layout)
	{
		Node->Layout->SetIgnoreWarningsLOD(Value);
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::FillComboBoxOptionsArrays(
	TSharedPtr<FString>& CurrGridSize, 
	TSharedPtr<FString>& CurrStrategy, 
	TSharedPtr<FString>& CurrAutoBlocks,
	TSharedPtr<FString>& CurrAutoBlocksMerge,
	TSharedPtr<FString>& CurrMaxSize,
	TSharedPtr<FString>& CurrRedMethod)
{
	if (Node->Layout)
	{
		// Static const variable?
		int32 MaxGridSize = 128;
		LayoutGridSizes.Empty();

		for (int32 Size = 1; Size <= MaxGridSize; Size *= 2)
		{
			LayoutGridSizes.Add(MakeShareable(new FString(FString::Printf(TEXT("%d x %d"), Size, Size))));

			if (Node->Layout->GetGridSize() == FIntPoint(Size))
			{
				CurrGridSize = LayoutGridSizes.Last();
			}

			if (Node->Layout->GetMaxGridSize() == FIntPoint(Size))
			{
				CurrMaxSize = LayoutGridSizes.Last();
			}
		}

		// Layout Strategy options. Hardcoded: we should get names and tooltips from the enum property
		{
			LayoutPackingStrategies.Empty();
			LayoutPackingStrategiesOptions.Empty();

			LayoutPackingStrategies.Add(MakeShareable(new FString("Package and Grow Textures")));
			LayoutPackingStrategiesOptions.Add(
				{
					ECustomizableObjectTextureLayoutPackingStrategy::Resizable ,
					LOCTEXT("LayoutDetails_ResizableStrategyTooltip", "In a layout merge, Layout size will increase if blocks don't fit inside.")
				});

			LayoutPackingStrategies.Add(MakeShareable(new FString("Package and Shrink Textures")));
			LayoutPackingStrategiesOptions.Add(
				{
					ECustomizableObjectTextureLayoutPackingStrategy::Fixed,
					LOCTEXT("LayoutDetails_FixedStrategyTooltip", "In a layout merge, the layout will increase its size until the maximum layout grid size"
					"\nBlock sizes will be reduced if they don't fit inside the layout."
					"\nSet the reduction priority of each block to control which blocks are reduced first and how they are reduced.")
				});

			LayoutPackingStrategies.Add(MakeShareable(new FString("Overlay")));
			LayoutPackingStrategiesOptions.Add(
				{
					ECustomizableObjectTextureLayoutPackingStrategy::Overlay,
					LOCTEXT("LayoutDetails_OverlayStrategyTooltip", "In a layout merge, the layout will not be modified and blocks will be ignored."
					"\nExtend material nodes just add their layouts on top of the base one")
				});
		}
		CurrStrategy = LayoutPackingStrategies[(uint32)Node->Layout->PackingStrategy];

		BlockReductionMethods.Empty();
		BlockReductionMethods.Add(MakeShareable(new FString("Halve")));
		BlockReductionMethods.Add(MakeShareable(new FString("Unitary")));
		CurrRedMethod = BlockReductionMethods[(uint32)Node->Layout->BlockReductionMethod];

		{
			AutoBlocksStrategies.Empty();
			AutoBlocksStrategiesOptions.Empty();

			AutoBlocksStrategies.Add(MakeShareable(new FString("Rectangles")));
			AutoBlocksStrategiesOptions.Add(
				{
					ECustomizableObjectLayoutAutomaticBlocksStrategy::Rectangles ,
					LOCTEXT("AutoBlockDetails_RectanglesStrategyTooltip", "Try to build rectangles splitting the UVs.")
				});

			AutoBlocksStrategies.Add(MakeShareable(new FString("UV islands")));
			AutoBlocksStrategiesOptions.Add(
				{
					ECustomizableObjectLayoutAutomaticBlocksStrategy::UVIslands,
					LOCTEXT("AutoBlockDetails_UVIslandsStrategyTooltip", "Try to build rectangles around each UV island, with a mask.")
				});

			AutoBlocksStrategies.Add(MakeShareable(new FString("Ignore (legacy)")));
			AutoBlocksStrategiesOptions.Add(
				{
					ECustomizableObjectLayoutAutomaticBlocksStrategy::Ignore,
					LOCTEXT("AutoBlockDetails_IgnoreStrategyTooltip", "Legacy behavior: assign to first block, or ignore if none.")
				});
		}
		CurrAutoBlocks = AutoBlocksStrategies[(uint32)Node->Layout->AutomaticBlocksStrategy];

		{
			AutoBlocksMergeStrategies.Empty();
			AutoBlocksMergeStrategiesOptions.Empty();

			AutoBlocksMergeStrategies.Add(MakeShareable(new FString("Don't merge")));
			AutoBlocksMergeStrategiesOptions.Add(
				{
					ECustomizableObjectLayoutAutomaticBlocksMergeStrategy::DontMerge ,
					LOCTEXT("AutoBlockMerge_DontMergeTooltip", "Don't merge and make each UV island a unique block.")
				});

			AutoBlocksMergeStrategies.Add(MakeShareable(new FString("Merge child blocks")));
			AutoBlocksMergeStrategiesOptions.Add(
				{
					ECustomizableObjectLayoutAutomaticBlocksMergeStrategy::MergeChildBlocks,
					LOCTEXT("AutoBlockMerge_ChildBlocksTooltip", "Merge the blocks that are already fully included in another block.")
				});
		}
		CurrAutoBlocksMerge = AutoBlocksMergeStrategies[(uint32)Node->Layout->AutomaticBlocksMergeStrategy];

	}
}

EVisibility FCustomizableObjectNodeLayoutBlocksDetails::FixedStrategyOptionsVisibility() const
{
	return Node->Layout->PackingStrategy == ECustomizableObjectTextureLayoutPackingStrategy::Fixed ? EVisibility::Visible : EVisibility::Collapsed;
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (Node->Layout)
	{
		int Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (Node->Layout->GetGridSize().X != Size || Node->Layout->GetGridSize().Y != Size)
		{
			Node->Layout->SetGridSize(FIntPoint(Size));

			Node->MarkPackageDirty();

			// Reset to update the UI.
			LayoutBlocksEditor->SetCurrentLayout(Node->Layout);
		}
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (Node->Layout)
	{
		uint32 Selection = LayoutPackingStrategies.IndexOfByKey(NewSelection);

		if (Node->Layout->PackingStrategy != LayoutPackingStrategiesOptions[Selection].Value)
		{
			Node->Layout->PackingStrategy = LayoutPackingStrategiesOptions[Selection].Value;
			Node->MarkPackageDirty();

			// Reset to update the UI.
			LayoutBlocksEditor->SetCurrentLayout(Node->Layout);
		}
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnAutoBlocksChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (Node->Layout)
	{
		uint32 Selection = AutoBlocksStrategies.IndexOfByKey(NewSelection);

		if (Node->Layout->AutomaticBlocksStrategy != AutoBlocksStrategiesOptions[Selection].Value)
		{
			Node->Layout->AutomaticBlocksStrategy = AutoBlocksStrategiesOptions[Selection].Value;
			Node->MarkPackageDirty();

			// Reset to update the UI.
			LayoutBlocksEditor->SetCurrentLayout(Node->Layout);
		}
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnAutoBlocksMergeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (Node->Layout)
	{
		uint32 Selection = AutoBlocksMergeStrategies.IndexOfByKey(NewSelection);

		if (Node->Layout->AutomaticBlocksMergeStrategy != AutoBlocksMergeStrategiesOptions[Selection].Value)
		{
			Node->Layout->AutomaticBlocksMergeStrategy = AutoBlocksMergeStrategiesOptions[Selection].Value;
			Node->MarkPackageDirty();

			// Reset to update the UI.
			LayoutBlocksEditor->SetCurrentLayout(Node->Layout);
		}
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (Node->Layout)
	{
		int32 Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (Node->Layout->GetMaxGridSize().X != Size || Node->Layout->GetMaxGridSize().Y != Size)
		{
			Node->Layout->SetMaxGridSize(FIntPoint(Size));
			Node->Layout->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (Node->Layout)
	{
		uint32 Selection = BlockReductionMethods.IndexOfByKey(NewSelection);

		if (Node->Layout->BlockReductionMethod != (ECustomizableObjectLayoutBlockReductionMethod)Selection)
		{
			Node->Layout->BlockReductionMethod = (ECustomizableObjectLayoutBlockReductionMethod)Selection;
			Node->MarkPackageDirty();
		}
	}
}


TSharedRef<SWidget> FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateStrategyComboBox(TSharedPtr<FString> InItem) const
{
	FText Tooltip;

	if (InItem.IsValid())
	{
		int32 TooltipIndex = LayoutPackingStrategies.IndexOfByKey(InItem);

		if (LayoutPackingStrategies.IsValidIndex(TooltipIndex))
		{
			//A list of tool tips should have been populated in a 1 to 1 correspondance
			check(LayoutPackingStrategies.Num() == LayoutPackingStrategies.Num());
			Tooltip = LayoutPackingStrategiesOptions[TooltipIndex].Tooltip;
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(*InItem.Get()))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(Tooltip);
}


TSharedRef<SWidget> FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateAutoBlocksComboBox(TSharedPtr<FString> InItem) const
{
	FText Tooltip;

	if (InItem.IsValid())
	{
		int32 TooltipIndex = AutoBlocksStrategies.IndexOfByKey(InItem);

		if (AutoBlocksStrategies.IsValidIndex(TooltipIndex))
		{
			//A list of tool tips should have been populated in a 1 to 1 correspondance
			check(AutoBlocksStrategies.Num() == AutoBlocksStrategies.Num());
			Tooltip = AutoBlocksStrategiesOptions[TooltipIndex].Tooltip;
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(*InItem.Get()))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(Tooltip);
}


TSharedRef<SWidget> FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateAutoBlocksMergeComboBox(TSharedPtr<FString> InItem) const
{
	FText Tooltip;

	if (InItem.IsValid())
	{
		int32 TooltipIndex = AutoBlocksMergeStrategies.IndexOfByKey(InItem);

		if (AutoBlocksMergeStrategies.IsValidIndex(TooltipIndex))
		{
			//A list of tool tips should have been populated in a 1 to 1 correspondance
			check(AutoBlocksMergeStrategies.Num() == AutoBlocksMergeStrategies.Num());
			Tooltip = AutoBlocksMergeStrategiesOptions[TooltipIndex].Tooltip;
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(*InItem.Get()))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(Tooltip);
}


TSharedRef<SWidget> FCustomizableObjectNodeLayoutBlocksDetails::OnGenerateReductionMethodComboBox(TSharedPtr<FString> InItem) const
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


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutStrategyName() const
{
	if (Node->Layout)
	{
		return FText::FromString(*LayoutPackingStrategies[(uint32)Node->Layout->PackingStrategy]);
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksName() const
{
	if (Node->Layout)
	{
		return FText::FromString(*AutoBlocksStrategies[(uint32)Node->Layout->AutomaticBlocksStrategy]);
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksMergeName() const
{
	if (Node->Layout)
	{
		return FText::FromString(*AutoBlocksMergeStrategies[(uint32)Node->Layout->AutomaticBlocksMergeStrategy]);
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutReductionMethodName() const
{
	if (Node->Layout)
	{
		return FText::FromString(*BlockReductionMethods[(uint32)Node->Layout->BlockReductionMethod]);
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutStrategyTooltip() const
{
	if (Node->Layout)
	{
		//A list of tool tips should have been populated in a 1 to 1 correspondance
		check(LayoutPackingStrategies.Num() == LayoutPackingStrategiesOptions.Num());

		return LayoutPackingStrategiesOptions[(uint32)Node->Layout->PackingStrategy].Tooltip;
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedLayoutReductionMethodTooltip() const
{
	if (Node->Layout)
	{
		//A list of tool tips should have been populated in a 1 to 1 correspondance
		check(BlockReductionMethods.Num() == BlockReductionMethodsTooltips.Num());

		return BlockReductionMethodsTooltips[(uint32)Node->Layout->BlockReductionMethod];
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksTooltip() const
{
	if (Node->Layout)
	{
		//A list of tool tips should have been populated in a 1 to 1 correspondance
		check(AutoBlocksStrategies.Num() == AutoBlocksStrategiesOptions.Num());

		return AutoBlocksStrategiesOptions[(uint32)Node->Layout->AutomaticBlocksStrategy].Tooltip;
	}

	return FText();
}


FText FCustomizableObjectNodeLayoutBlocksDetails::GetSelectedAutoBlocksMergeTooltip() const
{
	if (Node->Layout)
	{
		//A list of tool tips should have been populated in a 1 to 1 correspondance
		check(AutoBlocksMergeStrategies.Num() == AutoBlocksMergeStrategiesOptions.Num());

		return AutoBlocksMergeStrategiesOptions[(uint32)Node->Layout->AutomaticBlocksMergeStrategy].Tooltip;
	}

	return FText();
}

#undef LOCTEXT_NAMESPACE
