// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterIdSuggestionFactory.h"

#include "Rundown/Factories/Filters/AvaRundownFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaRundownFilterIdSuggestionFactory"

const FName FAvaRundownFilterIdSuggestionFactory::KeyName = FName(TEXT("ID"));

void FAvaRundownFilterIdSuggestionFactory::AddSuggestion(const TSharedRef<FAvaRundownFilterSuggestionPayload>& InPayload)
{
	const FText IdCategoryLabel = LOCTEXT("IdCategoryLabel", "Ava-Rundown-Id");
	FString IdNameSuggestion = TEXT("Id");
	const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || IdNameSuggestion.Contains(InPayload->FilterValue);
	if (bIsFilterValueValid && !InPayload->FilterCache.Contains(IdNameSuggestion))
	{
		InPayload->FilterCache.Add(IdNameSuggestion);
		InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(IdNameSuggestion), LOCTEXT("IdSuggestion","Id"), IdCategoryLabel});
	}
}

bool FAvaRundownFilterIdSuggestionFactory::SupportSuggestionType(EAvaRundownSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaRundownSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
