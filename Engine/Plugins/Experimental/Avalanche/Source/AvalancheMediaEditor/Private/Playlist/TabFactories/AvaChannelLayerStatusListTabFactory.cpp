// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaChannelLayerStatusListTabFactory.h"
#include "AvaMediaEditorStyle.h"
#include "Playlist/ChannelLayerStatus/SAvaChannelLayerStatusList.h"

const FName FAvaChannelLayerStatusListTabFactory::TabID(TEXT("AvalancheChannelLayerStatusList"));

#define LOCTEXT_NAMESPACE "AvaChannelLayerStatusListTabFactory"

FAvaChannelLayerStatusListTabFactory::FAvaChannelLayerStatusListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("ChannelLayerStatus_TabLabel", "Layer Status");
	TabIcon = FSlateIcon(FAvaMediaEditorStyle::GetStyleSetName(), "AvalancheMediaEditor.BroadcastIcon");

	bIsSingleton = true;
	bShouldAutosize = true;

	ViewMenuDescription = LOCTEXT("ChannelLayerStatus_ViewMenu_Desc", "Displays the Status of all Channel Layers in Broadcast");
	ViewMenuTooltip = LOCTEXT("ChannelLayerStatus_ViewMenu_ToolTip", "Displays the Status of all Channel Layers in Broadcast");
}

TSharedRef<SWidget> FAvaChannelLayerStatusListTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAvaChannelLayerStatusList, PlaylistEditorWeak.Pin());
}

#undef LOCTEXT_NAMESPACE
