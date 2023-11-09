// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModularRigHierarchyCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigModularRigHierarchyCommands"

void FControlRigModularHierarchyCommands::RegisterCommands()
{
	UI_COMMAND(AddModuleItem, "New Module", "Add new module to the rig.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
