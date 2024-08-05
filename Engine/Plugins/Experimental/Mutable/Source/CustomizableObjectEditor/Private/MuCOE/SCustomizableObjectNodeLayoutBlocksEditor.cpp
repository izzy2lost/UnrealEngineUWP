// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SCustomizableObjectLayoutGrid.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SToolTip.h"
#include "ScopedTransaction.h"

class ISlateStyle;
class SWidget;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

/**
 * 
 */
class FLayoutEditorCommands : public TCommands<FLayoutEditorCommands>
{

public:
	FLayoutEditorCommands() : TCommands<FLayoutEditorCommands>
	(
		"LayoutEditorCommands", // Context name for fast lookup
		NSLOCTEXT( "CustomizableObjectEditor", "LayoutEditorCommands", "Layout Editor" ), // Localized context name for displaying
		NAME_None, // Parent
		FCustomizableObjectEditorStyle::GetStyleSetName()
	)
	{
	}	
	
	/**  */
	TSharedPtr< FUICommandInfo > AddBlock;
	TSharedPtr< FUICommandInfo > RemoveBlock;
	TSharedPtr< FUICommandInfo > GenerateBlocks;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override
	{
		UI_COMMAND( AddBlock, "Add Block", "Add a new block to the layout.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( RemoveBlock, "Remove Block", "Remove a block from the layout.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( GenerateBlocks, "Generate Blocks", "Generate Blocks automatically from UVs", EUserInterfaceActionType::Button, FInputChord() );
	}
};


SCustomizableObjectNodeLayoutBlocksEditor::SCustomizableObjectNodeLayoutBlocksEditor() : UICommandList(new FUICommandList())
{
}


void SCustomizableObjectNodeLayoutBlocksEditor::Construct(const FArguments& InArgs)
{
	CurrentLayout = 0;
	
	BindCommands();
}


void SCustomizableObjectNodeLayoutBlocksEditor::SetCurrentLayout( UCustomizableObjectLayout* Layout, UCustomizableObjectLayout* UVOverrideLayout )
{
	CurrentLayout = Layout;

	UCustomizableObjectLayout* LayoutForUVs = UVOverrideLayout ? UVOverrideLayout : Layout;

	// Try to locate the source mesh
	TArray<FVector2f> UVs;
	TArray<FVector2f> UnassignedUVs;

	if (CurrentLayout)
	{
		LayoutForUVs->GetUVChannel(UVs, LayoutForUVs->GetUVChannel());

		UnassignedUVs = TArray<FVector2f>();
		
		if (CurrentLayout->UnassignedUVs.Num())
		{
			UnassignedUVs = LayoutForUVs->UnassignedUVs[0];
		}
	}

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(0.0f,2.0f,0.0f,0.0f )
		.AutoHeight()
		[
			BuildLayoutToolBar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SAssignNew(LayoutGridWidget, SCustomizableObjectLayoutGrid)
			.Mode(ELGM_Edit)
			.GridSize(this, &SCustomizableObjectNodeLayoutBlocksEditor::GetGridSize)
			.Blocks(this, &SCustomizableObjectNodeLayoutBlocksEditor::GetBlocks)
			.UVLayout(UVs)
			.UnassignedUVLayoutVertices(UnassignedUVs)
			.SelectionColor(FColor(75, 106, 230, 155))
			.OnBlockChanged(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnBlockChanged)
			.OnDeleteBlocks(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnRemoveBlock)
			.OnAddBlockAt(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlockAt)
			.OnSetBlockPriority(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockPriority)
			.OnSetReduceBlockSymmetrically(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockReductionSymmetry)
			.OnSetReduceBlockByTwo(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockReductionByTwo)
			.OnSetBlockMask(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockMask)
		]
	];	
}


SCustomizableObjectNodeLayoutBlocksEditor::~SCustomizableObjectNodeLayoutBlocksEditor()
{
}


void SCustomizableObjectNodeLayoutBlocksEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CurrentLayout );
}


