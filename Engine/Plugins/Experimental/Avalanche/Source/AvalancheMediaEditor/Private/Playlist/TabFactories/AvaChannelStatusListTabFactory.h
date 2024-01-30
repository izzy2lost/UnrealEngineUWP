// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistTabFactory.h"

class FAvaChannelStatusListTabFactory : public FAvaPlaylistTabFactory
{
public:
	static const FName TabID;
	
	FAvaChannelStatusListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
