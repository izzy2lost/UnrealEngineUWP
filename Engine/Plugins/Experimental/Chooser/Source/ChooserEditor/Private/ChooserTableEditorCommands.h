// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ChooserTableEditorCommands"
	
class FChooserTableEditorCommands : public TCommands<FChooserTableEditorCommands>
{
public:
	/** Constructor */
	FChooserTableEditorCommands() 
		: TCommands<FChooserTableEditorCommands>("ChooserTableEditor", NSLOCTEXT("Contexts", "ChooserTableEditor", "Chooser Table Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo> EditChooserSettings;
	TSharedPtr<FUICommandInfo> AutoPopulateAll;
	TSharedPtr<FUICommandInfo> AutoPopulateSelection;
	TSharedPtr<FUICommandInfo> RemoveDisabledData;
	TSharedPtr<FUICommandInfo> Delete;
	TSharedPtr<FUICommandInfo> Disable;

	/** Initialize commands */
	virtual void RegisterCommands() override
	{
		UI_COMMAND(EditChooserSettings, "Table Settings", "Edit the root properties of the ChooserTable asset.", EUserInterfaceActionType::Button, FInputChord())
		UI_COMMAND(AutoPopulateAll, "AutoPopulate All", "Auto Populate cell data for all supported columns", EUserInterfaceActionType::Button, FInputChord())
		UI_COMMAND(AutoPopulateSelection, "AutoPopulate", "Auto Populate cell data for selected rows for all supported columns", EUserInterfaceActionType::Button, FInputChord())
		UI_COMMAND(RemoveDisabledData, "Remove Disabled Data", "Delete all data that's marked as disabled.", EUserInterfaceActionType::Button, FInputChord())
		UI_COMMAND(Delete, "Delete", "Delete the selected Rows or Column.", EUserInterfaceActionType::Button, FInputChord())
		UI_COMMAND(Disable, "Disable", "Disable the selected Rows or Column.", EUserInterfaceActionType::Check, FInputChord())
	}
};
	
#undef LOCTEXT_NAMESPACE
