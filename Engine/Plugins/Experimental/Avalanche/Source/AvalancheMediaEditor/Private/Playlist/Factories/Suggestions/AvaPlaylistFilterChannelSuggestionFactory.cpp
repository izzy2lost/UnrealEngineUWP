// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterChannelSuggestionFactory.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistFilterChannelSuggestionFactory"

const FName FAvaPlaylistFilterChannelSuggestionFactory::KeyName = FName(TEXT("CHANNEL"));

void FAvaPlaylistFilterChannelSuggestionFactory::AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload)
{
	const FAvalanchePage& PageItem = UAvalanchePlaylist::GetPageSafe(InPayload->Playlist, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText ChannelCategoryLabel = LOCTEXT("ChannelCategoryLabel", "Ava-Playlist-Channel");
		const FString ChannelName = PageItem.GetChannelName().ToString();

		FString ChannelNameSuggestion = FString::Printf(TEXT("Channel=%s"), *ChannelName);
		const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || ChannelNameSuggestion.Contains(InPayload->FilterValue);

		if (bIsFilterValueValid && !InPayload->FilterCache.Contains(ChannelNameSuggestion))
		{
			InPayload->FilterCache.Add(ChannelNameSuggestion);
			InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(ChannelNameSuggestion), FText::FromString(ChannelName), ChannelCategoryLabel});
		}
	}
}

bool FAvaPlaylistFilterChannelSuggestionFactory::SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const
{
	return InSuggestionType == EAvaPlaylistSearchListType::Instanced;
}

#undef LOCTEXT_NAMESPACE
