// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITypedElementDataStorageInterface;
class ITypedElementDataStorageUiInterface;
struct FTypedElementWidgetConstructor;

namespace UE::EditorDataStorage
{
	// A row in the SRowDetails widget that represents a column on the TEDS row we are viewing
	struct FRowDetailsItem
	{
		// The column we this row is displaying data for
		TWeakObjectPtr<const UScriptStruct> ColumnType;

		// Widget for the column
		TUniquePtr<FTypedElementWidgetConstructor> WidgetConstructor;
		
		TypedElementDataStorage::RowHandle Row = TypedElementDataStorage::InvalidRowHandle;
		TypedElementDataStorage::RowHandle WidgetRow = TypedElementDataStorage::InvalidRowHandle;

		FRowDetailsItem(const TWeakObjectPtr<const UScriptStruct>& InColumnType, TUniquePtr<FTypedElementWidgetConstructor> InWidgetConstructor,
			TypedElementDataStorage::RowHandle InRow);
	};
	
	using RowDetailsItemPtr = TSharedPtr<FRowDetailsItem>;

	// A widget to display all the columns/tags on a given row
	class TEDSTABLEVIEWER_API SRowDetails : public SCompoundWidget
	{
	public:
		
		~SRowDetails() override = default;
		
		SLATE_BEGIN_ARGS(SRowDetails)
			: _ShowAllDetails(true)
		{}

			// Whether or not to show columns that don't have a dedicated widget to represent them
			SLATE_ARGUMENT(bool, ShowAllDetails)

			// Override for the default widget purposes used to create widgets for the columns
			SLATE_ARGUMENT(TArray<FName>, WidgetPurposesOverride)

		
		SLATE_END_ARGS()
		
		void Construct(const FArguments& InArgs);

		// Set the row to view
		void SetRow(TypedElementDataStorage::RowHandle Row);

		// Clear the row to view
		void ClearRow();
		
	private:
		
		TSharedRef<ITableRow> CreateRow(RowDetailsItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

		TSharedPtr<SListView<RowDetailsItemPtr>> ListView;

		TArray<RowDetailsItemPtr> Items;

		ITypedElementDataStorageInterface* DataStorage = nullptr; 
		ITypedElementDataStorageUiInterface* DataStorageUi = nullptr;

		bool bShowAllDetails = true;

		TArray<FName> WidgetPurposes;

	};

	class SRowDetailsRow : public SMultiColumnTableRow<RowDetailsItemPtr>
	{
	public:
		
		SLATE_BEGIN_ARGS(SRowDetailsRow) {}
		
			SLATE_ARGUMENT(RowDetailsItemPtr, Item)

		SLATE_END_ARGS()
		
		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, ITypedElementDataStorageInterface* InDataStorage,
			ITypedElementDataStorageUiInterface* InDataStorageUi);
		
		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	private:
		RowDetailsItemPtr Item;
		ITypedElementDataStorageInterface* DataStorage = nullptr;
		ITypedElementDataStorageUiInterface* DataStorageUi = nullptr;
	};
} // namespace UE::EditorDataStorage