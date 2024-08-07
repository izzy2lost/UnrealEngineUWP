// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "QueryStack/IQueryStackNode_Row.h"
#include "TedsTableViewerModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class SHeaderRow;

namespace UE::EditorDataStorage
{
	class FTedsTableViewerColumn;

	/*
	 * A table viewer widget can be used to show a visual representation of data in TEDS.
	 * The rows to display can be specified using a RowQueryStack, and the columns to display are directly input into the widget
	 * Example usage:
	 * 
	 *	SNew(STedsTableViewer)
     *		.QueryStack(MakeShared<UE::EditorDataStorage::FQueryStackNode_RowView>(&Rows))
	 *		.Columns({FTypedElementLabelColumn::StaticStruct(), FTypedElementClassTypeInfoColumn::StaticStruct());
	 */
	class STedsTableViewer : public SCompoundWidget
	{
	public:
		
		// Delegate fired when the selection in the table viewer changes
		DECLARE_DELEGATE_OneParam(FOnSelectionChanged, TypedElementDataStorage::RowHandle)

		SLATE_BEGIN_ARGS(STedsTableViewer)
			: _CellWidgetPurposes({TEXT("General.Cell")})
		{
			
		}
		

		// Query Stack that will supply the rows to be displayed
		SLATE_ARGUMENT(TSharedPtr<IQueryStackNode_Row>, QueryStack)

		// The Columns that this table viewer will display
		// Table Viewer TODO: How do we specify column metadata (ReadOnly or ReadWrite)?
		SLATE_ARGUMENT(TArray<TWeakObjectPtr<const UScriptStruct>>, Columns)

		// The widget purposes to use to create the widgets
		SLATE_ARGUMENT(TArray<FName>, CellWidgetPurposes)
		
		// Delegate called when the selection changes
		SLATE_ARGUMENT(FOnSelectionChanged, OnSelectionChanged)

		SLATE_END_ARGS()

	public:
		
		TEDSTABLEVIEWER_API void Construct(const FArguments& InArgs);

		// Clear the current list of columns being displayed and set it to the given list
		TEDSTABLEVIEWER_API void SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& Columns);

		// Add a custom column to display in the table viewer, that doesn't necessarily map to a Teds column
		TEDSTABLEVIEWER_API void AddCustomColumn(const TSharedRef<FTedsTableViewerColumn>& InColumn);


	protected:
		
		TSharedRef<ITableRow> MakeTableRowWidget(TableViewerItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const;

		bool IsItemVisible(TableViewerItemPtr InItem) const;

		void AssignChildSlot();

		void RefreshColumnWidgets();

		void OnListSelectionChanged(TableViewerItemPtr Item, ESelectInfo::Type SelectInfo);

	private:

		// The actual ListView widget that displays the rows
		TSharedPtr<SListView<TableViewerItemPtr>> ListView;

		// The actual header widget
		TSharedPtr<SHeaderRow> HeaderRowWidget;

		// Our model class
		TSharedPtr<FTedsTableViewerModel> Model;

		// Delegate fired when the selection changes
		FOnSelectionChanged OnSelectionChanged;
	};
}