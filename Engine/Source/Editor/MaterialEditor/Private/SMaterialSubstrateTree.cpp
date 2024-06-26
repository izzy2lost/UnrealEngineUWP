// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMaterialSubstrateTree.h"
#include "SMaterialLayersFunctionsTree.h"

#include "SMaterialLayersFunctionsTree.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorMaterialLayersParameterValue.h"
#include "MaterialEditor/DEditorRuntimeVirtualTextureParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Materials/Material.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Styling/AppStyle.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "IPropertyRowGenerator.h"
#include "Widgets/Views/STreeView.h"
#include "IDetailTreeNode.h"
#include "AssetThumbnail.h"
#include "MaterialEditorInstanceDetailCustomization.h"
#include "MaterialPropertyHelpers.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorSupportDelegates.h"
#include "Widgets/Images/SImage.h"
#include "MaterialEditor/MaterialEditorPreviewParameters.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "EditorFontGlyphs.h"
#include "SResetToDefaultPropertyEditor.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "MaterialEditorStyle.h"

#define LOCTEXT_NAMESPACE "MaterialSubstrateTree"


FString CreateNodeKeyForBackgroundParameter(const FString& InNodeKeyBase)
{
	return FString(InNodeKeyBase + TEXT("_BackgroundParameter"));
}
bool IsNodeKeyForBackgroundParameter(const FString InNodeKey)
{
	return (InNodeKey.Find(TEXT("_BackgroundParameter")) >= 0);
}

/// ===========================================================================================================
/// SMaterialSubstrateItem Methods
/// ===========================================================================================================
FString SMaterialSubstrateTreeItem::GetCurvePath(UDEditorScalarParameterValue* Parameter) const
{
	FString Path = Parameter->AtlasData.Curve->GetPathName();
	return Path;
}

const FSlateBrush* SMaterialSubstrateTreeItem::GetBorderImage() const
{
	return FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
}

FSlateColor SMaterialSubstrateTreeItem::GetOuterBackgroundColor(TSharedPtr<FSortedParamData> InParamData) const
{
	if (InParamData->StackDataType == EStackDataType::Stack)
	{
		if (bIsBeingDragged)
		{
			return FAppStyle::Get().GetSlateColor("Colors.Recessed");
		}
		else if (bIsHoveredDragTarget)
		{
			return FAppStyle::Get().GetSlateColor("Colors.Highlight");
		}
		else
		{
			return FAppStyle::Get().GetSlateColor("Colors.Header");
		}
	}
	else if (IsHovered() || InParamData->StackDataType == EStackDataType::Group)
	{
		return FAppStyle::Get().GetSlateColor("Colors.Header");
	}

	return FAppStyle::Get().GetSlateColor("Colors.Panel");
}

void SMaterialSubstrateTreeItem::RefreshOnRowChange(const FAssetData& AssetData, SMaterialSubstrateTree* InTree)
{
	if (SMaterialLayersFunctionsInstanceWrapper* Wrapper = InTree->GetWrapper())
	{
		if (Wrapper->OnLayerPropertyChanged.IsBound())
		{
			Wrapper->OnLayerPropertyChanged.Execute();
		}
		else
		{
			InTree->CreateGroupsWidget();
		}
	}
}

bool SMaterialSubstrateTreeItem::GetFilterState(SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData) const
{
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter)
	{
		return InTree->FunctionInstance->EditorOnly.RestrictToLayerRelatives[InStackData->ParameterInfo.Index];
	}
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
	{
		return InTree->FunctionInstance->EditorOnly.RestrictToBlendRelatives[InStackData->ParameterInfo.Index];
	}
	return false;
}

void SMaterialSubstrateTreeItem::FilterClicked(const ECheckBoxState NewCheckedState, SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData)
{
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter)
	{
		InTree->FunctionInstance->EditorOnly.RestrictToLayerRelatives[InStackData->ParameterInfo.Index] = !InTree->FunctionInstance->EditorOnly.RestrictToLayerRelatives[InStackData->ParameterInfo.Index];
	}
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
	{
		InTree->FunctionInstance->EditorOnly.RestrictToBlendRelatives[InStackData->ParameterInfo.Index] = !InTree->FunctionInstance->EditorOnly.RestrictToBlendRelatives[InStackData->ParameterInfo.Index];
	}
}

