// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigCommands.h"

#define LOCTEXT_NAMESPACE "IKRigCommands"

void FIKRigCommands::RegisterCommands()
{
	UI_COMMAND(Reset, "Reset", "Reset state of the rig and goals to initial pose.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GenerateRetargetChains, "Auto Retarget Chains", "Analyse the skeleton and automatically split it into retarget chains.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowAssetSettings, "Asset Settings", "Show the settings for this IK Rig asset.", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
