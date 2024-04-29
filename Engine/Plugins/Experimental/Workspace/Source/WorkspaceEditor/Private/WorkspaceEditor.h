// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
class SWorkspaceTabWrapper;
class IStructureDetailsView;

namespace UE::Workspace
{

struct FGraphDocumentSummoner;
struct FWorkspaceTabSummoner;
struct FAssetDocumentSummoner;
class FWorkspaceEditorModule;
class SWorkspaceView;

namespace WorkspaceModes
{
	extern const FName WorkspaceEditor;
}

namespace WorkspaceTabs
{
	extern const FName Details;
	extern const FName WorkspaceView;
}

class FWorkspaceEditor : public IWorkspaceEditor, public FGCObject
{
public:
	FWorkspaceEditor(UWorkspaceAssetEditor* InOwningAssetEditor);
	virtual ~FWorkspaceEditor() override {}

private:
	friend class FWorkspaceEditorMode;
	friend class FWorkspaceEditorModule;
	friend class SGraphDocument;
	friend SWorkspaceTabWrapper;
	friend struct FGraphDocumentSummoner;
	friend struct FWorkspaceTabSummoner;
	friend struct FAssetDocumentSummoner;

	// FBaseAssetToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void CreateWidgets() override;
	virtual void PostInitAssetEditor() override;
	virtual void InitToolMenuContext(FToolMenuContext& InMenuContext) override;
	virtual void SaveAsset_Execute() override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void OnClose() override;
	virtual void RegisterToolbar() override;
	virtual bool ShouldReopenEditorForSavedAsset(const UObject* Asset) const override;

	// FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Workspace);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FWorkspaceEditor");
	}

	// IWorkspaceEditor interface
	virtual void OpenAssets(TConstArrayView<FAssetData> InAssets) override;
	virtual void OpenObjects(TConstArrayView<UObject*> InObjects) override;
	virtual void CloseObjects(TConstArrayView<UObject*> InObjects) override;
	virtual void SetDetailsObjects(const TArray<UObject*>& InObjects) override;
	virtual void RefreshDetails() override;

	void BindCommands();

	void ExtendMenu();

	void ExtendToolbar();

	void CloseDocumentTab(const UObject* DocumentID) const;

	bool InEditingMode() const;

	void RestoreEditedObjectState();

	void SaveEditedObjectState() const;

	TSharedPtr<SDockTab> OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause);

	void RecordDocumentState(const TInstancedStruct<FWorkspaceDocumentState>& InState) const;

	void NavigateBack();
	void NavigateForward();

	/** The asset being edited */
	TObjectPtr<UWorkspace> Workspace = nullptr;

	// Document tracker
	TSharedPtr<FDocumentTracker> DocumentManager;

	TSharedPtr<SWorkspaceView> WorkspaceView;

	bool bSavingWorkspaceOnly = false;
	bool bClosingDown = false;
};

}
