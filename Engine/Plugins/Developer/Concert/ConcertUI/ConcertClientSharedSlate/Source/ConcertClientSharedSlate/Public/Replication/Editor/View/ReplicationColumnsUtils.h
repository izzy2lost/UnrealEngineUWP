// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Replication/Editor/View/ReplicationColumn.h"
#include "Widgets/Input/SCheckBox.h"
#include "Templates/SharedPointer.h"

struct FConcertPropertyChain;

namespace UE::ConcertClientSharedSlate
{
	class IEditableReplicationStreamModel;
	class IReplicationStreamViewer;
	class IReplicationStreamModel;
}

namespace UE::ConcertClientSharedSlate
{
	template<typename TListItemType>
	struct TReplicationColumnDelegates
	{
		DECLARE_DELEGATE_RetVal_OneParam(ECheckBoxState, FGetColumnCheckboxState, const TListItemType& /*ObjectData*/);
		DECLARE_DELEGATE_TwoParams(FOnColumnCheckboxChanged, bool /*bIsChecked*/, const TListItemType& /*ObjectData*/);
		DECLARE_DELEGATE_RetVal_OneParam(bool, FIsEnabled, const TListItemType& /*ObjectData*/);
		DECLARE_DELEGATE_RetVal_OneParam(FText, FGetToolTipText, const TListItemType& /*ObjectData*/);

		TReplicationColumnDelegates(
			FGetColumnCheckboxState InGetCheckboxStateDelegate,
			FOnColumnCheckboxChanged InOnCheckboxChangedDelegate,
			FGetToolTipText InGetToolTipTextDelegate = {},
			FIsEnabled InIsEnabledDelegate = {}
			)
			: GetCheckboxStateDelegate(MoveTemp(InGetCheckboxStateDelegate))
			, OnCheckboxChangedDelegate(MoveTemp(InOnCheckboxChangedDelegate))
			, GetToolTipTextDelegate(MoveTemp(InGetToolTipTextDelegate))
			, IsEnabledDelegate(MoveTemp(InIsEnabledDelegate))
		{}
		
		TReplicationColumnDelegates(
			FGetColumnCheckboxState InGetCheckboxStateDelegate,
			FOnColumnCheckboxChanged InOnCheckboxChangedDelegate,
			TAttribute<FText> InToolTipTextAttribute,
			FIsEnabled InIsEnabledDelegate = {}
			)
			: GetCheckboxStateDelegate(MoveTemp(InGetCheckboxStateDelegate))
			, OnCheckboxChangedDelegate(MoveTemp(InOnCheckboxChangedDelegate))
			, GetToolTipTextDelegate(FGetToolTipText::CreateLambda([GetToolTipText = MoveTemp(InToolTipTextAttribute)](const TListItemType&){ return GetToolTipText.Get(); }))
			, IsEnabledDelegate(MoveTemp(InIsEnabledDelegate))
		{}

		FGetColumnCheckboxState GetCheckboxStateDelegate;
		FOnColumnCheckboxChanged OnCheckboxChangedDelegate;

		/** Optional. Defaults to FText::GetEmpty() */
		FGetToolTipText GetToolTipTextDelegate;
		/** Optional. Default to true. */
		FIsEnabled IsEnabledDelegate;
	};

	/** Util for making a checkbox column */
	template<typename TListItemType>
	TReplicationColumn<TListItemType> MakeCheckboxColumn(
		FName ColumnId,
		TReplicationColumnDelegates<TListItemType> Delegates,
		FText DefaultLabel,
		const int32 Priority,
		const float ColumnWidth = 20.f
		)
	{
		check(Delegates.GetCheckboxStateDelegate.IsBound() && Delegates.OnCheckboxChangedDelegate.IsBound());
		return TReplicationColumn<TListItemType>(
			typename TReplicationColumn<TListItemType>::FArguments()
				.PopulateSearchItems_Lambda([](const auto&, auto&){})
				.GenerateWidgetColumn_Lambda(
					[Delegates](const typename TReplicationColumn<TListItemType>::FBuildArgs& Args)
					{
						return SNew(SCheckBox)
							.ToolTipText_Lambda([GetToolTipDelegate = Delegates.GetToolTipTextDelegate, RowData = Args.RowData]()
							{
								return GetToolTipDelegate.IsBound() ? GetToolTipDelegate.Execute(RowData) : FText::GetEmpty();
							})
							.IsEnabled_Lambda([IsEnabledDelegate = Delegates.IsEnabledDelegate, RowData = Args.RowData]()
							{
								return !IsEnabledDelegate.IsBound() || IsEnabledDelegate.Execute(RowData);
							})
							.IsChecked_Lambda([IsCheckedDelegate = Delegates.GetCheckboxStateDelegate, RowData = Args.RowData]()
							{
								return IsCheckedDelegate.Execute(RowData);
							})
							.OnCheckStateChanged_Lambda([OnChangedDelegate = Delegates.OnCheckboxChangedDelegate, RowData = Args.RowData](ECheckBoxState NewState)
							{
								const bool bIsChecked = NewState == ECheckBoxState::Checked;
								OnChangedDelegate.Execute(bIsChecked, RowData);
							});
					})
				.ColumnSortOrder(Priority),
			SHeaderRow::Column(ColumnId)
				.DefaultLabel(DefaultLabel)
				.FixedWidth(ColumnWidth)
			);
	}
}
