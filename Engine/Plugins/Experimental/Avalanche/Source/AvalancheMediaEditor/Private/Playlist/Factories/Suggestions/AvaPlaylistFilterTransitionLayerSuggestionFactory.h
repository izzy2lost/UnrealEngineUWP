// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Factories/Filters/IAvaPlaylistFilterSuggestionFactory.h"

class FAvaPlaylistFilterTransitionLayerSuggestionFactory : public IAvaPlaylistFilterSuggestionFactory
{
public:
	static const FName KeyName;

	//~ Begin IAvaFilterSuggestionFactory interface
	virtual FName GetSuggestionIdentifier() const override { return KeyName; }
	virtual bool IsSimpleSuggestion() const override { return false; }
	virtual void AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload) override;
	virtual bool SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const override;
	//~ End IAvaFilterSuggestionFactory interface
};
