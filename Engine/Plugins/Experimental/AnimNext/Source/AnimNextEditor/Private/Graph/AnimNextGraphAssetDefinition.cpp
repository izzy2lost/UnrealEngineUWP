// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraphAssetDefinition.h"
#include "AnimNextGraphEditor.h"
#include "ContentBrowserMenuContexts.h"
#include "EditorCVars.h"
#include "FileHelpers.h"
#include "IWorkspaceEditorModule.h"
#include "ToolMenus.h"
#include "Workspace/AnimNextWorkspaceFactory.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_AnimNextGraph"

EAssetCommandResult UAssetDefinition_AnimNextGraph::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::AnimNext::Editor;
	using namespace UE::Workspace;

	for (UAnimNextGraph* Asset : OpenArgs.LoadObjects<UAnimNextGraph>())
	{
		if(CVars::GUseWorkspaceEditor.GetValueOnGameThread())
		{
			IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<IWorkspaceEditorModule>("WorkspaceEditor");
			WorkspaceEditorModule.OpenWorkspaceForObject(Asset, EOpenWorkspaceMethod::Default, UAnimNextWorkspaceFactory::StaticClass());
		}
		else
		{
			TSharedRef<FGraphEditor> GraphEditor = MakeShared<FGraphEditor>();
			GraphEditor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
		}
	}

	return EAssetCommandResult::Handled;
}


FText UAssetDefinition_AnimNextParameter::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

FText UAssetDefinition_AnimNextAnimationGraph::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

FText UAssetDefinition_AnimNextEventGraph::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

namespace UE::AnimNext::Editor
{

static FDelayedAutoRegisterHelper AutoRegisterAssetMenuItems(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UAnimNextGraph::StaticClass());
		 
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			InSection.AddMenuEntry(
				"ForceSave",
				LOCTEXT("ForceSaveLabel", "Force Save"),
				LOCTEXT("ForceSaveTooltip", "Force the save of this item and all its subobjects."),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save"),
				FToolUIAction(FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
				{
					if(UContentBrowserAssetContextMenuContext* AssetContext = InContext.FindContext<UContentBrowserAssetContextMenuContext>())
					{
						TArray<UPackage*> PackagesToSave;
						for (const FAssetData& AssetData : AssetContext->SelectedAssets)
						{
							if (UPackage* Package = AssetData.GetPackage())
							{
								PackagesToSave.Add(Package);
								PackagesToSave.Append(Package->GetExternalPackages());
							}
						}

						if(PackagesToSave.Num() > 0)
						{
							FEditorFileUtils::FPromptForCheckoutAndSaveParams SaveParams;
							SaveParams.bCheckDirty = false;
							SaveParams.bPromptToSave = false;
							SaveParams.bIsExplicitSave = true;

							FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, SaveParams);
						}
					}
				})));
		}));
	}));
});

}

#undef LOCTEXT_NAMESPACE