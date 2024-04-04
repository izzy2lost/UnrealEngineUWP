// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceEditor.h"

#include "WorkspaceEditorMode.h"
#include "Workspace.h"
#include "AssetDocumentSummoner.h"
#include "ExternalPackageHelper.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "SWorkspacePicker.h"
#include "WorkspaceState.h"
#include "WorkspaceDocumentState.h"
#include "WorkspaceEditorModule.h"
#include "Engine/Blueprint.h"

#define LOCTEXT_NAMESPACE "WorkspaceEditor"

namespace UE::Workspace
{

namespace WorkspaceModes
{
	const FName WorkspaceEditor("WorkspaceEditorMode");
}

namespace WorkspaceTabs
{
	const FName Details("DetailsTab");
	const FName WorkspaceView("WorkspaceView");
	const FName LeftDocumentArea("LeftDocumentArea");
	const FName MiddleDocumentArea("MiddleDocumentArea");
	const FName RightDocumentArea("RightDocumentArea");
}

const FName WorkspaceAppIdentifier("WorkspaceEditor");

FWorkspaceEditor::FWorkspaceEditor()
{
}

FWorkspaceEditor::~FWorkspaceEditor()
{
}

void FWorkspaceEditor::InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UWorkspace* InWorkspace)
{
	Workspace = InWorkspace;

	Workspace->LoadState();

	DocumentManager = MakeShared<FDocumentTracker>(NAME_None);
	DocumentManager->Initialize(SharedThis(this));

	FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<FWorkspaceEditorModule>("WorkspaceEditor");

	// Build document summoners for each workspace layout area
	TSharedRef<FAssetDocumentSummoner> LeftAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::LeftDocumentArea, SharedThis(this));
	LeftAssetDocumentSummoner->SetAllowedClassPaths(WorkspaceEditorModule.GetAllowedObjectTypesForArea(WorkspaceTabs::LeftDocumentArea));
	DocumentManager->RegisterDocumentFactory(LeftAssetDocumentSummoner);

	TSharedRef<FAssetDocumentSummoner> MiddleAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::MiddleDocumentArea, SharedThis(this));
	MiddleAssetDocumentSummoner->SetAllowedClassPaths(WorkspaceEditorModule.GetAllowedObjectTypesForArea(WorkspaceTabs::MiddleDocumentArea));
	DocumentManager->RegisterDocumentFactory(MiddleAssetDocumentSummoner);

	TSharedRef<FAssetDocumentSummoner> RightAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::RightDocumentArea, SharedThis(this));
	RightAssetDocumentSummoner->SetAllowedClassPaths(WorkspaceEditorModule.GetAllowedObjectTypesForArea(WorkspaceTabs::RightDocumentArea));
	DocumentManager->RegisterDocumentFactory(RightAssetDocumentSummoner);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	InitAssetEditor(InMode, InInitToolkitHost, WorkspaceAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InWorkspace);

	BindCommands();

	AddApplicationMode(WorkspaceModes::WorkspaceEditor, MakeShared<FWorkspaceEditorMode>(SharedThis(this)));
	SetCurrentMode(WorkspaceModes::WorkspaceEditor);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FWorkspaceEditor::RestoreEditedObjectState()
{
	UWorkspaceState* State = Workspace->GetState();
	for (const TInstancedStruct<FWorkspaceDocumentState>& DocumentState : State->DocumentStates)
	{
		if (UObject* Object = DocumentState.Get().Object.TryLoad())
		{
			if(TSharedPtr<SDockTab> DockTab = OpenDocument(Object, FDocumentTracker::RestorePreviousDocument))
			{
				FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<FWorkspaceEditorModule>("WorkspaceEditor");
				const FObjectDocumentArgs* DocumentArgs = WorkspaceEditorModule.FindObjectDocumentType(Object->GetClass()->GetClassPathName());
				if(DocumentArgs != nullptr && DocumentArgs->OnSetDocumentState.IsBound())
				{
					DocumentArgs->OnSetDocumentState.Execute(FWorkspaceEditorContext(SharedThis(this), Object), DockTab->GetContent(), DocumentState);
				}
			}
		}
	}
}

void FWorkspaceEditor::SaveEditedObjectState()
{
	// Clear edited document state
	UWorkspaceState* State = Workspace->GetState();
	State->DocumentStates.Empty();

	// Ask all open documents to save their state, which will update edited documents
	DocumentManager->SaveAllState();

	// Persist state
	Workspace->SaveState();
}

