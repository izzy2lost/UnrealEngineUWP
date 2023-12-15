// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModeToolkit.h"

/**
 * The dataflow editor mode toolkit is responsible for the panel on the side in the dataflow editor
 * that shows mode and tool properties. Tool buttons would go in Init().
 * Note: When there are separate viewports/worlds/modemanagers/toolscontexts, this ModeToolkit will track which
 * one is currently active.
 */

class DATAFLOWEDITOR_API FDataflowEditorModeToolkit : public FBaseCharacterFXEditorModeToolkit
{
public:

	// FModeToolkit interface
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder) override;

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	// FBaseCharacterFXEditorModeToolkit interface
	virtual const FSlateBrush* GetActiveToolIcon(const FString& Identifier) const override;
};
