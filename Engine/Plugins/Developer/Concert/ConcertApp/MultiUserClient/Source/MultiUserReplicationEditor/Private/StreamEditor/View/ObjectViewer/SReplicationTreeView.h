// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationColumn.h"
#include "SReplicationColumnRow.h"

#include "Algo/RemoveIf.h"
#include "Misc/TextFilter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SReplicationListView"

namespace UE::MultiUserReplicationEditor
{
	/**
	 * Shared code for the list view for replicated actors and properties.
	 * It is a table view that is searchable with a search box.
	 */
	template<typename TItemType>
	class SReplicationTreeView : public SCompoundWidget
	{
	public:

		DECLARE_DELEGATE_OneParam(FDeleteItems, const TArray<TItemType>& SelectedItems);
		DECLARE_DELEGATE_TwoParams(FGetItemChildren, TItemType Item, TFunctionRef<void(TItemType)> ProcessChild);
		DECLARE_DELEGATE(FOnSelectionChanged);

		SLATE_BEGIN_ARGS(SReplicationTreeView<TItemType>)
			: _SelectionMode(ESelectionMode::Single)
		{}
			/** The items to display */
			SLATE_ARGUMENT(TArray<TItemType>*, RootItemsSource)

			/** Gets an items children for the tree view */
			SLATE_EVENT(FGetItemChildren, OnGetChildren)

			/** Optional. Called when the user presses the delete key */
			SLATE_EVENT(FDeleteItems, OnDeleteItems)

			/** Called to generate the context menu for an item */
			SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

			/** Called when the selection changes. Call GetSelectedItems to get the selected items. */
			SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
			
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<TReplicationColumn<TItemType>>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)

			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			AllRootItems = InArgs._RootItemsSource;
			check(AllRootItems);

			OnGetChildrenDelegate = InArgs._OnGetChildren;
			OnDeleteItemsDelegate = InArgs._OnDeleteItems;
			ExpandableColumnId = InArgs._ExpandableColumnLabel;
			
			SearchText = MakeShared<FText>();
			SearchTextFilter = MakeShared<TTextFilter<const TItemType&>>(TTextFilter<const TItemType&>::FItemToStringArray::CreateSP(this, &SReplicationTreeView::PopulateSearchStrings));
			SearchTextFilter->OnChanged().AddSP(this, &SReplicationTreeView::OnFilterChanged);
			
			ChildSlot
			[
				SNew(SVerticalBox)

				// Search
				+SVerticalBox::Slot()
				.Padding(1.f)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.Padding(1.f)
					.AutoWidth()
					[
						InArgs._LeftOfSearchBar.Widget
					]

					+SHorizontalBox::Slot()
					.Padding(1.f)
					.FillWidth(1.f)
					[
						SAssignNew(SearchBox, SSearchBox)
						.HintText(LOCTEXT("SearchHint", "Search..."))
						.OnTextChanged(this, &SReplicationTreeView::OnSearchTextChanged)
						.OnTextCommitted(this, &SReplicationTreeView::OnSearchTextCommitted)
						.DelayChangeNotificationsWhileTyping(true)
					]
				]

