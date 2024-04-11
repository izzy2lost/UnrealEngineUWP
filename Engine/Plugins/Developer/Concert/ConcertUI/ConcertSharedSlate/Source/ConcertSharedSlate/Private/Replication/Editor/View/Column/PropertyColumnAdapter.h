// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Data/PropertyData.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Replication/Editor/View/Column/IReplicationTreeColumn.h"

namespace UE::ConcertSharedSlate
{
	/** Adapts an IPropertyTreeColumn to IReplicationTreeColumn<FPropertyData>. It simply passes additional info down to IPropertyTreeColumn. */
	class FPropertyColumnAdapter : public IReplicationTreeColumn<FPropertyData>
	{
	public:

		static TArray<TReplicationColumnEntry<FPropertyData>> Transform(const TArray<FPropertyColumnEntry>& Entries)
		{
			TArray<TReplicationColumnEntry<FPropertyData>> Result;
			Algo::Transform(Entries, Result, [](const FPropertyColumnEntry& Entry) -> TReplicationColumnEntry<FPropertyData>
			{
				return {
					TReplicationColumnDelegates<FPropertyData>::FCreateColumn::CreateLambda([CreateDelegate = Entry.CreateColumn]()
					{
						return MakeShared<FPropertyColumnAdapter>(CreateDelegate.Execute());
					}),
					Entry.ColumnId,
					Entry.ColumnInfo
				};
			});
			return Result;
		}
		
		FPropertyColumnAdapter(TSharedRef<IPropertyTreeColumn> InAdaptedColumn)
			: AdaptedColumn(MoveTemp(InAdaptedColumn))
		{}
		
		virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override { return AdaptedColumn->CreateHeaderRowArgs(); }
		virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
		{
			return AdaptedColumn->GenerateColumnWidget({ InArgs.HighlightText, Transform(InArgs.RowItem) });
		}
		virtual void PopulateSearchString(const FPropertyData& InItem, TArray<FString>& InOutSearchStrings) const override
		{
			return AdaptedColumn->PopulateSearchString(Transform(InItem), InOutSearchStrings);
		}

		virtual bool CanBeSorted() const override { return AdaptedColumn->CanBeSorted(); }
		virtual bool IsLessThan(const FPropertyData& Left, const FPropertyData& Right) const override { return AdaptedColumn->IsLessThan(Transform(Left), Transform(Right)); }

	private:

		TSharedRef<IPropertyTreeColumn> AdaptedColumn;

		static FPropertyTreeRowContext Transform(FPropertyData Data)
		{
			return { MoveTemp(Data) };
		}
	};
}
