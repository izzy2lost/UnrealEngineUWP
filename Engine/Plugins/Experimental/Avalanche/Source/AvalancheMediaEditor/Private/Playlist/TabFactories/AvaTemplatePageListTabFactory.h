// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistTabFactory.h"

class FAvaTemplatePageListTabFactory : public FAvaPlaylistTabFactory
{
public:
	static const FName TabID;
	
	FAvaTemplatePageListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;
};