ECheckBoxState SMaterialSubstrateTreeItem::GetFilterChecked(SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData) const
{
	return GetFilterState(InTree, InStackData) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SMaterialSubstrateTreeItem::ToggleLayerVisibility()
{
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	return Tree->ToggleLayerVisibility(LayerFuncIndex);
}

bool SMaterialSubstrateTreeItem::IsLayerVisible() const
{
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	return Tree->IsLayerVisible(LayerFuncIndex);
}

FReply SMaterialSubstrateTreeItem::UnlinkLayer()
{
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	return Tree->UnlinkLayer(LayerFuncIndex);
}

EVisibility SMaterialSubstrateTreeItem::GetUnlinkLayerVisibility() const
{
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	return Tree->GetUnlinkLayerVisibility(LayerFuncIndex);
}

FText SMaterialSubstrateTreeItem::GetLayerName() const
{
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	return Tree->FunctionInstance->GetLayerName(LayerFuncIndex);
}

void SMaterialSubstrateTreeItem::OnNameChanged(const FText& InText, ETextCommit::Type CommitInfo)
{
	const FScopedTransaction Transaction(LOCTEXT("RenamedSection", "Renamed layer and blend section"));
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	Tree->FunctionInstanceHandle->NotifyPreChange();
	Tree->FunctionInstance->EditorOnly.LayerNames[LayerFuncIndex] = InText;
	Tree->FunctionInstance->UnlinkLayerFromParent(LayerFuncIndex);
	Tree->MaterialEditorInstance->CopyToSourceInstance(true);
	Tree->FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

TOptional<EItemDropZone> SMaterialSubstrateTreeItem::CanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSortedParamDataPtr Item)
{
	TSharedPtr<FLayerDragDropOp> LayerDragDropOperation = DragDropEvent.GetOperationAs<FLayerDragDropOp>();
	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (LayerDragDropOperation.IsValid() || AssetDragDropOperation.IsValid())
	{
		return DropZone;
	}
	return TOptional<EItemDropZone>();
}

FReply SMaterialSubstrateTreeItem::OnLayerDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone,	FSortedParamDataPtr TargetItem)
{
	if (!bIsHoveredDragTarget)
	{
		return FReply::Unhandled();
	}
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MoveLayer", "Move Layer"));
	Tree->FunctionInstanceHandle->NotifyPreChange();
	bIsHoveredDragTarget = false;
	TSharedPtr<FLayerDragDropOp> ArrayDropOp = DragDropEvent.GetOperationAs< FLayerDragDropOp >();
	TSharedPtr<SMaterialSubstrateTreeItem> LayerPtr = nullptr;
	if (ArrayDropOp.IsValid() && ArrayDropOp->OwningStack.IsValid())
	{
		LayerPtr = StaticCastWeakPtr<SMaterialSubstrateTreeItem>(ArrayDropOp->OwningStack).Pin();
	}
	else
	{
		// see if it is an accepted asset drop
		TSharedPtr<FAssetDragDropOp> AssetDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();

		if (AssetDropOp.IsValid())
		{
			bool bIsBackgroundItem = IsNodeKeyForBackgroundParameter(StackParameterData->NodeKey);

			// Drop above or below could CREATE a new layer node:
			int32 TargetNodeId = StackParameterData->ParameterInfo.Index;
			int32 ParentNodeId = (bIsBackgroundItem ? TargetNodeId : Tree->FunctionInstance->GetNodeParent(TargetNodeId));
			auto ChildrenNodeId = Tree->FunctionInstance->GetNodeChildren(ParentNodeId);
			int32 SiblingIdx = -1;
			bool FoundTarget = ChildrenNodeId.Find(TargetNodeId, SiblingIdx);

			switch (DropZone)
			{
			// NOTE: the Sibling Idx should be swaped when we will disoplay the items from bottom up
			case EItemDropZone::AboveItem:
				TargetNodeId = Tree->FunctionInstance->AppendLayerNode(ParentNodeId, SiblingIdx);
				break;

			case EItemDropZone::BelowItem:
				TargetNodeId = Tree->FunctionInstance->AppendLayerNode(ParentNodeId, SiblingIdx + 1);
				break;
			default:
				break;
			}

			// Then drop
			bool DidModifyTree = false;
			for (const FAssetData& AssetData : AssetDropOp->GetAssets())
			{
				EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter;
			
				if (AssetData.AssetClassPath.GetAssetName() == TEXT("MaterialFunctionMaterialLayer"))
				{
					InAssociation = EMaterialParameterAssociation::LayerParameter;

					Tree->RefreshOnAssetChange(AssetData, TargetNodeId, InAssociation);
					DidModifyTree = true;
				}
				else if (AssetData.AssetClassPath.GetAssetName() == TEXT("MaterialFunctionMaterialLayerBlend"))
				{
					InAssociation = EMaterialParameterAssociation::BlendParameter;
					Tree->RefreshOnAssetChange(AssetData, TargetNodeId, InAssociation);
					DidModifyTree = true;
				}
			}

			if (DidModifyTree)
			{
				return FReply::Handled();
			}
			
		}
		return FReply::Unhandled();
	}
	
	if (!LayerPtr.IsValid())
	{
		return FReply::Unhandled();
	}
	
	LayerPtr->bIsBeingDragged = false;
	TSharedPtr<FSortedParamData> SwappingPropertyData = LayerPtr->StackParameterData;
	TSharedPtr<FSortedParamData> SwappablePropertyData = StackParameterData;
	if (SwappingPropertyData.IsValid() && SwappablePropertyData.IsValid())
	{
		if (SwappingPropertyData != SwappablePropertyData)
		{
			int32 OriginalIndex = SwappingPropertyData->ParameterInfo.Index;
			if (SwappingPropertyData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
			{
				OriginalIndex++;
			}

			int32 NewIndex = SwappablePropertyData->ParameterInfo.Index;
			if (SwappablePropertyData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
			{
				NewIndex++;
			}

			if (OriginalIndex != NewIndex)
			{
				Tree->MaterialEditorInstance->SourceInstance->SwapLayerParameterIndices(OriginalIndex, NewIndex);

				// Need to save the moving and target expansion states before swapping
				const bool bOriginalSwappableExpansion = IsItemExpanded();
				const bool bOriginalSwappingExpansion = LayerPtr->IsItemExpanded();

				TArray<void*> StructPtrs;
				Tree->FunctionInstanceHandle->AccessRawData(StructPtrs);
				FMaterialLayersFunctions* MaterialLayersFunctions = static_cast<FMaterialLayersFunctions*>(StructPtrs[0]);
				MaterialLayersFunctions->MoveBlendedLayer(OriginalIndex, NewIndex);

				Tree->OnExpansionChanged(SwappablePropertyData, bOriginalSwappingExpansion);
				Tree->OnExpansionChanged(SwappingPropertyData, bOriginalSwappableExpansion);
				Tree->FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				Tree->CreateGroupsWidget();
				Tree->RequestTreeRefresh();
				Tree->SetParentsExpansionState();
			}
		}
	}

	return FReply::Handled();
}


bool SMaterialSubstrateTree::IsOverriddenExpression(class UDEditorParameterValue* Parameter, int32 InIndex)
{
	return FMaterialPropertyHelpers::IsOverriddenExpression(Parameter) && FunctionInstance->EditorOnly.LayerStates[InIndex];
}

bool SMaterialSubstrateTree::IsOverriddenExpression(TObjectPtr<UDEditorParameterValue> Parameter, int32 InIndex)
{
	return IsOverriddenExpression(Parameter.Get(), InIndex);
}

FGetShowHiddenParameters SMaterialSubstrateTree::GetShowHiddenDelegate() const
{
	return ShowHiddenDelegate;
}

void  SMaterialSubstrateTreeItem::OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter)
{
	FMaterialPropertyHelpers::OnOverrideParameter(NewValue, Parameter, MaterialEditorInstance);
}

void  SMaterialSubstrateTreeItem::OnOverrideParameter(bool NewValue, TObjectPtr<UDEditorParameterValue> Parameter)
{
	OnOverrideParameter(NewValue, Parameter.Get());
}

void SMaterialSubstrateTreeItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	StackParameterData = InArgs._StackParameterData;
	MaterialEditorInstance = InArgs._MaterialEditorInstance;
	Tree = InArgs._InTree;

	TSharedRef<SWidget> LeftSideWidget = SNullWidget::NullWidget;
	TSharedRef<SWidget> RightSideWidget = SNullWidget::NullWidget;
	TSharedRef<SWidget> ResetWidget = SNullWidget::NullWidget;
	FText NameOverride;
	TSharedRef<SVerticalBox> WrapperWidget = SNew(SVerticalBox);
	EHorizontalAlignment ValueAlignment = HAlign_Left;

	bool bIsBackgroundItem = IsNodeKeyForBackgroundParameter(StackParameterData->NodeKey);
	bool bCanAppendSubLayer = Tree->FunctionInstance->CanAppendLayerNode(StackParameterData->ParameterInfo.Index) && !bIsBackgroundItem;
	bool bCanRemoveLayer = Tree->FunctionInstance->CanRemoveLayerNode(StackParameterData->ParameterInfo.Index) && !bIsBackgroundItem;
	bool bCanReorderLayer = !(bIsBackgroundItem || StackParameterData->ParameterInfo.Index == 0);

// STACK --------------------------------------------------
	if (StackParameterData->StackDataType == EStackDataType::Stack)
	{
		// WrapperWidget->AddSlot()
		// 	.Padding(5.0f)
		// 	.AutoHeight()
		// 	[
		// 		SNullWidget::NullWidget
		// 	];
#if WITH_EDITOR
		int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
		int32 BlendFuncIndex = Tree->FunctionInstance->GetBlendFuncIndex(StackParameterData->ParameterInfo.Index);
		NameOverride = Tree->FunctionInstance->GetLayerName(LayerFuncIndex);
#endif
		TSharedRef<SHorizontalBox> HeaderRowWidget = SNew(SHorizontalBox);

		if (StackParameterData->ParameterInfo.Index != 0)
		{
			TAttribute<bool>::FGetter IsEnabledGetter = TAttribute<bool>::FGetter::CreateSP(this, &SMaterialSubstrateTreeItem::IsLayerVisible);
			TAttribute<bool> IsEnabledAttribute = TAttribute<bool>::Create(IsEnabledGetter);

			FOnClicked VisibilityClickedDelegate = FOnClicked::CreateSP(this, &SMaterialSubstrateTreeItem::ToggleLayerVisibility);

			HeaderRowWidget->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeVisibilityButton(VisibilityClickedDelegate, FText(), IsEnabledAttribute)
				];
		}
		const float ThumbnailSize = 64.0f;
		TArray<TSharedPtr<FSortedParamData>> AssetChildren = StackParameterData->Children;
		// Extract the asset elements to represent them as thumbnail boxes
		for (TSharedPtr<FSortedParamData> AssetChild : AssetChildren)
		{
			if (AssetChild->StackDataType == EStackDataType::Asset)
			{
				TSharedPtr<SBox> ThumbnailBox;
				UObject* AssetObject = nullptr;
				AssetChild->ParameterHandle->GetValue(AssetObject);
				int32 PreviewIndex = INDEX_NONE;
				int32 ThumbnailIndex = INDEX_NONE;
				EMaterialParameterAssociation PreviewAssociation = EMaterialParameterAssociation::GlobalParameter;
				if (AssetObject)
				{
					if (Cast<UMaterialFunctionInterface>(AssetObject)->GetMaterialFunctionUsage() == EMaterialFunctionUsage::MaterialLayer)
					{
						PreviewIndex = LayerFuncIndex;
						PreviewAssociation = EMaterialParameterAssociation::LayerParameter;
						Tree->UpdateThumbnailMaterial(PreviewAssociation, PreviewIndex);
						ThumbnailIndex = PreviewIndex;
					}
					if (Cast<UMaterialFunctionInterface>(AssetObject)->GetMaterialFunctionUsage() == EMaterialFunctionUsage::MaterialLayerBlend)
					{
						PreviewIndex = BlendFuncIndex;
						PreviewAssociation = EMaterialParameterAssociation::BlendParameter;
						Tree->UpdateThumbnailMaterial(PreviewAssociation, PreviewIndex);
						ThumbnailIndex = PreviewIndex;
					}
				}
				HeaderRowWidget->AddSlot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(4.0f)
					.MaxWidth(ThumbnailSize)
					[
						SAssignNew(ThumbnailBox, SBox)
							[
								Tree->CreateThumbnailWidget(PreviewAssociation, ThumbnailIndex, ThumbnailSize)
							]
					];
				ThumbnailBox->SetMaxDesiredHeight(ThumbnailSize);
				ThumbnailBox->SetMinDesiredHeight(ThumbnailSize);
				ThumbnailBox->SetMinDesiredWidth(ThumbnailSize);
				ThumbnailBox->SetMaxDesiredWidth(ThumbnailSize);
			}
		}

		

		if (StackParameterData->ParameterInfo.Index != 0)
		{
			HeaderRowWidget->AddSlot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(5.0f)
				[
					SNew(SEditableTextBox)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SMaterialSubstrateTreeItem::GetLayerName)))
					.OnTextCommitted(FOnTextCommitted::CreateSP(this, &SMaterialSubstrateTreeItem::OnNameChanged))
				];
		}
		else
		{
			HeaderRowWidget->AddSlot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(5.0f)
				[
					SNew(STextBlock)
					.Text(NameOverride)
				];
		}

		// Unlink UI
		HeaderRowWidget->AddSlot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNullWidget::NullWidget
			];

		HeaderRowWidget->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Unlink", "Unlink"))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SMaterialSubstrateTreeItem::UnlinkLayer)
				.ToolTipText(LOCTEXT("UnlinkLayer", "Whether or not to unlink this layer/blend combination from the parent."))
				.Visibility(this, &SMaterialSubstrateTreeItem::GetUnlinkLayerVisibility)
			];

		// Can only add sub layer up to a certain level
		if (bCanAppendSubLayer)
		{
			HeaderRowWidget->AddSlot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateSP(InArgs._InTree, &SMaterialSubstrateTree::AddNodeLayer, StackParameterData->ParameterInfo.Index))
				];
		}

		// Can only remove layers that aren't the base layer.
		if (bCanRemoveLayer)
		{
			HeaderRowWidget->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(InArgs._InTree, &SMaterialSubstrateTree::RemoveNodeLayer, StackParameterData->ParameterInfo.Index))
				];
		}
		LeftSideWidget = HeaderRowWidget;
	}
