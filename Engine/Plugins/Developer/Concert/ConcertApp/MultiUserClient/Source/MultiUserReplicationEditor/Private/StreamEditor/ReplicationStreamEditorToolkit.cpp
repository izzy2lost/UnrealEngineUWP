// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationStreamEditorToolkit.h"

#include "MultiUserReplicationStreamAsset.h"
#include "StreamEditor/View/SReplicationStreamEditor.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FReplicationStreamEditorToolkit"

namespace UE::MultiUserReplicationEditor
{
	const FName FReplicationStreamEditorToolkit::ContentTabId(TEXT("ReplicationStreamAssetEditor_Content"));
	
	FReplicationStreamEditorToolkit::FReplicationStreamEditorToolkit(UAssetEditor* InOwningAssetEditor)
		: FBaseAssetToolkit(InOwningAssetEditor)
	{
		LayoutAppendix = TEXT("ReplicationStreamEditor_v1");
		const FString LayoutString = TEXT("Standalone_Layout_") + LayoutAppendix;
		StandaloneDefaultLayout = FTabManager::NewLayout(FName(LayoutString))
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(1.f)
					->SetHideTabWell(true)
					->AddTab(ContentTabId, ETabState::OpenedTab)
				)
			);
	}

	void FReplicationStreamEditorToolkit::CreateWidgets()
	{
		// Do not call Super because we do not want to create the viewport and details panel from FBaseAssetToolkit
		
	}

	void FReplicationStreamEditorToolkit::SetEditingObject(UObject* InObject)
	{
		// Do not call Super because we do not wish to use the details view from FBaseAssetToolkit
	}

	void FReplicationStreamEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_ReplicationStreamAssetEditor", "Replication Strean Asset Editor"));
		FBaseAssetToolkit::RegisterTabSpawners(InTabManager);

		InTabManager->RegisterTabSpawner(ContentTabId, FOnSpawnTab::CreateSP(this, &FReplicationStreamEditorToolkit::SpawnTab_Content))
			.SetDisplayName(LOCTEXT("ConfigTab", "Config"))
			.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	}

	void FReplicationStreamEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
		InTabManager->UnregisterTabSpawner(ContentTabId);
	}

	UMultiUserReplicationStreamAsset* FReplicationStreamEditorToolkit::GetEditedStreamAsset() const
	{
		return CastChecked<UMultiUserReplicationStreamAsset>(GetEditingObject());
	}

	TSharedRef<SDockTab> FReplicationStreamEditorToolkit::SpawnTab_Content(const FSpawnTabArgs& SpawnTabArgs)
	{
		return SNew(SDockTab)
			.Label(LOCTEXT("BaseDetailsTitle", "Details"))
			[
				SAssignNew(RootWidget, SReplicationStreamEditor, *GetEditedStreamAsset())
			];
	}
}

#undef LOCTEXT_NAMESPACE