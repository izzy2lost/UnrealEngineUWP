// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWorkspaceView.h"
#include "Workspace.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SAssetDropTarget.h"
#include "SceneOutlinerPublicTypes.h"
#include "ScopedTransaction.h"
#include "SSceneOutliner.h"
#include "WorkspaceSchema.h"
#include "Framework/Commands/GenericCommands.h"
#include "Outliner/WorkspaceOutlinerColumns.h"
#include "Outliner/WorkspaceOutlinerMode.h"

#define LOCTEXT_NAMESPACE "SWorkspaceView"

namespace UE::Workspace
{

void SWorkspaceView::Construct(const FArguments& InArgs, UWorkspace* InWorkspace, TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor)
{
	Workspace = InWorkspace;

	FSceneOutlinerInitializationOptions InitOptions;
	{
		InitOptions.OutlinerIdentifier = TEXT("WorkspaceEditorOutliner");
		InitOptions.bShowHeaderRow = true;
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10));
		InitOptions.ColumnMap.Add(FWorkspaceOutlinerFileStateColumn::GetID(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& InSceneOutliner) { return MakeShareable(new FWorkspaceOutlinerFileStateColumn(InSceneOutliner)); }), false));
		InitOptions.ColumnMap.Add(FWorkspaceOutlinerSourceControlColumn::GetID(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 100, FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& InSceneOutliner) { return MakeShareable(new FWorkspaceOutlinerSourceControlColumn(InSceneOutliner)); }), false));	
		InitOptions.ModeFactory = FCreateSceneOutlinerMode::CreateLambda([this, WeakWorkspaceEditor=InWorkspaceEditor.ToWeakPtr()](SSceneOutliner* InOutliner) { return new UE::Workspace::FWorkspaceOutlinerMode(UE::Workspace::FWorkspaceOutlinerMode(InOutliner, Workspace, WeakWorkspaceEditor)); });
	}
	SceneWorkspaceOutliner = SNew(SSceneOutliner, InitOptions);

	ChildSlot
	[
		SNew(SAssetDropTarget)
		.bSupportsMultiDrop(true)
		.OnAssetsDropped_Lambda([this](const FDragDropEvent& InEvent, TArrayView<FAssetData> InAssets)
		{
			FScopedTransaction Transaction(LOCTEXT("AddAssets", "Add assets to workspace"));

			Workspace->AddAssets(InAssets);
		})
		.OnAreAssetsAcceptableForDropWithReason_Lambda([this](TArrayView<FAssetData> InAssets, FText& OutText)
		{
			for(const FAssetData& Asset : InAssets)
			{
				if(Workspace->IsAssetSupported(Asset))
				{
					return true;
				}
			}

			OutText = LOCTEXT("AssetsUnsupportedInWorkspace", "Assets are not supported by this workspace");
			return false;
		})
		.Content()
		[
			SceneWorkspaceOutliner.ToSharedRef()
		]
	];
}

}

#undef LOCTEXT_NAMESPACE