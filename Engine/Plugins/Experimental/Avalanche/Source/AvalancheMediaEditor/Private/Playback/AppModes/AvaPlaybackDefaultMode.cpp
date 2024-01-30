// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackDefaultMode.h"
#include "IAvaMediaEditorModule.h"
#include "Playback/AvaPlaybackEditor.h"
#include "Playback/TabFactories/AvaPlaybackDetailsTabFactory.h"
#include "Playback/TabFactories/AvaPlaybackGraphTabFactory.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackDefaultMode"

FAvaPlaybackDefaultMode::FAvaPlaybackDefaultMode(const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor)
	: FAvaPlaybackAppMode(InPlaybackEditor, FAvaPlaybackAppMode::DefaultMode)
{
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_AvaPlayback", "Motion Design Playback"));
	
	check(InPlaybackEditor.IsValid());
	TabLayout = FTabManager::NewLayout("AvalanchePlaybackEditor_Default_Layout_V1")
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.75f)
					->AddTab(FAvaPlaybackGraphTabFactory::TabID, ETabState::OpenedTab)
					->SetForegroundTab(FAvaPlaybackGraphTabFactory::TabID)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(FAvaPlaybackDetailsTabFactory::TabID, ETabState::OpenedTab)
				)
			)
		);

	// Add Tab Spawners
	TabFactories.RegisterFactory(MakeShared<FAvaPlaybackGraphTabFactory>(InPlaybackEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaPlaybackDetailsTabFactory>(InPlaybackEditor));

	//Make sure we start with our existing list of extenders instead of creating a new one
	IAvaMediaEditorModule& AvaMediaEditorModule = IAvaMediaEditorModule::Get();
	ToolbarExtender = AvaMediaEditorModule.GetPlaybackToolBarExtensibilityManager()->GetAllExtenders();
	InPlaybackEditor->ExtendToolBar(ToolbarExtender);
}

#undef LOCTEXT_NAMESPACE
