// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "MixerInsightEditorStyle.h"

class FMixerInsightEditorCommands : public TCommands<FMixerInsightEditorCommands>
{
public:

	FMixerInsightEditorCommands()
		: TCommands<FMixerInsightEditorCommands>(TEXT("MixerInsightEditor"), NSLOCTEXT("Contexts", "MixerInsightEditor", "MixerInsightEditor Plugin"), NAME_None, FMixerInsightEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

// public:
	// TSharedPtr< FUICommandInfo > OpenPluginWindow;
};
