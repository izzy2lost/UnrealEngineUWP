// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterTransitionLayerSuggestionFactory.h"

#include "Rundown/AvaRundown.h"
#include "Rundown/Factories/Filters/AvaRundownFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaRundownFilterLayerSuggestionFactory"

const FName FAvaRundownFilterTransitionLayerSuggestionFactory::KeyName = FName(TEXT("TRANSITIONLAYER"));

void FAvaRundownFilterTransitionLayerSuggestionFactory::AddSuggestion(const TSharedRef<FAvaRundownFilterSuggestionPayload>& InPayload)
{
	const FAvaRundownPage& PageItem = UAvaRundown::GetPageSafe(InPayload->Rundown, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText TransitionLayerCategoryLabel = LOCTEXT("TransitionLayerCategoryLabel", "Ava-Rundown-Transition-Layer");
		const FString PageLayer = PageItem.GetTransitionLayer(InPayload->Rundown).ToString();

		FString TransitionLayerSuggestion = FString::Printf(TEXT("TransitionLayer=%s"), *PageLayer);
		const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || TransitionLayerSuggestion.Contains(InPayload->FilterValue);

		if (bIsFilterValueValid && !InPayload->FilterCache.Contains(TransitionLayerSuggestion))
		{
			InPayload->FilterCache.Add(TransitionLayerSuggestion);
			InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(TransitionLayerSuggestion), FText::FromString(PageLayer), TransitionLayerCategoryLabel});
		}
	}
}

bool FAvaRundownFilterTransitionLayerSuggestionFactory::SupportSuggestionType(EAvaRundownSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaRundownSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
