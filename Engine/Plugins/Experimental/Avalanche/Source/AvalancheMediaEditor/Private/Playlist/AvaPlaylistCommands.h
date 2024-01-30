// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaEditorStyle.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FAvaPlaylistCommands : public TCommands<FAvaPlaylistCommands>
{
public:
	
	FAvaPlaylistCommands()
		: TCommands<FAvaPlaylistCommands>(TEXT("AvaPlaylistCommands")
		, NSLOCTEXT("AvaPlaylistCommands", "AvaPlaylistCommands", "Motion Design Playlist")
		, NAME_None
		, FAvaMediaEditorStyle::GetStyleSetName())
	{
	}

	/** Initialize commands */
	virtual void RegisterCommands() override;

	/** Page List Commands */
	TSharedPtr<FUICommandInfo> AddTemplate;
	TSharedPtr<FUICommandInfo> CreatePageInstanceFromTemplate;
	TSharedPtr<FUICommandInfo> CreateComboTemplate;
	TSharedPtr<FUICommandInfo> RemovePage;
	TSharedPtr<FUICommandInfo> RenumberPage;
	TSharedPtr<FUICommandInfo> ReimportPage;
	TSharedPtr<FUICommandInfo> EditPageSource;
	TSharedPtr<FUICommandInfo> ExportPagesToPlaylist;
	TSharedPtr<FUICommandInfo> ExportPagesToJson;
	TSharedPtr<FUICommandInfo> ExportPagesToXml;

	/** Show Control / Playlist Commands */
	TSharedPtr<FUICommandInfo> Play;
	TSharedPtr<FUICommandInfo> UpdateValues;
	TSharedPtr<FUICommandInfo> Stop;
	TSharedPtr<FUICommandInfo> ForceStop;
	TSharedPtr<FUICommandInfo> Continue;
	TSharedPtr<FUICommandInfo> PlayNext;

	/** Preview Control / Playlist Commands */
	TSharedPtr<FUICommandInfo> PreviewFrame;
	TSharedPtr<FUICommandInfo> PreviewPlay;
	TSharedPtr<FUICommandInfo> PreviewStop;
	TSharedPtr<FUICommandInfo> PreviewForceStop;
	TSharedPtr<FUICommandInfo> PreviewContinue;
	TSharedPtr<FUICommandInfo> PreviewPlayNext;
	TSharedPtr<FUICommandInfo> TakeToProgram;
};
