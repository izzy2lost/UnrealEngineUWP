// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Framework/Commands/Commands.h"

/**
 * Defines commands for the pixel mapping editor.
 */
class FDMXPixelMappingEditorCommands 
	: public TCommands<FDMXPixelMappingEditorCommands>
{
public:
	FDMXPixelMappingEditorCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> AddMapping;
	TSharedPtr<FUICommandInfo> PlayDMX;
	TSharedPtr<FUICommandInfo> StopPlayingDMX;

	// Designer related
	TSharedPtr<FUICommandInfo> EnableResizeMode;
	TSharedPtr<FUICommandInfo> EnableRotateMode;

	TSharedPtr<FUICommandInfo> ToggleGridSnapping;

	TSharedPtr<FUICommandInfo> SizeComponentToTexture;

	TSharedPtr<FUICommandInfo> ToggleScaleChildrenWithParent;
	TSharedPtr<FUICommandInfo> ToggleAlwaysSelectGroup;
	TSharedPtr<FUICommandInfo> ToggleApplyLayoutScriptWhenLoaded;
	TSharedPtr<FUICommandInfo> ToggleShowMatrixCells;
	TSharedPtr<FUICommandInfo> ToggleShowComponentNames;
	TSharedPtr<FUICommandInfo> ToggleShowPatchInfo;
	TSharedPtr<FUICommandInfo> ToggleShowCellIDs;
	TSharedPtr<FUICommandInfo> ToggleShowPivot;
};
