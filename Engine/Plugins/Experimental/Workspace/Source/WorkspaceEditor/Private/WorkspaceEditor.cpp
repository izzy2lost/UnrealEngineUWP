// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceEditor.h"

#include "Workspace.h"
#include "AssetDocumentSummoner.h"
#include "ExternalPackageHelper.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "WorkspaceAssetEditor.h"
#include "WorkspaceState.h"
#include "WorkspaceDocumentState.h"
#include "WorkspaceEditorModule.h"
#include "SWorkspaceView.h"

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

FWorkspaceEditor::FWorkspaceEditor(UWorkspaceAssetEditor* InOwningAssetEditor) : IWorkspaceEditor(InOwningAssetEditor)
{
	Workspace = Cast<UWorkspaceAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
}

void FWorkspaceEditor::CreateWidgets()
{
	DocumentManager = MakeShared<FDocumentTracker>(NAME_None);
	DocumentManager->Initialize(SharedThis(this));
	
	FBaseAssetToolkit::CreateWidgets();

	const FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<FWorkspaceEditorModule>("WorkspaceEditor");

	// Build document summoners for each workspace layout area
	const TSharedRef<FAssetDocumentSummoner> LeftAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::LeftDocumentArea, SharedThis(this));
	LeftAssetDocumentSummoner->SetAllowedClassPaths(WorkspaceEditorModule.GetAllowedObjectTypesForArea(WorkspaceTabs::LeftDocumentArea));
	DocumentManager->RegisterDocumentFactory(LeftAssetDocumentSummoner);

	const TSharedRef<FAssetDocumentSummoner> MiddleAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::MiddleDocumentArea, SharedThis(this));
	MiddleAssetDocumentSummoner->SetAllowedClassPaths(WorkspaceEditorModule.GetAllowedObjectTypesForArea(WorkspaceTabs::MiddleDocumentArea));
	DocumentManager->RegisterDocumentFactory(MiddleAssetDocumentSummoner);

	const TSharedRef<FAssetDocumentSummoner> RightAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::RightDocumentArea, SharedThis(this));
	RightAssetDocumentSummoner->SetAllowedClassPaths(WorkspaceEditorModule.GetAllowedObjectTypesForArea(WorkspaceTabs::RightDocumentArea));
	DocumentManager->RegisterDocumentFactory(RightAssetDocumentSummoner);
	
	check(DetailsView.IsValid());
	WorkspaceEditorModule.ApplyWorkspaceDetailsCustomization(DetailsView);

	StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_WorkspaceEditor_Layout_v1.1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()
			->SetSizeCoefficient(1.0f)
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->SetHideTabWell(false)
				->AddTab(WorkspaceTabs::WorkspaceView, ETabState::OpenedTab)
				->AddTab(WorkspaceTabs::LeftDocumentArea, ETabState::ClosedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.5f)
				->SetHideTabWell(false)
				->AddTab(WorkspaceTabs::MiddleDocumentArea, ETabState::ClosedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->SetHideTabWell(false)
				->AddTab(WorkspaceTabs::RightDocumentArea, ETabState::ClosedTab)
				->AddTab(FBaseAssetToolkit::DetailsTabID, ETabState::OpenedTab)
			)
		)
	);

	TWeakPtr<FWorkspaceEditor> WeakToolkit = StaticCastSharedRef<FWorkspaceEditor>(AsShared());
	WorkspaceView = SNew(SWorkspaceView, Workspace)
		.OnAssetsOpened_Lambda([WeakToolkit](TConstArrayView<FAssetData> InAssets)
		{
			if(const TSharedPtr<FWorkspaceEditor> HostingApp = WeakToolkit.Pin())
			{
				HostingApp->OpenAssets(InAssets);
			}
		});

	
	BindCommands();
}

