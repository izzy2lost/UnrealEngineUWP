// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaChannelStatusListTabFactory.h"
#include "AvaMediaEditorStyle.h"
#include "Playlist/ChannelStatus/SAvaChannelStatusList.h"

const FName FAvaChannelStatusListTabFactory::TabID(TEXT("AvalancheChannelStatusList"));

#define LOCTEXT_NAMESPACE "AvaChannelStatusListTabFactory"

FAvaChannelStatusListTabFactory::FAvaChannelStatusListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("ChannelStatus_TabLabel", "Channel Status");
	TabIcon = FSlateIcon(FAvaMediaEditorStyle::GetStyleSetName(), "AvalancheMediaEditor.BroadcastIcon");

	bIsSingleton = true;
	bShouldAutosize = true;

	ViewMenuDescription = LOCTEXT("ChannelStatus_ViewMenu_Desc", "Displays the Status of all Channels in Broadcast");
	ViewMenuTooltip = LOCTEXT("ChannelStatus_ViewMenu_ToolTip", "Displays the Status of all Channels in Broadcast");
}

TSharedRef<SWidget> FAvaChannelStatusListTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAvaChannelStatusList);
}

#undef LOCTEXT_NAMESPACE
