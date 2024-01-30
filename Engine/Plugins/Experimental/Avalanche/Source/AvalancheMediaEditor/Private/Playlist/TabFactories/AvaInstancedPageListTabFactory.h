// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistTabFactory.h"

class FAvaInstancedPageListTabFactory : public FAvaPlaylistTabFactory
{
public:
	static const FName TabID;
	
	FAvaInstancedPageListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;

protected:
	virtual TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& InSpawnArgs, TWeakPtr<FTabManager> InWeakTabManager) const override;
};
