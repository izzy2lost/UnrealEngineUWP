// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMaskEditorStyle.h"
#include "Framework/Commands/Commands.h"

#define LOCTEXT_NAMESPACE "AvaMaskEditorCommands"

class FAvaMaskEditorCommands : public TCommands<FAvaMaskEditorCommands>
{
public:
	FAvaMaskEditorCommands()
		: TCommands<FAvaMaskEditorCommands>(
			TEXT("AvalancheMaskEditor"),
			LOCTEXT("MotionDesignMaskEditor", "Motion Design Masking"),
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

#undef LOCTEXT_NAMESPACE