// END STACK


// FINAL WRAPPER
	if (StackParameterData->StackDataType == EStackDataType::Stack)
	{
		TSharedPtr<SHorizontalBox> FinalStack;
		WrapperWidget->AddSlot()
			.Padding(15.0f)/*.AutoHeight()*/
			[
				SAssignNew(FinalStack, SHorizontalBox)
			];

		if (bCanReorderLayer)
		{
			FinalStack->AddSlot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.5f, 0)
				.AutoWidth()
				[
					FMaterialPropertyHelpers::MakeStackReorderHandle(SharedThis(this))
				];
		}

		if (bCanAppendSubLayer)
		{
			FinalStack->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2.0f))
				[
					SNew(SExpanderArrow, SharedThis(this))
				];
		}

		FinalStack->AddSlot()
			.Padding(FMargin(2.0f))
			.VAlign(VAlign_Center)
			[
				LeftSideWidget
			];
	}

	this->ChildSlot
		[
			WrapperWidget
		];

	FOnTableRowDragEnter LayerDragDelegate = FOnTableRowDragEnter::CreateSP(this, &SMaterialSubstrateTreeItem::OnLayerDragEnter);
	FOnTableRowDragLeave LayerDragLeaveDelegate = FOnTableRowDragLeave::CreateSP(this, &SMaterialSubstrateTreeItem::OnLayerDragLeave);

	STableRow< TSharedPtr<FSortedParamData> >::ConstructInternal(
		STableRow< TSharedPtr<FSortedParamData> >::FArguments()
		.Padding(20.0f)
		.OnPaintDropIndicator(this, &SMaterialSubstrateTreeItem::OnLayerItemPaintDropIndicator)
		.Style(FSubstrateMaterialEditorStyle::Get(), "LayerView.Row")
		.ShowSelection(true)
		.OnCanAcceptDrop(this, &SMaterialSubstrateTreeItem::CanAcceptDrop)
		.OnAcceptDrop(this, &SMaterialSubstrateTreeItem::OnLayerDrop)
		.OnDragEnter(LayerDragDelegate)
		.OnDragLeave(LayerDragLeaveDelegate),
		InOwnerTableView
	);

}
int32 SMaterialSubstrateTreeItem::OnLayerItemPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* DropIndicatorBrush = GetDropIndicatorBrush(InItemDropZone);
	static float OffsetX = 10.0f;
	FVector2D Offset(OffsetX * GetIndentLevel(), 0.0f);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize() - Offset), FSlateLayoutTransform(Offset)),
		DropIndicatorBrush,
		ESlateDrawEffect::None,
		DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	return LayerId;
}
FString SMaterialSubstrateTreeItem::GetInstancePath(SMaterialSubstrateTree* InTree) const
{
	int32 LayerFuncIndex = Tree->FunctionInstance->GetLayerFuncIndex(StackParameterData->ParameterInfo.Index);
	int32 BlendFuncIndex = Tree->FunctionInstance->GetBlendFuncIndex(StackParameterData->ParameterInfo.Index);

	FString InstancePath;
	if (StackParameterData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter && InTree->FunctionInstance->Blends.IsValidIndex(BlendFuncIndex))
	{
		InstancePath = InTree->FunctionInstance->Blends[BlendFuncIndex]->GetPathName();
	}
	else if (StackParameterData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter && InTree->FunctionInstance->Layers.IsValidIndex(LayerFuncIndex))
	{
		InstancePath = InTree->FunctionInstance->Layers[LayerFuncIndex]->GetPathName();
	}
	return InstancePath;
}

