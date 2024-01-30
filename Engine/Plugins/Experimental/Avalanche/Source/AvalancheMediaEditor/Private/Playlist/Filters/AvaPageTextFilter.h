// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/IFilter.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Templates/SharedPointer.h"

class FAvaPageFilterExpressionContext;
class FText;
class UAvalanchePlaylist;
enum class EAvaPlaylistSearchListType : uint8;

class FAvaPageTextFilter
	: public IFilter<const FAvalanchePage&>
	, public TSharedFromThis<FAvaPageTextFilter>
{
public:
	FAvaPageTextFilter();

	//~ Begin IFilter
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }
	virtual bool PassesFilter(const FAvalanchePage& InItem) const override;
	//~ End IFilter

	FText GetFilterText() const;

	void SetFilterText(const FText& InFilterText);

	void SetItem(const FAvalanchePage& InItem, const UAvalanchePlaylist* InPlaylist, EAvaPlaylistSearchListType InPlaylistSearchListType) const;

private:
	/** Transient context data, used when calling PassesFilter. Kept around to minimize re-allocations between multiple calls to PassesFilter */
	TSharedRef<FAvaPageFilterExpressionContext> TextFilterExpressionContext;

	FTextFilterExpressionEvaluator TextFilterExpressionEvaluator;

	FChangedEvent ChangedEvent;
};
