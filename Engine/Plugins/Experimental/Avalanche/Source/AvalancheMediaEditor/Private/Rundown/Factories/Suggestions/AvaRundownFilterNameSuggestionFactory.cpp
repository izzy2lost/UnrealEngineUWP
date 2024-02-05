// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterNameSuggestionFactory.h"

#include "Rundown/AvaRundown.h"
#include "Rundown/Factories/Filters/AvaRundownFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaRundownFilterNameSuggestionFactory"

const FName FAvaRundownFilterNameSuggestionFactory::KeyName = FName(TEXT("NAME"));

void FAvaRundownFilterNameSuggestionFactory::AddSuggestion(const TSharedRef<FAvaRundownFilterSuggestionPayload>& InPayload)
{
	const FAvaRundownPage& PageItem = UAvaRundown::GetPageSafe(InPayload->Rundown, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText NameCategoryLabel = LOCTEXT("NameCategoryLabel", "Ava-Rundown-Name");
		const FText PageText = PageItem.GetPageDescription();

		FString PageName = TEXT("\"");
		PageName +=	PageItem.GetPageDescription().ToString();
		PageName.Append(TEXT("\""));

		FString NameSuggestion = FString::Printf(TEXT("Name=%s"), *PageName);
		const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || NameSuggestion.Contains(InPayload->FilterValue);

		if (bIsFilterValueValid && !InPayload->FilterCache.Contains(NameSuggestion))
		{
			InPayload->FilterCache.Add(NameSuggestion);
			InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(NameSuggestion), PageText, NameCategoryLabel});
		}
	}
}

bool FAvaRundownFilterNameSuggestionFactory::SupportSuggestionType(EAvaRundownSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaRundownSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
