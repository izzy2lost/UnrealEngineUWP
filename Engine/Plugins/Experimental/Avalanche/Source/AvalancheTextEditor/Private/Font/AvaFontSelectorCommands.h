// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FAvaFontSelectorCommands
	: public TCommands<FAvaFontSelectorCommands>
{
public:
	FAvaFontSelectorCommands()
		: TCommands<FAvaFontSelectorCommands>(TEXT("AvalancheFontSelector")
		, NSLOCTEXT("AvaFontSelectorCommands", "AvalancheFontSelector", "Motion Design Font Selector")
		, NAME_None
		, FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> ShowMonospacedFonts;
	TSharedPtr<FUICommandInfo> ShowBoldFonts;
	TSharedPtr<FUICommandInfo> ShowItalicFonts;
};