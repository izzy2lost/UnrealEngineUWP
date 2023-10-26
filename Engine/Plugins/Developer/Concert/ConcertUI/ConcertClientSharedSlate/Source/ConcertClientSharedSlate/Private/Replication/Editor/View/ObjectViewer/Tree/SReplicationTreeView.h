// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/ReplicationColumn.h"
#include "SReplicationColumnRow.h"

#include "Algo/RemoveIf.h"
#include "Misc/TextFilter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SReplicationListView"

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Shared code for the list view for replicated actors and properties.
	 * It is a table view that is searchable with a search box and exposes slots to add more filter widgets, such as SBasicFilterBar.
	 */
	template<typename TItemType>
	class SReplicationTreeView : public SCompoundWidget
	{
	public:

		using TOverrideColumnWidget = typename SReplicationColumnRow<TItemType>::FOverrideColumnWidget;

		DECLARE_DELEGATE_OneParam(FDeleteItems, const TArray<TSharedPtr<TItemType>>& SelectedItems);
		DECLARE_DELEGATE_TwoParams(FGetItemChildren, TSharedPtr<TItemType> Item, TFunctionRef<void(TSharedPtr<TItemType>)> ProcessChild);
		DECLARE_DELEGATE(FOnSelectionChanged);
		
		DECLARE_DELEGATE_RetVal_OneParam(bool, FCustomFilter, const TSharedPtr<TItemType>& Item);
		DECLARE_DELEGATE_RetVal_OneParam(bool, FIsSearchableItem, const TSharedPtr<TItemType>& Item);

		enum class EContent
		{
			TreeView,
			Custom
		};

		SLATE_BEGIN_ARGS(SReplicationTreeView<TItemType>)
			: _HeaderRowVisibility(EVisibility::Visible)
			, _SelectionMode(ESelectionMode::Single)
		{}
			/** The items to display */
			SLATE_ARGUMENT(TArray<TSharedPtr<TItemType>>*, RootItemsSource)

			/** Gets an items children for the tree view */
			SLATE_EVENT(FGetItemChildren, OnGetChildren)

			/** Optional. Called when the user presses the delete key */
			SLATE_EVENT(FDeleteItems, OnDeleteItems)

			/** Called to generate the context menu for an item */
			SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

			/** Called when the selection changes. Call GetSelectedItems to get the selected items. */
			SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

			/** Optional callback to do even more filtering of items. */
			SLATE_EVENT(FCustomFilter, FilterItem)
		
			/**
			 * Optional. If the delegate returns non-null, that widget will be used instead of the one the column would generate.
			 * This is useful, e.g. if you want to generate a separator widget between items.
			 */
			SLATE_EVENT(TOverrideColumnWidget, OverrideColumnWidget)
			/** Optional callback for determining whether this item can be searched. */
			SLATE_EVENT(FIsSearchableItem, IsSearchableItem)
			
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<TReplicationColumn<TItemType>>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
			/** Visibility of the header row */
			SLATE_ARGUMENT(EVisibility, HeaderRowVisibility)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)

			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)
			/** Optional widget to add between the search bar and the table view (e.g. a SBasicFilterBar). */
			SLATE_NAMED_SLOT(FArguments, RowBelowSearchBar)

			/** Optional, alternate content to show instead of the tree view when there are no rows. */
			SLATE_NAMED_SLOT(FArguments, NoItemsContent)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			AllRootItems = InArgs._RootItemsSource;
			check(AllRootItems);

			OnGetChildrenDelegate = InArgs._OnGetChildren;
			OnDeleteItemsDelegate = InArgs._OnDeleteItems;
			CustomFilterDelegate = InArgs._FilterItem;
			OverrideColumnWidget = InArgs._OverrideColumnWidget;
			IsSearchableItemDelegate = InArgs._IsSearchableItem;
			ExpandableColumnId = InArgs._ExpandableColumnLabel;
			
			SearchText = MakeShared<FText>();
			SearchTextFilter = MakeShared<TTextFilter<const TSharedPtr<TItemType>&>>(TTextFilter<const TSharedPtr<TItemType>&>::FItemToStringArray::CreateSP(this, &SReplicationTreeView::PopulateSearchStrings));
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

				// Optional slot between search bar and table, e.g. for an external SBasicFilterBar
				+SVerticalBox::Slot()
				.Padding(1.f)
				.AutoHeight()
				[
					InArgs._RowBelowSearchBar.Widget
				]

				// Table row
				+SVerticalBox::Slot()
				.Padding(1.f)
				.FillHeight(1.f)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					.FillSize(1.f)
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
		
		void SetSelectedItems(const TArray<TSharedPtr<TItemType>>& ObjectsToSelect, bool bIsSelected)
		{
			TreeView->ClearSelection();
			TreeView->SetItemSelection(ObjectsToSelect, bIsSelected);
		}
		void SetExpandedItems(const TArray<TSharedPtr<TItemType>>& ObjectsToSelect, bool bIsExpanded)
		{
			for (TSharedPtr<TItemType> Item : ObjectsToSelect)
			{
				TreeView->SetItemExpansion(Item, bIsExpanded);
			}
		}
		
		TArray<TSharedPtr<TItemType>> GetSelectedItems() const { return TreeView->GetSelectedItems(); }
		const TArray<TSharedPtr<TItemType>>& GetFilteredRootItems() const { return FilteredRootItems; }
		
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:

		/** The widget being used for search. */
		TSharedPtr<SSearchBox> SearchBox;
		/** Used to highlight text in text widgets */
		TSharedPtr<FText> SearchText;
		/** Performs text search */
		TSharedPtr<TTextFilter<const TSharedPtr<TItemType>&>> SearchTextFilter;

		/** ListView's header row */
		TSharedPtr<SHeaderRow> HeaderRow;
		/** Displays the contents */
		TSharedPtr<STreeView<TSharedPtr<TItemType>>> TreeView;
		/** The name of the column which will have the SExpandableArrow widget for the tree view. */
		FName ExpandableColumnId;

		TArray<TSharedPtr<TItemType>>* AllRootItems = nullptr;
		TArray<TSharedPtr<TItemType>> FilteredRootItems;

		/** Callback for getting an item's children. */
		FGetItemChildren OnGetChildrenDelegate;
		/** Optional delegate for responding to pressing the delete button */
		FDeleteItems OnDeleteItemsDelegate;
		/** Optional delegate for filtering the items even more. */
		FCustomFilter CustomFilterDelegate;
		/** Optional delegate for overriding the column widgets. */
		TOverrideColumnWidget OverrideColumnWidget;
		/** Optional callback for determining whether this item can be filtered. If false, it will not be shown when searched. */
		FIsSearchableItem IsSearchableItemDelegate;
		

		TSharedRef<SWidget> CreateTreeView(const FArguments& InArgs);
		TSharedRef<SHeaderRow> CreateHeaderRow(const FArguments& InArgs);
		TSharedRef<ITableRow> OnGenerateRowWidget(TSharedPtr<TItemType> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void GetRowChildren(TSharedPtr<TItemType> Item, TArray<TSharedPtr<TItemType>>& OutChildren);
		
		void OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type CommitType);
		void OnSearchTextChanged(const FText& InSearchText);

		void PopulateSearchStrings(const TSharedPtr<TItemType>& Item, TArray<FString>& OutSearchStrings);
		void OnFilterChanged();
		bool PassesFilters(const TSharedPtr<TItemType>& Item);
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
		TSharedPtr<SVerticalBox> VerticalBox;
		
		TSharedRef<SWidget> Result = SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.6, 0.6, 0.6)))
			.Padding(0)
			[
				SAssignNew(VerticalBox, SVerticalBox)
				
				+SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<TItemType>>)
					.OnGetChildren(this, &SReplicationTreeView::GetRowChildren)
					.TreeItemsSource(&FilteredRootItems)
					.OnGenerateRow(this, &SReplicationTreeView::OnGenerateRowWidget)
					.OnContextMenuOpening(InArgs._OnContextMenuOpening)
					.OnSelectionChanged_Lambda([OnSelectionChanged = InArgs._OnSelectionChanged](auto, auto){ OnSelectionChanged.ExecuteIfBound(); })
					.SelectionMode(InArgs._SelectionMode)
					.AllowOverscroll(EAllowOverscroll::No)
					.HeaderRow(CreateHeaderRow(InArgs))
				]

				+SVerticalBox::Slot()
				.FillHeight(1.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 0.f, 20.f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this](){ return AllRootItems->IsEmpty() ? 1 : 0; })
					.Visibility_Lambda([this](){ return AllRootItems->IsEmpty() || FilteredRootItems.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
					+SWidgetSwitcher::Slot() [ SNew(STextBlock).Text(LOCTEXT("AllFiltered", "All items are filtered.")) ]
					+SWidgetSwitcher::Slot()
					[
						InArgs._NoItemsContent.Widget
					]
				]
			];
		
		return Result;
	}

	template <typename TItemType>
	TSharedRef<SHeaderRow> SReplicationTreeView<TItemType>::CreateHeaderRow(const FArguments& InArgs)
	{
		TArray<TReplicationColumn<TItemType>> Columns = InArgs._Columns;
		Columns.Sort([](const TReplicationColumn<TItemType>& Left, const TReplicationColumn<TItemType>& Right) { return Left.GetColumnSortOrderValue() < Right.GetColumnSortOrderValue(); });
		
		HeaderRow = SNew(SHeaderRow).Visibility(InArgs._HeaderRowVisibility);
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
	TSharedRef<ITableRow> SReplicationTreeView<TItemType>::OnGenerateRowWidget(TSharedPtr<TItemType> Item, const TSharedRef<STableViewBase>& OwnerTable)
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
			.OverrideColumnWidget(OverrideColumnWidget)
			.RowData(Item)
			.ExpandableColumnLabel(ExpandableColumnId);
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::GetRowChildren(TSharedPtr<TItemType> Item, TArray<TSharedPtr<TItemType>>& OutChildren)
	{
		if (OnGetChildrenDelegate.IsBound())
		{
			OnGetChildrenDelegate.Execute(Item, [this, &OutChildren](TSharedPtr<TItemType> ItemToAdd)
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
	void SReplicationTreeView<TItemType>::PopulateSearchStrings(const TSharedPtr<TItemType>& Item, TArray<FString>& OutSearchStrings)
	{
		if (IsSearchableItemDelegate.IsBound() && !IsSearchableItemDelegate.Execute(Item))
		{
			return;
		}
	
		for (const SHeaderRow::FColumn& Column : HeaderRow->GetColumns())
		{
			const TReplicationColumn<TItemType>& CastColumn = static_cast<const TReplicationColumn<TItemType>&>(Column);
			CastColumn.ExecutePopulateSearchString(*Item.Get(), OutSearchStrings);
		}
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::OnFilterChanged()
	{
		// Try preserving the selected activity.
		TArray<TSharedPtr<TItemType>> SelectedItems = TreeView->GetSelectedItems();

		// Reset the list of displayed activities.
		FilteredRootItems.Reset(AllRootItems->Num());

		// Apply the filter.
		for (const TSharedPtr<TItemType>& Activity : *AllRootItems)
		{
			if (PassesFilters(Activity))
			{
				FilteredRootItems.Add(Activity);
			}
		}

		// Restore/reset the selected activity.
		SelectedItems.SetNum(Algo::RemoveIf(SelectedItems, [this](const TSharedPtr<TItemType>& Item){ return !FilteredRootItems.Contains(Item); }));
		if (!SelectedItems.IsEmpty())
		{
			TreeView->SetItemSelection(SelectedItems, true); // Restore previous selection.
			TreeView->RequestScrollIntoView(SelectedItems[0]);
		}

		TreeView->RequestListRefresh();
	}

	template <typename TItemType>
	bool SReplicationTreeView<TItemType>::PassesFilters(const TSharedPtr<TItemType>& Item)
	{
		return SearchTextFilter->PassesFilter(Item)
			&& (!CustomFilterDelegate.IsBound() || CustomFilterDelegate.Execute(Item));
	}
}

#undef LOCTEXT_NAMESPACE