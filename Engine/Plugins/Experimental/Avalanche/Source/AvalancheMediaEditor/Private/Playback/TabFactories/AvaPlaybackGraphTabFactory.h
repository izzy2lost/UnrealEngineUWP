// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackTabFactory.h"

class FAvaPlaybackGraphTabFactory : public FAvaPlaybackTabFactory
{
public:
	static const FName TabID;
	
	FAvaPlaybackGraphTabFactory(const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};
