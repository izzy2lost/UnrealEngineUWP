// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterChannelSuggestionFactory.h"

#include "Rundown/AvaRundown.h"
#include "Rundown/Factories/Filters/AvaRundownFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaRundownFilterChannelSuggestionFactory"

const FName FAvaRundownFilterChannelSuggestionFactory::KeyName = FName(TEXT("CHANNEL"));

void FAvaRundownFilterChannelSuggestionFactory::AddSuggestion(const TSharedRef<FAvaRundownFilterSuggestionPayload>& InPayload)
{
	const FAvaRundownPage& PageItem = UAvaRundown::GetPageSafe(InPayload->Rundown, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText ChannelCategoryLabel = LOCTEXT("ChannelCategoryLabel", "Ava-Rundown-Channel");
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

bool FAvaRundownFilterChannelSuggestionFactory::SupportSuggestionType(EAvaRundownSearchListType InSuggestionType) const
{
	return InSuggestionType == EAvaRundownSearchListType::Instanced;
}

#undef LOCTEXT_NAMESPACE
