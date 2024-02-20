// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphEditor.h"
#include "InstancedStruct.h"
#include "IWorkspaceEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

struct FWorkspaceDocumentState;
struct FWorkspaceAssetRegistryExports;
class UWorkspace;
class FDocumentTracker;
class FDocumentTabFactory;
class FTabInfo;
class FTabManager;
class IToolkitHost;

namespace UE::Workspace
{

class FWorkspaceEditorMode;
struct FGraphDocumentSummoner;
struct FWorkspaceTabSummoner;
struct FAssetDocumentSummoner;
class FWorkspaceEditorModule;

namespace WorkspaceModes
{
	extern const FName WorkspaceEditor;
}

namespace WorkspaceTabs
{
	extern const FName Details;
	extern const FName WorkspaceView;
}

class FWorkspaceEditor : public IWorkspaceEditor
{
public:
	FWorkspaceEditor();
	virtual ~FWorkspaceEditor() override;

	/** Edits the specified asset */
	void InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UWorkspace* InWorkspace);

private:
	friend class FWorkspaceEditorMode;
	friend struct FGraphDocumentSummoner;
	friend class SGraphDocument;
	friend struct FWorkspaceTabSummoner;
	friend struct FAssetDocumentSummoner;
	friend class FWorkspaceEditorModule;

	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& InMenuContext) override;
	virtual void SaveAsset_Execute() override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void OnClose() override;

	// FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	// IWorkspaceEditor interface
	virtual void OpenAssets(TConstArrayView<FAssetData> InAssets) override;
	virtual void OpenObjects(TConstArrayView<UObject*> InObjects) override;
	virtual void CloseObjects(TConstArrayView<UObject*> InObjects) override;
	virtual void SetDetailsObjects(const TArray<UObject*>& InObjects) override;
	virtual void RefreshDetails() override;

	void BindCommands();

	void ExtendMenu();

	void ExtendToolbar();

	void CloseDocumentTab(const UObject* DocumentID);

	void HandleDetailsViewCreated(TSharedRef<IDetailsView> InDetailsView)
	{
		DetailsView = InDetailsView;
	}
	
	bool InEditingMode() const;

	void RestoreEditedObjectState();

	void SaveEditedObjectState();

	TSharedPtr<SDockTab> OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause);

	void RecordDocumentState(const TInstancedStruct<FWorkspaceDocumentState>& InState);

	// The asset we are editing
	UWorkspace* Workspace = nullptr;

	// Document tracker
	TSharedPtr<FDocumentTracker> DocumentManager;

	// Command list for this editor
	TSharedPtr<FUICommandList> CommandList;

	// Our details panel
	TSharedPtr<IDetailsView> DetailsView;

	bool bSavingWorkspaceOnly = false;
};

}