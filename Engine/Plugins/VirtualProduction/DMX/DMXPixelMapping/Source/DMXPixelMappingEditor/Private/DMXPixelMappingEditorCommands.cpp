// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingEditorCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingEditorCommands"

FDMXPixelMappingEditorCommands::FDMXPixelMappingEditorCommands()
	: TCommands<FDMXPixelMappingEditorCommands>
	(
		TEXT("DMXPixelMappingEditor"),
		NSLOCTEXT("Contexts", "DMXPixelMappingEditor", "DMX Pixel Mapping"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{}

void FDMXPixelMappingEditorCommands::RegisterCommands()
{
	UI_COMMAND(AddMapping, "Add Source", "Adds a new Source Texure, Material or User Widget to the Pixel Map asset.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PlayDMX, "Plays DMX", "Plays DMX", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PauseDMX, "Pause DMX", "Pauses playing DMX. DMX values will still be sent, at a lower rate.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResumeDMX, "Resume DMX", "Resumes playing DMX after being paused. ", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopDMX, "Stop DMX", "Stops playing DMX.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TogglePlayPauseDMX, "Toggle Play/Pause DMX", "Toggles between playing and pausing DMX", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::SpaceBar));
	UI_COMMAND(TogglePlayStopDMX, "Toggle Play/Stop DMX", "Toggles between playing and stopping DMX", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));

	UI_COMMAND(EditorStopSendsZeroValues, "Stop sends Zero Values", "When stop is clicked in the editor, zero values are sent to all patches in use", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(EditorStopSendsDefaultValues, "Stop sends Default Values", "When stop is clicked in the editor, default values are sent to all patches in use", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(EditorStopKeepsLastValues, "Stop keeps last Values", "When stop is clicked in the editor, the last pixel mapped values are kept", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(EnableResizeMode, "Resize Mode", "Resizes components when transform handles are being dragged", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(EnableRotateMode, "Rotate Mode", "Rotates components when transform handles are being dragged", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(ToggleGridSnapping, "Toggle Grid Snapping", "Enables/disables grid snapping", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(SizeComponentToTexture, "Size Component to Texture", "Sizes the selected group to Texture.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::S));
	
	UI_COMMAND(ToggleScaleChildrenWithParent, "Scale Children with Parent", "Sets if children are scaled with their parent, when the parent is resized.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::Q));
	UI_COMMAND(ToggleAlwaysSelectGroup, "Always select Group", "Sets if the parent is selected, if a child is clicked.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::W));
	UI_COMMAND(ToggleApplyLayoutScriptWhenLoaded, "Apply Layout Script instantly", "Sets if layout script are applied as soon as they are loaded.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::E));
	UI_COMMAND(ToggleShowMatrixCells, "Show Matrix Cells", "Sets if matrix cells are displayed. Can be turned off for better editor performance when pixel mapping large quantities of fixtures.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::A));
	UI_COMMAND(ToggleShowComponentNames, "Show Component Names", "Sets if the name of components are displayed. Can be turned off for better editor performance when pixel mapping large quantities of fixtures.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::S));
	UI_COMMAND(ToggleShowPatchInfo, "Show Patch Info", "Sets if information about the pach is displayed. Can be turned off for better editor performance when pixel mapping large quantities of fixtures.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::D));
	UI_COMMAND(ToggleShowCellIDs, "Show Cell IDs", "Sets if the cell IDs of matrix cells are displayed. Can be turned off for better editor performance when pixel mapping large quantities of fixtures.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::F));
	UI_COMMAND(ToggleShowPivot, "Show Pivot", "Sets if the pivot is displayed for selected components.", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE 
