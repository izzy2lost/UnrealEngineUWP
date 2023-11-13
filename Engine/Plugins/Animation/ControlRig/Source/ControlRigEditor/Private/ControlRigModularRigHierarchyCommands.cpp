// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModularRigHierarchyCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigModularRigHierarchyCommands"

void FControlRigModularHierarchyCommands::RegisterCommands()
{
	UI_COMMAND(AddModuleItem, "New Module", "Add new module to the rig.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RenameModuleItem, "Rename", "Rename module to the rig.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));
	UI_COMMAND(DeleteModuleItem, "Delete", "Delete module from the rig.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
}

#undef LOCTEXT_NAMESPACE