TSharedPtr<SDockTab> FWorkspaceEditor::OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(InForObject);
	TSharedPtr<SDockTab> NewTab = DocumentManager->OpenDocument(Payload, InCause);

	if(InCause != FDocumentTracker::RestorePreviousDocument)
	{
		FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<FWorkspaceEditorModule>("WorkspaceEditor");
		const FObjectDocumentArgs* DocumentArgs = WorkspaceEditorModule.FindObjectDocumentType(InForObject->GetClass()->GetClassPathName());
		if(DocumentArgs != nullptr && DocumentArgs->OnGetDocumentState.IsBound())
		{
			RecordDocumentState(DocumentArgs->OnGetDocumentState.Execute(FWorkspaceEditorContext(SharedThis(this), const_cast<UObject*>(InForObject)), NewTab->GetContent()));
		}
		else
		{
			RecordDocumentState(TInstancedStruct<FWorkspaceDocumentState>::Make(InForObject));
		}
	}

	return NewTab;
}

void FWorkspaceEditor::OpenAssets(TConstArrayView<FAssetData> InAssets)
{
	for(const FAssetData& Asset : InAssets)
	{
		if(UObject* LoadedAsset = Asset.GetAsset())
		{
			OpenDocument(LoadedAsset, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
		}
	}
}

void FWorkspaceEditor::OpenObjects(TConstArrayView<UObject*> InObjects)
{
	for(UObject* Object : InObjects)
	{
		OpenDocument(Object, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
	}
}

void FWorkspaceEditor::CloseObjects(TConstArrayView<UObject*> InObjects)
{
	if(InObjects.Num() > 0)
	{
		for(UObject* Object : InObjects)
		{
			CloseDocumentTab(Object);
		}
	}
}

void FWorkspaceEditor::SetDetailsObjects(const TArray<UObject*>& InObjects)
{
	if(DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FWorkspaceEditor::RefreshDetails()
{
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
}

void FWorkspaceEditor::BindCommands()
{
}

void FWorkspaceEditor::ExtendMenu()
{
	
}

void FWorkspaceEditor::ExtendToolbar()
{
	
}

void FWorkspaceEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FWorkspaceEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FWorkflowCentricApplication::UnregisterTabSpawners(InTabManager);
}

FName FWorkspaceEditor::GetToolkitFName() const
{
	return FName("WorkspaceEditor");
}

FText FWorkspaceEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "WorkspaceEditor");
}

FString FWorkspaceEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "WorkspaceEditor ").ToString();
}

FLinearColor FWorkspaceEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FWorkspaceEditor::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
}

void FWorkspaceEditor::SaveAsset_Execute()
{
	// If asset is a default 'Untitled' workspace, redirect to the 'save as' flow
	FString AssetPath = Workspace->GetOutermost()->GetPathName();
	if(AssetPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		// Ensure we dont also 'save as' other externally linked assets at this point
		TGuardValue<bool> SaveWorkspaceOnly(bSavingWorkspaceOnly, true);

		SaveAssetAs_Execute();
	}
	else
	{
		FWorkflowCentricApplication::SaveAsset_Execute();
	}
}

void FWorkspaceEditor::CloseDocumentTab(const UObject* DocumentID)
{
	UWorkspaceState* State = Workspace->GetState();
	State->DocumentStates.Remove(TInstancedStruct<FWorkspaceDocumentState>::Make(DocumentID));

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

bool FWorkspaceEditor::InEditingMode() const
{
	return true;
}

void FWorkspaceEditor::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	// Base class will pick up edited object
	FWorkflowCentricApplication::GetSaveableObjects(OutObjects);

	if(!bSavingWorkspaceOnly)
	{
		for(const UWorkspaceAssetEntry* Entry : Workspace->AssetEntries)
		{
			if(UObject* Asset = Entry->Asset.Get())
			{
				// Add object referenced by workspace
				OutObjects.Add(Asset);

				// Get external objects too
				FExternalPackageHelper::GetExternalSaveableObjects(Asset, OutObjects);
			}
		}
	}
}

void FWorkspaceEditor::RecordDocumentState(const TInstancedStruct<FWorkspaceDocumentState>& InState)
{
	UWorkspaceState* State = Workspace->GetState();
	State->DocumentStates.AddUnique(InState);
}

bool FWorkspaceEditor::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	auto RequiresSave = [this]()
	{
		UPackage* Package = Workspace->GetOutermost();
		return Package->GetPathName().StartsWith(TEXT("/Temp/Untitled"));
	};

	// Give the user opportunity to save temp workspaces
	if(RequiresSave())
	{
		// Ensure we dont also 'save as' other externally linked assets at this point
		TGuardValue<bool> SaveWorkspaceOnly(bSavingWorkspaceOnly, true);

		SaveAssetAs_Execute();
	}

	return true;
}

void FWorkspaceEditor::OnClose()
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
		DetailsView.Reset();
	}

	FWorkflowCentricApplication::OnClose();
}

}

#undef LOCTEXT_NAMESPACE