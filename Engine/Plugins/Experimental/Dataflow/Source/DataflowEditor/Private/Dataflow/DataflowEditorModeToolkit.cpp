// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModeToolkit.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "FDataflowEditorModeToolkit"

void FDataflowEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	ensure(Tool == Manager->GetActiveTool(EToolSide::Left));

	ActiveToolName = Tool->GetToolInfo().ToolDisplayName;
	FString ActiveToolIdentifier = Manager->GetActiveToolName(EToolSide::Mouse);
	ActiveToolIdentifier.InsertAt(0, ".");
	ActiveToolIcon = GetActiveToolIcon(ActiveToolIdentifier);
}

FName FDataflowEditorModeToolkit::GetToolkitFName() const
{
	return FName("DataflowEditorMode");
}

FText FDataflowEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("DataflowEditorModeToolkit", "DisplayName", "DataflowEditorMode");
}

const FSlateBrush* FDataflowEditorModeToolkit::GetActiveToolIcon(const FString& ActiveToolIdentifier) const
{
	FName ActiveToolIconName = ISlateStyle::Join(FDataflowEditorCommandsImpl::Get().GetContextName(), TCHAR_TO_ANSI(*ActiveToolIdentifier));
	return FDataflowEditorStyle::Get().GetOptionalBrush(ActiveToolIconName);
}

#undef LOCTEXT_NAMESPACE
