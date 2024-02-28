// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceEditorModule.h"

#include "EdGraphUtilities.h"
#include "WorkspaceDocumentState.h"
#include "GraphDocumentState.h"
#include "SGraphDocument.h"
#include "SWorkspacePicker.h"
#include "Workspace.h"
#include "WorkspaceEditor.h"
#include "WorkspaceFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "WorkspaceEditorModule"

namespace UE::Workspace
{

void FWorkspaceEditorModule::RegisterObjectDocumentType(const FTopLevelAssetPath& InClassPath, const FObjectDocumentArgs& InParams)
{
	ensure(InParams.SpawnLocation != NAME_None);

	DocumentAreaMap.FindOrAdd(InParams.SpawnLocation).Add(InClassPath);
	ObjectDocumentArgs.Add(InClassPath, InParams);
}

void FWorkspaceEditorModule::UnregisterObjectDocumentType(const FTopLevelAssetPath& InClassPath)
{
	if(const FObjectDocumentArgs* ExistingType = FindObjectDocumentType(InClassPath))
	{
		DocumentAreaMap.FindChecked(ExistingType->SpawnLocation).Remove(InClassPath);
	}
	ObjectDocumentArgs.Remove(InClassPath);
}

FObjectDocumentArgs FWorkspaceEditorModule::CreateGraphDocumentArgs(const FGraphDocumentWidgetArgs& InArgs)
{
	FObjectDocumentArgs Args;
	Args.OnMakeDocumentWidget = FOnMakeDocumentWidget::CreateLambda([InArgs](const FWorkspaceEditorContext& InContext)
	{
		return SNew(SGraphDocument, StaticCastSharedRef<FWorkspaceEditor>(InContext.WorkspaceEditor), CastChecked<UEdGraph>(InContext.Object))
			.OnCreateActionMenu_Lambda([OnCreateActionMenu = InArgs.OnCreateActionMenu](const FWorkspaceEditorContext& InContext, UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bInAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
			{
				if(OnCreateActionMenu.IsBound())
				{
					return OnCreateActionMenu.Execute(InContext, InGraph, InNodePosition, InDraggedPins, bInAutoExpand, InOnMenuClosed);
				}
				return FActionMenuContent();
			})
			.OnNodeTextCommitted(InArgs.OnNodeTextCommitted)
			.OnGraphSelectionChanged(InArgs.OnGraphSelectionChanged)
			.OnCanDeleteSelectedNodes(InArgs.OnCanDeleteSelectedNodes)
			.OnDeleteSelectedNodes(InArgs.OnDeleteSelectedNodes)
			.OnCanCutSelectedNodes(InArgs.OnCanCutSelectedNodes)
			.OnCutSelectedNodes(InArgs.OnCutSelectedNodes)
			.OnCanCopySelectedNodes(InArgs.OnCanCopySelectedNodes)
			.OnCopySelectedNodes(InArgs.OnCopySelectedNodes)
			.OnCanPasteNodes(InArgs.OnCanPasteNodes)
			.OnPasteNodes(InArgs.OnPasteNodes)
			.OnCanDuplicateSelectedNodes(InArgs.OnCanDuplicateSelectedNodes)
			.OnDuplicateSelectedNodes(InArgs.OnDuplicateSelectedNodes);
	});
	Args.OnGetTabIcon = FOnGetTabIcon::CreateLambda([](const FWorkspaceEditorContext& InContext)
	{
		return FAppStyle::Get().GetBrush(TEXT("GraphEditor.EventGraph_16x"));
	});
	Args.OnGetTabName = FOnGetTabName::CreateLambda([](const FWorkspaceEditorContext& InContext)
	{
		if(UEdGraph* Graph = Cast<UEdGraph>(InContext.Object))
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				FGraphDisplayInfo Info;
				Schema->GetGraphDisplayInformation(*Graph, /*out*/ Info);
				return Info.DisplayName;
			}
			else
			{
				// if we don't have a schema, we're dealing with a malformed (or incomplete graph)...
				// possibly in the midst of some transaction - here we return the object's outer path 
				// so we can at least get some context as to which graph we're referring
				return FText::FromString(Graph->GetPathName());
			}
		}
		return LOCTEXT("UnknownGraphName", "Unknown");
	});
	Args.OnGetDocumentState = FOnGetDocumentState::CreateLambda([](const FWorkspaceEditorContext& InContext, TSharedRef<SWidget> InWidget)
	{
		TSharedRef<SGraphDocument> GraphDocument = StaticCastSharedRef<SGraphDocument>(InWidget);
		FVector2D ViewLocation;
		float ZoomAmount;
		GraphDocument->GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);
		return TInstancedStruct<FGraphDocumentState>::Make(InContext.Object, ViewLocation, ZoomAmount);
	});
	Args.OnSetDocumentState = FOnSetDocumentState::CreateLambda([](const FWorkspaceEditorContext& InContext, TSharedRef<SWidget> InWidget, const TInstancedStruct<FWorkspaceDocumentState>& InDocumentState)
	{
		if(const FGraphDocumentState* GraphDocumentState = InDocumentState.GetPtr<FGraphDocumentState>())
		{
			TSharedRef<SGraphDocument> GraphDocument = StaticCastSharedRef<SGraphDocument>(InWidget);
			GraphDocument->GraphEditor->SetViewLocation(GraphDocumentState->ViewLocation, GraphDocumentState->ZoomAmount);
		}
	});

	return Args;
}

bool FWorkspaceEditorModule::GetExportedAssetsForWorkspace(const FAssetData& InWorkspaceAsset, FWorkspaceAssetRegistryExports& OutExports)
{
	const FString TagValue = InWorkspaceAsset.GetTagValueRef<FString>(UWorkspace::ExportsAssetRegistryTag);
	return FWorkspaceAssetRegistryExports::StaticStruct()->ImportText(*TagValue, &OutExports, nullptr, PPF_None, nullptr, FWorkspaceAssetRegistryExports::StaticStruct()->GetName()) != nullptr;
}

void FWorkspaceEditorModule::OpenWorkspaceForObject(UObject* InObject, EOpenWorkspaceMethod InOpenMethod)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> RelevantWorkspaceAssets;

	if(InOpenMethod != EOpenWorkspaceMethod::AlwaysOpenNewWorkspace)
	{
		// Look for existing workspaces that export this asset
		FARFilter ARFilter;
		ARFilter.ClassPaths.Add(UWorkspace::StaticClass()->GetClassPathName());
		ARFilter.bRecursiveClasses = true;

		TArray<FAssetData> AllWorkspaceAssets;
		AssetRegistryModule.Get().GetAssets(ARFilter, AllWorkspaceAssets);

		for(const FAssetData& WorkspaceAsset : AllWorkspaceAssets)
		{
			FWorkspaceAssetRegistryExports Exports;
			GetExportedAssetsForWorkspace(WorkspaceAsset, Exports);

			FSoftObjectPath ObjectPath(InObject);
			for(const FWorkspaceAssetRegistryExportEntry& ExportEntry : Exports.Assets)
			{
				if(ExportEntry.Asset == ObjectPath)
				{
					RelevantWorkspaceAssets.Add(WorkspaceAsset);
					break;
				}
			}
		}
	}

	FWorkspaceEditor* WorkspaceEditor = nullptr;

	auto HandleNewWorkspace = [InObject, &WorkspaceEditor]()
	{
		UWorkspaceFactory* Factory = NewObject<UWorkspaceFactory>();
		UPackage* Package = CreatePackage(nullptr);
		FName PackageName = *FPaths::GetBaseFilename(Package->GetName());
		UWorkspace* NewWorkspace = CastChecked<UWorkspace>(Factory->FactoryCreateNew(UWorkspace::StaticClass(), Package, PackageName, RF_Public | RF_Standalone, NULL, GWarn));
		NewWorkspace->AddAsset(InObject, false);
		NewWorkspace->MarkPackageDirty();
		TSharedRef<FWorkspaceEditor> Editor = MakeShared<FWorkspaceEditor>();
		Editor->InitEditor(EToolkitMode::Standalone, nullptr, NewWorkspace);

		WorkspaceEditor = &Editor.Get();
	};

	auto HandleExistingWorkspace = [InObject, &WorkspaceEditor](const FAssetData& InAssetData)
	{
		if(UWorkspace* ExistingWorkspace = Cast<UWorkspace>(InAssetData.GetAsset()))
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ExistingWorkspace);

			WorkspaceEditor = static_cast<FWorkspaceEditor*>(GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(ExistingWorkspace, true));
		}
	};

	if(InOpenMethod == EOpenWorkspaceMethod::AlwaysOpenNewWorkspace || RelevantWorkspaceAssets.Num() == 0)
	{
		// No relevant workspaces, so open a new one and add the asset
		HandleNewWorkspace();
	}
	else if(RelevantWorkspaceAssets.Num() == 1)
	{
		// One existing workspace, open it
		HandleExistingWorkspace(RelevantWorkspaceAssets[0]);
	}
	else
	{
		// Multiple existing workspaces, present a window to let the user choose one to open with
		TSharedRef<SWorkspacePicker> WorkspacePicker = SNew(SWorkspacePicker)
			.WorkspaceAssets(RelevantWorkspaceAssets)
			.OnAssetSelected_Lambda(HandleExistingWorkspace)
			.OnNewAsset_Lambda(HandleNewWorkspace);

		WorkspacePicker->ShowModal();
	}
	
	if(WorkspaceEditor)
	{
		WorkspaceEditor->OpenAssets({InObject});
	}
}

const FObjectDocumentArgs* FWorkspaceEditorModule::FindObjectDocumentType(const FTopLevelAssetPath& InClassPath) const
{
	return ObjectDocumentArgs.Find(InClassPath);
}

TArray<FTopLevelAssetPath> FWorkspaceEditorModule::GetAllowedObjectTypesForArea(FName InSpawnLocation) const
{
	return DocumentAreaMap.FindRef(InSpawnLocation).Array();
}

}

IMPLEMENT_MODULE(UE::Workspace::FWorkspaceEditorModule, WorkspaceEditor);

#undef LOCTEXT_NAMESPACE