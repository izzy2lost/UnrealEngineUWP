// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyTreeView.h"

#include "Replication/Editor/Model/Data/PropertyData.h"

#include "Algo/ForEach.h"
#include "Replication/Editor/View/Column/PropertyColumnAdapter.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "SReplicatedPropertiesView"

namespace UE::ConcertSharedSlate
{
	void SPropertyTreeView::Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SAssignNew(TreeView, SReplicationTreeView<FPropertyData>)
				.RootItemsSource(&RootPropertyRowData)
				.OnGetChildren(this, &SPropertyTreeView::GetPropertyRowChildren)
				.FilterItem(InArgs._FilterItem)
				.Columns(FPropertyColumnAdapter::Transform(InArgs._Columns))
				.ExpandableColumnLabel(InArgs._ExpandableColumnLabel)
				.PrimarySort(InArgs._PrimarySort)
				.SecondarySort(InArgs._SecondarySort)
				.SelectionMode(InArgs._SelectionMode)
				.LeftOfSearchBar() [ InArgs._LeftOfSearchBar.Widget ]
				.RightOfSearchBar() [ InArgs._RightOfSearchBar.Widget ]
				.RowBelowSearchBar() [ InArgs._RowBelowSearchBar.Widget ]
				.NoItemsContent() [ InArgs._NoItemsContent.Widget ]
		];
	}

	void SPropertyTreeView::RefreshPropertyData(const TArray<FPropertyAssignmentEntry>& Entries, bool bCanReuseExistingRowItems)
	{
		if (!bCanReuseExistingRowItems)
		{
			ChainToPropertyDataCache.Reset();
		}
		
		// Try to re-use old instances by using the old ChainToPropertyDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FConcertPropertyChain, TSharedPtr<FPropertyData>> NewChainToPropertyDataCache;
		
		PropertyRowData.Empty();
		for (const FPropertyAssignmentEntry& Entry : Entries)
		{
			for (const FConcertPropertyChain& PropertyChain : Entry.PropertiesToDisplay)
			{
				const TSharedPtr<FPropertyData>* ExistingItem = ChainToPropertyDataCache.Find(PropertyChain);
				const TSharedRef<FPropertyData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocatePropertyData(Entry.ContextObjects, Entry.Class, PropertyChain);
				PropertyRowData.Emplace(Item);
				NewChainToPropertyDataCache.Emplace(PropertyChain, Item);
			}
		}

		// If an item was removed, then NewPathToPropertyDataCache does not contain it. 
		ChainToPropertyDataCache = MoveTemp(NewChainToPropertyDataCache);
		
		// The tree view requires the item source to only contain the root items.
		BuildRootPropertyRowData();

		TreeView->RequestRefilter();
	}

	void SPropertyTreeView::RequestScrollIntoView(const FConcertPropertyChain& PropertyChain)
	{
		const int32 Index = PropertyRowData.IndexOfByPredicate([&PropertyChain](const TSharedPtr<FPropertyData>& Data)
		{
			return Data->GetProperty() == PropertyChain;
		});
		if (PropertyRowData.IsValidIndex(Index))
		{
			TreeView->SetExpandedItems({ PropertyRowData[Index] }, true);
			TreeView->RequestScrollIntoView(PropertyRowData[Index]);
		}
	}

	inline TSharedRef<FPropertyData> SPropertyTreeView::AllocatePropertyData(TSet<TSoftObjectPtr<>> ContextObjects, FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain)
	{
		return MakeShared<FPropertyData>(MoveTemp(ContextObjects), MoveTemp(OwningClass), MoveTemp(PropertyChain));
	}

	void SPropertyTreeView::BuildRootPropertyRowData()
	{
		RootPropertyRowData.Empty(PropertyRowData.Num());
		for (const TSharedPtr<FPropertyData>& PropertyData : PropertyRowData)
		{
			if (PropertyData->GetProperty().IsRootProperty())
			{
				RootPropertyRowData.Emplace(PropertyData);
			}
		}
	}

	void SPropertyTreeView::GetPropertyRowChildren(TSharedPtr<FPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FPropertyData>)> ProcessChild)
	{
		TArray<TSharedPtr<FPropertyData>> Children;
		
		// Not the most efficient but it should be fine.
		for (const TSharedPtr<FPropertyData>& Data : PropertyRowData)
		{
			if (Data->GetProperty().IsDirectChildOf(ReplicatedPropertyData->GetProperty()))
			{
				Children.Add(Data);
			}
		}

		Algo::ForEach(Children, [&ProcessChild](const TSharedPtr<FPropertyData>& Data){ ProcessChild(Data); });
	}
}

#undef LOCTEXT_NAMESPACE