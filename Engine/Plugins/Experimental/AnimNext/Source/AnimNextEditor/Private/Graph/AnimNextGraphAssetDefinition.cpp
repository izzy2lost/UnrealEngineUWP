// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraphAssetDefinition.h"
#include "AnimNextGraphEditor.h"
#include "EditorCVars.h"
#include "IWorkspaceEditorModule.h"
#include "Workspace/AnimNextWorkspaceFactory.h"

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