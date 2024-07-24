// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerMode.h"

class UWorkspace;

namespace UE::Workspace
{
	class IWorkspaceEditor;

	class FWorkspaceOutlinerMode : public ISceneOutlinerMode
	{
	public:
		FWorkspaceOutlinerMode(SSceneOutliner* InSceneOutliner, const TWeakObjectPtr<UWorkspace>& InWeakWorkspace, const TWeakPtr<IWorkspaceEditor>& InWeakWorkspaceEditor);
		virtual ~FWorkspaceOutlinerMode() override;

		// Begin ISceneOutlinerMode overrides
		virtual void Rebuild() override;
		virtual TSharedPtr<SWidget> CreateContextMenu() override;		
		virtual void OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item) override;
		virtual void OnItemClicked(FSceneOutlinerTreeItemPtr Item) override;
		virtual FReply OnKeyDown(const FKeyEvent& InKeyEvent) override;
		void HandleItemSelection(const FSceneOutlinerItemSelection& Selection);
		virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;
		virtual bool CanCustomizeToolbar() const { return true; }
		virtual ESelectionMode::Type GetSelectionMode() const { return ESelectionMode::Multi; }
	protected:
		virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;		
		// End ISceneOutlinerMode overrides

		void OnWorkspaceModified(UWorkspace* InWorkspace);
		void ResetOutlinerSelection();

		void OpenItems(TArrayView<const FSceneOutlinerTreeItemPtr> Items) const;
		void DeleteItems(TArrayView<const FSceneOutlinerTreeItemPtr> Items) const;

		void OnAssetRegistryAssetUpdate(const FAssetData& AssetData);
	private:
		TWeakObjectPtr<UWorkspace> WeakWorkspace;
		TWeakPtr<IWorkspaceEditor> WeakWorkspaceEditor;
	};
}