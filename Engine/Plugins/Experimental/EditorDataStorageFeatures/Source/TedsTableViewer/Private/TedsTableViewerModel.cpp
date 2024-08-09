// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsTableViewerModel.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "QueryStack/IQueryStackNode_Row.h"
#include "TedsTableViewerColumn.h"
#include "TedsTableViewerUtils.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"

namespace UE::EditorDataStorage
{
	FTedsTableViewerModel::FTedsTableViewerModel(const TSharedPtr<IQueryStackNode_Row>& InRowQueryStack,
		const TArray<TWeakObjectPtr<const UScriptStruct>>& InRequestedColumns, const TArray<FName>& InCellWidgetPurposes,
		const FIsItemVisible& InIsItemVisibleDelegate)
		: RowQueryStack(InRowQueryStack)
		, RequestedTedsColumns(InRequestedColumns)
		, CellWidgetPurposes(InCellWidgetPurposes)
		, IsItemVisible(InIsItemVisibleDelegate)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		
		checkf(Registry, TEXT("Unable to create a Table Viewer before the Typed Element Registry is initialized."));
		if (Registry)
		{
			Storage = Registry->GetMutableDataStorage();
			StorageUi = Registry->GetMutableDataStorageUi();
			StorageCompatibility = Registry->GetMutableDataStorageCompatibility();
		}
		
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FTedsTableViewerModel::Tick), 0);

		GenerateColumns();
		Refresh();
	}

	FTedsTableViewerModel::~FTedsTableViewerModel()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	
	void FTedsTableViewerModel::Refresh()
	{
		Items.Empty();
		
		for(const TypedElementDataStorage::RowHandle RowHandle : RowQueryStack->GetOrderedRowList())
		{
			if(IsRowDisplayable(RowHandle))
			{
				Items.Add(RowHandle);
			}
		}

		CachedRowQueryStackRevision = RowQueryStack->GetRevisionId();

		OnModelChanged.Broadcast();
	}

	bool FTedsTableViewerModel::IsRowDisplayable(TypedElementDataStorage::RowHandle InRowHandle) const
	{
		// We don't want to display any second level widgets (widgets for widgets and so on...) because they will keep cause the table viewer to
		// infinitely grow as you keep scrolling (which creates new widgets)
		if(Storage->HasColumns<FTypedElementSlateWidgetReferenceColumn>(InRowHandle))
		{
			if(const FTypedElementRowReferenceColumn* RowReferenceColumn = Storage->GetColumn<FTypedElementRowReferenceColumn>(InRowHandle))
			{
				if(Storage->HasColumns<FTypedElementSlateWidgetReferenceColumn>(RowReferenceColumn->Row))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool FTedsTableViewerModel::Tick(float DeltaTime)
	{
		// If the revision ID has changed, refresh to update our rows
		if(RowQueryStack->GetRevisionId() != CachedRowQueryStackRevision)
		{
			Refresh();
		}

		// Tick all the individual column views
		for(const TSharedRef<FTedsTableViewerColumn>&Column : ColumnsView)
		{
			Column->Tick();
		}
		
		return true;
	}

	const TArray<TableViewerItemPtr>& FTedsTableViewerModel::GetItems() const
	{
		return Items;
	}

	uint64 FTedsTableViewerModel::GetRowCount() const
	{
		return Items.Num();
	}

	uint64 FTedsTableViewerModel::GetColumnCount() const
	{
		return ColumnsView.Num();
	}

	TSharedPtr<FTedsTableViewerColumn> FTedsTableViewerModel::GetColumn(const FName& ColumnName) const
	{
		const TSharedRef<FTedsTableViewerColumn>* Column = ColumnsView.FindByPredicate([ColumnName]
			(const TSharedRef<FTedsTableViewerColumn>& InColumn)
		{
			return InColumn->GetColumnName() == ColumnName;
		});
		
		if(Column)
		{
			return *Column;
		}

		return nullptr;
	}

	void FTedsTableViewerModel::ForEachColumn(const TFunctionRef<void(const TSharedRef<FTedsTableViewerColumn>&)>& Delegate) const
	{
		for(const TSharedRef<FTedsTableViewerColumn>& Column : ColumnsView)
		{
			Delegate(Column);
		}
	}

	FTedsTableViewerModel::FOnModelChanged& FTedsTableViewerModel::GetOnModelChanged()
	{
		return OnModelChanged;
	}

	void FTedsTableViewerModel::SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& InColumns)
	{
		RequestedTedsColumns = InColumns;
		GenerateColumns();
	}

	void FTedsTableViewerModel::AddCustomColumn(const TSharedRef<FTedsTableViewerColumn>& InColumn)
	{
		// Table Viewer TODO: We should allow users to specify sort order using a TEDS column on the UI row, but for now we put any custom
		// columns on the front
		ColumnsView.Insert(InColumn, 0);
	}

	void FTedsTableViewerModel::GenerateColumns()
	{
		using MatchApproach = ITypedElementDataStorageUiInterface::EMatchApproach;
		int32 IndexOffset = 0;

		ColumnsView.Empty();
		
		// A Map of TEDS Columns -> UI columns so we can add them in the same order they were specified
		TMap<TWeakObjectPtr<const UScriptStruct>, TSharedRef<FTedsTableViewerColumn>> NewColumnMap;

		// A copy of the columns to preserve the order since TEDS UI modifies the array directly
		TArray<TWeakObjectPtr<const UScriptStruct>> ColumnsCopy = RequestedTedsColumns;
		
		// Lambda to create the constructor for a given list of columns
		auto ColumnConstructor = [this, &IndexOffset, &NewColumnMap](
			TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> MatchedColumns)
			{
				TSharedPtr<FTypedElementWidgetConstructor> CellConstructor(Constructor.Release());

				TSharedPtr<FTypedElementWidgetConstructor> HeaderConstructor =
					TableViewerUtils::CreateHeaderWidgetConstructor(*StorageUi, TypedElementDataStorage::FMetaDataView(), MatchedColumns, CellWidgetPurposes);
			
				FName NameId = TableViewerUtils::FindLongestMatchingName(MatchedColumns, IndexOffset);

				TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnsCopy(MatchedColumns);
				TSharedRef<FTedsTableViewerColumn> Column = MakeShared<FTedsTableViewerColumn>(NameId, CellConstructor, MoveTemp(MatchedColumnsCopy), HeaderConstructor);
			
				Column->SetIsRowVisibleDelegate(FTedsTableViewerColumn::FIsRowVisible::CreateRaw(this, &FTedsTableViewerModel::IsRowVisible));

				for (const TWeakObjectPtr<const UScriptStruct>& ColumnType : MatchedColumns)
				{
					NewColumnMap.Emplace(ColumnType, Column);
				}
			
				++IndexOffset;
				return true;
			};

		// Create the widget constructors for the columns
		for(const FName& WidgetPurpose : CellWidgetPurposes)
		{
			StorageUi->CreateWidgetConstructors(WidgetPurpose, MatchApproach::LongestMatch, ColumnsCopy, 
			TypedElementDataStorage::FMetaDataView(), ColumnConstructor);
		}

		// For any remaining columns, we'll try to find and use any default widgets
		for (TWeakObjectPtr<const UScriptStruct>& ColumnType : ColumnsCopy)
		{
			auto AssignWidgetToColumn = [this, ColumnType, &IndexOffset, &NewColumnMap](
				TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)
			{
				TSharedPtr<FTypedElementWidgetConstructor> CellConstructor(Constructor.Release());

				TSharedPtr<FTypedElementWidgetConstructor> HeaderConstructor =
					TableViewerUtils::CreateHeaderWidgetConstructor(*StorageUi, TypedElementDataStorage::FMetaDataView(), {ColumnType}, CellWidgetPurposes);

				FName NameId = FName(ColumnType->GetDisplayNameText().ToString());

				TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnsCopy({ColumnType});
				TSharedRef<FTedsTableViewerColumn> Column = MakeShared<FTedsTableViewerColumn>(NameId, CellConstructor, MoveTemp(MatchedColumnsCopy), HeaderConstructor);
				
				Column->SetIsRowVisibleDelegate(FTedsTableViewerColumn::FIsRowVisible::CreateRaw(this, &FTedsTableViewerModel::IsRowVisible));

				NewColumnMap.Emplace(ColumnType, Column);
				
				++IndexOffset;
				return false;
			};

			const int32 BeforeIndexOffset = IndexOffset;
			
			for(const FName& WidgetPurpose : CellWidgetPurposes)
			{
				const FName DefaultWidgetPurpose(WidgetPurpose.ToString() + TEXT(".Default"));

				StorageUi->CreateWidgetConstructors(DefaultWidgetPurpose, TypedElementDataStorage::FMetaDataView(), AssignWidgetToColumn);
				
				if (BeforeIndexOffset != IndexOffset)
				{
					break;
				}
			}

			if (BeforeIndexOffset == IndexOffset)
			{
				++IndexOffset;
			}
		}

		// Add the actual UI columns in the order the Teds Columns were specified
		for (const TWeakObjectPtr<const UScriptStruct>& ColumnType : RequestedTedsColumns)
		{
			if(const TSharedRef<FTedsTableViewerColumn>* FoundColumn = NewColumnMap.Find(ColumnType))
			{
				// If the column already exists, a widget matched it and a previously encountered column together and was already added
				// so we can safely ignore it here
				if(!GetColumn((*FoundColumn)->GetColumnName()))
				{
					ColumnsView.Add(*FoundColumn);
				}
			}
		}
	}

	bool FTedsTableViewerModel::IsRowVisible(TypedElementDataStorage::RowHandle InRowHandle) const
	{
		if(!IsItemVisible.IsBound())
		{
			return true;
		}
		
		// Table Viewer TODO: We can probably store a map of the items instead but this works for now
		for(const TableViewerItemPtr& Item : Items)
		{
			if(Item == InRowHandle)
			{
				return IsItemVisible.Execute(Item);
			}
		}

		return true;
	}


}
