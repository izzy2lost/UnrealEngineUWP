// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModeToolkit.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "InteractiveToolManager.h"
#include "MeshAttributePaintTool.h"

#define LOCTEXT_NAMESPACE "FDataflowEditorModeToolkit"

void FDataflowEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FBaseCharacterFXEditorModeToolkit::OnToolStarted(Manager, Tool);
}

void FDataflowEditorModeToolkit::BuildToolPalette(FName PaletteIndex, class FToolBarBuilder& ToolbarBuilder)
{
	const FDataflowEditorCommandsImpl& Commands = FDataflowEditorCommands::Get();
	if (PaletteIndex == ToolsTabName)
	{
		// @todo(DynamicMeshRendering) : Enable Dynamic Mesh Rendering for dataflow terminals. 
		//ToolbarBuilder.AddToolBarButton(Commands.BeginAttributeEditorTool);
		//ToolbarBuilder.AddToolBarButton(Commands.BeginMeshSelectionTool);
	}
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
