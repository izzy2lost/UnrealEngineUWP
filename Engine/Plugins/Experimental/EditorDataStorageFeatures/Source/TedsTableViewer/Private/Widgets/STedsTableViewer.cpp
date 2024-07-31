// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/STedsTableViewer.h"

#include "TedsTableViewerColumn.h"
#include "Widgets/STedsTableViewerRow.h"
#include "Widgets/Views/SListView.h"

namespace UE::EditorDataStorage
{
	void STedsTableViewer::Construct(const FArguments& InArgs)
	{
		Model = MakeShared<FTedsTableViewerModel>(InArgs._QueryStack, InArgs._Columns, InArgs._CellWidgetPurposes,
			FTedsTableViewerModel::FIsItemVisible::CreateSP(this, &STedsTableViewer::IsItemVisible));
		
		HeaderRowWidget = SNew( SHeaderRow )
							.CanSelectGeneratedColumn(true);

		ChildSlot
		[
			SAssignNew(ListView, SListView<TableViewerItemPtr>)
			.HeaderRow(HeaderRowWidget)
			.ListItemsSource(&Model->GetItems())
			.OnGenerateRow(this, &STedsTableViewer::MakeTableRowWidget)
		];

		// Add each Teds column from the model to our header row widget
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		// Whenever the model changes, refresh the list to update the UI
		Model->GetOnModelChanged().AddLambda([this]()
		{
			ListView->RequestListRefresh();
		});
	}

	void STedsTableViewer::SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& Columns) const
	{
		Model->SetColumns(Columns);
		
		HeaderRowWidget->ClearColumns();
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});
	}

	bool STedsTableViewer::IsItemVisible(TableViewerItemPtr InItem) const
	{
		return ListView->IsItemVisible(InItem);
	}

	TSharedRef<ITableRow> STedsTableViewer::MakeTableRowWidget(TableViewerItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const
	{
		return SNew(STedsTableViewerRow, OwnerTable, Model.ToSharedRef())
				.Item(InItem);
	}

	
}

