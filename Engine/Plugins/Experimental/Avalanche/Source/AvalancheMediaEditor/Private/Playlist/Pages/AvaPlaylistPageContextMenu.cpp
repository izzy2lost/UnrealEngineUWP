// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistPageContextMenu.h"
#include "Framework/Commands/GenericCommands.h"
#include "IAvaMediaEditorModule.h"
#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Pages/AvaPlaylistPageContext.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistPageContextMenu"

TSharedRef<SWidget> FAvaPlaylistPageContextMenu::GeneratePageContextMenuWidget(const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditorWeak, const FAvaPageListReference& InPageListReference, const TSharedPtr<FUICommandList>& InCommandList)
{
	UToolMenus* const ToolMenus = UToolMenus::Get();
	check(ToolMenus);

	const FName PageContextMenuName = IAvaMediaEditorModule::GetPlaylistPageMenuName();

	if (!ToolMenus->IsMenuRegistered(PageContextMenuName))
	{
		UToolMenu* const ContextMenu = ToolMenus->RegisterMenu(PageContextMenuName, NAME_None, EMultiBoxType::Menu);
		check(ContextMenu);

		ContextMenu->AddDynamicSection("PopulateContextMenu", FNewToolMenuDelegate::CreateStatic(
			[](UToolMenu* InMenu)
			{
				if (!InMenu)
				{
					return;
				}
				if (UAvaPlaylistPageContext* PageContext = InMenu->FindContext<UAvaPlaylistPageContext>())
				{
					if (TSharedPtr<FAvaPlaylistPageContextMenu> SourceContextMenu = PageContext->ContextMenuWeak.Pin())
					{
						SourceContextMenu->PopulatePageContextMenu(*InMenu, *PageContext);
					}
				}
			}));
	}

	UAvaPlaylistPageContext* const ContextObject = NewObject<UAvaPlaylistPageContext>();
	check(ContextObject);
	ContextObject->ContextMenuWeak    = SharedThis(this);
	ContextObject->PlaylistEditorWeak = InPlaylistEditorWeak;
	ContextObject->PageListReference  = InPageListReference;

	TSharedPtr<FExtender> Extender;

	// Compatibility with IAvaMediaEditorModule Playlist Menu Extensibility Manager
	if (InPageListReference.Type == EAvaPageListType::Template && InCommandList.IsValid())
	{
		TSharedPtr<FExtensibilityManager> MenuExtensibility = IAvaMediaEditorModule::Get().GetPlaylistMenuExtensibilityManager();

		TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = InPlaylistEditorWeak.Pin();

		if (MenuExtensibility.IsValid() && PlaylistEditor.IsValid())
		{
			if (const TArray<UObject*>* EditingObjects = PlaylistEditor->GetObjectsCurrentlyBeingEdited())
			{
				Extender = MenuExtensibility->GetAllExtenders(InCommandList.ToSharedRef(), *EditingObjects);
			}
		}
	}

	FToolMenuContext Context(InCommandList, Extender, ContextObject);
	return ToolMenus->GenerateWidget(PageContextMenuName, Context);
}

void FAvaPlaylistPageContextMenu::PopulatePageContextMenu(UToolMenu& InMenu, UAvaPlaylistPageContext& InContext)
{
	TSharedPtr<FExtensibilityManager> MenuExtensibility;

	const FGenericCommands& GenericCommands = FGenericCommands::Get();

	const FAvaPlaylistCommands& PlaylistCommands = FAvaPlaylistCommands::Get();

	const FAvaPageListReference& PageListReference = InContext.GetPageListReference();

	// Page List Operations
	{
		FToolMenuSection& Section = InMenu.FindOrAddSection(TEXT("PageListOperations"), LOCTEXT("PlayListOperationsHeader", "Page List Actions"));

		if (PageListReference.Type == EAvaPageListType::Template)
		{
			Section.AddMenuEntry(PlaylistCommands.AddTemplate);
			Section.AddMenuEntry(PlaylistCommands.CreatePageInstanceFromTemplate);
			Section.AddMenuEntry(PlaylistCommands.CreateComboTemplate);
		}

		Section.AddMenuEntry(PlaylistCommands.RemovePage);
		Section.AddMenuEntry(PlaylistCommands.RenumberPage);
		Section.AddMenuEntry(GenericCommands.Rename);
		Section.AddMenuEntry(GenericCommands.Cut);
		Section.AddMenuEntry(GenericCommands.Copy);
		Section.AddMenuEntry(GenericCommands.Paste);
		Section.AddMenuEntry(GenericCommands.Duplicate);
		Section.AddMenuEntry(PlaylistCommands.ReimportPage);
		Section.AddMenuEntry(PlaylistCommands.EditPageSource);

		if (PageListReference.Type == EAvaPageListType::Template || PageListReference.Type == EAvaPageListType::Instance)
		{
			Section.AddSubMenu(TEXT("ExportPages"),
				LOCTEXT("ExportPagesSubMenu", "Export Pages ..."),
				LOCTEXT("ExportPagesSubMenuTooltip", "Export selected pages to a separate resource."),
				FNewMenuDelegate::CreateLambda([&PlaylistCommands](FMenuBuilder& InMenuBuilder)
				{
					// When exporting instanced pages, corresponding templates follow along.
					InMenuBuilder.AddMenuEntry(PlaylistCommands.ExportPagesToPlaylist);
					InMenuBuilder.AddMenuEntry(PlaylistCommands.ExportPagesToJson);
					InMenuBuilder.AddMenuEntry(PlaylistCommands.ExportPagesToXml);
				}),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.SaveAll")
			);
		}
	}

	// Show Control Actions
	if (PageListReference.Type != EAvaPageListType::Template)
	{
		FToolMenuSection& Section = InMenu.FindOrAddSection(TEXT("ShowControlOperations"), LOCTEXT("ShowControlOperationsHeader", "Show Control Actions"));

		Section.AddMenuEntry(PlaylistCommands.Play);
		Section.AddMenuEntry(PlaylistCommands.UpdateValues);
		Section.AddMenuEntry(PlaylistCommands.Continue);
		Section.AddMenuEntry(PlaylistCommands.Stop);
		Section.AddMenuEntry(PlaylistCommands.ForceStop);
		Section.AddMenuEntry(PlaylistCommands.PlayNext);
	}

	// Preview Actions
	{
		FToolMenuSection& Section = InMenu.FindOrAddSection(TEXT("PreviewOperations"), LOCTEXT("PreviewOperationsHeader", "Preview Actions"));

		Section.AddMenuEntry(PlaylistCommands.PreviewPlay);
		Section.AddMenuEntry(PlaylistCommands.PreviewFrame);
		Section.AddMenuEntry(PlaylistCommands.PreviewContinue);
		Section.AddMenuEntry(PlaylistCommands.PreviewStop);
		Section.AddMenuEntry(PlaylistCommands.PreviewForceStop);
		Section.AddMenuEntry(PlaylistCommands.PreviewPlayNext);
		Section.AddMenuEntry(PlaylistCommands.TakeToProgram);
	}
}

#undef LOCTEXT_NAMESPACE