void SMaterialSubstrateTree::Construct(const FArguments& InArgs)
{
	ColumnSizeData.SetValueColumnWidth(0.5f);

	MaterialEditorInstance = InArgs._InMaterialEditorInstance;
	Wrapper = InArgs._InWrapper;
	ShowHiddenDelegate = InArgs._InShowHiddenDelegate;
	CreateGroupsWidget();

#if WITH_EDITOR
	//Fixup for adding new bool arrays to the class
	if (FunctionInstance)
	{
		if (FunctionInstance->Layers.Num() != FunctionInstance->EditorOnly.RestrictToLayerRelatives.Num())
		{
			int32 OriginalSize = FunctionInstance->EditorOnly.RestrictToLayerRelatives.Num();
			for (int32 LayerIt = 0; LayerIt < FunctionInstance->Layers.Num() - OriginalSize; LayerIt++)
			{
				FunctionInstance->EditorOnly.RestrictToLayerRelatives.Add(false);
			}
		}
		if (FunctionInstance->Blends.Num() != FunctionInstance->EditorOnly.RestrictToBlendRelatives.Num())
		{
			int32 OriginalSize = FunctionInstance->EditorOnly.RestrictToBlendRelatives.Num();
			for (int32 BlendIt = 0; BlendIt < FunctionInstance->Blends.Num() - OriginalSize; BlendIt++)
			{
				FunctionInstance->EditorOnly.RestrictToBlendRelatives.Add(false);
			}
		}
	}
#endif

	STreeView<TSharedPtr<FSortedParamData>>::Construct(
		STreeView::FArguments()
		.TreeItemsSource(&LayerProperties)
		.SelectionMode(ESelectionMode::Single)
		.OnSelectionChanged(this, &SMaterialSubstrateTree::OnSelectionChangedMaterialSubstrateView)
		.OnGenerateRow(this, &SMaterialSubstrateTree::OnGenerateRowMaterialLayersFunctionsTreeView)
		.OnGetChildren(this, &SMaterialSubstrateTree::OnGetChildrenMaterialLayersFunctionsTreeView)
		.OnExpansionChanged(this, &SMaterialSubstrateTree::OnExpansionChanged)
	);

	SetParentsExpansionState();
}
void SMaterialSubstrateTree::OnSelectionChangedMaterialSubstrateView(TSharedPtr<FSortedParamData> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	MaterialEditorInstance->DetailsView.Pin()->ForceRefresh();
}
TSharedRef< ITableRow > SMaterialSubstrateTree::OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	TSharedRef< SMaterialSubstrateTreeItem > ReturnRow = SNew(SMaterialSubstrateTreeItem, OwnerTable)
		.StackParameterData(Item)
		.MaterialEditorInstance(MaterialEditorInstance)
		.InTree(this);
	return ReturnRow;
}

void SMaterialSubstrateTree::OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> InParent, TArray< TSharedPtr<FSortedParamData> >& OutChildren)
{
	OutChildren = InParent->Children;
}


void SMaterialSubstrateTree::OnExpansionChanged(TSharedPtr<FSortedParamData> Item, bool bIsExpanded)
{
	bool* ExpansionValue = MaterialEditorInstance->SourceInstance->LayerParameterExpansion.Find(Item->NodeKey);
	if (ExpansionValue == nullptr)
	{
		MaterialEditorInstance->SourceInstance->LayerParameterExpansion.Add(Item->NodeKey, bIsExpanded);
	}
	else if (*ExpansionValue != bIsExpanded)
	{
		MaterialEditorInstance->SourceInstance->LayerParameterExpansion.Emplace(Item->NodeKey, bIsExpanded);
	}
	// Expand any children that are also expanded
	for (auto Child : Item->Children)
	{
		bool* ChildExpansionValue = MaterialEditorInstance->SourceInstance->LayerParameterExpansion.Find(Child->NodeKey);
		if (ChildExpansionValue != nullptr && *ChildExpansionValue == true)
		{
			SetItemExpansion(Child, true);
		}
	}
}

