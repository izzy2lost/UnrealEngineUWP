// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageTextFilter.h"
#include "AvaPageFilterExpressionContext.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"

FAvaPageTextFilter::FAvaPageTextFilter()
	: TextFilterExpressionContext(MakeShared<FAvaPageFilterExpressionContext>())
	, TextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::Complex)
{
}

bool FAvaPageTextFilter::PassesFilter(const FAvalanchePage& InItem) const
{
	const bool bMatched = TextFilterExpressionEvaluator.TestTextFilter(*TextFilterExpressionContext);
	TextFilterExpressionContext->ClearItem();
	return bMatched;
}

FText FAvaPageTextFilter::GetFilterText() const
{
	return TextFilterExpressionEvaluator.GetFilterText();
}

void FAvaPageTextFilter::SetFilterText(const FText& InFilterText)
{
	if (TextFilterExpressionEvaluator.SetFilterText(InFilterText))
	{
		OnChanged().Broadcast();
	}
}

void FAvaPageTextFilter::SetItem(const FAvalanchePage& InItem, const UAvalanchePlaylist* InPlaylist, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	TextFilterExpressionContext->SetItem(InItem, InPlaylist, InPlaylistSearchListType);
}
