// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ControlRigEditorStyle.h"

class FControlRigModularHierarchyCommands : public TCommands<FControlRigModularHierarchyCommands>
{
public:
	FControlRigModularHierarchyCommands() : TCommands<FControlRigModularHierarchyCommands>
	(
		"ControlRigModuleRigHierarchy",
		NSLOCTEXT("Contexts", "ModuleRigHierarchy", "Module Rig Hierarchy"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FControlRigEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Add Module at root */
	TSharedPtr< FUICommandInfo > AddModuleItem;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