void SMaterialSubstrateTree::SetParentsExpansionState()
{
	for (const auto& Pair : LayerProperties)
	{
		if (Pair->Children.Num())
		{
			bool* bIsExpanded = MaterialEditorInstance->SourceInstance->LayerParameterExpansion.Find(Pair->NodeKey);
			if (bIsExpanded)
			{
				SetItemExpansion(Pair, *bIsExpanded);
			}
		}
	}
}

void SMaterialSubstrateTree::RefreshOnAssetChange(const struct FAssetData& InAssetData, int32 InNodeId, EMaterialParameterAssociation MaterialType)
{
	auto NodePayload = FunctionInstance->GetNodePayload(InNodeId);

	int32 Index = (MaterialType == EMaterialParameterAssociation::BlendParameter ? NodePayload.Blend : NodePayload.Layer);
	// Early exit no op if the index for the asset modified is not valid
	if (Index < 0)
		return;

	FMaterialPropertyHelpers::OnMaterialLayerAssetChanged(InAssetData, Index, MaterialType, FunctionInstanceHandle, FunctionInstance);
	//set their overrides back to 0
	MaterialEditorInstance->CleanParameterStack(Index, MaterialType);
	CreateGroupsWidget();
	MaterialEditorInstance->ResetOverrides(Index, MaterialType);
	RequestTreeRefresh();
}

void SMaterialSubstrateTree::ResetAssetToDefault(TSharedPtr<FSortedParamData> InData)
{
	FMaterialPropertyHelpers::ResetLayerAssetToDefault(InData->Parameter, InData->ParameterInfo.Association, InData->ParameterInfo.Index, MaterialEditorInstance);
	UpdateThumbnailMaterial(InData->ParameterInfo.Association, InData->ParameterInfo.Index, false);
	CreateGroupsWidget();
	RequestTreeRefresh();
}

void SMaterialSubstrateTree::AddNodeLayer(int32 InParent)
{
	/// Only if can really add a sub layer!
	if (!FunctionInstance->CanAppendLayerNode(InParent))
		return;

	const FScopedTransaction Transaction(LOCTEXT("AddLayer", "Add a new Layer in the tree"));
	FunctionInstanceHandle->NotifyPreChange();

	// Create a new node
	FunctionInstance->AppendLayerNode(InParent);
	
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	CreateGroupsWidget();
	RequestTreeRefresh();
}

void SMaterialSubstrateTree::RemoveNodeLayer(int32 InNodeId)
{
	/// Only if can really remove a sub layer!
	if (!FunctionInstance->CanRemoveLayerNode(InNodeId))
		return;

	const FScopedTransaction Transaction(LOCTEXT("RemoveLayerAndBlend", "Remove a Layer and the attached Blend"));
	FunctionInstanceHandle->NotifyPreChange();
	
	// Remove the node
	auto NodePayload = FunctionInstance->GetNodePayload(InNodeId);

	FunctionInstance->RemoveLayerNodeAt(InNodeId);
	
	MaterialEditorInstance->SourceInstance->RemoveLayerParameterIndex(NodePayload.Layer);
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ArrayRemove);
	CreateGroupsWidget();
	RequestTreeRefresh();
}

FReply SMaterialSubstrateTree::UnlinkLayer(int32 Index)
{
	const FScopedTransaction Transaction(LOCTEXT("UnlinkLayerFromParent", "Unlink a layer from the parent"));
	FunctionInstanceHandle->NotifyPreChange();
	FunctionInstance->UnlinkLayerFromParent(Index);
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	CreateGroupsWidget();
	return FReply::Handled();
}

FReply SMaterialSubstrateTree::RelinkLayersToParent()
{
	const FScopedTransaction Transaction(LOCTEXT("RelinkLayersToParent", "Relink layers to parent"));
	FunctionInstanceHandle->NotifyPreChange();
	FunctionInstance->RelinkLayersToParent();
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	MaterialEditorInstance->RegenerateArrays();
	CreateGroupsWidget();
	return FReply::Handled();
}

EVisibility SMaterialSubstrateTree::GetUnlinkLayerVisibility(int32 Index) const
{
	if (FunctionInstance->IsLayerLinkedToParent(Index))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

EVisibility SMaterialSubstrateTree::GetRelinkLayersToParentVisibility() const
{
	if (FunctionInstance->HasAnyUnlinkedLayers())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FReply SMaterialSubstrateTree::ToggleLayerVisibility(int32 Index)
{
	if (!FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Alt))
	{
		bLayerIsolated = false;
		const FScopedTransaction Transaction(LOCTEXT("ToggleLayerAndBlendVisibility", "Toggles visibility for a blended layer"));
		FunctionInstanceHandle->NotifyPreChange();
		FunctionInstance->ToggleBlendedLayerVisibility(Index);
		FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		CreateGroupsWidget();
		return FReply::Handled();
	}
	else
	{
		const FScopedTransaction Transaction(LOCTEXT("ToggleLayerAndBlendVisibility", "Toggles visibility for a blended layer"));
		FunctionInstanceHandle->NotifyPreChange();
		if (FunctionInstance->GetLayerVisibility(Index) == false)
		{
			// Reset if clicking on a disabled layer
			FunctionInstance->SetBlendedLayerVisibility(Index, true);
			bLayerIsolated = false;
		}
		for (int32 LayerIt = 1; LayerIt < FunctionInstance->EditorOnly.LayerStates.Num(); LayerIt++)
		{
			if (LayerIt != Index)
			{
				FunctionInstance->SetBlendedLayerVisibility(LayerIt, bLayerIsolated);
			}
		}

		bLayerIsolated = !bLayerIsolated;
		FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		CreateGroupsWidget();
		return FReply::Handled();
	}

}

TSharedPtr<class FAssetThumbnailPool> SMaterialSubstrateTree::GetTreeThumbnailPool()
{
	return UThumbnailManager::Get().GetSharedThumbnailPool();
}

TSharedPtr<IDetailTreeNode> SMaterialSubstrateTree::FindParameterGroupsNode(TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator)
{
	const TArray<TSharedRef<IDetailTreeNode>> RootNodes = PropertyRowGenerator->GetRootTreeNodes();
	if (RootNodes.Num() > 0)
	{
		TSharedPtr<IDetailTreeNode> Category = RootNodes[0];
		TArray<TSharedRef<IDetailTreeNode>> Children;
		Category->GetChildren(Children);

		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ChildIdx++)
		{
			TSharedPtr<IPropertyHandle> PropertyHandle = Children[ChildIdx]->CreatePropertyHandle();
			if (PropertyHandle.IsValid() && PropertyHandle->GetProperty() && PropertyHandle->GetProperty()->GetName() == "ParameterGroups")
			{
				return Children[ChildIdx];
			}
		}
	}
	return nullptr;
}

