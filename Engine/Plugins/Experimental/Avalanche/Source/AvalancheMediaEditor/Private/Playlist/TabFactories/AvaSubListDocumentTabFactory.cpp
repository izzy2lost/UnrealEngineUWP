// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSubListDocumentTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Pages/Slate/SAvaInstancedPageList.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FAvaSubListDocumentTabFactory"

const FName FAvaSubListDocumentTabFactory::FactoryId = "AvaSubListTabFactory";
const FString FAvaSubListDocumentTabFactory::BaseTabName(TEXT("AvaSubListDocument"));

FName FAvaSubListDocumentTabFactory::GetTabId(int32 InSubListIndex)
{
	return FName(BaseTabName + "_" + FString::FromInt(InSubListIndex));
}

FAvaSubListDocumentTabFactory::FAvaSubListDocumentTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FDocumentTabFactory(FactoryId, InPlaylistEditor)
	, PlaylistEditorWeak(InPlaylistEditor)
	, SubListIndex(UAvalanchePlaylist::InstancePageList.SubListIndex)
{
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All");
}

TSharedRef<SWidget> FAvaSubListDocumentTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	if (SubListIndex <= UAvalanchePlaylist::InstancePageList.SubListIndex)
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SAvaInstancedPageList, PlaylistEditorWeak.Pin(), UAvalanchePlaylist::CreateSubListReference(SubListIndex));
}

TSharedRef<SDockTab> FAvaSubListDocumentTabFactory::SpawnSubListTab(const FWorkflowTabSpawnInfo& InInfo, int32 InSubListIndex)
{
	SubListIndex = InSubListIndex;
	TabIdentifier = GetTabId(InSubListIndex);
	TabLabel = FText::Format(LOCTEXT("PlaylistSubListDocument_TabLabel", "Page View {0}"), FText::AsNumber(SubListIndex + 1));
	ViewMenuDescription = FText::Format(LOCTEXT("PlaylistSubListDocument_ViewMenu_Desc", "Page View {0}"), FText::AsNumber(SubListIndex + 1));
	ViewMenuTooltip = FText::Format(LOCTEXT("PlaylistSubListDocument_ViewMenu_ToolTip", "Page View {0}"), FText::AsNumber(SubListIndex + 1));

	TSharedRef<SDockTab> NewTab = SpawnTab(InInfo);

	TSharedRef<SAvaInstancedPageList> PageList = StaticCastSharedRef<SAvaInstancedPageList>(NewTab->GetContent());
	NewTab->SetOnTabActivated(SDockTab::FOnTabActivatedCallback::CreateSP(PageList, &SAvaInstancedPageList::OnTabActivated));
	PageList->SetMyTab(NewTab);

	if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PageList->GetPlaylistEditor())
	{
		UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();

		if (IsValid(Playlist) && Playlist->IsValidSubListIndex(InSubListIndex)
			&& !Playlist->GetSubList(InSubListIndex).Name.IsEmpty())
		{
			NewTab->SetLabel(Playlist->GetSubList(InSubListIndex).Name);
		}
	}

	return NewTab;
}

#undef LOCTEXT_NAMESPACE
