// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMaskEditorStyle.h"
#include "Framework/Commands/Commands.h"

class FAvaMaskEditorCommands : public TCommands<FAvaMaskEditorCommands>
{
public:
	FAvaMaskEditorCommands()
		: TCommands<FAvaMaskEditorCommands>(
			TEXT("AvalancheMaskEditor"),
			NSLOCTEXT("Contexts", "AvalancheMaskEditor", "Motion Design Masking"),
			NAME_None,
			FAvalancheMaskEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> ToggleMaskMode;
	TSharedPtr<FUICommandInfo> ToggleShowAllMasks;
	TSharedPtr<FUICommandInfo> ToggleIsolateMask;
	TSharedPtr<FUICommandInfo> ToggleEnableMask;
};
