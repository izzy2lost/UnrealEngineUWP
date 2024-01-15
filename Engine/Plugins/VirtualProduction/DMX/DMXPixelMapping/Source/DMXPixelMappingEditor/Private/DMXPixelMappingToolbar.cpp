// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingToolbar.h"

#include "DMXPixelMappingEditorCommands.h"
#include "DMXPixelMappingEditorStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Styling/AppStyle.h"
#include "Toolkits/DMXPixelMappingToolkit.h"


#define LOCTEXT_NAMESPACE "FDMXPixelMappingToolbar"

FDMXPixelMappingToolbar::FDMXPixelMappingToolbar(TSharedPtr<FDMXPixelMappingToolkit> InToolkit)
	: ToolkitWeakPtr(InToolkit)
{}

void FDMXPixelMappingToolbar::BuildToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		Toolkit->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FDMXPixelMappingToolbar::BuildToolbarCallback)
	);
}

void FDMXPixelMappingToolbar::BuildToolbarCallback(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Renderers");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().AddMapping,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::Get().GetStyleSetName(), "Icons.AddSource"),
			FName(TEXT("Add Source")));
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("PlayAndStopDMX");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().PlayDMX,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::Get().GetStyleSetName(), "Icons.PlayDMX"),
			FName(TEXT("Play DMX")));

		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().StopPlayingDMX,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::Get().GetStyleSetName(), "Icons.StopPlayingDMX"),
			FName(TEXT("Stop Playing DMX")));
	}
	ToolbarBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE
