// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistDefaultMode.h"
#include "IAvaMediaEditorModule.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/TabFactories/AvaChannelLayerStatusListTabFactory.h"
#include "Playlist/TabFactories/AvaChannelStatusListTabFactory.h"
#include "Playlist/TabFactories/AvaInstancedPageListTabFactory.h"
#include "Playlist/TabFactories/AvaPageDetailsTabFactory.h"
#include "Playlist/TabFactories/AvaPageViewerTabFactory.h"
#include "Playlist/TabFactories/AvaShowControlTabFactory.h"
#include "Playlist/TabFactories/AvaSubListDocumentTabFactory.h"
#include "Playlist/TabFactories/AvaSubListTabFactory.h"
#include "Playlist/TabFactories/AvaTemplatePageListTabFactory.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistDefaultMode"

FAvaPlaylistDefaultMode::FAvaPlaylistDefaultMode(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistAppMode(InPlaylistEditor, FAvaPlaylistAppMode::DefaultMode)
{
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_AvaPlaylist", "Motion Design Rundown"));
	
	check(InPlaylistEditor.IsValid());
	TabLayout = FTabManager::NewLayout("AvalanchePlaylistEditor_Default_Layout_V2.4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.5f)
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(FAvaTemplatePageListTabFactory::TabID, ETabState::OpenedTab)
					->SetForegroundTab(FAvaTemplatePageListTabFactory::TabID)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.05f)
					->AddTab(FAvaShowControlTabFactory::TabID, ETabState::OpenedTab)
					->SetForegroundTab(FAvaShowControlTabFactory::TabID)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(FAvaInstancedPageListTabFactory::TabID, ETabState::OpenedTab)
					->SetForegroundTab(FAvaInstancedPageListTabFactory::TabID)
					->SetHideTabWell(false)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(FAvaSubListTabFactory::TabID, ETabState::ClosedTab)
					->SetForegroundTab(FAvaSubListTabFactory::TabID)
					->SetHideTabWell(false)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.60f)
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.3f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.7f)
						->AddTab(FAvaPageDetailsTabFactory::TabID, ETabState::OpenedTab)
						->SetForegroundTab(FAvaPageDetailsTabFactory::TabID)
						->SetHideTabWell(false)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.3f)
						->AddTab(FAvaChannelLayerStatusListTabFactory::TabID, ETabState::OpenedTab)
						->SetForegroundTab(FAvaChannelLayerStatusListTabFactory::TabID)
						->SetHideTabWell(false)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.02f)
					->AddTab(FAvaChannelStatusListTabFactory::TabID, ETabState::OpenedTab)
					->SetForegroundTab(FAvaChannelStatusListTabFactory::TabID)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(FAvaPageViewerTabFactory::TabID, ETabState::OpenedTab)
					->SetForegroundTab(FAvaPageViewerTabFactory::TabID)
					->SetHideTabWell(false)
				)
			)
		);

	// Add Tab Spawners
	TabFactories.RegisterFactory(MakeShared<FAvaTemplatePageListTabFactory>(InPlaylistEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaInstancedPageListTabFactory>(InPlaylistEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaPageDetailsTabFactory>(InPlaylistEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaShowControlTabFactory>(InPlaylistEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaSubListTabFactory>(InPlaylistEditor));	
	TabFactories.RegisterFactory(MakeShared<FAvaPageViewerTabFactory>(InPlaylistEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaChannelStatusListTabFactory>(InPlaylistEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaChannelLayerStatusListTabFactory>(InPlaylistEditor));

	DocumentTabFactories.Emplace(FAvaSubListDocumentTabFactory::FactoryId, MakeShared<FAvaSubListDocumentTabFactory>(InPlaylistEditor));

	// Make sure we start with our existing list of extenders instead of creating a new one
	IAvaMediaEditorModule& AvaMediaEditorModule = IAvaMediaEditorModule::Get();
	ToolbarExtender = AvaMediaEditorModule.GetPlaylistToolBarExtensibilityManager()->GetAllExtenders(
		InPlaylistEditor->GetToolkitCommands(),
		*InPlaylistEditor->GetObjectsCurrentlyBeingEdited()
	);
	InPlaylistEditor->ExtendToolBar(ToolbarExtender);
}

#undef LOCTEXT_NAMESPACE
