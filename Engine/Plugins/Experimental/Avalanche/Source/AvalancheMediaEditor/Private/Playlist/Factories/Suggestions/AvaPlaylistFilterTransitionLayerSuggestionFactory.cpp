// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterTransitionLayerSuggestionFactory.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistFilterLayerSuggestionFactory"

const FName FAvaPlaylistFilterTransitionLayerSuggestionFactory::KeyName = FName(TEXT("TRANSITIONLAYER"));

void FAvaPlaylistFilterTransitionLayerSuggestionFactory::AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload)
{
	const FAvalanchePage& PageItem = UAvalanchePlaylist::GetPageSafe(InPayload->Playlist, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText TransitionLayerCategoryLabel = LOCTEXT("TransitionLayerCategoryLabel", "Ava-Rundown-Transition-Layer");
		const FString PageLayer = PageItem.GetTransitionLayer(InPayload->Playlist).ToString();

		FString TransitionLayerSuggestion = FString::Printf(TEXT("TransitionLayer=%s"), *PageLayer);
		const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || TransitionLayerSuggestion.Contains(InPayload->FilterValue);

		if (bIsFilterValueValid && !InPayload->FilterCache.Contains(TransitionLayerSuggestion))
		{
			InPayload->FilterCache.Add(TransitionLayerSuggestion);
			InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(TransitionLayerSuggestion), FText::FromString(PageLayer), TransitionLayerCategoryLabel});
		}
	}
}

bool FAvaPlaylistFilterTransitionLayerSuggestionFactory::SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaPlaylistSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
