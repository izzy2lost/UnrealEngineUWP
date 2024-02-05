// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterStatusSuggestionFactory.h"

#include "Rundown/AvaRundown.h"
#include "Rundown/Factories/Filters/AvaRundownFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaRundownFilterStatusSuggestionFactory"

const FName FAvaRundownFilterStatusSuggestionFactory::KeyName = FName(TEXT("STATUS"));

void FAvaRundownFilterStatusSuggestionFactory::AddSuggestion(const TSharedRef<FAvaRundownFilterSuggestionPayload>& InPayload)
{
	const FAvaRundownPage& PageItem = UAvaRundown::GetPageSafe(InPayload->Rundown, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText StatusCategoryLabel = LOCTEXT("StatusCategoryLabel", "Ava-Rundown-Status");
		TArray<FAvaRundownChannelPageStatus> StatusPages = PageItem.GetPageContextualStatuses(InPayload->Rundown);
		for (FAvaRundownChannelPageStatus Status : StatusPages)
		{
			const FString StatusName = StaticEnum<EAvaRundownPageStatus>()->GetNameStringByValue(static_cast<int32>(Status.Status));

			FString StatusNameSuggestion = FString::Printf(TEXT("Status=%s"), *StatusName);
			const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || StatusNameSuggestion.Contains(InPayload->FilterValue);

			if (bIsFilterValueValid && !InPayload->FilterCache.Contains(StatusNameSuggestion))
			{
				InPayload->FilterCache.Add(StatusNameSuggestion);
				InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(StatusNameSuggestion), FText::FromString(StatusName), StatusCategoryLabel});
			}
		}
	}
}

bool FAvaRundownFilterStatusSuggestionFactory::SupportSuggestionType(EAvaRundownSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaRundownSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