struct FRecursiveCreateWidgetsContext
{
	UDEditorParameterValue* Parameter;
	TSharedPtr<IPropertyHandle>	LayerHandle;
	TSharedPtr<IPropertyHandle>	BlendHandle;
};

void SMaterialSubstrateTree::RecursiveCreateWidgets(FRecursiveCreateWidgetsContext* InContext, FNodeId InNodeId, TArray<TSharedPtr<FSortedParamData>>& InParentContainer, bool GenerateChildren, bool bIsBackgroundItem)
{
	auto Payload = FunctionInstance->Tree.Payloads[InNodeId];


	TSharedRef<FSortedParamData> StackProperty = MakeShared<FSortedParamData>();
	StackProperty->StackDataType = EStackDataType::Stack;
	StackProperty->Parameter = InContext->Parameter;
	StackProperty->ParameterInfo.Index = InNodeId;
	StackProperty->NodeKey = FString::FromInt(StackProperty->ParameterInfo.Index);

	if (bIsBackgroundItem)
	{
		StackProperty->NodeKey = CreateNodeKeyForBackgroundParameter(StackProperty->NodeKey);
	}


	if (GenerateChildren)
	{
		// Create the default background sub item representing the layer and blend func stored in the layer item
		RecursiveCreateWidgets(InContext, InNodeId, StackProperty->Children, false, true);

		// Sub layers
		auto RootChildren = FunctionInstance->GetNodeChildren(InNodeId);
		for (int i = 0; i < RootChildren.Num(); ++i)
		{
			RecursiveCreateWidgets(InContext, RootChildren[i], StackProperty->Children, false, false);
		}
	}

	if (Payload.Layer != -1)
	{
		TSharedRef<FSortedParamData> ChildProperty = MakeShared<FSortedParamData>();
		ChildProperty->StackDataType = EStackDataType::Asset;
		ChildProperty->Parameter = InContext->Parameter;
		ChildProperty->ParameterHandle = InContext->LayerHandle->AsArray()->GetElement(Payload.Layer);
		ChildProperty->ParameterNode = Generator->FindTreeNode(ChildProperty->ParameterHandle);
		ChildProperty->ParameterInfo.Index = Payload.Layer;
		ChildProperty->ParameterInfo.Association = EMaterialParameterAssociation::LayerParameter;
		ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association);

		{
			UObject* AssetObject = nullptr;
			ChildProperty->ParameterHandle->GetValue(AssetObject);
			if (AssetObject)
			{
				if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] == nullptr)
				{
					MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] = (NewObject<UMaterialInstanceConstant>(MaterialEditorInstance, NAME_None));
				}
				UMaterialInterface* EditedMaterial = Cast<UMaterialFunctionInterface>(AssetObject)->GetPreviewMaterial();
				if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] && MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->Parent != EditedMaterial)
				{
					MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->SetParentEditorOnly(EditedMaterial);
				}
			}
		}

		StackProperty->Children.Add(ChildProperty);
		ShowSubParameters(ChildProperty);

	}

	if (!bIsBackgroundItem && Payload.Blend != -1)
	{
		TSharedRef<FSortedParamData> ChildProperty = MakeShared<FSortedParamData>();
		ChildProperty->StackDataType = EStackDataType::Asset;
		ChildProperty->Parameter = InContext->Parameter;
		ChildProperty->ParameterHandle = InContext->BlendHandle->AsArray()->GetElement(Payload.Blend);
		ChildProperty->ParameterNode = Generator->FindTreeNode(ChildProperty->ParameterHandle);
		ChildProperty->ParameterInfo.Index = Payload.Blend;
		ChildProperty->ParameterInfo.Association = EMaterialParameterAssociation::BlendParameter;
		ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association);
		{
			UObject* AssetObject = nullptr;
			ChildProperty->ParameterHandle->GetValue(AssetObject);
			if (AssetObject)
			{
				if (MaterialEditorInstance->StoredBlendPreviews[Payload.Blend] == nullptr)
				{
					MaterialEditorInstance->StoredBlendPreviews[Payload.Blend] = (NewObject<UMaterialInstanceConstant>(MaterialEditorInstance, NAME_None));
				}
				UMaterialInterface* EditedMaterial = Cast<UMaterialFunctionInterface>(AssetObject)->GetPreviewMaterial();
				if (MaterialEditorInstance->StoredBlendPreviews[Payload.Blend] && MaterialEditorInstance->StoredBlendPreviews[Payload.Blend]->Parent != EditedMaterial)
				{
					MaterialEditorInstance->StoredBlendPreviews[Payload.Blend]->SetParentEditorOnly(EditedMaterial);
				}
			}
		}

		StackProperty->Children.Add(ChildProperty);
		ShowSubParameters(ChildProperty);
	}

	InParentContainer.Add(StackProperty);
}

