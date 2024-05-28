// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleAssetDefinition.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "EditorCVars.h"
#include "IWorkspaceEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Workspace/AnimNextWorkspaceFactory.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

EAssetCommandResult UAssetDefinition_AnimNextSchedule::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::AnimNext::Editor;
	using namespace UE::Workspace;

	for(UObject* Asset : OpenArgs.LoadObjects<UObject>())
	{
		if(CVars::GUseWorkspaceEditor.GetValueOnGameThread())
		{
			IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<IWorkspaceEditorModule>("WorkspaceEditor");
			WorkspaceEditorModule.OpenWorkspaceForObject(Asset, EOpenWorkspaceMethod::Default, UAnimNextWorkspaceFactory::StaticClass());
		}
		else
		{
			FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, Asset);
		}
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE