// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterPathSuggestionFactory.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistFilterPathSuggestionFactory"

const FName FAvaPlaylistFilterPathSuggestionFactory::KeyName = FName(TEXT("ASSET"));

void FAvaPlaylistFilterPathSuggestionFactory::AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload)
{
	const FAvalanchePage& PageItem = UAvalanchePlaylist::GetPageSafe(InPayload->Playlist, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText PathCategoryLabel = LOCTEXT("PathCategoryLabel", "Ava-Playlist-Asset");
		const FString AssetName = PageItem.GetAvalancheAssetPath(InPayload->Playlist).GetAssetName();

		FString BlueprintNameSuggestion = FString::Printf(TEXT("Asset=%s"), *AssetName);
		const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || BlueprintNameSuggestion.Contains(InPayload->FilterValue);

		if (bIsFilterValueValid && !InPayload->FilterCache.Contains(BlueprintNameSuggestion))
		{
			InPayload->FilterCache.Add(BlueprintNameSuggestion);
			InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(BlueprintNameSuggestion), FText::FromString(AssetName), PathCategoryLabel});
		}
	}
}

bool FAvaPlaylistFilterPathSuggestionFactory::SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaPlaylistSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
