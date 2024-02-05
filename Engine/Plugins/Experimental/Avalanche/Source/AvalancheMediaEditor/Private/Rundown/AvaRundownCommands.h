// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaEditorStyle.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "AvaRundownCommands"

class FAvaRundownCommands : public TCommands<FAvaRundownCommands>
{
public:
	
	FAvaRundownCommands()
		: TCommands<FAvaRundownCommands>(TEXT("AvaRundownCommands")
		, LOCTEXT("AvaRundownCommands", "Motion Design Rundown")
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
	TSharedPtr<FUICommandInfo> ExportPagesToRundown;
	TSharedPtr<FUICommandInfo> ExportPagesToJson;
	TSharedPtr<FUICommandInfo> ExportPagesToXml;

	/** Show Control / Rundown Commands */
	TSharedPtr<FUICommandInfo> Play;
	TSharedPtr<FUICommandInfo> UpdateValues;
	TSharedPtr<FUICommandInfo> Stop;
	TSharedPtr<FUICommandInfo> ForceStop;
	TSharedPtr<FUICommandInfo> Continue;
	TSharedPtr<FUICommandInfo> PlayNext;

	/** Preview Control / Rundown Commands */
	TSharedPtr<FUICommandInfo> PreviewFrame;
	TSharedPtr<FUICommandInfo> PreviewPlay;
	TSharedPtr<FUICommandInfo> PreviewStop;
	TSharedPtr<FUICommandInfo> PreviewForceStop;
	TSharedPtr<FUICommandInfo> PreviewContinue;
	TSharedPtr<FUICommandInfo> PreviewPlayNext;
	TSharedPtr<FUICommandInfo> TakeToProgram;
};

#undef LOCTEXT_NAMESPACE
