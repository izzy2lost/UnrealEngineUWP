// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistTabFactory.h"

class FAvaChannelLayerStatusListTabFactory : public FAvaPlaylistTabFactory
{
public:
	static const FName TabID;

	FAvaChannelLayerStatusListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