void FWorkspaceEditor::PostInitAssetEditor()
{
	Workspace->LoadState();
	RestoreEditedObjectState();

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
			if(const TSharedPtr<SDockTab> DockTab = OpenDocument(Object, FDocumentTracker::RestorePreviousDocument))
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

void FWorkspaceEditor::SaveEditedObjectState() const
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
	const TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(InForObject);
	TSharedPtr<SDockTab> NewTab = DocumentManager->OpenDocument(Payload, InCause);

	if(InCause != FDocumentTracker::RestorePreviousDocument)
	{
		const FWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<FWorkspaceEditorModule>("WorkspaceEditor");
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
		if(const UObject* LoadedAsset = Asset.GetAsset())
		{
			OpenDocument(LoadedAsset, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
		}
	}
}

void FWorkspaceEditor::OpenObjects(TConstArrayView<UObject*> InObjects)
{
	for(const UObject* Object : InObjects)
	{
		OpenDocument(Object, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
	}
}

void FWorkspaceEditor::CloseObjects(TConstArrayView<UObject*> InObjects)
{
	if(InObjects.Num() > 0)
	{
		for(const UObject* Object : InObjects)
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
	FBaseAssetToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(WorkspaceTabs::WorkspaceView, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
	{
		check(Args.GetTabId() == WorkspaceTabs::WorkspaceView);
		
		return SNew(SDockTab)
			.Label(LOCTEXT("WorkspaceTabLabel", "Workspace"))
			[
				WorkspaceView.ToSharedRef()
			];
	}))
	.SetDisplayName(LOCTEXT("WorkspaceTabLabel", "Workspace"))
	.SetIcon(FSlateIcon("EditorStyle", "LevelEditor.Tabs.Outliner"))
	.SetTooltipText(LOCTEXT("WorkspaceTabToolTip", "Shows the workspace outliner tab."));

	DocumentManager->SetTabManager(InTabManager);
}

void FWorkspaceEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::UnregisterTabSpawners(InTabManager);
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
	const FString AssetPath = Workspace->GetOutermost()->GetPathName();
	if(AssetPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		// Ensure we do not also 'save as' other externally linked assets at this point
		TGuardValue<bool> SaveWorkspaceOnly(bSavingWorkspaceOnly, true);

		SaveAssetAs_Execute();
	}
	else
	{
		FBaseAssetToolkit::SaveAsset_Execute();
	}
}

void FWorkspaceEditor::CloseDocumentTab(const UObject* DocumentID) const
{
	UWorkspaceState* State = Workspace->GetState();
	State->DocumentStates.Remove(TInstancedStruct<FWorkspaceDocumentState>::Make(DocumentID));

	const TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

bool FWorkspaceEditor::InEditingMode() const
{
	return true;
}

void FWorkspaceEditor::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	// Base class will pick up edited object
	FBaseAssetToolkit::GetSaveableObjects(OutObjects);

	for (UObject* Object : GetEditingObjects())
	{
		// Get external objects too
		FExternalPackageHelper::GetExternalSaveableObjects(Object, OutObjects);	
	}
	
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

void FWorkspaceEditor::RecordDocumentState(const TInstancedStruct<FWorkspaceDocumentState>& InState) const
{
	UWorkspaceState* State = Workspace->GetState();
	State->DocumentStates.AddUnique(InState);
}

bool FWorkspaceEditor::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	TGuardValue<bool> ClosingDown(bClosingDown, true);

	auto RequiresSave = [this]()
	{
		const UPackage* Package = Workspace->GetOutermost();
		return Package->GetPathName().StartsWith(TEXT("/Temp/Untitled"));
	};

	// Give the user opportunity to save temp workspaces
	if(RequiresSave() && !bSavingWorkspaceOnly)
	{
		// Ensure we dont also 'save as' other externally linked assets at this point
		TGuardValue<bool> SaveWorkspaceOnly(bSavingWorkspaceOnly, true);

		SaveAssetAs_Execute();
	}

	return true;
}

void FWorkspaceEditor::OnClose()
{
	SaveEditedObjectState();
	
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
		DetailsView.Reset();
	}

	FBaseAssetToolkit::OnClose();
}

void FWorkspaceEditor::RegisterToolbar()
{
	IWorkspaceEditor::RegisterToolbar();
}

bool FWorkspaceEditor::ShouldReopenEditorForSavedAsset(const UObject* Asset) const
{
	return !bClosingDown;
}
}

#undef LOCTEXT_NAMESPACE