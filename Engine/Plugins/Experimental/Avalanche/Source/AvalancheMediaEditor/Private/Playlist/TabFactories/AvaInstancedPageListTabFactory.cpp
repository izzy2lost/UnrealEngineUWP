// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaInstancedPageListTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Pages/Slate/SAvaInstancedPageList.h"
#include "Widgets/Docking/SDockTab.h"

const FName FAvaInstancedPageListTabFactory::TabID(TEXT("AvalancheInstancedPlaylistPageList"));

#define LOCTEXT_NAMESPACE "AvaInstancedPageListTabFactory"

FAvaInstancedPageListTabFactory::FAvaInstancedPageListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("PlaylistPageList_TabLabel", "Pages");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaylistPageList_ViewMenu_Desc", "Pages");
	ViewMenuTooltip = LOCTEXT("PlaylistPageList_ViewMenu_ToolTip", "Pages");
}

TSharedRef<SWidget> FAvaInstancedPageListTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	return SNew(SAvaInstancedPageList, PlaylistEditorWeak.Pin(), UAvalanchePlaylist::InstancePageList);
}

TSharedRef<SDockTab> FAvaInstancedPageListTabFactory::OnSpawnTab(const FSpawnTabArgs& InSpawnArgs,
	TWeakPtr<FTabManager> InWeakTabManager) const
{
	TSharedRef<SDockTab> NewTab = FAvaPlaylistTabFactory::OnSpawnTab(InSpawnArgs, InWeakTabManager);
	TSharedRef<SWidget> Content = NewTab->GetContent();

	if (Content->GetWidgetClass().GetWidgetType() == SBorder::StaticWidgetClass().GetWidgetType()
		&& Content->GetTypeAsString() == "SBorder") // Subclasses of SBorder that do not propertly declare themselves will have the same widget class.
	{
		Content = StaticCastSharedRef<SBorder>(Content)->GetContent();
	}

	TSharedRef<SAvaInstancedPageList> PageList = StaticCastSharedRef<SAvaInstancedPageList>(Content);
	NewTab->SetOnTabActivated(SDockTab::FOnTabActivatedCallback::CreateSP(PageList, &SAvaInstancedPageList::OnTabActivated));

	return NewTab;
}

#undef LOCTEXT_NAMESPACE