void SMaterialSubstrateTree::CreateGroupsWidget()
{
	check(MaterialEditorInstance);
	MaterialEditorInstance->RegenerateArrays();
	NonLayerProperties.Empty();
	LayerProperties.Empty();
	FunctionParameter = nullptr;
	TSharedPtr<IPropertyHandle> FunctionParameterHandle;

	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	if (!Generator.IsValid())
	{
		FPropertyRowGeneratorArgs Args;
		Generator = Module.CreatePropertyRowGenerator(Args);
		// the sizes of the parameter lists are only based on the parent material and not changed out from under the details panel 
		// When a parameter is added open MI editors are refreshed
		// the tree should also refresh if one of the layer or blend assets is swapped

		auto ValidationLambda = ([](const FRootPropertyNodeList& PropertyNodeList) { return true; });
		Generator->SetCustomValidatePropertyNodesFunction(FOnValidatePropertyRowGeneratorNodes::CreateLambda(MoveTemp(ValidationLambda)));

		TArray<UObject*> Objects;
		Objects.Add(MaterialEditorInstance);
		Generator->SetObjects(Objects);
	}
	else
	{
		TArray<UObject*> Objects;
		Objects.Add(MaterialEditorInstance);
		Generator->SetObjects(Objects);
	}


	TSharedPtr<IDetailTreeNode> ParameterGroups = FindParameterGroupsNode(Generator);
	if (ParameterGroups.IsValid())
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		ParameterGroups->GetChildren(Children);
		// the order of DeferredSearches should correspond to NonLayerProperties exactly
		TArray<TSharedPtr<IPropertyHandle>> DeferredSearches;
		for (int32 GroupIdx = 0; GroupIdx < Children.Num(); ++GroupIdx)
		{
			TArray<void*> GroupPtrs;
			TSharedPtr<IPropertyHandle> ChildHandle = Children[GroupIdx]->CreatePropertyHandle();
			ChildHandle->AccessRawData(GroupPtrs);
			auto GroupIt = GroupPtrs.CreateConstIterator();
			const FEditorParameterGroup* ParameterGroupPtr = reinterpret_cast<FEditorParameterGroup*>(*GroupIt);
			const FEditorParameterGroup& ParameterGroup = *ParameterGroupPtr;

			for (int32 ParamIdx = 0; ParamIdx < ParameterGroup.Parameters.Num(); ParamIdx++)
			{
				UDEditorParameterValue* Parameter = ParameterGroup.Parameters[ParamIdx];

				TSharedPtr<IPropertyHandle> ParametersArrayProperty = ChildHandle->GetChildHandle("Parameters");
				TSharedPtr<IPropertyHandle> ParameterProperty = ParametersArrayProperty->GetChildHandle(ParamIdx);
				TSharedPtr<IPropertyHandle> ParameterValueProperty = ParameterProperty->GetChildHandle("ParameterValue");

				if (Cast<UDEditorMaterialLayersParameterValue>(Parameter))
				{
					FunctionParameterHandle = ChildHandle;
					if (FunctionParameter == nullptr)
					{
						FunctionParameter = Parameter;
					}
					TArray<void*> StructPtrs;
					ParameterValueProperty->AccessRawData(StructPtrs);
					auto It = StructPtrs.CreateConstIterator();
					FunctionInstance = reinterpret_cast<FMaterialLayersFunctions*>(*It);
					FunctionInstanceHandle = ParameterValueProperty;
				}
				else
				{
					FUnsortedParamData NonLayerProperty;
					UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);

					if (ScalarParam && ScalarParam->SliderMax > ScalarParam->SliderMin)
					{
						ParameterValueProperty->SetInstanceMetaData("UIMin", FString::Printf(TEXT("%f"), ScalarParam->SliderMin));
						ParameterValueProperty->SetInstanceMetaData("UIMax", FString::Printf(TEXT("%f"), ScalarParam->SliderMax));
					}

					NonLayerProperty.Parameter = Parameter;
					NonLayerProperty.ParameterGroup = ParameterGroup;

					DeferredSearches.Add(ParameterValueProperty);
					NonLayerProperty.UnsortedName = Parameter->ParameterInfo.Name;

					NonLayerProperties.Add(NonLayerProperty);
				}
			}
		}

		checkf(NonLayerProperties.Num() == DeferredSearches.Num(), TEXT("Internal inconsistency: number of node searches does not match the number of properties"));
		TArray<TSharedPtr<IDetailTreeNode>> DeferredResults = Generator->FindTreeNodes(DeferredSearches);
		checkf(NonLayerProperties.Num() == DeferredResults.Num(), TEXT("Internal inconsistency: number of node search results does not match the number of properties"));

		for (int Idx = 0, NumUnsorted = NonLayerProperties.Num(); Idx < NumUnsorted; ++Idx)
		{
			FUnsortedParamData& NonLayerProperty = NonLayerProperties[Idx];
			NonLayerProperty.ParameterNode = DeferredResults[Idx];
			NonLayerProperty.ParameterHandle = NonLayerProperty.ParameterNode->CreatePropertyHandle();
		}

		DeferredResults.Empty();
		DeferredSearches.Empty();

		// Create the hierarchy of Sorted items recursivelly following the LayerFunctions Tree
#ifdef ENABLE_MATERIAL_LAYER_PROTOTYPE
		if (FunctionParameterHandle)
		{
			TSharedPtr<IPropertyHandle>	LayerHandle = FunctionParameterHandle->GetChildHandle("Layers").ToSharedRef();
			TSharedPtr<IPropertyHandle> BlendHandle = FunctionParameterHandle->GetChildHandle("Blends").ToSharedRef();
			uint32 NumLayerChildren;
			LayerHandle->GetNumChildren(NumLayerChildren);
			uint32 NumBlendChildren;
			BlendHandle->GetNumChildren(NumBlendChildren);
			if (MaterialEditorInstance->StoredLayerPreviews.Num() != NumLayerChildren)
			{
				MaterialEditorInstance->StoredLayerPreviews.Empty();
				MaterialEditorInstance->StoredLayerPreviews.AddDefaulted(NumLayerChildren);
			}
			if (MaterialEditorInstance->StoredBlendPreviews.Num() != NumBlendChildren)
			{
				MaterialEditorInstance->StoredBlendPreviews.Empty();
				MaterialEditorInstance->StoredBlendPreviews.AddDefaulted(NumBlendChildren);
			}

			// root 
			auto StrongFunctionParameter = FunctionParameter.Pin();

			FRecursiveCreateWidgetsContext Context{
				.Parameter = StrongFunctionParameter.Get(),
				.LayerHandle = LayerHandle,
				.BlendHandle = BlendHandle
			};

			auto RootChildren = FunctionInstance->GetNodeChildren(-1);
			for (int i = 0; i < RootChildren.Num(); ++i)
			{
				RecursiveCreateWidgets(&Context, RootChildren[i], LayerProperties, true, false);
			}
		}
#endif // ENABLE_MATERIAL_LAYER_PROTOTYPE
	}
	
	SetParentsExpansionState();
}

bool SMaterialSubstrateTree::IsLayerVisible(int32 Index) const
{
	if (FunctionParameter.IsValid())
	{
		return FunctionInstance->GetLayerVisibility(Index);
	}
	return false;
}

