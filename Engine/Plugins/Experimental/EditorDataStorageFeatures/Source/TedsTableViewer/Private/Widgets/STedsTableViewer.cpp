// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/STedsTableViewer.h"

#include "TedsTableViewerColumn.h"
#include "Widgets/STedsTableViewerRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STedsTableViewer"

namespace UE::EditorDataStorage
{
	void STedsTableViewer::Construct(const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;
		
		Model = MakeShared<FTedsTableViewerModel>(InArgs._QueryStack, InArgs._Columns, InArgs._CellWidgetPurposes,
			FTedsTableViewerModel::FIsItemVisible::CreateSP(this, &STedsTableViewer::IsItemVisible));
		
		HeaderRowWidget = SNew( SHeaderRow )
							.CanSelectGeneratedColumn(true);

		ListView = SNew(SListView<TableViewerItemPtr>)
			.HeaderRow(HeaderRowWidget)
			.ListItemsSource(&Model->GetItems())
			.OnGenerateRow(this, &STedsTableViewer::MakeTableRowWidget)
			.OnSelectionChanged(this, &STedsTableViewer::OnListSelectionChanged)
			.SelectionMode(ESelectionMode::Single); // We only support single selection for now in the table viewer
		
		AssignChildSlot();
		
		// Add each Teds column from the model to our header row widget
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		// Whenever the model changes, refresh the list to update the UI
		Model->GetOnModelChanged().AddLambda([this]()
		{
			ListView->RequestListRefresh();
			AssignChildSlot();
		});
	}

	void STedsTableViewer::AssignChildSlot()
	{
		if(Model->GetRowCount() == 0)
		{
			ChildSlot
			[
				SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("EmptyTableViewerQueryText", "The input query has no results"))
					]
			];
		}
		else if(Model->GetColumnCount() == 0)
		{
			ChildSlot
			[
				SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("EmptyTableViewerColumnsText", "There were no columns specified to display"))
					]
			];
		}
		else
		{
			ChildSlot
			[
				ListView.ToSharedRef()	
			];
		}
	}

	void STedsTableViewer::RefreshColumnWidgets()
	{
		HeaderRowWidget->ClearColumns();
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		AssignChildSlot();
	}

	void STedsTableViewer::OnListSelectionChanged(TableViewerItemPtr Item, ESelectInfo::Type SelectInfo)
	{
		if(OnSelectionChanged.IsBound())
		{
			OnSelectionChanged.Execute(Item);
		}
	}

	void STedsTableViewer::SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& Columns)
	{
		Model->SetColumns(Columns);
		RefreshColumnWidgets();
	}

	void STedsTableViewer::AddCustomColumn(const TSharedRef<FTedsTableViewerColumn>& InColumn)
	{
		Model->AddCustomColumn(InColumn);
		RefreshColumnWidgets();
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

#undef LOCTEXT_NAMESPACE //"STedsTableViewer"

