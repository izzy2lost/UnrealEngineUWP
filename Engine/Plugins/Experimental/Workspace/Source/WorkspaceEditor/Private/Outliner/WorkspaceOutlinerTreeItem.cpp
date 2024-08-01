// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceOutlinerTreeItem.h"

#include "ISceneOutliner.h"
#include "Styling/SlateColor.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "WorkspaceEditorModule.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "WorkspaceOutlinerTreeItem"

namespace UE::Workspace
{
	const FSceneOutlinerTreeItemType FWorkspaceOutlinerTreeItem::Type;
	class SWorkspaceOutlinerTreelabel : FSceneOutlinerCommonLabelData, public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SWorkspaceOutlinerTreelabel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, FWorkspaceOutlinerTreeItem& InTreeItem, ISceneOutliner& SceneOutliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
		{
			WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());
			TreeItem = StaticCastSharedRef<FWorkspaceOutlinerTreeItem>(InTreeItem.AsShared());

			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FSceneOutlinerDefaultTreeItemMetrics::IconPadding())
				[
					SNew(SBox)
					.WidthOverride(FSceneOutlinerDefaultTreeItemMetrics::IconSize())
					.HeightOverride(FSceneOutlinerDefaultTreeItemMetrics::IconSize())
					[
						SNew(SImage)
						.Image_Lambda([this]() -> const FSlateBrush* 
						{
							if (const TSharedPtr<FWorkspaceOutlinerTreeItem> SharedTreeItem = TreeItem.Pin())
							{
								if (SharedTreeItem->ItemDetails.IsValid())
								{
									return SharedTreeItem->ItemDetails->GetItemIcon();
								}
							}
							
							return FAppStyle::GetBrush(TEXT("ClassIcon.Default"));
						})
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(this, &SWorkspaceOutlinerTreelabel::GetDisplayText)
					.HighlightText(SceneOutliner.GetFilterHighlightText())
					.ColorAndOpacity(this, &SWorkspaceOutlinerTreelabel::GetForegroundColor)
				]
			];
		}

		FText GetDisplayText() const
		{
			if (const TSharedPtr<FWorkspaceOutlinerTreeItem> Item = TreeItem.Pin())
			{				
				return FText::FromString(Item->GetDisplayString());
			}
			return FText();
		}
		
		virtual FSlateColor GetForegroundColor() const override
		{
			const TOptional<FLinearColor> BaseColor = FSceneOutlinerCommonLabelData::GetForegroundColor(*TreeItem.Pin());
			return BaseColor.IsSet() ? BaseColor.GetValue() : FSlateColor::UseForeground();
		}

	private:
		TWeakPtr<FWorkspaceOutlinerTreeItem> TreeItem;
	};	
	
	FWorkspaceOutlinerTreeItem::FWorkspaceOutlinerTreeItem(const FItemData& InItemData) : ISceneOutlinerTreeItem(FWorkspaceOutlinerTreeItem::Type), Export(InItemData.Export)
	{
		ItemDetails = FWorkspaceEditorModule::GetOutlinerItemDetails(MakeOutlinerDetailsId(Export));
	}

	bool FWorkspaceOutlinerTreeItem::IsValid() const
	{
		return Export.GetIdentifier().IsValid();
	}

	FSceneOutlinerTreeItemID FWorkspaceOutlinerTreeItem::GetID() const
	{
		return GetTypeHash(Export);
	}

	FString FWorkspaceOutlinerTreeItem::GetDisplayString() const
	{
		return Export.GetIdentifier().ToString();
	}

	TSharedRef<SWidget> FWorkspaceOutlinerTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
	{
		return SNew(SWorkspaceOutlinerTreelabel, *this, Outliner, InRow);
	}

	FString FWorkspaceOutlinerTreeItem::GetPackageName() const
	{
		if (const TSharedPtr<IWorkspaceOutlinerItemDetails> SharedFactory = FWorkspaceEditorModule::GetOutlinerItemDetails(MakeOutlinerDetailsId(Export)))
		{
			const UPackage* Package = SharedFactory->GetPackage(Export);
			return Package != nullptr ? Package->GetName() : FString();
		}
		else if (Export.GetParentIdentifier() == NAME_None && Export.GetAssetPath().IsValid())
		{
			return Export.GetAssetPath().GetLongPackageName();
		}		
		
		return ISceneOutlinerTreeItem::GetPackageName();
	}
}

#undef LOCTEXT_NAMESPACE // "WorkspaceOutlinerTreeItem"