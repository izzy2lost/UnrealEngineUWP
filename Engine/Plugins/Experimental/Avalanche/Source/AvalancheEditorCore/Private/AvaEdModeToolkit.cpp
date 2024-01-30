// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEdModeToolkit.h"
#include "AvaEdMode.h"
#include "IAvaEditor.h"
#include "IAvaEditorCoreModule.h"

#define LOCTEXT_NAMESPACE "AvaEdModeToolkit"

namespace UE::AvalancheEditorCore::Private
{
	FName ToolkitName = "AvaEdModeToolkit";
	FText ToolkitBaseName = LOCTEXT("ToolkitName", "Motion Design Editor Toolkit");
}

FAvaEdModeToolkit::FAvaEdModeToolkit(UAvaEdMode* InEdMode)
	: EdModeWeak(InEdMode)
{
}

FName FAvaEdModeToolkit::GetToolkitFName() const
{
	return UE::AvalancheEditorCore::Private::ToolkitName;
}

FText FAvaEdModeToolkit::GetBaseToolkitName() const
{
	return UE::AvalancheEditorCore::Private::ToolkitBaseName;
}

void FAvaEdModeToolkit::ExtendSecondaryModeToolbar(UToolMenu* InToolbarMenu)
{
	if (!InToolbarMenu)
	{
		return;
	}

	UAvaEdMode* EdMode = EdModeWeak.Get();
	if (!EdMode)
	{
		return;
	}

	if (TSharedPtr<IAvaEditor> Editor = EdMode->GetEditor())
	{
		Editor->ExtendToolbarMenu(InToolbarMenu);
	}
	IAvaEditorCoreModule::Get().GetOnExtendEditorToolbar().Broadcast(*InToolbarMenu);
}

#undef LOCTEXT_NAMESPACE
