// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SHeaderRow.h"

namespace UE::ConcertClientSharedSlate
{
	template<typename TListItemType>
	class TReplicationColumn : public SHeaderRow::FColumn
	{
	public:

		struct FBuildArgs
		{
			TSharedPtr<FText> HighlightText;
			TListItemType RowData;
		};

		/** Generates the widget content for a column in a row */
		DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FGenerateColumnWidget,
			const FBuildArgs& InArgs
			);

		DECLARE_DELEGATE_TwoParams(FPopulateSearchString,
			const TListItemType& InArgs,
			TArray<FString>& InOutSearchStrings
			);
		
		SLATE_BEGIN_ARGS(TReplicationColumn)
		{}
			/** Callback to use for generating the widget in the column */
			SLATE_EVENT(FGenerateColumnWidget, GenerateWidgetColumn)
			/** Callback used to generate search items for this column */
			SLATE_EVENT(FPopulateSearchString, PopulateSearchItems)
			/** Where in the row this column is found with respect to the other columns */
			SLATE_ARGUMENT(int32, ColumnSortOrder)
		SLATE_END_ARGS()

		TReplicationColumn(const FArguments& InArgs, const FColumn::FArguments& InColumnArgs)
			: FColumn(InColumnArgs)
			, GenerateColumnWidgetCallback(InArgs._GenerateWidgetColumn)
			, PopulateSearchStringCallback(InArgs._PopulateSearchItems)
			, ColumnSortOrderValue(InArgs._ColumnSortOrder)
		{}
		
		TReplicationColumn(const FArguments& InArgs, const FColumn& Column)
			: FColumn(Column)
			, GenerateColumnWidgetCallback(InArgs._GenerateWidgetColumn)
			, PopulateSearchStringCallback(InArgs._PopulateSearchItems)
			, ColumnSortOrderValue(InArgs._ColumnSortOrder)
		{}

		TSharedRef<SWidget> BuildColumnWidget(const FBuildArgs& InArgs) const
		{
			return ensure(GenerateColumnWidgetCallback.IsBound())
				? GenerateColumnWidgetCallback.Execute(InArgs)
				: SNullWidget::NullWidget;
		}

		void ExecutePopulateSearchString(const TListItemType& InRowData, TArray<FString>& InOutSearchStrings) const
		{
			if (PopulateSearchStringCallback.IsBound())
			{
				PopulateSearchStringCallback.Execute(InRowData, InOutSearchStrings);
			}
		}
		
		int32 GetColumnSortOrderValue() const { return ColumnSortOrderValue; }

		/**
		 * Wraps a TReplicationColumn<TListItemType> in a TReplicationColumn<TOtherColumnType> which transforms
		 * TOtherColumnType to TListItemType.
		 *
		 * This is useful e.g. if TOtherColumnType inherits from TListItemType.
		 * The default TTransformOp argument handles this situation.
		 */
		template<typename TOtherColumnType, typename TTransformOp>
		TReplicationColumn<TOtherColumnType> TransformColumn(TTransformOp TransformOperation = [](const TOtherColumnType& RowData) -> TListItemType { return RowData; }) const
		{
			using TReturnColumnType = TReplicationColumn<TOtherColumnType>;
			FGenerateColumnWidget GenerateWidget = GenerateColumnWidgetCallback;
			FPopulateSearchString PopulateSearchString = PopulateSearchStringCallback;
			return TReturnColumnType(
				typename TReturnColumnType::FArguments()
					.GenerateWidgetColumn_Lambda([TransformOperation, GenerateWidget](const typename TReturnColumnType::FBuildArgs& BuildArgs)
					{
						return GenerateWidget.Execute({ BuildArgs.HighlightText, TransformOperation(BuildArgs.RowData)});
					})
					.PopulateSearchItems_Lambda([TransformOperation, PopulateSearchString](const TOtherColumnType& InOtherRowData, TArray<FString>& InOutSearchStrings)
					{
						if (ensure(PopulateSearchString.IsBound()))
						{
							PopulateSearchString.Execute(TransformOperation(InOtherRowData), InOutSearchStrings);
						}
					})
					.ColumnSortOrder(ColumnSortOrderValue),
				*this
			);
		}
		
	private:
		
		FGenerateColumnWidget GenerateColumnWidgetCallback;
		FPopulateSearchString PopulateSearchStringCallback;
		/** Determines whether this column is the first, etc. */
		int32 ColumnSortOrderValue = 0;
	};
}