// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserReplicationEditorStyle.h"
#include "ReplicationColumn.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

namespace UE::MultiUserReplicationEditor
{
	/**
	 * Displays the summary of an activity recorded and recoverable in the SConcertSessionRecovery list view.
	 */
	template<typename TListItemType>
	class SReplicationColumnRow : public SMultiColumnTableRow<TSharedPtr<TListItemType>>
	{
	public:

		using TColumType = TReplicationColumn<TListItemType>;
		
		DECLARE_DELEGATE_RetVal_OneParam(const TColumType*, FGetColumn,
			const FName& ColumnId
			);
		
		SLATE_BEGIN_ARGS(SReplicationColumnRow)
		{}
			/** Used for highlighting the text being searched. */
			SLATE_ARGUMENT(TSharedPtr<FText>, HighlightText)

			/** Gets columns info about a certain column */
			SLATE_EVENT(FGetColumn, ColumnGetter)

			/** The data to pass to TReplicationColumn::BuildColumnWidget. */
			SLATE_ARGUMENT(TListItemType, RowData)
		
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
			TSharedRef<STableViewBase> InOwner)
		{
			ColumnGetterDelegate = InArgs._ColumnGetter;
			HighlightText = InArgs._HighlightText;
			RowData = InArgs._RowData;
			ExpandableColumnLabel = InArgs._ExpandableColumnLabel;
			
			SMultiColumnTableRow<TSharedPtr<TListItemType>>::Construct({}, InOwner);
		}

		/** Generates the widget representing this row. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			const TColumType* Column = ColumnGetterDelegate.Execute(ColumnName); ensure(Column);
			if (!Column)
			{
				return SNullWidget::NullWidget;
			}
			
			const TSharedRef<SWidget> ColumnWidget = Column->BuildColumnWidget({ HighlightText, RowData });
			const bool bNeedsExpanderArrow = ColumnName == ExpandableColumnLabel;
			if (!bNeedsExpanderArrow)
			{
				return ColumnWidget;
			}
			
			return SNew(SBox)
				.MinDesiredHeight(FMultiUserReplicationEditorStyle::Get().GetFloat("ReplicationTreeView.RowHeight"))
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6, 0, 0, 0)
					[
						SNew(SExpanderArrow, SReplicationColumnRow::SharedThis(this))
						.IndentAmount(12)
					]

					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						ColumnWidget
					]
				];
	
		}

	private:
		
		FGetColumn ColumnGetterDelegate;
		TSharedPtr<FText> HighlightText;
		TListItemType RowData;
		FName ExpandableColumnLabel;
	};
}