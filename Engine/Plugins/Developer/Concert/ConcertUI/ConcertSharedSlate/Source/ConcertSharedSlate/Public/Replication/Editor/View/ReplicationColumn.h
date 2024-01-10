// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SHeaderRow.h"

namespace UE::ConcertSharedSlate
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
		/** Optional Whether Left < Right for this column's displayed content. Used for sorting. */
		DECLARE_DELEGATE_RetVal_TwoParams(bool, FIsLessThan, const TListItemType& Left, const TListItemType& Right);
		
		SLATE_BEGIN_ARGS(TReplicationColumn)
		{}
			/** Callback to use for generating the widget in the column */
			SLATE_EVENT(FGenerateColumnWidget, GenerateWidgetColumn)
			/** Callback used to generate search items for this column */
			SLATE_EVENT(FPopulateSearchString, PopulateSearchItems)
			/** Optional Whether Left <= Right for this column's displayed content. Used for sorting. */
			SLATE_EVENT(FIsLessThan, IsLessThan)
			/** Where in the row this column is found with respect to the other columns */
			SLATE_ARGUMENT(int32, ColumnSortOrder)
		SLATE_END_ARGS()

		TReplicationColumn(const FArguments& InArgs, const FColumn::FArguments& InColumnArgs)
			: FColumn(InColumnArgs)
			, GenerateColumnWidgetCallback(InArgs._GenerateWidgetColumn)
			, PopulateSearchStringCallback(InArgs._PopulateSearchItems)
			, IsLessThanCallback(InArgs._IsLessThan)
			, ColumnSortOrderValue(InArgs._ColumnSortOrder)
		{}
		
		TReplicationColumn(const FArguments& InArgs, const FColumn& Column)
			: FColumn(Column)
			, GenerateColumnWidgetCallback(InArgs._GenerateWidgetColumn)
			, PopulateSearchStringCallback(InArgs._PopulateSearchItems)
			, IsLessThanCallback(InArgs._IsLessThan)
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
			using TTargetPopulateSearchString = typename TReplicationColumn<TOtherColumnType>::FPopulateSearchString;
			using TTargetLessThan = typename TReplicationColumn<TOtherColumnType>::FIsLessThan;
			
			FGenerateColumnWidget ThisGenerateWidget = GenerateColumnWidgetCallback;
			FPopulateSearchString ThisPopulateSearchString = PopulateSearchStringCallback;
			FIsLessThan ThisIsLessThan = IsLessThanCallback;

			auto PopulateSearchItemsLambda = [TransformOperation, ThisPopulateSearchString](const TOtherColumnType& InOtherRowData, TArray<FString>& InOutSearchStrings)
			{
				ThisPopulateSearchString.Execute(TransformOperation(InOtherRowData), InOutSearchStrings);
			};
			auto IsLessThanLambda = [TransformOperation, ThisIsLessThan](const TOtherColumnType& Left, const TOtherColumnType& Right)
			{
				return ThisIsLessThan.Execute(TransformOperation(Left), TransformOperation(Right));
			};
			
			TTargetPopulateSearchString TargetPopulateSearchString = ThisPopulateSearchString.IsBound()
				? TTargetPopulateSearchString::CreateLambda(PopulateSearchItemsLambda)
				: TTargetPopulateSearchString{};
			TTargetLessThan TargetLessThan = ThisIsLessThan.IsBound()
				? TTargetLessThan::CreateLambda(IsLessThanLambda)
				: TTargetLessThan{};
			
			return TReturnColumnType(
				typename TReturnColumnType::FArguments()
					.GenerateWidgetColumn_Lambda([TransformOperation, ThisGenerateWidget](const typename TReturnColumnType::FBuildArgs& BuildArgs)
					{
						return ThisGenerateWidget.Execute({ BuildArgs.HighlightText, TransformOperation(BuildArgs.RowData)});
					})
					.PopulateSearchItems(TargetPopulateSearchString)
					.IsLessThan(TargetLessThan)
					.ColumnSortOrder(ColumnSortOrderValue),
				*this
			);
		}

		bool CanBeSorted() const { return IsLessThanCallback.IsBound(); }
		bool IsLessThan(const TListItemType& Left, const TListItemType& Right) const
		{
			return ensure(IsLessThanCallback.IsBound())
				&& IsLessThanCallback.Execute(Left, Right);
		}
		
	private:
		
		FGenerateColumnWidget GenerateColumnWidgetCallback;
		FPopulateSearchString PopulateSearchStringCallback;
		FIsLessThan IsLessThanCallback;
		/** Determines whether this column is the first, etc. */
		int32 ColumnSortOrderValue = 0;
	};
}