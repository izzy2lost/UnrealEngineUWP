
// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlacementModeCommands.h"
#include "IPlacementModeModule.h"
#include "Framework/Commands/UICommandInfo.h"

#define LOCTEXT_NAMESPACE "PlacementModeCommands"

FPlacementModeCommands::FPlacementModeCommands()
	: TCommands<FPlacementModeCommands>(
		TEXT("PlacementModeCommands"), 
		LOCTEXT( "PlacementModeCommands", "Placement Mode"), 
		NAME_None, 
		"FPlacementModeCommandsStyle")
{}

void FPlacementModeCommands::RegisterCommands()
{
	TArray<FPlacementCategoryInfo> Categories;
	IPlacementModeModule::Get().GetSortedCategories(Categories);
	
	for (FPlacementCategoryInfo& Category : Categories)		
	{
		TSharedPtr<FUICommandInfo> Command;

		FUICommandInfo::MakeCommandInfo(
			this->AsShared(),
			Command,
			Category.UniqueHandle,
			Category.DisplayName,
			Category.DisplayName,
			Category.DisplayIcon,
			EUserInterfaceActionType::ToggleButton,
			FInputChord());
			ToolkitCategoryCommands.Add( Command );
	}
}

const FPlacementModeCommands& FPlacementModeCommands::Get()
{
	return TCommands<FPlacementModeCommands>::Get();
}

const TArray<TSharedPtr<FUICommandInfo>>& FPlacementModeCommands::GetPlacementToolkitCategoryCommands() const
{
	return ToolkitCategoryCommands;
}

#undef LOCTEXT_NAMESPACE
