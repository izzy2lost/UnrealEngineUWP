// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

struct FPlacementCategoryInfo;

/**
* Creates  the commands for placement mode, creating one command to load each  placement mode category 
 */
class  FPlacementModeCommands : public TCommands<FPlacementModeCommands>
{
public:
	/** default constructor  */
	FPlacementModeCommands();

	/**
	 * Registers the commands for placement mode, creating one command to load each
	 * placement mode category 
	 */
	virtual void RegisterCommands() override;

	/**
	 * returns a static instance of the placement mode commands
	 */
	static const FPlacementModeCommands& Get();

	/**
	 * Returns an array containing the commands that load the categories of things to be placed in the Placement Panel
	 */
	const TArray<TSharedPtr<FUICommandInfo>>& GetPlacementToolkitCategoryCommands() const;

private:
	/**
	 * An array containing the commands that load the categories of things to be placed in the Placement Panel
	 */
	TArray<TSharedPtr<FUICommandInfo>> ToolkitCategoryCommands;
};

