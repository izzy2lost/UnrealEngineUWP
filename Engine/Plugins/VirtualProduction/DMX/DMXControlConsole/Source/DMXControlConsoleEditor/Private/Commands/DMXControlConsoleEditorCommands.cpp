// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorCommands.h"

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorCommands"

FDMXControlConsoleEditorCommands::FDMXControlConsoleEditorCommands()
	: TCommands<FDMXControlConsoleEditorCommands>
	(
		TEXT("DMXControlConsoleEditor"),
		LOCTEXT("DMXControlConsoleEditor", "DMX DMX Control Console"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()
	)
{}

void FDMXControlConsoleEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenControlConsole, "Open Control Console", "Opens the DMX Control Console", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ToggleSendDMX, "Toggle Send DMX", "Starts/Stops sending DMX.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::N));
	UI_COMMAND(RemoveElements, "Remove Elements", "Removes the selected elements from the current Control Console", EUserInterfaceActionType::None, FInputChord(EKeys::Delete));
	UI_COMMAND(SelectAll, "Select All", "Selects all the visible elements in the Control Console", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));
	UI_COMMAND(ClearAll, "Clear All", "Clears the entire console", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetToDefault, "Reset to Default", "Resets all the elements in the Control Console to their default values", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetToZero, "Reset to Zero", "Resets all the elements in the Control Console to zero", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(Mute, "Mute", "Mutes the selected Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MuteAll, "Mute All", "Mutes all the Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Unmute, "Unmute", "Unmutes the selected Fader Groups.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UnmuteAll, "Unmute All", "Mutes all the Fader Groups.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(AddPatchNext, "Add Patches to the right", "Adds the selected Fixture Patches to the right on the same row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPatchNextRow, "Add Patches on new row", "Adds the selected Fixture Patches to the next row.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPatchToSelection, "Set Patch", "Uses the selected Fixture Patch in the selected Fader Group. Clears the previous patch.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE 
