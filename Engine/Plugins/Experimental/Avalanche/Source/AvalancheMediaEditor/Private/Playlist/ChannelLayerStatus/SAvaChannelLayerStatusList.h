// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Delegates/IDelegateInstance.h"

class FAvaMediaPlaybackInstance;
class FAvaPlaylistEditor;
class SAvaChannelLayerStatus;
class SWrapBox;

struct FAvaChannelLayerEntry
{
	FName ChannelName;
	FName LayerName;
	int32 PageId;
	FText LayerDescription;
	FLinearColor ComboPageColor;
	bool bOverridden;
};

/*
 * Widget containing the Status of all ChannelLayers in Broadcast. It contains a list of SAvaChannelLayerStatus widgets
 */
class SAvaChannelLayerStatusList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaChannelLayerStatusList) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);

protected:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
	TSharedPtr<SWrapBox> Container;
	FDelegateHandle UpdateHandle;

	//   Channel,    Layer, Details
	TMap<FName, TMap<FName, FAvaChannelLayerEntry>> ChannelLayerStatusList;

	void RefreshList();

	void OnPlaybackInstanceStatusChanged(const FAvaMediaPlaybackInstance& InPlaybackInstance);
};
