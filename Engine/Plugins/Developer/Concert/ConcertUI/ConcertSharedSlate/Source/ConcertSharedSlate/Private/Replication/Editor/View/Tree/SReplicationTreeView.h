// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/ReplicationColumn.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
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

namespace UE::ConcertSharedSlate
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
			/** Initial primary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, PrimarySort)
			/** Initial secondary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, SecondarySort)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)

			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)
			/** Optional widget to add to the right of the search bar. */
			SLATE_NAMED_SLOT(FArguments, RightOfSearchBar)
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
			SearchTextFilter->OnChanged().AddSP(this, &SReplicationTreeView::RequestRefilter);
			
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

					+SHorizontalBox::Slot()
					.Padding(1.f)
					.AutoWidth()
					[
						InArgs._RightOfSearchBar.Widget
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

			if (!InArgs._PrimarySort.SortedColumnId.IsNone())
			{
				SetPrimarySortMode(
					InArgs._PrimarySort.SortedColumnId,
					// In case None was specified by accident, use good defaults or no sorting will occur
					InArgs._PrimarySort.SortMode == EColumnSortMode::None ? EColumnSortMode::Ascending : InArgs._PrimarySort.SortMode
					);
			}
			if (!InArgs._SecondarySort.SortedColumnId.IsNone())
			{
				SetSecondarySortMode(
					InArgs._SecondarySort.SortedColumnId, 
					// In case None was specified by accident, use good defaults or no sorting will occur
					InArgs._SecondarySort.SortMode == EColumnSortMode::None ? EColumnSortMode::Ascending : InArgs._SecondarySort.SortMode
					);
			}
		}
		
		void OnItemsChanged()
		{
			// Re-filter everything. There should not be many items so filtering everything again should be fine
			bFilterChanged = true;
			bRequestedSort = true;
		}
		void RequestRefilter()
		{
			bFilterChanged = true;
			bRequestedSort = true;
		}
		void RequestResort()
		{
			bRequestedSort = true;
		}
		
		/** Requests that the given column be resorted, if it currently affects the row sorting. */
		void RequestResortForColumn(const FName& ColumnId)
		{
			if (PrimarySortInfo.SortedColumnId == ColumnId || SecondarySortInfo.SortedColumnId == ColumnId)
			{
				RequestResort();
			}
		}

		void SetPrimarySortMode(FName SortedColumnId, EColumnSortMode::Type SortMode)
		{
			const TReplicationColumn<TItemType>* Column = FindColumnByName(SortedColumnId);
			if (ensure(Column && Column->CanBeSorted()))
			{
				PrimarySortInfo = { SortedColumnId, SortMode };
				
				if (SortedColumnId == SecondarySortInfo.SortedColumnId) // Cannot be primary and secondary at the same time.
				{
					SecondarySortInfo.SortedColumnId = FName();
					SecondarySortInfo.SortMode = EColumnSortMode::None;
				}
			}
		}
		void SetSecondarySortMode(FName SortedColumnId, EColumnSortMode::Type SortMode)
		{
			if (!PrimarySortInfo.IsValid())
			{
				SetPrimarySortMode(SortedColumnId, SortMode);
				return;
			}

			if (PrimarySortInfo.SortedColumnId == SortedColumnId)
			{
				return;
			}
			
			const TReplicationColumn<TItemType>* Column = FindColumnByName(SortedColumnId);
			if (ensure(Column && Column->CanBeSorted()))
			{
				SecondarySortInfo = { SortedColumnId, SortMode };
			}
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
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

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

		bool bFilterChanged = false;
		bool bRequestedSort = false;

		FColumnSortInfo PrimarySortInfo;
		FColumnSortInfo SecondarySortInfo;

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

		// STreeView creation
		TSharedRef<SWidget> CreateTreeView(const FArguments& InArgs);
		TSharedRef<SHeaderRow> CreateHeaderRow(const FArguments& InArgs);
		TSharedRef<ITableRow> OnGenerateRowWidget(TSharedPtr<TItemType> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void GetRowChildren(TSharedPtr<TItemType> Item, TArray<TSharedPtr<TItemType>>& OutChildren);
		
		// Sorting callbacks
		EColumnSortPriority::Type GetColumnSortPriority(const FName ColumnId) const;
		EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
		void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

		// Searching callbacks
		void OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type CommitType);
		void OnSearchTextChanged(const FText& InSearchText);

		// Filtering
		void PopulateSearchStrings(const TSharedPtr<TItemType>& Item, TArray<FString>& OutSearchStrings);
		void ReapplyFilters();
		bool PassesFilters(const TSharedPtr<TItemType>& Item);

		// Sorting
		void Resort();
		void Sort(TArray<TSharedPtr<TItemType>>& Items);
		const TReplicationColumn<TItemType>* FindColumnByName(const FName& ColumnId) const;
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
	void SReplicationTreeView<TItemType>::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (bFilterChanged)
		{
			ReapplyFilters();
		}

		if (bRequestedSort)
		{
			Resort();
		}
		
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
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
					// Preserve the selection when the selected item is hidden due to a parent collapsing
					.AllowInvisibleItemSelection(true)
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
			const FName ColumnId = Column.ColumnId;
			check(!DuplicateColumnDetection.Contains(ColumnId));
			DuplicateColumnDetection.Add(ColumnId);
			
			// SHeaderRow owns the columns and deletes them when destroyed
			TReplicationColumn<TItemType>* ManagedByHeaderRow = new TReplicationColumn<TItemType>(MoveTemp(Column));
			ManagedByHeaderRow->SortPriority.Bind(TAttribute<EColumnSortPriority::Type>::FGetter::CreateSP(this, &SReplicationTreeView::GetColumnSortPriority, ColumnId));
			ManagedByHeaderRow->SortMode.Bind(TAttribute<EColumnSortMode::Type>::FGetter::CreateSP(this, &SReplicationTreeView::GetColumnSortMode, ColumnId));
			ManagedByHeaderRow->OnSortModeChanged.BindSP(this, &SReplicationTreeView::OnColumnSortModeChanged);
			HeaderRow->AddColumn(*ManagedByHeaderRow);
		}

		return HeaderRow.ToSharedRef();
	}

	template <typename TItemType>
	TSharedRef<ITableRow> SReplicationTreeView<TItemType>::OnGenerateRowWidget(TSharedPtr<TItemType> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const typename SReplicationColumnRow<TItemType>::FGetColumn ColumnGetter =
			SReplicationColumnRow<TItemType>::FGetColumn::CreateSP(
				this,
				&SReplicationTreeView::FindColumnByName
			);
	
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
			
			Sort(OutChildren);
		}
	}

	template <typename TItemType>
	EColumnSortPriority::Type SReplicationTreeView<TItemType>::GetColumnSortPriority(const FName ColumnId) const
	{
		if (ColumnId == PrimarySortInfo.SortedColumnId)
		{
			return EColumnSortPriority::Primary;
		}
		if (ColumnId == SecondarySortInfo.SortedColumnId)
		{
			return EColumnSortPriority::Secondary;
		}

		return EColumnSortPriority::Max; // No specific priority.
	}

	template <typename TItemType>
	EColumnSortMode::Type SReplicationTreeView<TItemType>::GetColumnSortMode(const FName ColumnId) const
	{
		if (ColumnId == PrimarySortInfo.SortedColumnId && PrimarySortInfo.IsValid())
		{
			return PrimarySortInfo.SortMode;
		}
		if (ColumnId == SecondarySortInfo.SortedColumnId && SecondarySortInfo.IsValid())
		{
			return SecondarySortInfo.SortMode;
		}
		return EColumnSortMode::None;
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::OnColumnSortModeChanged(
		const EColumnSortPriority::Type SortPriority,
		const FName& ColumnId,
		const EColumnSortMode::Type InSortMode
		)
	{
		const TReplicationColumn<TItemType>* Column = FindColumnByName(ColumnId);
		if (!ensure(Column)
			// Cannot bind
			|| !Column->CanBeSorted())
		{
			return;
		}
		
		if (SortPriority == EColumnSortPriority::Primary)
		{
			SetPrimarySortMode(ColumnId, InSortMode);
		}
		else if (SortPriority == EColumnSortPriority::Secondary)
		{
			SetSecondarySortMode(ColumnId, InSortMode);
		}

		RequestResort();
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
	void SReplicationTreeView<TItemType>::ReapplyFilters()
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

		bFilterChanged = false;
		RequestResort();
		TreeView->RequestListRefresh();
	}

	template <typename TItemType>
	bool SReplicationTreeView<TItemType>::PassesFilters(const TSharedPtr<TItemType>& Item)
	{
		return SearchTextFilter->PassesFilter(Item)
			&& (!CustomFilterDelegate.IsBound() || CustomFilterDelegate.Execute(Item));
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::Resort()
	{
		Sort(FilteredRootItems);
		// GetRowChildren will be called again, which will do the resort the children.
		TreeView->RequestListRefresh();
	}

	template <typename TItemType>
	void SReplicationTreeView<TItemType>::Sort(TArray<TSharedPtr<TItemType>>& Items)
	{
		const auto IsLessThan = [this](const TSharedPtr<TItemType>& Left, const TSharedPtr<TItemType>& Right, const FName& ColumnName, EColumnSortMode::Type SortMode)
		{
			const TReplicationColumn<TItemType>* Column = FindColumnByName(ColumnName);
			if (!ensure(Column) || !ensureMsgf(Column->CanBeSorted(), TEXT("Validate why invariant was broken.")))
			{
				return false;
			}

			switch (SortMode)
			{
				case EColumnSortMode::Ascending: return Column->IsLessThan(*Left, *Right);
				case EColumnSortMode::Descending: return Column->IsLessThan(*Right, *Left);
				case EColumnSortMode::None:
				default: return false;
			};
		};
		
		Items.Sort([this, &IsLessThan](const TSharedPtr<TItemType>& Left, const TSharedPtr<TItemType>& Right)
		{
			if (PrimarySortInfo.IsValid() && IsLessThan(Left, Right, PrimarySortInfo.SortedColumnId, PrimarySortInfo.SortMode))
			{
				return true; // Left comes before Right
			}
			// Check for equality by inverting
			if (PrimarySortInfo.IsValid() && IsLessThan(Right, Left, PrimarySortInfo.SortedColumnId, PrimarySortInfo.SortMode))
			{
				return false; // Right comes before Left
			}
			
			// Lhs == Rhs on the primary column, need to order according to the secondary column if one is set.
			return !SecondarySortInfo.IsValid()
				? false
				: IsLessThan(Left, Right, SecondarySortInfo.SortedColumnId, SecondarySortInfo.SortMode);
		});

		bRequestedSort = false;
	}

	template <typename TItemType>
	const TReplicationColumn<TItemType>* SReplicationTreeView<TItemType>::FindColumnByName(const FName& ColumnId) const
	{
		for (const SHeaderRow::FColumn& Column : HeaderRow->GetColumns())
		{
			if (Column.ColumnId == ColumnId)
			{
				return static_cast<const TReplicationColumn<TItemType>*>(&Column);
			}
		}
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE