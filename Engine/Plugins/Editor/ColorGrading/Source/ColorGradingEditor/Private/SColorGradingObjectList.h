// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Framework/SlateDelegates.h"
#include "GameFramework/Actor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "ColorGradingListItem.h"

class ITableRow;
class STableViewBase;

template<class T>
class SListView;

/** Displays a list of color gradable items */
class SColorGradingObjectList : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_ThreeParams(FOnSelectionChanged, TSharedRef<SColorGradingObjectList>, FColorGradingListItemRef, ESelectInfo::Type);

public:
	SLATE_BEGIN_ARGS(SColorGradingObjectList) {}
		SLATE_ARGUMENT(const TArray<FColorGradingListItemRef>*, ColorGradingItemsSource)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refreshes the list, updating the UI to reflect the current state of the source items list*/
	void RefreshList();

	/** Gets a list of currently selected items */
	TArray<FColorGradingListItemRef> GetSelectedItems();

	/** Selects the specified list of items */
	void SetSelectedItems(const TArray<FColorGradingListItemRef>& InSelectedItems);

private:
	/** Generates the table row widget for the specified list item */
	TSharedRef<ITableRow> GenerateListItemRow(FColorGradingListItemRef Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Raised when the internal list view's selection has changed */
	void OnSelectionChanged(FColorGradingListItemRef SelectedItem, ESelectInfo::Type SelectInfo);

private:
	/** Internal list view used to display the list of color gradable items */
	TSharedPtr<SListView<FColorGradingListItemRef>> ListView;

	/** A delegate that is raised when the list of selected items is changed */
	FOnSelectionChanged OnSelectionChangedDelegate;
};