				// Table row
				+SVerticalBox::Slot()
				.Padding(1.f)
				.FillHeight(1.f)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					[
						CreateTreeView(InArgs)
					]
				]
			];
		}
		
		void OnItemsChanged()
		{
			// Re-filter everything. There should not be many items so filtering everything again should be fine. Calls RequestListRefresh as well.
			OnFilterChanged();
		}
		
		void SetSelectedItems(const TArray<TItemType>& ObjectsToSelect, bool bIsSelected)
		{
			TreeView->ClearSelection();
			TreeView->SetItemSelection(ObjectsToSelect, bIsSelected);
		}
		void SetExpandedItems(const TArray<TItemType>& ObjectsToSelect, bool bIsExpanded)
		{
			for (TItemType Item : ObjectsToSelect)
			{
				TreeView->SetItemExpansion(Item, bIsExpanded);
			}
		}
		
		TArray<TItemType> GetSelectedItems() const { return TreeView->GetSelectedItems(); }
		
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:

		/** The widget being used for search. */
		TSharedPtr<SSearchBox> SearchBox;
		/** Used to highlight text in text widgets */
		TSharedPtr<FText> SearchText;
		/** Performs text search */
		TSharedPtr<TTextFilter<const TItemType&>> SearchTextFilter;

		/** ListView's header row */
		TSharedPtr<SHeaderRow> HeaderRow;
		/** Displays the contents */
		TSharedPtr<STreeView<TItemType>> TreeView;
		/** The name of the column which will have the SExpandableArrow widget for the tree view. */
		FName ExpandableColumnId;

		TArray<TItemType>* AllRootItems = nullptr;
		TArray<TItemType> FilteredRootItems;

		/** Callback for getting an item's children. */
		FGetItemChildren OnGetChildrenDelegate;
		/** Optional delegate for responding to pressing the delete button */
		FDeleteItems OnDeleteItemsDelegate;
		

		TSharedRef<SWidget> CreateTreeView(const FArguments& InArgs);
		TSharedRef<SHeaderRow> CreateHeaderRow(const FArguments& InArgs);
		TSharedRef<ITableRow> OnGenerateRowWidget(TItemType Item, const TSharedRef<STableViewBase>& OwnerTable);
		void GetRowChildren(TItemType Item, TArray<TItemType>& OutChildren);
		
		void OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type CommitType);
		void OnSearchTextChanged(const FText& InSearchText);

		void PopulateSearchStrings(const TItemType& Item, TArray<FString>& OutSearchStrings);
		void OnFilterChanged();
		bool PassesFilters(const TItemType& Item);
	};

	template <typename TItemType>
	FReply SReplicationTreeView<TItemType>::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (InKeyEvent.GetKey() == EKeys::Delete && OnDeleteItemsDelegate.IsBound())
		{
			OnDeleteItemsDelegate.Execute(TreeView->GetSelectedItems());
			return FReply::Handled();
		}
	
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	template <typename TItemType>
	TSharedRef<SWidget> SReplicationTreeView<TItemType>::CreateTreeView(const FArguments& InArgs)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.6, 0.6, 0.6)))
			.Padding(0)
			[
				SAssignNew(TreeView, STreeView<TItemType>)
				.OnGetChildren(this, &SReplicationTreeView::GetRowChildren)
				.TreeItemsSource(&FilteredRootItems)
				.OnGenerateRow(this, &SReplicationTreeView::OnGenerateRowWidget)
				.OnContextMenuOpening(InArgs._OnContextMenuOpening)
				.OnSelectionChanged_Lambda([OnSelectionChanged = InArgs._OnSelectionChanged](auto, auto){ OnSelectionChanged.ExecuteIfBound(); })
				.SelectionMode(InArgs._SelectionMode)
				.AllowOverscroll(EAllowOverscroll::No)
				.HeaderRow(CreateHeaderRow(InArgs))
			];
	}

	template <typename TItemType>
	TSharedRef<SHeaderRow> SReplicationTreeView<TItemType>::CreateHeaderRow(const FArguments& InArgs)
	{
		TArray<TReplicationColumn<TItemType>> Columns = InArgs._Columns;
		Columns.Sort([](const TReplicationColumn<TItemType>& Left, const TReplicationColumn<TItemType>& Right) { return Left.GetColumnSortOrderValue() < Right.GetColumnSortOrderValue(); });
		
		HeaderRow = SNew(SHeaderRow);
		TSet<FName> DuplicateColumnDetection;
		for (TReplicationColumn<TItemType>& Column : Columns)
		{
			check(!DuplicateColumnDetection.Contains(Column.ColumnId));
			DuplicateColumnDetection.Add(Column.ColumnId);
			
			// SHeaderRow owns the columns and deletes them when destroyed
			TReplicationColumn<TItemType>* ManagedByHeaderRow = new TReplicationColumn<TItemType>(MoveTemp(Column));
			HeaderRow->AddColumn(*ManagedByHeaderRow);
		}

		return HeaderRow.ToSharedRef();
	}

	template <typename TItemType>
	TSharedRef<ITableRow> SReplicationTreeView<TItemType>::OnGenerateRowWidget(TItemType Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const typename SReplicationColumnRow<TItemType>::FGetColumn ColumnGetter = SReplicationColumnRow<TItemType>::FGetColumn::CreateLambda([this](const FName& ColumnId) -> const TReplicationColumn<TItemType>*
		{
			for (const SHeaderRow::FColumn& Column : HeaderRow->GetColumns())
			{
				if (Column.ColumnId == ColumnId)
				{
					return static_cast<const TReplicationColumn<TItemType>*>(&Column);
				}
			}

			checkNoEntry();
			return nullptr;
		});
	
		return SNew(SReplicationColumnRow<TItemType>, OwnerTable)
			.HighlightText(SearchText)
			.ColumnGetter(ColumnGetter)
			.RowData(Item)
			.ExpandableColumnLabel(ExpandableColumnId);
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::GetRowChildren(TItemType Item, TArray<TItemType>& OutChildren)
	{
		if (OnGetChildrenDelegate.IsBound())
		{
			OnGetChildrenDelegate.Execute(Item, [this, &OutChildren](TItemType ItemToAdd)
			{
				if (PassesFilters(ItemToAdd))
				{
					OutChildren.Add(ItemToAdd);
				}
			});
		}
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type CommitType)
	{
		if (!InFilterText.EqualTo(*SearchText))
		{
			OnSearchTextChanged(InFilterText);
		}
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::OnSearchTextChanged(const FText& InSearchText)
	{
		*SearchText = InSearchText;
		SearchTextFilter->SetRawFilterText(InSearchText);
		SearchBox->SetError(SearchTextFilter->GetFilterErrorText());
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::PopulateSearchStrings(const TItemType& Item, TArray<FString>& OutSearchStrings)
	{
		for (const SHeaderRow::FColumn& Column : HeaderRow->GetColumns())
		{
			const TReplicationColumn<TItemType>& CastColumn = static_cast<const TReplicationColumn<TItemType>&>(Column);
			CastColumn.ExecutePopulateSearchString(Item, OutSearchStrings);
		}
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::OnFilterChanged()
	{
		// Try preserving the selected activity.
		TArray<TItemType> SelectedItems = TreeView->GetSelectedItems();

		// Reset the list of displayed activities.
		FilteredRootItems.Reset(AllRootItems->Num());

		// Apply the filter.
		for (const TItemType& Activity : *AllRootItems)
		{
			if (PassesFilters(Activity))
			{
				FilteredRootItems.Add(Activity);
			}
		}

		// Restore/reset the selected activity.
		SelectedItems.SetNum(Algo::RemoveIf(SelectedItems, [this](const TItemType& Item){ return !FilteredRootItems.Contains(Item); }));
		if (!SelectedItems.IsEmpty())
		{
			TreeView->SetItemSelection(SelectedItems, true); // Restore previous selection.
			TreeView->RequestScrollIntoView(SelectedItems[0]);
		}

		TreeView->RequestListRefresh();
	}

	template <typename TItemType>
	bool SReplicationTreeView<TItemType>::PassesFilters(const TItemType& Item)
	{
		return SearchTextFilter->PassesFilter(Item);
	}
}

#undef LOCTEXT_NAMESPACE