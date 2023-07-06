// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/BaseAssetToolkit.h"

class UMultiUserReplicationStreamAsset;

namespace UE::MultiUserReplicationEditor
{
	class SReplicationStreamEditor;

	class FReplicationStreamEditorToolkit : public FBaseAssetToolkit
	{
	public:
		
		static const FName ContentTabId;

		FReplicationStreamEditorToolkit(UAssetEditor* InOwningAssetEditor);

		//~ Begin FAssetEditorToolkit Interface
		virtual void CreateWidgets();
		virtual void SetEditingObject(UObject* InObject) override;
		virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
		virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
		virtual FName GetToolkitFName() const override { return TEXT("ReplicationStreamEditorToolkit"); }
		//~ End FAssetEditorToolkit Interface

	private:

		/** Root widget editing the asset */
		TSharedPtr<SReplicationStreamEditor> RootWidget;
		
		UMultiUserReplicationStreamAsset* GetEditedStreamAsset() const;
		
		TSharedRef<SDockTab> SpawnTab_Content(const FSpawnTabArgs& SpawnTabArgs);
	};
}

