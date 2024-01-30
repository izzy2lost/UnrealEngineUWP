// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterIdSuggestionFactory.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistFilterIdSuggestionFactory"

const FName FAvaPlaylistFilterIdSuggestionFactory::KeyName = FName(TEXT("ID"));

void FAvaPlaylistFilterIdSuggestionFactory::AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload)
{
	const FText IdCategoryLabel = LOCTEXT("IdCategoryLabel", "Ava-Playlist-Id");
	FString IdNameSuggestion = TEXT("Id");
	const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || IdNameSuggestion.Contains(InPayload->FilterValue);
	if (bIsFilterValueValid && !InPayload->FilterCache.Contains(IdNameSuggestion))
	{
		InPayload->FilterCache.Add(IdNameSuggestion);
		InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(IdNameSuggestion), LOCTEXT("IdSuggestion","Id"), IdCategoryLabel});
	}
}

bool FAvaPlaylistFilterIdSuggestionFactory::SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaPlaylistSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
