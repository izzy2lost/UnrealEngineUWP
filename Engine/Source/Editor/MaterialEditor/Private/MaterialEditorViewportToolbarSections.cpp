// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorViewportToolbarSections.h"
#include "EditorViewportCommands.h"
#include "MaterialEditorActions.h"
#include "SEditorViewport.h"
#include "SMaterialEditorViewport.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "MaterialEditorViewportToolbarSections"

TSharedRef<SWidget> UE::MaterialEditor::CreateShowMenuWidget(const TSharedRef<SMaterialEditor3DPreviewViewport>& InMaterialEditorViewport, bool bInShowViewportStatsToggle
)
{
	InMaterialEditorViewport->OnFloatingButtonClicked();

	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, InMaterialEditorViewport->GetCommandList());
	{
		FMaterialEditorCommands Commands = FMaterialEditorCommands::Get();

		if (bInShowViewportStatsToggle)
		{
			ShowMenuBuilder.AddMenuEntry(
				FEditorViewportCommands::Get().ToggleStats, "ViewportStats", LOCTEXT("ViewportStatsLabel", "Viewport Stats")
			);

			ShowMenuBuilder.AddMenuSeparator();
		}

		ShowMenuBuilder.AddMenuEntry(Commands.ToggleMaterialStats);

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(Commands.TogglePreviewBackground);
	}

	return ShowMenuBuilder.MakeWidget();
}

FToolMenuEntry UE::MaterialEditor::CreateShowSubmenu(TWeakPtr<SMaterialEditor3DPreviewViewport> InViewport)
{
	return FToolMenuEntry::InitSubMenu(
		"Show",
		LOCTEXT("ShowSubmenuLabel", "Show"),
		LOCTEXT("ShowSubmenuTooltip", "Show options"),
		FNewToolMenuDelegate::CreateLambda(
			[InViewport](UToolMenu* Submenu) -> void
			{
				TSharedPtr<SMaterialEditor3DPreviewViewport> Viewport = InViewport.Pin();
				if (!Viewport)
				{
					return;
				}

				FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection("", LOCTEXT("UnnamedLabel", ""));

				UnnamedSection.AddEntry(FToolMenuEntry::InitWidget(
					"ShowMenuItems", UE::MaterialEditor::CreateShowMenuWidget(Viewport.ToSharedRef()), FText(), true
				));
			}
		)
	);
}

#undef LOCTEXT_NAMESPACE
