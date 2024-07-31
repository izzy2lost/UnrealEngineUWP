// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Framework/TypedElementMetaData.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "Widgets/Views/SHeaderRow.h"

class ITypedElementDataStorageCompatibilityInterface;
class ITypedElementDataStorageUiInterface;
class ITypedElementDataStorageInterface;
struct FTypedElementWidgetConstructor;
class SWidget;

namespace UE::EditorDataStorage
{
	/*
	 * Class representing a column in the UI of the table viewer. Can be constructed using a NameID and a WidgetConstructor to create the actual
	 * widgets for rows (optionally supplying a header widget constructor and widget metadata to use)
	 */
	class FTedsTableViewerColumn
	{
	public:

		// Delegate to check if a row is currently visible in the owning table viewer's UI
		DECLARE_DELEGATE_RetVal_OneParam(bool, FIsRowVisible, const TypedElementDataStorage::RowHandle);

		TEDSTABLEVIEWER_API FTedsTableViewerColumn(
			const FName& ColumnName, // The unique ID of this column
			const TSharedPtr<FTypedElementWidgetConstructor>& InCellWidgetConstructor, // The widget constructor to use for this column
			const TArray<TWeakObjectPtr<const UScriptStruct>>& InMatchedColumns = {}, // An optional list of matched Teds columns 
			const TSharedPtr<FTypedElementWidgetConstructor>& InHeaderWidgetConstructor = nullptr, // Optional constructor to use for the header widget
			const TypedElementDataStorage::FMetaDataView& InWidgetMetaData = TypedElementDataStorage::FMetaDataView()); // Optional metadata to use when constructing widgets

		TEDSTABLEVIEWER_API ~FTedsTableViewerColumn();
		
		TEDSTABLEVIEWER_API	TSharedPtr<SWidget> ConstructRowWidget(TypedElementDataStorage::RowHandle RowHandle) const;
		
		TEDSTABLEVIEWER_API SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() const;
		
		TEDSTABLEVIEWER_API void Tick();
		
		TEDSTABLEVIEWER_API void SetIsRowVisibleDelegate(const FIsRowVisible& InIsRowVisibleDelegate);
		
		TEDSTABLEVIEWER_API FName GetColumnName() const;

		TEDSTABLEVIEWER_API TConstArrayView<TWeakObjectPtr<const UScriptStruct>> GetMatchedColumns() const;


	protected:

		void RegisterQueries();
		void UnRegisterQueries();
		bool IsRowVisible(const TypedElementDataStorage::RowHandle InRowHandle) const;
		void UpdateWidgets();
		
	private:
		// The ID of the column
		FName ColumnName;

		// Widget Constructors
		TSharedPtr<FTypedElementWidgetConstructor> CellWidgetConstructor;
		TSharedPtr<FTypedElementWidgetConstructor> HeaderWidgetConstructor;

		// Teds Columns this widget constructor matched with
		TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
		
		// The Metadata used to create widgets
		TypedElementDataStorage::FMetaDataView WidgetMetaData;

		// TEDS Constructs
		ITypedElementDataStorageInterface* Storage;
		ITypedElementDataStorageUiInterface* StorageUi;
		ITypedElementDataStorageCompatibilityInterface* StorageCompatibility;

		// Queries used to virtualize widgets when a column is added to/remove from a row
		TArray<TypedElementDataStorage::QueryHandle> InternalObserverQueries;
		TypedElementDataStorage::QueryHandle WidgetQuery;
		TMap<TypedElementDataStorage::RowHandle, bool> RowsToUpdate;

		// Delegate to check if a row is visible in the owning table viewer
		FIsRowVisible IsRowVisibleDelegate;
	};
}