TSharedRef<SWidget> SMaterialSubstrateTree::CreateThumbnailWidget(EMaterialParameterAssociation InAssociation, int32 InIndex, float InThumbnailSize)
{
	UObject* ThumbnailObject = nullptr;
	if (InAssociation == EMaterialParameterAssociation::LayerParameter)
	{
		ThumbnailObject = MaterialEditorInstance->StoredLayerPreviews[InIndex];
	}
	else if (InAssociation == EMaterialParameterAssociation::BlendParameter)
	{
		ThumbnailObject = MaterialEditorInstance->StoredBlendPreviews[InIndex];
	}
	const TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(ThumbnailObject, InThumbnailSize, InThumbnailSize, GetTreeThumbnailPool()));
	TSharedRef<SWidget> ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
	AssetThumbnail->SetRealTime(true);
	ThumbnailWidget->SetOnMouseDoubleClick(FPointerEventHandler::CreateSP(this, &SMaterialSubstrateTree::OnThumbnailDoubleClick, InAssociation, InIndex));
	return ThumbnailWidget;
}

void SMaterialSubstrateTree::UpdateThumbnailMaterial(TEnumAsByte<EMaterialParameterAssociation> InAssociation, int32 InIndex, bool bAlterBlendIndex)
{
	UMaterialInstanceConstant* MaterialToUpdate = nullptr;
	int32 ParameterIndex = InIndex;
	if (InAssociation == EMaterialParameterAssociation::LayerParameter)
	{
		MaterialToUpdate = MaterialEditorInstance->StoredLayerPreviews[ParameterIndex];
	}
	if (InAssociation == EMaterialParameterAssociation::BlendParameter)
	{
		if (bAlterBlendIndex)
		{
			ParameterIndex--;
		}
		MaterialToUpdate = MaterialEditorInstance->StoredBlendPreviews[ParameterIndex];
	}

	if (MaterialToUpdate != nullptr)
	{
		// Need to invert index b/c layer properties is generated in reverse order
	/*	TArray<TSharedPtr<FSortedParamData>> AssetChildren = LayerProperties[LayerProperties.Num() - 1 - InIndex]->Children;

		TArray<FEditorParameterGroup> ParameterGroups;
		for (TSharedPtr<FSortedParamData> AssetChild : AssetChildren)
		{
			for (TSharedPtr<FSortedParamData> Group : AssetChild->Children)
			{
				if (Group->ParameterInfo.Association == InAssociation)
				{
					FEditorParameterGroup DuplicatedGroup = FEditorParameterGroup();
					DuplicatedGroup.GroupAssociation = Group->Group.GroupAssociation;
					DuplicatedGroup.GroupName = Group->Group.GroupName;
					DuplicatedGroup.GroupSortPriority = Group->Group.GroupSortPriority;
					for (UDEditorParameterValue* Parameter : Group->Group.Parameters)
					{
						if (Parameter->ParameterInfo.Index == ParameterIndex)
						{
							DuplicatedGroup.Parameters.Add(Parameter);
						}
					}
					ParameterGroups.Add(DuplicatedGroup);
				}
			}

	

		}

		FMaterialPropertyHelpers::TransitionAndCopyParameters(MaterialToUpdate, ParameterGroups, true);
*/
	}
}

FReply SMaterialSubstrateTree::OnThumbnailDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent, EMaterialParameterAssociation InAssociation, int32 InIndex)
{
	UMaterialFunctionInterface* AssetToOpen = nullptr;
	if (InAssociation == EMaterialParameterAssociation::BlendParameter)
	{
		AssetToOpen = FunctionInstance->Blends[InIndex];
	}
	else if (InAssociation == EMaterialParameterAssociation::LayerParameter)
	{
		AssetToOpen = FunctionInstance->Layers[InIndex];
	}
	if (AssetToOpen != nullptr)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetToOpen);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMaterialSubstrateTree::ShowSubParameters(TSharedPtr<FSortedParamData> ParentParameter)
{
	for (FUnsortedParamData Property : NonLayerProperties)
	{
		UDEditorParameterValue* Parameter = Property.Parameter;
		if (Parameter->ParameterInfo.Index == ParentParameter->ParameterInfo.Index
			&& Parameter->ParameterInfo.Association == ParentParameter->ParameterInfo.Association)
		{
			TSharedPtr<FSortedParamData> GroupProperty(new FSortedParamData());
			GroupProperty->StackDataType = EStackDataType::Group;
			GroupProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
			GroupProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
			GroupProperty->Group = Property.ParameterGroup;
			GroupProperty->NodeKey = FString::FromInt(GroupProperty->ParameterInfo.Index) + FString::FromInt(GroupProperty->ParameterInfo.Association) + Property.ParameterGroup.GroupName.ToString();

			bool bAddNewGroup = true;
			for (TSharedPtr<struct FSortedParamData> GroupChild : ParentParameter->Children)
			{
				if (GroupChild->NodeKey == GroupProperty->NodeKey)
				{
					bAddNewGroup = false;
				}
			}
			if (bAddNewGroup)
			{
				ParentParameter->Children.Add(GroupProperty);
			}

			TSharedPtr<FSortedParamData> ChildProperty(new FSortedParamData());
			ChildProperty->StackDataType = EStackDataType::Property;
			ChildProperty->Parameter = Parameter;
			ChildProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
			ChildProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
			ChildProperty->ParameterNode = Property.ParameterNode;
			ChildProperty->PropertyName = Property.UnsortedName;
			ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association) +  Property.ParameterGroup.GroupName.ToString() + Property.UnsortedName.ToString();


			UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(Parameter);
			if (!CompMaskParam)
			{
				TArray<TSharedRef<IDetailTreeNode>> ParamChildren;
				Property.ParameterNode->GetChildren(ParamChildren);
				for (int32 ParamChildIdx = 0; ParamChildIdx < ParamChildren.Num(); ParamChildIdx++)
				{
					TSharedPtr<FSortedParamData> ParamChildProperty(new FSortedParamData());
					ParamChildProperty->StackDataType = EStackDataType::PropertyChild;
					ParamChildProperty->ParameterNode = ParamChildren[ParamChildIdx];
					ParamChildProperty->ParameterHandle = ParamChildProperty->ParameterNode->CreatePropertyHandle();
					ParamChildProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
					ParamChildProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
					ParamChildProperty->Parameter = ChildProperty->Parameter;
					ChildProperty->Children.Add(ParamChildProperty);
				}
			}
			for (TSharedPtr<struct FSortedParamData> GroupChild : ParentParameter->Children)
			{
				if (GroupChild->Group.GroupName == Property.ParameterGroup.GroupName
					&& GroupChild->ParameterInfo.Association == ChildProperty->ParameterInfo.Association
					&&  GroupChild->ParameterInfo.Index == ChildProperty->ParameterInfo.Index)
				{
					GroupChild->Children.Add(ChildProperty);
				}
			}

		}
	}
}

#undef LOCTEXT_NAMESPACE