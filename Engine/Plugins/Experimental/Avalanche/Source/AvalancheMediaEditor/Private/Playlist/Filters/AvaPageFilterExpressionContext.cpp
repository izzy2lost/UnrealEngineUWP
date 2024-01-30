// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageFilterExpressionContext.h"

#include "IAvaMediaEditorModule.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/IAvaPlaylistFilterExpressionFactory.h"

void FAvaPageFilterExpressionContext::SetItem(const FAvalanchePage& InItem,  const UAvalanchePlaylist* InPlaylist, EAvaPlaylistSearchListType InPlaylistSearchListType)
{
	ItemPageId = InItem.GetPageId();
	ItemPlaylist = InPlaylist;
	PlaylistSearchListType = InPlaylistSearchListType;

	FString NameInfo = InItem.GetPageName();
	ItemName = *NameInfo;

	const FText IdInfo = FText::AsNumber(InItem.GetPageId(), &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions);
	ItemId = *IdInfo.ToString();

	const FString PathInfo = InItem.GetAvalancheAssetPath(InPlaylist).GetAssetPathString();
	ItemPath = *PathInfo;

	const TArray<FAvalanchePageStatus> Statuses = InItem.GetPageProgramStatuses(InPlaylist);
	for (const FAvalanchePageStatus& Status : Statuses)
	{
		const FName StatusName = StaticEnum<EAvalanchePageStatus>()->GetNameByValue(static_cast<int32>(Status.Status));
		ItemStatuses.Add(StatusName);
	}
}

void FAvaPageFilterExpressionContext::ClearItem()
{
	ItemPageId = FAvalanchePage::InvalidPageId;
	ItemPlaylist = nullptr;
	ItemName = NAME_None;
	ItemId = NAME_None;
	ItemPath = NAME_None;
	ItemStatuses.Reset();
}

bool FAvaPageFilterExpressionContext::TestBasicStringExpression(const FTextFilterString& InValue
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	if (InValue.CompareName(ItemName, InTextComparisonMode))
	{
		return true;
	}
	if (InValue.CompareName(ItemId, InTextComparisonMode))
	{
		return true;
	}
	if (InValue.CompareName(ItemPath, ETextFilterTextComparisonMode::Partial))
	{
		return true;
	}
	for (const FName& Status : ItemStatuses)
	{
		if (InValue.CompareName(Status, InTextComparisonMode))
		{
			return true;
		}
	}
	return false;
}

bool FAvaPageFilterExpressionContext::TestComplexExpression(const FName& InKey
	, const FTextFilterString& InValue
	, const ETextFilterComparisonOperation InComparisonOperation
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	const IAvaMediaEditorModule& AvaMediaEditorModule = IAvaMediaEditorModule::Get();

	const FAvalanchePage& PageItem = UAvalanchePlaylist::GetPageSafe(ItemPlaylist, ItemPageId);

	if (PageItem.IsValidPage() && AvaMediaEditorModule.CanFilterSupportComparisonOperation(InKey, InComparisonOperation, PlaylistSearchListType))
	{
		FAvaPlaylistTextFilterArgs FilterArgs;
		FilterArgs.ValueToCheck = InValue;
		FilterArgs.ItemPlaylist = ItemPlaylist;
		FilterArgs.ComparisonMode = InTextComparisonMode;
		FilterArgs.ComparisonOperation = InComparisonOperation;

		return AvaMediaEditorModule.FilterExpression(InKey, PageItem, FilterArgs);
	}
	return false;
}
