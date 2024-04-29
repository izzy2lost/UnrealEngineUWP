// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceOutlinerColumns.h"

#include "ISourceControlModule.h"
#include "ISourceControlState.h"
#include "WorkspaceEditorModule.h"
#include "WorkspaceOutlinerTreeItem.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "WorkspaceOutlinerColumns"

namespace UE::Workspace
{
	FName WorkspaceOutlinerSourceControl("Source Control Status");
	FName FWorkspaceOutlinerSourceControlColumn::GetID()
	{
		return WorkspaceOutlinerSourceControl;
	}

	SHeaderRow::FColumn::FArguments FWorkspaceOutlinerSourceControlColumn::ConstructHeaderRowColumn()
	{
		return SHeaderRow::Column(GetColumnID())
		.FixedWidth(24.f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			.HeightOverride(16.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"))
				.ToolTipText(LOCTEXT("RevisionControlStatusTooltip", "Revision control status of this parameter"))
			]
		];	
	}

	const TSharedRef<SWidget> FWorkspaceOutlinerSourceControlColumn::ConstructRowWidget(FSceneOutlinerTreeItemRef Item, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
	{
		if (const FWorkspaceOutlinerTreeItem* TreeItem = Item->CastTo<FWorkspaceOutlinerTreeItem>())
		{
			FString PackagePath;
			if (const TSharedPtr<IWorkspaceOutlinerItemDetails> SharedFactory = FWorkspaceEditorModule::GetOutlinerItemDetails(MakeOutlinerDetailsId(TreeItem->Export)))
			{
				PackagePath = SourceControlHelpers::PackageFilename(SharedFactory->GetPackage(TreeItem->Export));
			}
			else if (TreeItem->Export.ParentIdentifier == NAME_None && TreeItem->Export.AssetPath.IsValid())
			{
				PackagePath = SourceControlHelpers::PackageFilename(TreeItem->Export.AssetPath.GetLongPackageName());
			}
		
			if (!PackagePath.IsEmpty())
			{
				return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(20.0f)
				[
					SNew(SImage)
					.ToolTipText_Lambda([this, PackagePath]() -> FText
					{
						ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
						if(const FSourceControlStatePtr State = SourceControlProvider.GetState(PackagePath, EStateCacheUsage::Use))
						{
							return FText::Format(LOCTEXT("RevisionControlStatusFormat", "File: {0}\nStatus: {1}"), State->GetDisplayName(), State->GetDisplayTooltip());
						}

						return LOCTEXT("RevisionControlStatus", "Revision control status of this parameter");
					})
					.Image_Lambda([this, PackagePath]() -> const FSlateBrush*
					{
						ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
						if(const FSourceControlStatePtr State = SourceControlProvider.GetState(PackagePath, EStateCacheUsage::Use))
						{
							return State->GetIcon().GetSmallIcon();
						}
						
						return nullptr;
					})
				];
			}
		}
		
		return SNullWidget::NullWidget;
	}
}

#undef LOCTEXT_NAMESPACE // "WorkspaceOutlinerColumns"