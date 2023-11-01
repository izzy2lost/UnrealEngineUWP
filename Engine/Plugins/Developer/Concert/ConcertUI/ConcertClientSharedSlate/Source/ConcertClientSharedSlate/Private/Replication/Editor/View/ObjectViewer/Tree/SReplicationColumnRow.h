// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/ReplicationColumn.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

namespace UE::ConcertClientSharedSlate
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
		DECLARE_DELEGATE_RetVal_TwoParams(TSharedPtr<SWidget>, FOverrideColumnWidget, const FName& ColumnName, const TListItemType& RowData);
		
		SLATE_BEGIN_ARGS(SReplicationColumnRow)
			: _RowHeight(20.f)
		{}
			/** Used for highlighting the text being searched. */
			SLATE_ARGUMENT(TSharedPtr<FText>, HighlightText)

			/** Gets columns info about a certain column */
			SLATE_EVENT(FGetColumn, ColumnGetter)

			/**
			 * Optional. If the delegate returns non-null, that widget will be used instead of the one the column would generate.
			 * This is useful, e.g. if you want to generate a separator widget between items.
			 */
			SLATE_EVENT(FOverrideColumnWidget, OverrideColumnWidget)

			/** The data to pass to TReplicationColumn::BuildColumnWidget. */
			SLATE_ARGUMENT(TSharedPtr<TListItemType>, RowData)
		
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)

			/** The height of the row */
			SLATE_ARGUMENT(float, RowHeight)
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
			TSharedRef<STableViewBase> InOwner)
		{
			ColumnGetterDelegate = InArgs._ColumnGetter;
			OverrideColumnWidgetDelegate = InArgs._OverrideColumnWidget;
			HighlightText = InArgs._HighlightText;
			RowData = InArgs._RowData;
			ExpandableColumnLabel = InArgs._ExpandableColumnLabel;
			RowHeight = InArgs._RowHeight;
			
			SMultiColumnTableRow<TSharedPtr<TListItemType>>::Construct({}, InOwner);
		}

		/** Generates the widget representing this row. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			const TSharedPtr<SWidget> ColumnOverride = OverrideColumnWidgetDelegate.IsBound()
				? OverrideColumnWidgetDelegate.Execute(ColumnName, *RowData.Get())
				: nullptr;
			if (ColumnOverride)
			{
				return ColumnOverride.ToSharedRef();
			}
			
			const TColumType* Column = ColumnGetterDelegate.Execute(ColumnName); ensure(Column);
			if (!Column)
			{
				return SNullWidget::NullWidget;
			}
			
			const TSharedRef<SWidget> ColumnWidget = Column->BuildColumnWidget({ HighlightText, *RowData.Get() });
			const bool bNeedsExpanderArrow = ColumnName == ExpandableColumnLabel;
			if (!bNeedsExpanderArrow)
			{
				return SNew(SBox)
					.MinDesiredHeight(RowHeight)
					.VAlign(VAlign_Center)
					[
						ColumnWidget
					];
			}
			
			return SNew(SBox)
				.MinDesiredHeight(RowHeight)
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
					.VAlign(VAlign_Center)
					[
						ColumnWidget
					]
				];
	
		}

	private:
		
		FGetColumn ColumnGetterDelegate;
		FOverrideColumnWidget OverrideColumnWidgetDelegate;
		TSharedPtr<FText> HighlightText;
		TSharedPtr<TListItemType> RowData;
		FName ExpandableColumnLabel;
		float RowHeight;
	};
}