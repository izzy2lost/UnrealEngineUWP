// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistTabFactory.h"

class FAvaPageDetailsTabFactory : public FAvaPlaylistTabFactory
{
public:
	static const FName TabID;
	
	FAvaPageDetailsTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