TSharedRef<SWidget> SCustomizableObjectNodeLayoutBlocksEditor::BuildLayoutToolBar()
{
	FSlimHorizontalToolBarBuilder LayoutToolbarBuilder(UICommandList, FMultiBoxCustomization::None, nullptr, true);
	LayoutToolbarBuilder.SetLabelVisibility(EVisibility::Visible);

	//Getting toolbar style
	const ISlateStyle* const StyleSet = &FCoreStyle::Get();
	const FName& StyleName = "ToolBar";

	LayoutToolbarBuilder.BeginSection("Blocks");
	{
		LayoutToolbarBuilder.AddToolBarButton(FLayoutEditorCommands::Get().AddBlock);
		LayoutToolbarBuilder.AddToolBarButton(FLayoutEditorCommands::Get().RemoveBlock);
		LayoutToolbarBuilder.AddToolBarButton(FLayoutEditorCommands::Get().GenerateBlocks);
	}
	LayoutToolbarBuilder.EndSection();

	LayoutToolbarBuilder.BeginSection("Info");
	{
		LayoutToolbarBuilder.AddWidget
		(
			SNew(SBox)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			[
				SNew(SImage)
				.Image(UE_MUTABLE_GET_BRUSH(TEXT("Icons.Info")))
				.ToolTip(GenerateInfoToolTip())
			]
		);
	}
	LayoutToolbarBuilder.EndSection();
	
	return
	SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.Padding(4,0)
	[
		SNew(SBorder)
		.Padding(2.0f)
		.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		[
			LayoutToolbarBuilder.MakeWidget()
		]
	];
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlock()
{
	if (CurrentLayout)
	{
		const FScopedTransaction Transaction(LOCTEXT("OnAddBlock", "Add Block"));
		CurrentLayout->Modify();

		FCustomizableObjectLayoutBlock Block;
		CurrentLayout->Blocks.Add(Block);

		if (LayoutGridWidget.IsValid())
		{
			LayoutGridWidget->SetSelectedBlock(Block.Id);
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlockAt(const FIntPoint Min, const FIntPoint Max)
{
	if (CurrentLayout)
	{
		const FScopedTransaction Transaction(LOCTEXT("OnAddBlockAt", "Add Block"));
		CurrentLayout->Modify();
		
		FCustomizableObjectLayoutBlock block(Min,Max);
		CurrentLayout->Blocks.Add(block);
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnRemoveBlock()
{
	if (CurrentLayout)
	{
		if (LayoutGridWidget.IsValid())
		{
			const FScopedTransaction Transaction(LOCTEXT("OnRemoveBlock", "Remove Block"));
			CurrentLayout->Modify();

			const TArray<FGuid>& Selected = LayoutGridWidget->GetSelectedBlocks();
			for (TArray<FCustomizableObjectLayoutBlock>::TIterator It = CurrentLayout->Blocks.CreateIterator(); It; ++It)
			{
				if (Selected.Contains(It->Id))
				{
					It.RemoveCurrent();
				}
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnGenerateBlocks()
{
	if (CurrentLayout)
	{
		const FScopedTransaction Transaction(LOCTEXT("OnGenerateBlocks", "Generate Blocks"));
		CurrentLayout->Modify();
		
		CurrentLayout->GenerateBlocksFromUVs();
	}
}


FIntPoint SCustomizableObjectNodeLayoutBlocksEditor::GetGridSize() const
{
	if ( CurrentLayout )
	{
		return CurrentLayout->GetGridSize();
	}
	return FIntPoint(1);
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnBlockChanged( FGuid BlockId, FIntRect Block )
{
	if (CurrentLayout)
	{
		const FScopedTransaction Transaction(LOCTEXT("OnBlockChanged", "Edit Block"));
		CurrentLayout->Modify();
		
		for (FCustomizableObjectLayoutBlock& B : CurrentLayout->Blocks)
		{
			if (B.Id == BlockId)
			{
				B.Min = Block.Min;
				B.Max = Block.Max;

				break;
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockPriority(int32 InValue)
{
	if (CurrentLayout)
	{
		const FScopedTransaction Transaction(LOCTEXT("OnSetBlockPriority", "Change Block Priority"));
		CurrentLayout->Modify();
		
		if (LayoutGridWidget.IsValid())
		{
			const TArray<FGuid>& SelectedBlocks = LayoutGridWidget->GetSelectedBlocks();

			for (FCustomizableObjectLayoutBlock& Block : CurrentLayout->Blocks)
			{
				if (SelectedBlocks.Contains(Block.Id))
				{
					Block.Priority = InValue;
				}
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockReductionSymmetry(bool bInValue)
{
	if (CurrentLayout)
	{
		if (LayoutGridWidget.IsValid())
		{
			const FScopedTransaction Transaction(LOCTEXT("OnSetBlockReductionSymmetry", "Change Block Symetry"));
			CurrentLayout->Modify();
			
			const TArray<FGuid>& SelectedBlocks = LayoutGridWidget->GetSelectedBlocks();

			for (FCustomizableObjectLayoutBlock& Block : CurrentLayout->Blocks)
			{
				if (SelectedBlocks.Contains(Block.Id))
				{
					Block.bReduceBothAxes = bInValue;
				}
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockReductionByTwo(bool bInValue)
{
	if (CurrentLayout)
	{
		if (LayoutGridWidget.IsValid())
		{
			const FScopedTransaction Transaction(LOCTEXT("OnSetBlockReductionByTwo", "Change Block Reduction By Two"));
			CurrentLayout->Modify();
			
			const TArray<FGuid>& SelectedBlocks = LayoutGridWidget->GetSelectedBlocks();

			for (FCustomizableObjectLayoutBlock& Block : CurrentLayout->Blocks)
			{
				if (SelectedBlocks.Contains(Block.Id))
				{
					Block.bReduceByTwo = bInValue;
				}
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockMask(UTexture2D* InValue)
{
	if (CurrentLayout)
	{
		const FScopedTransaction Transaction(LOCTEXT("OnSetBlockMask", "Change Block Mask"));
		CurrentLayout->Modify();

		if (LayoutGridWidget.IsValid())
		{
			const TArray<FGuid>& SelectedBlocks = LayoutGridWidget->GetSelectedBlocks();

			for (FCustomizableObjectLayoutBlock& Block : CurrentLayout->Blocks)
			{
				if (SelectedBlocks.Contains(Block.Id))
				{
					Block.Mask = InValue;
				}
			}
		}
	}
}


TArray<FCustomizableObjectLayoutBlock> SCustomizableObjectNodeLayoutBlocksEditor::GetBlocks() const
{
	TArray<FCustomizableObjectLayoutBlock> Blocks;

	if (CurrentLayout)
	{
		Blocks = CurrentLayout->Blocks;
	}

	return Blocks;
}


void SCustomizableObjectNodeLayoutBlocksEditor::BindCommands()
{
	// Register our commands. This will only register them if not previously registered
	FLayoutEditorCommands::Register();

	const FLayoutEditorCommands& Commands = FLayoutEditorCommands::Get();

	UICommandList->MapAction(
		Commands.AddBlock,
		FExecuteAction::CreateSP( this, &SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlock ),
		FCanExecuteAction(),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.RemoveBlock,
		FExecuteAction::CreateSP( this, &SCustomizableObjectNodeLayoutBlocksEditor::OnRemoveBlock ),
		FCanExecuteAction(),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.GenerateBlocks,
		FExecuteAction::CreateSP(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnGenerateBlocks),
		FCanExecuteAction(),
		FIsActionChecked());
}


TSharedPtr<IToolTip> SCustomizableObjectNodeLayoutBlocksEditor::GenerateInfoToolTip() const
{
	TSharedPtr<SGridPanel> ToolTipWidget = SNew(SGridPanel);
	int32 SlotCount = 0;

	auto BuildShortcutAndTooltip = [ToolTipWidget, &SlotCount](const FText& Shortcut, const FText& Tooltip)
	{
		// Command Shortcut
		ToolTipWidget->AddSlot(0, SlotCount)
		[
			SNew(STextBlock)
			.Text(Shortcut)
		];

		// Command Explanation
		ToolTipWidget->AddSlot(1, SlotCount)
		.Padding(15.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(Tooltip)
		];

		++SlotCount;
	};

	// Duplicate command
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_DuplicateBlocks", "CTRL + D"), LOCTEXT("Tooltip_DuplicateBlocks", "Duplicate selected block/s"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_CreateNewBlock", "CTRL + N"), LOCTEXT("Tooltip_CreateNewBlock", "Create new block"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_FillGridSize", "CTRL + F"), LOCTEXT("Tooltip_FillGridSize", "Resize selected block/s to grid size"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_DeleteSelectedBlock","DEL"), LOCTEXT("Tooltip_DeleteSelectedBlock","Delete selected block/s"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_SelectMultipleBlocksOneByOne","SHIFT + L Click"), LOCTEXT("Tooltip_SelectMultipleBlocksOneByOne","Select multiple blocks one by one"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_SelectMultipleBlocks","L Click + Drag"), LOCTEXT("Tooltip_SelectMultipleBlocks","Select blocks that intersect with the yellow rectangle"));

	return SNew(SToolTip)
	[
		ToolTipWidget.ToSharedRef()
	];
}


#undef LOCTEXT_NAMESPACE
