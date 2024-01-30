// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"

class FName;
class FText;
class UAvalanchePlaylist;
struct FAvalanchePage;

/** Expression context to test the given asset data against the current text filter */
class FAvaPageFilterExpressionContext : public ITextFilterExpressionContext
{
public:
	FAvaPageFilterExpressionContext()
		: ItemPageId(UE::AvalanchePlaylist::InvalidPageId)
		, ItemPlaylist(nullptr)
		, PlaylistSearchListType(EAvaPlaylistSearchListType::None)
	{}

	void SetItem(const FAvalanchePage& InItem, const UAvalanchePlaylist* InPlaylist, EAvaPlaylistSearchListType InPlaylistSearchListType);

	void ClearItem();

	virtual bool TestBasicStringExpression(const FTextFilterString& InValue
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;

	virtual bool TestComplexExpression(const FName& InKey
		, const FTextFilterString& InValue
		, const ETextFilterComparisonOperation InComparisonOperation
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;

private:
	/** Pointer to the asset we're currently filtering */
	int32 ItemPageId;

	const UAvalanchePlaylist* ItemPlaylist;

	EAvaPlaylistSearchListType PlaylistSearchListType;

	FName ItemName;

	FName ItemId;

	FName ItemPath;

	TArray<FName> ItemStatuses;
};
